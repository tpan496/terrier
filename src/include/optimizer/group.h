#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "optimizer/group_expression.h"
#include "optimizer/operator_node_contents.h"
#include "optimizer/optimizer_defs.h"
#include "optimizer/property.h"
#include "optimizer/property_set.h"
#include "optimizer/statistics/column_stats.h"

namespace noisepage::optimizer {

class GroupExpression;

/**
 * Group collects together GroupExpressions that represent logically
 * equivalent expression trees.  A Group tracks both logical and
 * physical GroupExpressions.
 */
class Group {
 public:
  /**
   * Constructor for a group
   * @param id ID of the Group
   * @param table_aliases Set of table aliases used by the Group
   */
  Group(group_id_t id, std::unordered_set<std::string> table_aliases)
      : id_(id), table_aliases_(std::move(table_aliases)), has_explored_(false) {}

  /**
   * Destructor
   * Deletes all owned pointers which includes PropertySet.
   * Deletes all GroupExpressions owned.
   */
  ~Group();

  /**
   * Adds an expression to the group
   *
   * If the GroupExpression is generated by applying a
   * property enforcer, we add them to enforced_exprs_
   * which will not be enumerated during OptimizeExpression.
   *
   * The GroupExpression will be owned by this Group
   */
  void AddExpression(GroupExpression *expr, bool enforced);

  /**
   * Sets metadata for the cost of an expression w.r.t PropertySet
   * @param expr GroupExpression whose metadata is to be updated
   * @param cost Cost
   * @param properties PropertySet satisfied by GroupExpression
   * @returns TRUE if expr recorded
   *
   * @note properties becomes owned by Group!
   * @note properties lifetime after not guaranteed
   */
  bool SetExpressionCost(GroupExpression *expr, double cost, PropertySet *properties);

  /**
   * Gets the best expression existing for a group satisfying
   * a certain PropertySet. HasExpressions() should return TRUE.
   *
   * @param properties PropertySet to use for search
   * @returns GroupExpression satisfing the PropertySet
   */
  GroupExpression *GetBestExpression(PropertySet *properties);

  /**
   * Determines whether or not a lowest cost expression exists
   * for this group that satisfies a certain PropertySet.
   *
   * @param properties PropertySet to use for search
   * @returns Boolean indicating whether expression is found
   */
  bool HasExpressions(PropertySet *properties) const;

  /**
   * @returns table aliases for this group
   */
  const std::unordered_set<std::string> &GetTableAliases() const { return table_aliases_; }

  /**
   * Gets the vector of all logical expressions
   * @returns Logical expressions belonging to this group
   */
  const std::vector<GroupExpression *> &GetLogicalExpressions() const { return logical_expressions_; }

  /**
   * Gets the vector of all physical expressions
   *@returns Physical expressions belonging to this group
   */
  const std::vector<GroupExpression *> &GetPhysicalExpressions() const { return physical_expressions_; }

  /**
   * Gets the cost lower bound
   * @note currently not set anywhere...
   * @returns lower cost bound
   */
  double GetCostLB() { return cost_lower_bound_; }

  /**
   * Sets a flag indicating the group has been explored
   */
  void SetExplorationFlag() { has_explored_ = true; }

  /**
   * Checks whether this group has been explored yet.
   * @returns TRUE if explored
   */
  bool HasExplored() { return has_explored_; }

  /**
   * Sets Number of rows
   * @param num_rows Number of rows
   */
  void SetNumRows(int num_rows) { num_rows_ = num_rows; }

  /**
   * Gets the estimated cardinality in # rows
   * @returns # rows estimated
   */
  int GetNumRows() { return num_rows_; }

  /**
   * Get stats for a column
   * @param column_name Column to get stats for
   */
  common::ManagedPointer<ColumnStats> GetStats(const std::string &column_name) {
    NOISEPAGE_ASSERT(stats_.count(column_name) != 0U, "Column Stats missing");
    return common::ManagedPointer<ColumnStats>(stats_[column_name].get());
  }

  /**
   * Checks if there are stats for a column
   * @param column_name Column to check
   */
  bool HasColumnStats(const std::string &column_name) { return stats_.count(column_name) != 0U; }

  /**
   * Add stats for a column
   * @param column_name Column to add stats
   * @param stats Stats to add
   */
  void AddStats(const std::string &column_name, std::unique_ptr<ColumnStats> stats) {
    stats_[column_name] = std::move(stats);
  }

  /**
   * Gets this Group's GroupID
   * @returns GroupID of this group
   */
  group_id_t GetID() const { return id_; }

  /**
   * Erase the logical expression stored by this group.
   * Should only be called during rewrite phase.
   */
  void EraseLogicalExpression();

  /**
   * Gets the logical expression stored by this group.
   * Should only be called during rewrite phase.
   */
  GroupExpression *GetLogicalExpression() {
    NOISEPAGE_ASSERT(logical_expressions_.size() == 1, "There should exist only 1 logical expression");
    NOISEPAGE_ASSERT(physical_expressions_.empty(), "No physical expressions should be present");
    return logical_expressions_[0];
  }

 private:
  /**
   * Group ID
   */
  group_id_t id_;

  /**
   * All the table alias this group represents. This will not change once create
   * TODO(boweic) Do not use string, store table alias id
   */
  std::unordered_set<std::string> table_aliases_;

  /**
   * Mapping from property requirements to a pair of (cost, GroupExpression)
   */
  std::unordered_map<PropertySet *, std::tuple<double, GroupExpression *>, PropSetPtrHash, PropSetPtrEq>
      lowest_cost_expressions_;

  /**
   * Whether equivalent logical expressions have been explored for this group
   */
  bool has_explored_;

  /**
   * Vector of equivalent logical expressions
   */
  std::vector<GroupExpression *> logical_expressions_;

  /**
   * Vector of equivalent physical expressions
   */
  std::vector<GroupExpression *> physical_expressions_;

  /**
   * Vector of expressions with properties enforced
   */
  std::vector<GroupExpression *> enforced_exprs_;

  /**
   * Stats (added lazily)
   * TODO(boweic):
   * 1. Use table alias ID  + column offset to identify column
   * 2. Support stats for arbitrary expressions
   */
  std::unordered_map<std::string, std::unique_ptr<ColumnStats>> stats_;

  /**
   * Number of rows
   */
  int num_rows_ = -1;

  /**
   * Cost Lower Bound
   */
  double cost_lower_bound_ = -1;
};

}  // namespace noisepage::optimizer
