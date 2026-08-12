#include "picoruby_ti_def.h"
#include "picoruby_ti_define_info.h"
#include "picoruby_ti_eval.h"
#include "picoruby_ti_t.h"
#include "picoruby_ti_t_frame.h"
#include <stdint.h>

static void
append_define_arg(
  TiContext *context,
  TiDefineInfo *define_info,
  pm_constant_id_t constant_id,
  TiDefineArgKind define_arg_kind
) {

  if (
    !define_info ||
    !constant_id ||
    define_info->define_arg_count >= TI_DEFINE_ARG_CAPACITY
  ) {

    return;
  }

  uint16_t name_id;

  if (!ti_convert_constant_id(context, constant_id, &name_id)) {
    context->failed = 1;
    return;
  }

  uint8_t index = define_info->define_arg_count++;
  define_info->define_arg_name_ids[index] = name_id;
  define_info->define_arg_kinds[index] = define_arg_kind;
}

static void
set_define_args(
  TiContext *context,
  TiDefineInfo *define_info,
  const pm_parameters_node_t *parameters
) {

  if (!define_info || !parameters)
    return;

  define_info->define_arg_count = 0;

  for (size_t index = 0; index < parameters->requireds.size; index++) {
    const pm_node_t *parameter = parameters->requireds.nodes[index];

    if (PM_NODE_TYPE(parameter) != PM_REQUIRED_PARAMETER_NODE)
      continue;

    append_define_arg(
      context,
      define_info,
      ((const pm_required_parameter_node_t *)parameter)->name,
      TI_DEFINE_ARG_REQUIRED
    );
  }

  if (
    parameters->rest &&
    PM_NODE_TYPE(parameters->rest) == PM_REST_PARAMETER_NODE
  ) {

    append_define_arg(
      context,
      define_info,
      ((const pm_rest_parameter_node_t *)parameters->rest)->name,
      TI_DEFINE_ARG_REST
    );
  }

  if (
    parameters->keyword_rest &&
    PM_NODE_TYPE(parameters->keyword_rest) == PM_KEYWORD_REST_PARAMETER_NODE
  ) {

    append_define_arg(
      context,
      define_info,
      ((const pm_keyword_rest_parameter_node_t *)parameters->keyword_rest)->name,
      TI_DEFINE_ARG_KEYWORD_REST
    );
  }
}

void
ti_eval_def(TiContext *context, const pm_def_node_t *def_node) {
  uint16_t name_id;

  if (!ti_convert_constant_id(context, def_node->name, &name_id)) {
    context->failed = 1;
    return;
  }

  uint16_t define_row =
    ti_calculate_row(context, def_node->base.location.start);

  TiDefineInfo *define_info =
    ti_set_define_info(name_id, context->current_class_name_id, define_row, 0);

  if (!define_info)
    return;

  set_define_args(context, define_info, def_node->parameters);

  uint16_t outer_return_t_node_index = context->return_t_node_index;
  context->return_t_node_index = 0;

  uint16_t last_evaluated_t_node_index =
    ti_eval_statements(
      context,
      (const pm_statements_node_t *)def_node->body,
      0
    );

  uint16_t return_t_node_index =
    ti_make_union(context->return_t_node_index, last_evaluated_t_node_index);

  context->return_t_node_index = outer_return_t_node_index;

  if (return_t_node_index == 0)
    return;

  define_info->return_t_node_index = return_t_node_index;

  if (
    !ti_set_method_t(
      context->current_class_id,
      name_id,
      return_t_node_index
    )
  ) {

    context->failed = 1;
  }
}
