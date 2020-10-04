#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalog/catalog.h"
#include "catalog/postgres/pg_namespace.h"
#include "gtest/gtest.h"
#include "main/db_main.h"
#include "network/itp/itp_protocol_interpreter.h"
#include "storage/garbage_collector_thread.h"
#include "storage/index/index_builder.h"
#include "storage/replication/replication_manager.h"
#include "storage/recovery/replication_log_provider.h"
#include "storage/sql_table.h"
#include "storage/write_ahead_log/log_manager.h"
#include "transaction/transaction_context.h"
#include "transaction/transaction_manager.h"
#include "messenger/connection_destination.h"
#include "test_util/catalog_test_util.h"
#include "test_util/sql_table_test_util.h"
#include "test_util/storage_test_util.h"
#include "test_util/test_harness.h"

// Make sure that if you create additional files, you call unlink on them after the test finishes. Otherwise, repeated
// executions will read old test's data, and the cause of the errors will be hard to identify. Trust me it will drive
// you nuts...
#define LOG_FILE_NAME "./test.log"

namespace terrier::storage {
  class ReplicationTests : public TerrierTest {
  protected:
    std::default_random_engine generator_;

    // Original Components
    std::unique_ptr<DBMain> master_db_main_;
    common::ManagedPointer<transaction::TransactionManager> master_txn_manager_;
    common::ManagedPointer<storage::LogManager> master_log_manager_;
    common::ManagedPointer<storage::BlockStore> master_block_store_;
    common::ManagedPointer<catalog::Catalog> master_catalog_;
    common::ManagedPointer<messenger::Messenger> master_messenger_;

    // Recovery Components
    std::unique_ptr<DBMain> replica_db_main_;
    common::ManagedPointer<transaction::TransactionManager> replica_txn_manager_;
    common::ManagedPointer<transaction::DeferredActionManager> replica_deferred_action_manager_;
    common::ManagedPointer<storage::BlockStore> replica_block_store_;
    common::ManagedPointer<catalog::Catalog> replica_catalog_;
    common::ManagedPointer<common::DedicatedThreadRegistry> replica_thread_registry_;
    common::ManagedPointer<messenger::Messenger> replica_messenger_;

    void SetUp() override {
      // Unlink log file incase one exists from previous test iteration
      unlink(LOG_FILE_NAME);

      master_db_main_ = terrier::DBMain::Builder()
          .SetWalFilePath(LOG_FILE_NAME)
          .SetUseLogging(true)
          .SetUseGC(true)
          .SetUseGCThread(true)
          .SetUseCatalog(true)
          .SetReplicaTCPAddress("tcp://localhost:9022")
          .Build();
      master_txn_manager_ = master_db_main_->GetTransactionLayer()->GetTransactionManager();
      master_log_manager_ = master_db_main_->GetLogManager();
      master_block_store_ = master_db_main_->GetStorageLayer()->GetBlockStore();
      master_catalog_ = master_db_main_->GetCatalogLayer()->GetCatalog();
      master_messenger_ = master_db_main_->GetMessengerLayer()->messenger_owner_->GetMessenger();

      replica_db_main_ = terrier::DBMain::Builder()
          .SetUseThreadRegistry(true)
          .SetUseGC(true)
          .SetUseGCThread(true)
          .SetUseCatalog(true)
          .SetCreateDefaultDatabase(false)
          .SetReplicaTCPAddress("tcp://localhost:9023")
          .Build();
      replica_txn_manager_ = replica_db_main_->GetTransactionLayer()->GetTransactionManager();
      replica_deferred_action_manager_ = replica_db_main_->GetTransactionLayer()->GetDeferredActionManager();
      replica_block_store_ = replica_db_main_->GetStorageLayer()->GetBlockStore();
      replica_catalog_ = replica_db_main_->GetCatalogLayer()->GetCatalog();
      replica_thread_registry_ = replica_db_main_->GetThreadRegistry();
      replica_messenger_ = replica_db_main_->GetMessengerLayer()->messenger_owner_->GetMessenger();
    }

    void TearDown() override {
      // Delete log file
      unlink(LOG_FILE_NAME);
    }

  };

  TEST_F(ReplicationTests, InitializationTest) {
    STORAGE_LOG_ERROR("start");

    common::ManagedPointer<messenger::MessengerLogic> logic{new messenger::MessengerLogic()};
    auto master_destination = messenger::ConnectionDestination::MakeTCP("localhost", 9023);
    master_messenger_->ListenForConnection(master_destination);
    auto replica_destination = messenger::ConnectionDestination::MakeTCP("localhost", 9022);
    replica_messenger_->ListenForConnection(replica_destination);

    auto replica_conn = master_messenger_->MakeConnection(replica_destination, "your master");
    auto master_conn = replica_messenger_->MakeConnection(master_destination, "your replica");
    STORAGE_LOG_ERROR("connected");

    common::ManagedPointer<messenger::ConnectionId> replica_conn_id{&replica_conn};
    common::ManagedPointer<messenger::ConnectionId> master_conn_id{&master_conn};

    std::string msg = "hi im master";
    master_messenger_->SendMessage(replica_conn_id, msg);
    replica_messenger_->SendMessage(master_conn_id, msg);

    STORAGE_LOG_ERROR("end");
  }

}  // namespace terrier::storage
