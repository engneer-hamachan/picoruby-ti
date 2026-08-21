#include "picoruby_ti_attribute.h"
#include "picoruby_ti_builtin_database.h"
#include "picoruby_ti_define_info.h"
#include "picoruby_ti_name.h"
#include "picoruby_ti_t_frame.h"
#include <stddef.h>
#include <string.h>

typedef enum {
  TI_ATTRIBUTE_READER,
  TI_ATTRIBUTE_WRITER,
  TI_ATTRIBUTE_ACCESSOR,
} TiAttributeKind;

static int
is_equal_pm_constant_bytes(
  const pm_constant_t *constant,
  const char *bytes,
  size_t byte_length
) {

  return constant->length == byte_length &&
         memcmp(constant->start, bytes, byte_length) == 0;
}

static int
find_attribute_kind(
  const pm_constant_t *method_name,
  TiAttributeKind *attribute_kind
) {

  if (
    is_equal_pm_constant_bytes(
      method_name,
      "attr_reader",
      sizeof("attr_reader") - 1
    )
  ) {

    *attribute_kind = TI_ATTRIBUTE_READER;

    return 1;
  }

  if (
    is_equal_pm_constant_bytes(
      method_name,
      "attr_writer",
      sizeof("attr_writer") - 1
    )
  ) {

    *attribute_kind = TI_ATTRIBUTE_WRITER;

    return 1;
  }

  if (
    is_equal_pm_constant_bytes(
      method_name,
      "attr_accessor",
      sizeof("attr_accessor") - 1
    )
  ) {

    *attribute_kind = TI_ATTRIBUTE_ACCESSOR;

    return 1;
  }

  return 0;
}

static void
register_attribute_method(
  TiContext *context,
  uint16_t method_name_id,
  uint16_t return_t_node_index,
  uint16_t define_arg_name_id,
  uint16_t define_row
) {

  if (
    !ti_set_method_t(
      context->current_class_id,
      method_name_id,
      return_t_node_index
    )
  ) {

    context->failed = 1;

    return;
  }

  TiDefineInfo *define_info =
    ti_set_define_info(
      method_name_id,
      context->current_class_name_id,
      define_row,
      0
    );

  if (!define_info)
    return;

  define_info->return_t_node_index = return_t_node_index;

  if (define_arg_name_id == 0)
    return;

  define_info->define_arg_count = 1;
  define_info->define_arg_name_ids[0] = define_arg_name_id;
  define_info->define_arg_kinds[0] = TI_DEFINE_ARG_REQUIRED;
}

static void
register_attribute(
  TiContext *context,
  const pm_symbol_node_t *symbol_node,
  TiAttributeKind attribute_kind,
  uint16_t define_row
) {

  const uint8_t *attribute_name_bytes = symbol_node->value_loc.start;

  size_t attribute_name_byte_length =
    (size_t)(symbol_node->value_loc.end - symbol_node->value_loc.start);

  if (attribute_name_byte_length == 0)
    return;

  uint16_t attribute_name_id;

  if (
    !ti_intern_concatenated_bytes(
      attribute_name_bytes,
      attribute_name_byte_length,
      NULL,
      0,
      &attribute_name_id
    )
  ) {

    context->failed = 1;

    return;
  }

  uint16_t instance_variable_t_node_index =
    ti_get_instance_variable_t(context->current_class_id, attribute_name_id);

  if (attribute_kind != TI_ATTRIBUTE_WRITER) {
    register_attribute_method(
      context,
      attribute_name_id,
      instance_variable_t_node_index,
      0,
      define_row
    );
  }

  if (attribute_kind == TI_ATTRIBUTE_READER)
    return;

  uint16_t writer_method_name_id;

  if (
    !ti_intern_concatenated_bytes(
      attribute_name_bytes,
      attribute_name_byte_length,
      (const uint8_t *)"=",
      1,
      &writer_method_name_id
    )
  ) {

    context->failed = 1;

    return;
  }

  register_attribute_method(
    context,
    writer_method_name_id,
    instance_variable_t_node_index,
    attribute_name_id,
    define_row
  );
}

void
ti_register_attribute_methods(
  TiContext *context,
  const pm_call_node_t *call_node
) {

  if (call_node->receiver || context->current_class_id == TI_CLASS_NONE)
    return;

  const pm_constant_t *method_name = ti_get_constant(context, call_node->name);

  if (!method_name)
    return;

  TiAttributeKind attribute_kind;

  if (!find_attribute_kind(method_name, &attribute_kind))
    return;

  const pm_arguments_node_t *arguments = call_node->arguments;

  if (!arguments)
    return;

  uint16_t define_row =
    ti_calculate_row(context, call_node->base.location.start);

  for (size_t index = 0; index < arguments->arguments.size; index++) {
    const pm_node_t *argument = arguments->arguments.nodes[index];

    if (PM_NODE_TYPE(argument) != PM_SYMBOL_NODE)
      continue;

    register_attribute(
      context,
      (const pm_symbol_node_t *)argument,
      attribute_kind,
      define_row
    );
  }
}
