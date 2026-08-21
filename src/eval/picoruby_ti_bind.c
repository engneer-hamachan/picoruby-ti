#include "picoruby_ti_bind.h"
#include "picoruby_ti_eval.h"
#include "picoruby_ti_t_frame.h"
#include <stdint.h>

uint16_t
ti_bind_scalar_assignment(
  TiContext *context,
  pm_constant_id_t constant_id,
  const pm_node_t *value,
  int depth
) {

  uint16_t name_id;

  if (!ti_convert_constant_id(context, constant_id, &name_id))
    return 0;

  uint16_t t_node_index = ti_eval_expression(context, value, depth + 1);

  if (t_node_index != 0 && !ti_set_value_t(name_id, t_node_index))
    context->failed = 1;

  return t_node_index;
}

uint16_t
ti_bind_instance_variable_assignment(
  TiContext *context,
  pm_constant_id_t constant_id,
  const pm_node_t *value,
  int depth
) {

  uint16_t attribute_name_id;

  if (
    !ti_convert_instance_variable_attribute_name_id(
      context,
      constant_id,
      &attribute_name_id
    )
  ) {

    return 0;
  }

  uint16_t t_node_index = ti_eval_expression(context, value, depth + 1);

  if (
    t_node_index != 0 &&
    !ti_set_instance_variable_t(
      context->current_class_id,
      attribute_name_id,
      t_node_index
    )
  ) {

    context->failed = 1;
  }

  return t_node_index;
}
