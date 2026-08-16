#include "picoruby_ti_case.h"
#include "picoruby_ti_builtin.h"
#include "picoruby_ti_eval.h"
#include "picoruby_ti_t.h"

uint16_t
ti_eval_case(TiContext *context, const pm_case_node_t *case_node, int depth) {
  ti_eval_node(context, case_node->predicate);

  uint16_t result_t_node_index = 0;

  for (
    size_t when_index = 0;
    when_index < case_node->conditions.size;
    when_index++
  ) {

    const pm_when_node_t *when_node =
      (const pm_when_node_t *)case_node->conditions.nodes[when_index];

    for (
      size_t condition_index = 0;
      condition_index < when_node->conditions.size;
      condition_index++
    ) {

      ti_eval_node(context, when_node->conditions.nodes[condition_index]);
    }

    uint16_t when_t_node_index =
      ti_eval_statements_or_nil(
        context,
        when_node->statements,
        depth + 1
      );

    result_t_node_index =
      ti_make_union(result_t_node_index, when_t_node_index);
  }

  uint16_t else_t_node_index;

  if (case_node->else_clause) {
    else_t_node_index =
      ti_eval_statements_or_nil(
        context,
        case_node->else_clause->statements,
        depth + 1
      );

  } else {
    else_t_node_index = ti_new_t(TI_CLASS_NIL, 0, 0);
  }

  return ti_make_union(result_t_node_index, else_t_node_index);
}

uint16_t ti_eval_case_match(
  TiContext *context,
  const pm_case_match_node_t *case_match_node,
  int depth
) {

  ti_eval_node(context, case_match_node->predicate);

  uint16_t result_t_node_index = 0;

  for (
    size_t in_index = 0;
    in_index < case_match_node->conditions.size;
    in_index++
  ) {

    const pm_in_node_t *in_node =
      (const pm_in_node_t *)case_match_node->conditions.nodes[in_index];

    ti_eval_node(context, in_node->pattern);

    uint16_t in_t_node_index =
      ti_eval_statements_or_nil(
        context,
        in_node->statements,
        depth + 1
      );

    result_t_node_index =
      ti_make_union(result_t_node_index, in_t_node_index);
  }

  if (case_match_node->else_clause) {
    uint16_t else_t_node_index =
      ti_eval_statements_or_nil(
        context,
        case_match_node->else_clause->statements,
        depth + 1
      );

    result_t_node_index =
      ti_make_union(result_t_node_index, else_t_node_index);
  }

  return result_t_node_index;
}
