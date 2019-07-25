#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "catalog/catalog_defs.h"
#include "catalog/schema.h"
#include "parser/expression/abstract_expression.h"
#include "planner/plannodes/abstract_plan_node.h"
#include "planner/plannodes/abstract_scan_plan_node.h"

namespace terrier::planner {

/**
 * Plan node for sequanial table scan
 */
class SeqScanPlanNode : public AbstractScanPlanNode {
 public:
  /**
   * Builder for a sequential scan plan node
   */
  class Builder : public AbstractScanPlanNode::Builder<Builder> {
   public:
    Builder() = default;

    /**
     * Don't allow builder to be copied or moved
     */
    DISALLOW_COPY_AND_MOVE(Builder);

    /**
     * @param oid oid for table to scan
     * @return builder object
     */
    Builder &SetTableOid(catalog::table_oid_t oid) {
      table_oid_ = oid;
      return *this;
    }

    /**
     * @param column_Ids OIDs of columns to scan
     * @return builder object
     */
    Builder &SetColumnIds(std::vector<catalog::col_oid_t> &&column_ids) {
      column_ids_ = std::move(column_ids);
      return *this;
    }

    /**
     * Build the sequential scan plan node
     * @return plan node
     */
    std::shared_ptr<SeqScanPlanNode> Build() {
      return std::shared_ptr<SeqScanPlanNode>(new SeqScanPlanNode(std::move(children_), std::move(output_schema_),
                                                                  scan_predicate_, std::move(column_ids_),
                                                                  is_for_update_, is_parallel_,
                                                                  database_oid_, namespace_oid_, table_oid_));
    }

   protected:
    /**
     * OIDs of columns to scan
     */
    std::vector<catalog::col_oid_t> column_ids_;

    /**
     * OID for table being scanned
     */
    catalog::table_oid_t table_oid_;
  };

 private:
  /**
   * @param children child plan nodes
   * @param output_schema Schema representing the structure of the output of this plan node
   * @param predicate scan predicate
   * @param column_ids OIDs of columns to scan
   * @param is_for_update flag for if scan is for an update
   * @param is_parallel flag for parallel scan
   * @param database_oid database oid for scan
   * @param table_oid OID for table to scan
   */
  SeqScanPlanNode(std::vector<std::shared_ptr<AbstractPlanNode>> &&children,
                  std::shared_ptr<OutputSchema> output_schema, const parser::AbstractExpression *predicate,
                  std::vector<catalog::col_oid_t> &&column_ids,
                  bool is_for_update, bool is_parallel, catalog::db_oid_t database_oid,
                  catalog::namespace_oid_t namespace_oid, catalog::table_oid_t table_oid)
      : AbstractScanPlanNode(std::move(children), std::move(output_schema), predicate, is_for_update, is_parallel,
                             database_oid, namespace_oid),
        column_ids_(std::move(column_ids)), table_oid_(table_oid) {}

 public:
  /**
   * Default constructor used for deserialization
   */
  SeqScanPlanNode() = default;

  DISALLOW_COPY_AND_MOVE(SeqScanPlanNode)

  /**
   * @return the type of this plan node
   */
  PlanNodeType GetPlanNodeType() const override { return PlanNodeType::SEQSCAN; }

  /**
   * @return OIDs of columns to scan
   */
  const std::vector<catalog::col_oid_t> &GetColumnIds() const { return column_ids_; }

  /**
   * @return the OID for the table being scanned
   */
  catalog::table_oid_t GetTableOid() const { return table_oid_; }

  /**
   * @return the hashed value of this plan node
   */
  common::hash_t Hash() const override;

  bool operator==(const AbstractPlanNode &rhs) const override;

  nlohmann::json ToJson() const override;
  void FromJson(const nlohmann::json &j) override;

 private:
  /**
   * OIDs of columns to scan
   */
  std::vector<catalog::col_oid_t> column_ids_;

  /**
   * OID for table being scanned
   */
  catalog::table_oid_t table_oid_;
};

DEFINE_JSON_DECLARATIONS(SeqScanPlanNode);

}  // namespace terrier::planner
