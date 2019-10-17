#include <string>
#include <unordered_map>
#include <vector>
#include "catalog/catalog.h"
#include "catalog/postgres/pg_namespace.h"
#include "gtest/gtest.h"
#include "main/db_main.h"
#include "storage/garbage_collector_thread.h"
#include "storage/index/index_builder.h"
#include "storage/recovery/recovery_manager.h"
#include "storage/recovery/replication_log_provider.h"
#include "storage/sql_table.h"
#include "storage/write_ahead_log/log_manager.h"
#include "transaction/transaction_context.h"
#include "transaction/transaction_manager.h"
#include "util/catalog_test_util.h"
#include "util/sql_table_test_util.h"
#include "util/storage_test_util.h"
#include "util/test_harness.h"

// Make sure that if you create additional files, you call unlink on them after the test finishes. Otherwise, repeated
// executions will read old test's data, and the cause of the errors will be hard to identify. Trust me it will drive
// you nuts...
#define LOG_FILE_NAME "./test.log"

namespace terrier::storage {
class ReplicationTests : public TerrierTest {
 protected:
  // Settings for log manager
  const uint64_t num_log_buffers_ = 100;
  const std::chrono::microseconds log_serialization_interval_{10};
  const std::chrono::milliseconds log_persist_interval_{20};
  const uint64_t log_persist_threshold_ = (1 << 20);  // 1MB

  std::default_random_engine generator_;
  storage::RecordBufferSegmentPool buffer_pool_{2000, 100};
  storage::BlockStore block_store_{100, 100};

  // Settings for gc
  const std::chrono::milliseconds gc_period_{10};

  // Original Components
  common::DedicatedThreadRegistry *thread_registry_;
  LogManager *log_manager_;
  transaction::TimestampManager *timestamp_manager_;
  transaction::DeferredActionManager *deferred_action_manager_;
  transaction::TransactionManager *txn_manager_;
  catalog::Catalog *catalog_;
  storage::GarbageCollector *gc_;
  storage::GarbageCollectorThread *gc_thread_;

  // Recovery Components
  transaction::TimestampManager *recovery_timestamp_manager_;
  transaction::DeferredActionManager *recovery_deferred_action_manager_;
  transaction::TransactionManager *recovery_txn_manager_;
  catalog::Catalog *recovery_catalog_;
  storage::GarbageCollector *recovery_gc_;
  storage::GarbageCollectorThread *recovery_gc_thread_;

  void SetUp() override {
    TerrierTest::SetUp();
    // Unlink log file incase one exists from previous test iteration
    unlink(LOG_FILE_NAME);
    thread_registry_ = new common::DedicatedThreadRegistry(DISABLED);
    log_manager_ =
        new LogManager(LOG_FILE_NAME, num_log_buffers_, log_serialization_interval_, log_persist_interval_,
                       log_persist_threshold_, "", 0, &buffer_pool_, common::ManagedPointer(thread_registry_));
    log_manager_->Start();
    timestamp_manager_ = new transaction::TimestampManager;
    deferred_action_manager_ = new transaction::DeferredActionManager(timestamp_manager_);
    txn_manager_ = new transaction::TransactionManager(timestamp_manager_, deferred_action_manager_, &buffer_pool_,
                                                       true, log_manager_);
    catalog_ = new catalog::Catalog(txn_manager_, &block_store_);
    gc_ = new storage::GarbageCollector(timestamp_manager_, deferred_action_manager_, txn_manager_, DISABLED);
    gc_thread_ = new storage::GarbageCollectorThread(gc_, gc_period_);  // Enable background GC

    recovery_timestamp_manager_ = new transaction::TimestampManager;
    recovery_deferred_action_manager_ = new transaction::DeferredActionManager(recovery_timestamp_manager_);
    recovery_txn_manager_ = new transaction::TransactionManager(
        recovery_timestamp_manager_, recovery_deferred_action_manager_, &buffer_pool_, true, DISABLED);
    recovery_catalog_ = new catalog::Catalog(recovery_txn_manager_, &block_store_);
    recovery_gc_ = new storage::GarbageCollector(recovery_timestamp_manager_, recovery_deferred_action_manager_,
                                                 recovery_txn_manager_, DISABLED);
    recovery_gc_thread_ = new storage::GarbageCollectorThread(recovery_gc_, gc_period_);  // Enable background GC
  }

  void TearDown() override {
    // Delete log file
    unlink(LOG_FILE_NAME);
    TerrierTest::TearDown();

    // Destroy recovered catalog if the test has not cleaned it up already
    if (recovery_catalog_ != nullptr) {
      recovery_catalog_->TearDown();
      delete recovery_gc_thread_;
      StorageTestUtil::FullyPerformGC(recovery_gc_, DISABLED);
    }

    // Destroy original catalog. We need to manually call GC followed by a ForceFlush because catalog deletion can defer
    // events that create new transactions, which then need to be flushed before they can be GC'd.
    catalog_->TearDown();
    delete gc_thread_;
    StorageTestUtil::FullyPerformGC(gc_, log_manager_);
    log_manager_->PersistAndStop();

    delete recovery_gc_;
    delete recovery_catalog_;
    delete recovery_txn_manager_;
    delete recovery_deferred_action_manager_;
    delete recovery_timestamp_manager_;
    delete gc_;
    delete catalog_;
    delete txn_manager_;
    delete deferred_action_manager_;
    delete timestamp_manager_;
    delete log_manager_;
    delete thread_registry_;
  }

  catalog::db_oid_t CreateDatabase(transaction::TransactionContext *txn, catalog::Catalog *catalog,
                                   const std::string &database_name) {
    auto db_oid = catalog->CreateDatabase(txn, database_name, true /* bootstrap */);
    EXPECT_TRUE(db_oid != catalog::INVALID_DATABASE_OID);
    return db_oid;
  }

