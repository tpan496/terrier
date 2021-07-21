#include "catalog/database_catalog.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "catalog/catalog_defs.h"
#include "catalog/index_schema.h"
#include "catalog/postgres/builder.h"
#include "catalog/postgres/pg_attribute.h"
#include "catalog/postgres/pg_class.h"
#include "catalog/postgres/pg_constraint.h"
#include "catalog/postgres/pg_index.h"
#include "catalog/postgres/pg_namespace.h"
#include "catalog/postgres/pg_proc.h"
#include "catalog/postgres/pg_statistic.h"
#include "catalog/postgres/pg_type.h"
#include "catalog/schema.h"
#include "common/error/error_code.h"
#include "execution/functions/function_context.h"
#include "nlohmann/json.hpp"
#include "storage/index/index.h"
#include "storage/sql_table.h"
#include "transaction/deferred_action_manager.h"
#include "transaction/transaction_context.h"
#include "transaction/transaction_defs.h"
#include "transaction/transaction_manager.h"
#include "type/type_id.h"

namespace noisepage::catalog {

DatabaseCatalog::DatabaseCatalog(const db_oid_t oid,
                                 const common::ManagedPointer<storage::GarbageCollector> garbage_collector)
    : write_lock_(transaction::INITIAL_TXN_TIMESTAMP),
      db_oid_(oid),
      garbage_collector_(garbage_collector),
      pg_core_(db_oid_),
      pg_type_(db_oid_),
      pg_constraint_(db_oid_),
      pg_language_(db_oid_),
      pg_proc_(db_oid_),
      pg_stat_(db_oid_) {}

void DatabaseCatalog::TearDown(const common::ManagedPointer<transaction::TransactionContext> txn) {
  auto teardown_pg_core = pg_core_.GetTearDownFn(txn, common::ManagedPointer(this));
  auto teardown_pg_constraint = pg_constraint_.GetTearDownFn(txn);
  auto teardown_pg_proc = pg_proc_.GetTearDownFn(txn);

  auto dbc_nuke = [=]() {
    teardown_pg_core();
    teardown_pg_constraint();
    teardown_pg_proc();
  };

  // No new transactions can see these object but there may be deferred index
  // and other operation.  Therefore, we need to defer the deallocation on delete
  txn->RegisterCommitAction([=](transaction::DeferredActionManager *deferred_action_manager) {
    deferred_action_manager->RegisterDeferredAction(dbc_nuke);
  });
}

void DatabaseCatalog::BootstrapPRIs() {
  // TODO(Matt): another potential optimization in the future would be to cache the offsets, rather than the maps
  // themselves (see TPC-C microbenchmark transactions for example). That seems premature right now though.
  pg_core_.BootstrapPRIs();
  pg_type_.BootstrapPRIs();
  pg_constraint_.BootstrapPRIs();
  pg_language_.BootstrapPRIs();
  pg_proc_.BootstrapPRIs();
  pg_stat_.BootstrapPRIs();
}

void DatabaseCatalog::Bootstrap(const common::ManagedPointer<transaction::TransactionContext> txn) {
  BootstrapPRIs();

  bool UNUSED_ATTRIBUTE retval;
  retval = TryLock(txn);
  NOISEPAGE_ASSERT(retval,
                   "Bootstrap operations should not fail to get write-lock: another thread grabbed it early? "
                   "Check recovery logic (most probable cause).");

  pg_core_.Bootstrap(txn, common::ManagedPointer(this));
  pg_type_.Bootstrap(txn, common::ManagedPointer(this));
  pg_constraint_.Bootstrap(txn, common::ManagedPointer(this));
  pg_language_.Bootstrap(txn, common::ManagedPointer(this));
  pg_proc_.Bootstrap(txn, common::ManagedPointer(this));
  pg_stat_.Bootstrap(txn, common::ManagedPointer(this));
}

namespace_oid_t DatabaseCatalog::CreateNamespace(const common::ManagedPointer<transaction::TransactionContext> txn,
                                                 const std::string &name) {
  if (!TryLock(txn)) return INVALID_NAMESPACE_OID;
  const namespace_oid_t ns_oid{next_oid_++};
  return pg_core_.CreateNamespace(txn, name, ns_oid) ? ns_oid : INVALID_NAMESPACE_OID;
}

bool DatabaseCatalog::DeleteNamespace(const common::ManagedPointer<transaction::TransactionContext> txn,
                                      const namespace_oid_t ns_oid) {
  if (!TryLock(txn)) return false;
  return pg_core_.DeleteNamespace(txn, common::ManagedPointer(this), ns_oid);
}

namespace_oid_t DatabaseCatalog::GetNamespaceOid(const common::ManagedPointer<transaction::TransactionContext> txn,
                                                 const std::string &name) {
  return pg_core_.GetNamespaceOid(txn, name);
}

table_oid_t DatabaseCatalog::CreateTable(const common::ManagedPointer<transaction::TransactionContext> txn,
                                         const namespace_oid_t ns, const std::string &name, const Schema &schema) {
  if (!TryLock(txn)) return INVALID_TABLE_OID;
  const table_oid_t table_oid = static_cast<table_oid_t>(next_oid_++);
  return CreateTableEntry(txn, table_oid, ns, name, schema) ? table_oid : INVALID_TABLE_OID;
}

bool DatabaseCatalog::DeleteTable(const common::ManagedPointer<transaction::TransactionContext> txn,
                                  const table_oid_t table) {
  if (!TryLock(txn)) return false;
  // Delete associated entries in pg_statistic.
  {
    auto result = pg_stat_.DeleteColumnStatistics(txn, table);
    if (!result) return false;
  }
  return pg_core_.DeleteTable(txn, common::ManagedPointer(this), table);
}

bool DatabaseCatalog::SetTablePointer(const common::ManagedPointer<transaction::TransactionContext> txn,
                                      const table_oid_t table, const storage::SqlTable *const table_ptr) {
  NOISEPAGE_ASSERT(
      write_lock_.load() == txn->FinishTime(),
      "Setting the object's pointer should only be done after successful DDL change request. i.e. this txn "
      "should already have the lock.");
  // We need to double-defer the deletion because there may be subsequent undo records into this table that need to be
  // GCed before we can safely delete this.  Specifically, the following ordering results in a use-after-free when the
  // unlink step dereferences a deleted SqlTable if the delete is only a single deferral:
  //
  //            Txn           |          Log Manager           |    GC
  // ---------------------------------------------------------------------------
  // CreateDatabase           |                                |
  // ABORT                    |                                |
  // Execute abort actions    |                                |
  //                          |                                | ENTER
  // Checkout ABORT timestamp |                                |
  //                          | Remove ABORT from running txns |
  //                          |                                | Read oldest running timestamp
  //                          |                                | Unlink (not unlinked because abort is "visible")
  //                          |                                | Process defers (deletes table)
  //                          |                                | EXIT
  //                          |                                | ENTER
  //                          |                                | Unlink (ASAN crashes process for use-after-free)
  //
  // TODO(John,Ling): This needs to become a triple deferral when DAF gets merged in order to maintain
  // assurances about object lifetimes in a multi-threaded GC situation.
  txn->RegisterAbortAction([=](transaction::DeferredActionManager *deferred_action_manager) {
    deferred_action_manager->RegisterDeferredAction(
        [=]() { deferred_action_manager->RegisterDeferredAction([=]() { delete table_ptr; }); });
  });
  return SetClassPointer(txn, table, table_ptr, postgres::PgClass::REL_PTR.oid_);
}

bool DatabaseCatalog::SetIndexPointer(const common::ManagedPointer<transaction::TransactionContext> txn,
                                      const index_oid_t index, storage::index::Index *const index_ptr) {
  NOISEPAGE_ASSERT(
      write_lock_.load() == txn->FinishTime(),
      "Setting the object's pointer should only be done after successful DDL change request. i.e. this txn "
      "should already have the lock.");
  if (index_ptr->Type() == storage::index::IndexType::BWTREE) {
    garbage_collector_->RegisterIndexForGC(common::ManagedPointer(index_ptr));
  }
  // This needs to be deferred because if any items were subsequently inserted into this index, they will have deferred
  // abort actions that will be above this action on the abort stack.  The defer ensures we execute after them.
  txn->RegisterAbortAction(
      [=, garbage_collector{garbage_collector_}](transaction::DeferredActionManager *deferred_action_manager) {
        if (index_ptr->Type() == storage::index::IndexType::BWTREE) {
          garbage_collector->UnregisterIndexForGC(common::ManagedPointer(index_ptr));
        }
        deferred_action_manager->RegisterDeferredAction([=]() { delete index_ptr; });
      });
  return SetClassPointer(txn, index, index_ptr, postgres::PgClass::REL_PTR.oid_);
}

table_oid_t DatabaseCatalog::GetTableOid(const common::ManagedPointer<transaction::TransactionContext> txn,
                                         const namespace_oid_t ns, const std::string &name) {
  const auto oid_pair = pg_core_.GetClassOidKind(txn, ns, name);
  if (oid_pair.first == catalog::NULL_OID || oid_pair.second != postgres::PgClass::RelKind::REGULAR_TABLE) {
    // User called GetTableOid on an object that doesn't have type REGULAR_TABLE
    return INVALID_TABLE_OID;
  }
  return table_oid_t(oid_pair.first);
}

index_oid_t DatabaseCatalog::GetIndexOid(const common::ManagedPointer<transaction::TransactionContext> txn,
                                         namespace_oid_t ns, const std::string &name) {
  const auto oid_pair = pg_core_.GetClassOidKind(txn, ns, name);
  if (oid_pair.first == NULL_OID || oid_pair.second != postgres::PgClass::RelKind::INDEX) {
    // User called GetIndexOid on an object that doesn't have type INDEX
    return INVALID_INDEX_OID;
  }
  return index_oid_t(oid_pair.first);
}

common::ManagedPointer<storage::SqlTable> DatabaseCatalog::GetTable(
    const common::ManagedPointer<transaction::TransactionContext> txn, const table_oid_t table) {
  const auto ptr_pair = pg_core_.GetClassPtrKind(txn, table.UnderlyingValue());
  if (ptr_pair.second != postgres::PgClass::RelKind::REGULAR_TABLE) {
    // User called GetTable with an OID for an object that doesn't have type REGULAR_TABLE
    return common::ManagedPointer<storage::SqlTable>(nullptr);
  }
  return common::ManagedPointer(reinterpret_cast<storage::SqlTable *>(ptr_pair.first));
}

common::ManagedPointer<storage::index::Index> DatabaseCatalog::GetIndex(
    const common::ManagedPointer<transaction::TransactionContext> txn, index_oid_t index) {
  const auto ptr_pair = pg_core_.GetClassPtrKind(txn, index.UnderlyingValue());
  if (ptr_pair.second != postgres::PgClass::RelKind::INDEX) {
    // User called GetTable with an OID for an object that doesn't have type INDEX
    return common::ManagedPointer<storage::index::Index>(nullptr);
  }
  return common::ManagedPointer(reinterpret_cast<storage::index::Index *>(ptr_pair.first));
}

const Schema &DatabaseCatalog::GetSchema(const common::ManagedPointer<transaction::TransactionContext> txn,
                                         const table_oid_t table) {
  const auto ptr_pair = pg_core_.GetClassSchemaPtrKind(txn, table.UnderlyingValue());
  NOISEPAGE_ASSERT(ptr_pair.first != nullptr, "Schema pointer shouldn't ever be NULL under current catalog semantics.");
  NOISEPAGE_ASSERT(ptr_pair.second == postgres::PgClass::RelKind::REGULAR_TABLE,
                   "Requested a table schema for a non-table");
  return *reinterpret_cast<Schema *>(ptr_pair.first);
}

const IndexSchema &DatabaseCatalog::GetIndexSchema(const common::ManagedPointer<transaction::TransactionContext> txn,
                                                   index_oid_t index) {
  auto ptr_pair = pg_core_.GetClassSchemaPtrKind(txn, index.UnderlyingValue());
  NOISEPAGE_ASSERT(ptr_pair.first != nullptr, "Schema pointer shouldn't ever be NULL under current catalog semantics.");
  NOISEPAGE_ASSERT(ptr_pair.second == postgres::PgClass::RelKind::INDEX, "Requested an index schema for a non-index");
  return *reinterpret_cast<IndexSchema *>(ptr_pair.first);
}

bool DatabaseCatalog::RenameTable(const common::ManagedPointer<transaction::TransactionContext> txn,
                                  const table_oid_t table, const std::string &name) {
  if (!TryLock(txn)) return false;

  return pg_core_.RenameTable(txn, common::ManagedPointer(this), table, name);
}

bool DatabaseCatalog::UpdateSchema(const common::ManagedPointer<transaction::TransactionContext> txn,
                                   const table_oid_t table, Schema *const new_schema) {
  if (!TryLock(txn)) return false;
  // TODO(John): Implement
  NOISEPAGE_ASSERT(false, "Not implemented");
  return false;
}

template <typename Column, typename ClassOid, typename ColOid>
std::vector<Column> DatabaseCatalog::GetColumns(const common::ManagedPointer<transaction::TransactionContext> txn,
                                                ClassOid class_oid) {
  return pg_core_.GetColumns<Column, ClassOid, ColOid>(txn, class_oid);
}

bool DatabaseCatalog::DeleteIndexes(const common::ManagedPointer<transaction::TransactionContext> txn,
                                    const table_oid_t table) {
  if (!TryLock(txn)) return false;
  // Get the indexes
  const auto index_oids = GetIndexOids(txn, table);
  // Delete all indexes
  for (const auto index_oid : index_oids) {
    auto result = DeleteIndex(txn, index_oid);
    if (!result) {
      // write-write conflict. Someone beat us to this operation.
      // TODO(WAN): Wait, can you end up deleting only some of the indexes?
      return false;
    }
  }
  return true;
}

std::vector<constraint_oid_t> DatabaseCatalog::GetConstraints(
    const common::ManagedPointer<transaction::TransactionContext> txn, table_oid_t table) {
  // TODO(John): Implement
  NOISEPAGE_ASSERT(false, "Not implemented");
  return {};
}

index_oid_t DatabaseCatalog::CreateIndex(const common::ManagedPointer<transaction::TransactionContext> txn,
                                         namespace_oid_t ns, const std::string &name, table_oid_t table,
                                         const IndexSchema &schema) {
  if (!TryLock(txn)) return INVALID_INDEX_OID;
  const index_oid_t index_oid = static_cast<index_oid_t>(next_oid_++);
  return CreateIndexEntry(txn, ns, table, index_oid, name, schema) ? index_oid : INVALID_INDEX_OID;
}

bool DatabaseCatalog::DeleteIndex(const common::ManagedPointer<transaction::TransactionContext> txn,
                                  index_oid_t index) {
  if (!TryLock(txn)) return false;
  return pg_core_.DeleteIndex(txn, common::ManagedPointer(this), index);
}

std::vector<index_oid_t> DatabaseCatalog::GetIndexOids(
    const common::ManagedPointer<transaction::TransactionContext> txn, table_oid_t table) {
  return pg_core_.GetIndexOids(txn, table);
}

std::vector<std::pair<common::ManagedPointer<storage::index::Index>, const IndexSchema &>> DatabaseCatalog::GetIndexes(
    const common::ManagedPointer<transaction::TransactionContext> txn, table_oid_t table) {
  return pg_core_.GetIndexes(txn, table);
}

type_oid_t DatabaseCatalog::GetTypeOidForType(const type::TypeId type) {
  // TODO(WAN): WARNING! Do not change this seeing PgCoreImpl::MakeColumn and PgCoreImpl::CreateColumn.
  return type_oid_t(static_cast<uint8_t>(type));
}

void DatabaseCatalog::BootstrapTable(const common::ManagedPointer<transaction::TransactionContext> txn,
                                     const table_oid_t table_oid, const namespace_oid_t ns_oid, const std::string &name,
                                     const Schema &schema, const common::ManagedPointer<storage::SqlTable> table_ptr) {
  bool UNUSED_ATTRIBUTE retval;
  retval = CreateTableEntry(txn, table_oid, ns_oid, name, schema);
  NOISEPAGE_ASSERT(retval, "Bootstrap of table should not fail (creating table entry).");
  retval = SetTablePointer(txn, table_oid, table_ptr.Get());
  NOISEPAGE_ASSERT(retval, "Bootstrap of table should not fail (setting table pointer).");
}

void DatabaseCatalog::BootstrapIndex(const common::ManagedPointer<transaction::TransactionContext> txn,
                                     const namespace_oid_t ns_oid, const table_oid_t table_oid,
                                     const index_oid_t index_oid, const std::string &name, const IndexSchema &schema,
                                     const common::ManagedPointer<storage::index::Index> index_ptr) {
  bool UNUSED_ATTRIBUTE retval;
  retval = CreateIndexEntry(txn, ns_oid, table_oid, index_oid, name, schema);
  NOISEPAGE_ASSERT(retval, "Bootstrap of index should not fail (creating index entry).");
  retval = SetIndexPointer(txn, index_oid, index_ptr.Get());
  NOISEPAGE_ASSERT(retval, "Bootstrap of index should not fail (setting index pointer).");
}

bool DatabaseCatalog::CreateTableEntry(const common::ManagedPointer<transaction::TransactionContext> txn,
                                       const table_oid_t table_oid, const namespace_oid_t ns_oid,
                                       const std::string &name, const Schema &schema) {
  if (pg_core_.CreateTableEntry(txn, table_oid, ns_oid, name, schema)) {
    CreateTableStatisticEntry(txn, table_oid, GetSchema(txn, table_oid));
    return true;
  }
  return false;
}

void DatabaseCatalog::CreateTableStatisticEntry(const common::ManagedPointer<transaction::TransactionContext> txn,
                                                const table_oid_t table_oid, const Schema &schema) {
  // Create associated entries in pg_statistic.
  for (const auto &col : schema.GetColumns()) {
    pg_stat_.CreateColumnStatistic(txn, table_oid, col.Oid(), col);
  }
}

bool DatabaseCatalog::CreateIndexEntry(const common::ManagedPointer<transaction::TransactionContext> txn,
                                       const namespace_oid_t ns_oid, const table_oid_t table_oid,
                                       const index_oid_t index_oid, const std::string &name,
                                       const IndexSchema &schema) {
  return pg_core_.CreateIndexEntry(txn, ns_oid, table_oid, index_oid, name, schema);
}

bool DatabaseCatalog::SetFunctionContextPointer(common::ManagedPointer<transaction::TransactionContext> txn,
                                                proc_oid_t proc_oid,
                                                const execution::functions::FunctionContext *func_context) {
  NOISEPAGE_ASSERT(
      write_lock_.load() == txn->FinishTime(),
      "Setting the object's pointer should only be done after successful DDL change request. i.e. this txn "
      "should already have the lock.");

  // The catalog owns this pointer now, so if the txn ends up aborting, we need to make sure it gets freed.
  txn->RegisterAbortAction([=](transaction::DeferredActionManager *deferred_action_manager) {
    deferred_action_manager->RegisterDeferredAction([=]() { delete func_context; });
  });

  return pg_proc_.SetProcCtxPtr(txn, proc_oid, func_context);
}

common::ManagedPointer<execution::functions::FunctionContext> DatabaseCatalog::GetFunctionContext(
    common::ManagedPointer<transaction::TransactionContext> txn, proc_oid_t proc_oid) {
  auto proc_ctx = pg_proc_.GetProcCtxPtr(txn, proc_oid);
  NOISEPAGE_ASSERT(proc_ctx != nullptr, "Dynamically added UDFs are currently not supported.");
  return proc_ctx;
}

std::unique_ptr<optimizer::ColumnStatsBase> DatabaseCatalog::GetColumnStatistics(
    common::ManagedPointer<transaction::TransactionContext> txn, table_oid_t table_oid, col_oid_t col_oid) {
  return pg_stat_.GetColumnStatistics(txn, common::ManagedPointer(this), table_oid, col_oid);
}

optimizer::TableStats DatabaseCatalog::GetTableStatistics(common::ManagedPointer<transaction::TransactionContext> txn,
                                                          table_oid_t table_oid) {
  return pg_stat_.GetTableStatistics(txn, common::ManagedPointer(this), table_oid);
}

bool DatabaseCatalog::TryLock(const common::ManagedPointer<transaction::TransactionContext> txn) {
  auto current_val = write_lock_.load();

  const transaction::timestamp_t txn_id = txn->FinishTime();     // this is the uncommitted txn id
  const transaction::timestamp_t start_time = txn->StartTime();  // this is the unchanging start time of the txn

  const bool already_hold_lock = current_val == txn_id;
  if (already_hold_lock) return true;

  const bool owned_by_other_txn = !transaction::TransactionUtil::Committed(current_val);
  const bool newer_committed_version = transaction::TransactionUtil::Committed(current_val) &&
                                       transaction::TransactionUtil::NewerThan(current_val, start_time);

  if (owned_by_other_txn || newer_committed_version) {
    txn->SetMustAbort();  // though no changes were written to the storage layer, we'll treat this as a DDL change
                          // failure and force the txn to rollback
    return false;
  }

  if (write_lock_.compare_exchange_strong(current_val, txn_id)) {
    // acquired the lock
    auto *const write_lock = &write_lock_;
    txn->RegisterCommitAction([=]() -> void { write_lock->store(txn->FinishTime()); });
    txn->RegisterAbortAction([=]() -> void { write_lock->store(current_val); });
    return true;
  }
  txn->SetMustAbort();  // though no changes were written to the storage layer, we'll treat this as a DDL change failure
                        // and force the txn to rollback
  return false;
}

language_oid_t DatabaseCatalog::CreateLanguage(const common::ManagedPointer<transaction::TransactionContext> txn,
                                               const std::string &lanname) {
  if (!TryLock(txn)) return INVALID_LANGUAGE_OID;
  auto oid = language_oid_t{next_oid_++};
  return pg_language_.CreateLanguage(txn, lanname, oid) ? oid : INVALID_LANGUAGE_OID;
}

bool DatabaseCatalog::DropLanguage(const common::ManagedPointer<transaction::TransactionContext> txn,
                                   language_oid_t oid) {
  if (!TryLock(txn)) return false;
  return pg_language_.DropLanguage(txn, oid);
}

language_oid_t DatabaseCatalog::GetLanguageOid(const common::ManagedPointer<transaction::TransactionContext> txn,
                                               const std::string &lanname) {
  return pg_language_.GetLanguageOid(txn, lanname);
}

proc_oid_t DatabaseCatalog::CreateProcedure(common::ManagedPointer<transaction::TransactionContext> txn,
                                            const std::string &procname, language_oid_t language_oid,
                                            namespace_oid_t procns, const std::vector<std::string> &args,
                                            const std::vector<type_oid_t> &arg_types,
                                            const std::vector<type_oid_t> &all_arg_types,
                                            const std::vector<postgres::PgProc::ArgModes> &arg_modes,
                                            type_oid_t rettype, const std::string &src, bool is_aggregate) {
  if (!TryLock(txn)) return INVALID_PROC_OID;
  proc_oid_t oid = proc_oid_t{next_oid_++};
  return pg_proc_.CreateProcedure(txn, oid, procname, language_oid, procns, args, arg_types, all_arg_types, arg_modes,
                                  rettype, src, is_aggregate)
             ? oid
             : INVALID_PROC_OID;
}

bool DatabaseCatalog::DropProcedure(const common::ManagedPointer<transaction::TransactionContext> txn,
                                    proc_oid_t proc) {
  if (!TryLock(txn)) return false;
  return pg_proc_.DropProcedure(txn, proc);
}

proc_oid_t DatabaseCatalog::GetProcOid(common::ManagedPointer<transaction::TransactionContext> txn,
                                       namespace_oid_t procns, const std::string &procname,
                                       const std::vector<type_oid_t> &arg_types) {
  return pg_proc_.GetProcOid(txn, common::ManagedPointer(this), procns, procname, arg_types);
}

template <typename ClassOid, typename Ptr>
bool DatabaseCatalog::SetClassPointer(const common::ManagedPointer<transaction::TransactionContext> txn,
                                      const ClassOid oid, const Ptr *const pointer, const col_oid_t class_col) {
  return pg_core_.SetClassPointer(txn, oid, pointer, class_col);
}

bool DatabaseCatalog::VerifyTableInsertConstraint(common::ManagedPointer<transaction::TransactionContext> txn, table_oid_t table, storage::ProjectedRow *pr) {
  // TODO: We do not know if this needs a lock or not
  /*if(!TryLock(txn)) return false;
  auto *const buffer = common::AllocationUtil::AllocateAligned(pg_constraints_all_cols_pri_.ProjectedRowSize());
  auto con_pri = pg_constraint_.constraints_table_index_->GetProjectedRowInitializer();
  auto *key_pr = con_pri.InitializeRow(buffer);
  auto *const con_table_oid_ptr = key_pr->AccessForceNotNull(0);
  *(reinterpret_cast<table_oid_t *>(con_table_oid_ptr)) = table;
  std::vector<storage::TupleSlot> index_scan_results;
  pg_constraint_.constraints_table_index_->ScanKey(*txn, *key_pr, &index_scan_results);
  // If we found no indexes, return an empty list
  if (index_scan_results.empty()) {
    delete[] buffer;
    return true;
  }
  auto *select_pr = pg_constraints_all_cols_pri_.InitializeRow(buffer);
  std::vector<postgres::PgConstraint> constraints;
  constraints.reserve(index_scan_results.size());
  auto *const child_buffer = common::AllocationUtil::AllocateAligned(pg_fk_constraints_all_cols_pri_.ProjectedRowSize());
  for (auto &slot : index_scan_results) {
    const auto result UNUSED_ATTRIBUTE = constraints_->Select(txn, slot, select_pr);
    TERRIER_ASSERT(result, "Index already verified visibility. This shouldn't fail.");
    auto offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONOID_COL_OID]);
    constraint_oid_t con_oid = *(reinterpret_cast<constraint_oid_t *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONNAME_COL_OID]);
    storage::VarlenEntry &con_name_varlen = *(reinterpret_cast<storage::VarlenEntry *>(offset));
    std::string con_name = VarlentoString(con_name_varlen);

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONNAMESPACE_COL_OID]);
    namespace_oid_t con_namespace = *(reinterpret_cast<namespace_oid_t *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONTYPE_COL_OID]);
    postgres::ConstraintType con_type = *(reinterpret_cast<postgres::ConstraintType *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONDEFERRABLE_COL_OID]);
    bool con_deferrable = *(reinterpret_cast<bool *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONDEFERRED_COL_OID]);
    bool con_deferred = *(reinterpret_cast<bool *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONVALIDATED_COL_OID]);
    bool con_validated = *(reinterpret_cast<bool *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONRELID_COL_OID]);
    table_oid_t con_rel = *(reinterpret_cast<table_oid_t *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONINDID_COL_OID]);
    index_oid_t con_index = *(reinterpret_cast<index_oid_t *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONFRELID_COL_OID]);
    storage::VarlenEntry &con_frelid_varlen = *(reinterpret_cast<storage::VarlenEntry *>(offset));
    std::string confrel_str = VarlentoString(con_frelid_varlen);

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONCOL_COL_OID]);
    auto con_col_varlen = *(reinterpret_cast<storage::VarlenEntry *>(offset));
    std::string con_col_str = VarlentoString(con_col_varlen);

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONCHECK_COL_OID]);
    constraint_oid_t con_check = *(reinterpret_cast<constraint_oid_t *>(offset));

    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONEXCLUSION_COL_OID]);
    constraint_oid_t con_exclusion = *(reinterpret_cast<constraint_oid_t *>(offset));
    // TODO: resolve CONBIN
    //    offset = select_pr->AccessForceNotNull(pg_constraints_all_cols_prm_[postgres::CONBIN_COL_OID]);
    //    auto con_oid = *(reinterpret_cast<planner::AbstractPlanNode **>(offset));
    postgres::PgConstraint con_obj = PG_Constraint(this, con_oid, con_name, con_namespace, con_type, con_deferrable, con_deferred, con_validated,
        con_rel, con_index, con_col_str);
    bool verify_res = true;
    // fill metadata depending on the type of the constraint
    if (con_obj.contype_ == postgres::ConstraintType::UNIQUE ||
        con_obj.contype_ == postgres::ConstraintType::PRIMARY_KEY) {
      verify_res = VerifyUniquePKConstraint(txn, con_obj, pr);
    }
    else if(con_obj.contype_ == postgres::ConstraintType::FOREIGN_KEY) {
        std::vector<constraint_oid_t> con_ids = SpaceSeparatedOidToVector<constraint_oid_t>(confrel_str);
        auto fk_index_pri = fk_constraints_oid_index_->GetProjectedRowInitializer();
        for (constraint_oid_t fk_id : con_ids) {
          // Find all entries for the given table using the index
          auto *fk_index_pr = fk_index_pri.InitializeRow(child_buffer);
          auto *const fk_oid_oid_ptr = fk_index_pr->AccessForceNotNull(0);
          *(reinterpret_cast<constraint_oid_t *>(fk_oid_oid_ptr)) = fk_id;
          std::vector<storage::TupleSlot> fk_index_scan_results;
          fk_constraints_oid_index_->ScanKey(*txn, *fk_index_pr, &fk_index_scan_results);

          // If we found no indexes, return an empty list
          TERRIER_ASSERT(!fk_index_scan_results.empty(),
              "if there is a foreign key in pg_constraint, then fk_constraint table has to have record in index");
          TERRIER_ASSERT(fk_index_scan_results.size() == 1,
                         "one fk_id stored in pg-constraint confrelid array should only have one entry in fk_constraint");
          auto *fk_select_pr = pg_fk_constraints_all_cols_pri_.InitializeRow(child_buffer);
          for (auto &fk_slot : fk_index_scan_results) {
            const auto fk_result UNUSED_ATTRIBUTE = fk_constraints_->Select(txn, fk_slot, fk_select_pr);
            TERRIER_ASSERT(fk_result, "Index already verified visibility. This shouldn't fail.");
            auto fk_offset = pg_fk_constraints_all_cols_prm_[postgres::FKID_COL_OID];
            constraint_oid_t fk_oid = *(reinterpret_cast<constraint_oid_t *>(fk_select_pr->AccessForceNotNull(fk_offset)));

            fk_offset = pg_fk_constraints_all_cols_prm_[postgres::FKCONID_COL_OID];
            TERRIER_ASSERT(*(reinterpret_cast<constraint_oid_t *>(fk_select_pr->AccessForceNotNull(fk_offset))) == con_oid, "entry selected from fk_constraint should have same con_id as the one called from pg_constraint");

            fk_offset = pg_fk_constraints_all_cols_prm_[postgres::FKREFTABLE_COL_OID];
            table_oid_t fk_ref_table = *(reinterpret_cast<table_oid_t *>(fk_select_pr->AccessForceNotNull(fk_offset)));

            fk_offset = pg_fk_constraints_all_cols_prm_[postgres::FKSRCTABLE_COL_OID];
            TERRIER_ASSERT(*(reinterpret_cast<table_oid_t *>(fk_select_pr->AccessForceNotNull(fk_offset))) == con_rel, "fk_constraint src table should be the same as con_rel");

            fk_offset = pg_fk_constraints_all_cols_prm_[postgres::FKREFCOL_COL_OID];
            col_oid_t fk_ref_col = *(reinterpret_cast<col_oid_t *>(fk_select_pr->AccessForceNotNull(fk_offset)));

            fk_offset = pg_fk_constraints_all_cols_prm_[postgres::FKSRCCOL_COL_OID];
            col_oid_t fk_src_col = *(reinterpret_cast<col_oid_t *>(fk_select_pr->AccessForceNotNull(fk_offset)));

            fk_offset = pg_fk_constraints_all_cols_prm_[postgres::FKUPDATEACTION_COL_OID];
            postgres::FKActionType update_action = *(reinterpret_cast<postgres::FKActionType *>(fk_select_pr->AccessForceNotNull(fk_offset)));

            fk_offset = pg_fk_constraints_all_cols_prm_[postgres::FKDELETEACTION_COL_OID];
            postgres::FKActionType delete_action = *(reinterpret_cast<postgres::FKActionType *>(fk_select_pr->AccessForceNotNull(fk_offset)));
            con_obj.AddFKConstraintMetadata(fk_ref_table, fk_oid, fk_src_col, fk_ref_col, update_action, delete_action);
          }
        }
        verify_res = VerifyFKConstraint(txn, con_obj, pr);
      }
      else if (con_obj.contype_ == postgres::ConstraintType::CHECK){
        // TODO: implement support for check constraint
        con_obj.AddCheckConstraintMetaData(con_check);
        verify_res = VerifyCheckConstraint(con_obj);
      }
      else if (con_obj.contype_ == postgres::ConstraintType::EXCLUSION){
        // TODO: implement support for exclusion constraint
        con_obj.AddExclusionConstraintMetadata(con_exclusion);
        verify_res = VerifyExclusionConstraint(con_obj);
      }

    if (!verify_res) {
      delete[] child_buffer;
      delete[] buffer;
      txn->SetMustAbort();
      return false;
    }
  }
  delete[] child_buffer;
  delete[] buffer;*/
  return true;
}


// Template instantiations.

#define DEFINE_SET_CLASS_POINTER(ClassOid, Ptr)                                                                        \
  template bool DatabaseCatalog::SetClassPointer<ClassOid, Ptr>(                                                       \
      const common::ManagedPointer<transaction::TransactionContext> txn, const ClassOid oid, const Ptr *const pointer, \
      const col_oid_t class_col);
#define DEFINE_GET_COLUMNS(Column, ClassOid, ColOid)                                  \
  template std::vector<Column> DatabaseCatalog::GetColumns<Column, ClassOid, ColOid>( \
      const common::ManagedPointer<transaction::TransactionContext> txn, const ClassOid class_oid);

DEFINE_SET_CLASS_POINTER(table_oid_t, storage::SqlTable);
DEFINE_SET_CLASS_POINTER(table_oid_t, Schema);
DEFINE_SET_CLASS_POINTER(index_oid_t, storage::index::Index);
DEFINE_SET_CLASS_POINTER(index_oid_t, IndexSchema);
DEFINE_GET_COLUMNS(Schema::Column, table_oid_t, col_oid_t);
DEFINE_GET_COLUMNS(IndexSchema::Column, index_oid_t, indexkeycol_oid_t);

#undef DEFINE_SET_CLASS_POINTER
#undef DEFINE_GET_COLUMNS

}  // namespace noisepage::catalog
