#pragma once

#include <map>
#include <vector>
#include <tuple>
#include <memory>

#include "loggers/optimizer_logger.h"

#include "optimizer/operator_node.h"
#include "optimizer/group.h"
#include "optimizer/pattern.h"
#include "optimizer/operator_expression.h"
#include "optimizer/memo.h"

namespace terrier::optimizer {

/**
 * Abstract interface for a BindingIterator defined similarly to
 * a traditional iterator (HasNext(), Next()).
 */
class BindingIterator {
 public:
  /**
   * Constructor for a binding iterator
   * @param memo Memo to be used
   */
  explicit BindingIterator(const Memo& memo) : memo_(memo) {}

  /**
   * Default destructor
   */
  virtual ~BindingIterator() = default;

  /**
   * Virtual function for whether a binding exists
   * @returns Whether or not a binding still exists
   */
  virtual bool HasNext() = 0;

  /**
   * Virtual function for getting the next binding
   * @returns next OperatorExpression that matches
   */
  virtual OperatorExpression* Next() = 0;

 protected:
  /**
   * Internal reference to Memo table
   */
  const Memo &memo_;
};

/**
 * GroupBindingIterator is an implementation of the BindingIterator abstract
 * class that is specialized for trying to bind a group against a pattern.
 */
class GroupBindingIterator : public BindingIterator {
 public:
  /**
   * Constructor for a group binding iterator
   * @param memo Memo to be used
   * @param id ID of the Group for binding
   * @param pattern Pattern to bind
   */
  GroupBindingIterator(const Memo& memo, GroupID id, Pattern* pattern)
    : BindingIterator(memo),
      group_id_(id),
      pattern_(pattern),
      target_group_(memo_.GetGroupByID(id)),
      num_group_items_(target_group_->GetLogicalExpressions().size()),
      current_item_index_(0) {
    OPTIMIZER_LOG_TRACE("Attempting to bind on group %d", id);
  }

  /**
   * Virtual function for whether a binding exists
   * @returns Whether or not a binding still exists
   */
  bool HasNext() override;

  /**
   * Virtual function for getting the next binding
   * @returns next OperatorExpression that matches
   */
  OperatorExpression* Next() override;

 private:
  /**
   * GroupID to try binding with
   */
  GroupID group_id_;

  /**
   * Pattern to try binding to
   */
  Pattern* pattern_;

  /**
   * Pointer to the group with GroupID group_id_
   */
  Group *target_group_;

  /**
   * Number of items in the Group to try
   */
  size_t num_group_items_;

  /**
   * Current GroupExpression being tried
   */
  size_t current_item_index_;

  /**
   * Iterator used for binding against GroupExpression
   */
  std::unique_ptr<BindingIterator> current_iterator_;
};

/**
 * GroupExprBindingIterator is an implementation of the BindingIterator abstract
 * class that is specialized for trying to bind a GroupExpression against a pattern.
 */
class GroupExprBindingIterator : public BindingIterator {
 public:
  /**
   * Constructor for a GroupExpression binding iterator
   * @param memo Memo to be used
   * @param gexpr GroupExpression to bind to
   * @param pattern Pattern to bind
   */
  GroupExprBindingIterator(const Memo& memo, GroupExpression *gexpr, Pattern* pattern);

  /**
   * Destructor
   */
  ~GroupExprBindingIterator() override {
    for (auto &vec : children_bindings_) {
      for (auto expr : vec) {
        delete expr;
      }
    }

    delete current_binding_;
  }

  /**
   * Virtual function for whether a binding exists
   * @returns Whether or not a binding still exists
   */
  bool HasNext() override;

  /**
   * Virtual function for getting the next binding
   * Pointer returned must be deleted by caller when done.
   * @returns next OperatorExpression that matches
   */
  OperatorExpression* Next() override {
    TERRIER_ASSERT(current_binding_, "binding must exist");
    auto binding = current_binding_;
    current_binding_ = nullptr;
    return binding;
  }

 private:
  /**
   * GroupExpression to bind with
   */
  GroupExpression* gexpr_;

  /**
   * Pattern to bind to
   */
  Pattern* pattern_;

  /**
   * Flag indicating whether first binding or not
   */
  bool first_;

  /**
   * Flag indicating whether there are anymore bindings
   */
  bool has_next_;

  /**
   * Current binding
   */
  OperatorExpression* current_binding_;

  /**
   * Stored bindings for children expressions
   */
  std::vector<std::vector<OperatorExpression*>> children_bindings_;

  /**
   * Position indicators tracking progress within children_bindings_
   */
  std::vector<size_t> children_bindings_pos_;
};

}  // namespace terrier::optimizer