  // Simulates the system shutting down and restarting
  void ShutdownAndRestartSystem() {
    // Simulate the system "shutting down". Guarantee persist of log records
    delete gc_thread_;
    StorageTestUtil::FullyPerformGC(gc_, log_manager_);
    log_manager_->PersistAndStop();

    // We now "boot up" up the system
    log_manager_->Start();
    gc_thread_ = new storage::GarbageCollectorThread(gc_, gc_period_);
  }
};

// Tests the correctness of the ReplicationLogProvider
// NOLINTNEXTLINE
TEST_F(ReplicationTests, ReplicationLogProviderTest) {
  constexpr uint8_t num_databases = 3;
  std::string database_name = "testdb";
  std::vector<catalog::db_oid_t> db_oids;
  db_oids.reserve(num_databases);

  // Create a bunch of databases and commit, we should see this one after recovery
  auto *txn = txn_manager_->BeginTransaction();
  for (uint8_t db_idx = 0; db_idx < num_databases; db_idx++) {
    db_oids.push_back(CreateDatabase(txn, catalog_, database_name + std::to_string(db_idx)));
  }
  txn_manager_->Commit(txn, transaction::TransactionUtil::EmptyCallback, nullptr);

  // Simulate the system "shutting down"
  ShutdownAndRestartSystem();

  // Initialize provider
  ReplicationLogProvider log_provider(std::chrono::seconds(1));

  // We read the contents into buffers, which we hand off to the replication log provider
  auto log_file_fd = storage::PosixIoWrappers::Open(LOG_FILE_NAME, O_RDONLY);
  while (true) {
    auto buffer = std::make_unique<network::ReadBuffer>();
    auto bytes_read = buffer->FillBufferFrom(log_file_fd);

    if (bytes_read == 0) {
      break;
    } else {
      log_provider.HandBufferToReplication(std::move(buffer));
    }
  }

  RecoveryManager recovery_manager(common::ManagedPointer<storage::AbstractLogProvider>(&log_provider),
                                   common::ManagedPointer(recovery_catalog_), recovery_txn_manager_,
                                   recovery_deferred_action_manager_, common::ManagedPointer(thread_registry_),
                                   &block_store_);
  recovery_manager.StartRecovery();
  recovery_manager.WaitForRecoveryToFinish();

  // Assert the database creations we committed exist
  txn = recovery_txn_manager_->BeginTransaction();
  for (uint8_t i = 0; i < num_databases; i++) {
    EXPECT_EQ(db_oids[i], recovery_catalog_->GetDatabaseOid(txn, database_name + std::to_string(i)));
    EXPECT_TRUE(recovery_catalog_->GetDatabaseCatalog(txn, db_oids[i]));
  }
  recovery_txn_manager_->Commit(txn, transaction::TransactionUtil::EmptyCallback, nullptr);
}

// Tests that recovery is able to process logs that arrive will recovery is happening (similar to an online setting like
// recovery) warning: This test could fail due to the 3 second timeout in the event of terrible thread scheduling or
// disk latency. The chance of this happening however is extremely low with a three second timeout.
// NOLINTNEXTLINE
TEST_F(ReplicationTests, ConcurrentReplicationLogProviderTest) {
  constexpr uint8_t num_databases = 5;
  std::string database_name = "testdb";
  std::vector<catalog::db_oid_t> db_oids;
  db_oids.reserve(num_databases);

  // Create a bunch of databases and commit, we should see this one after recovery
  auto *txn = txn_manager_->BeginTransaction();
  for (uint8_t db_idx = 0; db_idx < num_databases; db_idx++) {
    db_oids.push_back(CreateDatabase(txn, catalog_, database_name + std::to_string(db_idx)));
  }
  txn_manager_->Commit(txn, transaction::TransactionUtil::EmptyCallback, nullptr);

  // Simulate the system "shutting down"
  ShutdownAndRestartSystem();

  // Initialize provider
  ReplicationLogProvider log_provider(std::chrono::seconds(3));

  RecoveryManager recovery_manager(common::ManagedPointer<storage::AbstractLogProvider>(&log_provider),
                                   common::ManagedPointer(recovery_catalog_), recovery_txn_manager_,
                                   recovery_deferred_action_manager_, common::ManagedPointer(thread_registry_),
                                   &block_store_);
  recovery_manager.StartRecovery();

  // We read the contents into buffers in a background thread to simulate logs arriving while recovery is running.
  auto reader_thread = std::thread([&]() {
    auto log_file_fd = storage::PosixIoWrappers::Open(LOG_FILE_NAME, O_RDONLY);
    while (true) {
      auto buffer = std::make_unique<network::ReadBuffer>();
      auto bytes_read = buffer->FillBufferFrom(log_file_fd);

      if (bytes_read == 0) {
        break;
      } else {
        log_provider.HandBufferToReplication(std::move(buffer));
      }
    }
  });
  reader_thread.join();
  recovery_manager.WaitForRecoveryToFinish();

  // Assert the database creations we committed exist
  txn = recovery_txn_manager_->BeginTransaction();
  for (uint8_t i = 0; i < num_databases; i++) {
    EXPECT_EQ(db_oids[i], recovery_catalog_->GetDatabaseOid(txn, database_name + std::to_string(i)));
    EXPECT_TRUE(recovery_catalog_->GetDatabaseCatalog(txn, db_oids[i]));
  }
  recovery_txn_manager_->Commit(txn, transaction::TransactionUtil::EmptyCallback, nullptr);
}

}  // namespace terrier::storage
