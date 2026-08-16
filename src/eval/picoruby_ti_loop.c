#include "picoruby_ti_loop.h"
#include "picoruby_ti_builtin.h"
#include "picoruby_ti_eval.h"
#include "picoruby_ti_t.h"

uint16_t
ti_eval_while(
  TiContext *context,
  const pm_while_node_t *while_node,
  int depth
) {

  ti_eval_expression(context, while_node->predicate, depth + 1);
  ti_eval_statements(context, while_node->statements, depth + 1);

  return ti_new_t(TI_CLASS_NIL, 0, 0);
}

uint16_t
ti_eval_until(
  TiContext *context,
  const pm_until_node_t *until_node,
  int depth
) {

  ti_eval_expression(context, until_node->predicate, depth + 1);
  ti_eval_statements(context, until_node->statements, depth + 1);

  return ti_new_t(TI_CLASS_NIL, 0, 0);
}

uint16_t
ti_eval_for(
  TiContext *context,
  const pm_for_node_t *for_node,
  int depth
) {

  uint16_t collection_t_node_index =
    ti_eval_expression(context, for_node->collection, depth + 1);

  ti_eval_expression(context, for_node->index, depth + 1);
  ti_eval_statements(context, for_node->statements, depth + 1);

  return collection_t_node_index;
}
