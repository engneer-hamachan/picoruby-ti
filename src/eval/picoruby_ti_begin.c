#include "picoruby_ti_begin.h"
#include "picoruby_ti_eval.h"
#include "picoruby_ti_t.h"

uint16_t
ti_eval_begin(
  TiContext *context,
  const pm_begin_node_t *begin_node,
  int depth
) {

  uint16_t result_t_node_index =
    ti_eval_statements_or_nil(context, begin_node->statements, depth + 1);

  if (begin_node->else_clause) {
    result_t_node_index =
      ti_eval_statements_or_nil(
        context,
        begin_node->else_clause->statements,
        depth + 1
      );
  }

  if (begin_node->rescue_clause) {
    result_t_node_index =
      ti_make_union(
        result_t_node_index,
        ti_eval_rescue(context, begin_node->rescue_clause, depth + 1)
      );
  }

  if (begin_node->ensure_clause) {
    ti_eval_statements(
      context,
      begin_node->ensure_clause->statements,
      depth + 1
    );
  }

  return result_t_node_index;
}

uint16_t
ti_eval_rescue(
  TiContext *context,
  const pm_rescue_node_t *rescue_node,
  int depth
) {

  for (
    size_t exception_index = 0;
    exception_index < rescue_node->exceptions.size;
    exception_index++
  ) {

    ti_eval_expression(
      context,
      rescue_node->exceptions.nodes[exception_index],
      depth + 1
    );
  }

  ti_eval_expression(context, rescue_node->reference, depth + 1);

  uint16_t result_t_node_index =
    ti_eval_statements_or_nil(context, rescue_node->statements, depth + 1);

  if (rescue_node->subsequent) {
    result_t_node_index =
      ti_make_union(
        result_t_node_index,
        ti_eval_rescue(context, rescue_node->subsequent, depth + 1)
      );
  }

  return result_t_node_index;
}
