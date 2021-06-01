#include "planner/plannodes/tuple_delete_plan_node.h"

#include <memory>
#include <utility>
#include <vector>

#include "common/json.h"
#include "planner/plannodes/output_schema.h"

namespace noisepage::planner {

// TODO(Gus,Wen) Add SetParameters

std::unique_ptr<TupleDeletePlanNode> TupleDeletePlanNode::Builder::Build() {
  return std::unique_ptr<TupleDeletePlanNode>(new TupleDeletePlanNode(std::move(children_), std::make_unique<OutputSchema>(),
                                                            database_oid_, table_oid_, std::move(index_oids_),
                                                            plan_node_id_, tuple_slot_));
}

TupleDeletePlanNode::TupleDeletePlanNode(std::vector<std::unique_ptr<AbstractPlanNode>> &&children,
                               std::unique_ptr<OutputSchema> output_schema, catalog::db_oid_t database_oid,
                               catalog::table_oid_t table_oid,
                               std::vector<catalog::index_oid_t> &&index_oids,
                               plan_node_id_t plan_node_id,
                               storage::TupleSlot tuple_slot)
    : DeletePlanNode(std::move(children), std::move(output_schema), database_oid, table_oid, std::move(index_oids), plan_node_id),
      tuple_slot_(tuple_slot) {}

common::hash_t TupleDeletePlanNode::Hash() const {
  common::hash_t hash = AbstractPlanNode::Hash();

  // Hash database_oid
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::Hash(database_oid_));

  // Hash table_oid
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::Hash(table_oid_));

  // Hash tuple_slot
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::Hash(tuple_slot_));

  for (const auto &index_oid : index_oids_) {
    hash = common::HashUtil::CombineHashes(hash, common::HashUtil::Hash(index_oid));
  }

  return hash;
}

bool TupleDeletePlanNode::operator==(const AbstractPlanNode &rhs) const {
  if (!AbstractPlanNode::operator==(rhs)) return false;

  auto &other = dynamic_cast<const TupleDeletePlanNode &>(rhs);

  // Database OID
  if (database_oid_ != other.database_oid_) return false;

  // Table OID
  if (table_oid_ != other.table_oid_) return false;

  // Tuple Slot
  if (tuple_slot_ != other.tuple_slot_) return false;

  if (index_oids_.size() != other.index_oids_.size()) return false;
  for (int i = 0; i < static_cast<int>(index_oids_.size()); i++) {
    if (index_oids_[i] != other.index_oids_[i]) return false;
  }

  return true;
}

nlohmann::json TupleDeletePlanNode::ToJson() const {
  nlohmann::json j = AbstractPlanNode::ToJson();
  j["database_oid"] = database_oid_;
  j["table_oid"] = table_oid_;
  //j["tuple_slot"] = tuple_slot_;
  j["index_oids"] = index_oids_;
  return j;
}

std::vector<std::unique_ptr<parser::AbstractExpression>> TupleDeletePlanNode::FromJson(const nlohmann::json &j) {
  std::vector<std::unique_ptr<parser::AbstractExpression>> exprs;
  auto e1 = AbstractPlanNode::FromJson(j);
  exprs.insert(exprs.end(), std::make_move_iterator(e1.begin()), std::make_move_iterator(e1.end()));
  database_oid_ = j.at("database_oid").get<catalog::db_oid_t>();
  table_oid_ = j.at("table_oid").get<catalog::table_oid_t>();
  //tuple_slot_ = j.at("tuple_slot").get<storage::TupleSlot>();
  index_oids_ = j.at("index_oids").get<std::vector<catalog::index_oid_t>>();
  return exprs;
}

DEFINE_JSON_BODY_DECLARATIONS(TupleDeletePlanNode);

}  // namespace noisepage::planner
