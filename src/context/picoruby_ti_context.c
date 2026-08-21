#include "picoruby_ti_context.h"
#include "picoruby_ti_builtin_database.h"
#include "picoruby_ti_define_info.h"
#include "picoruby_ti_name.h"
#include <stddef.h>

int
ti_convert_constant_id(
  TiContext *context,
  pm_constant_id_t constant_id,
  uint16_t *name_id
) {

  if (!context)
    return 0;

  if (!ti_intern_constant(context->parser, constant_id, name_id)) {
    context->failed = 1;
    return 0;
  }

  return 1;
}

int
ti_convert_instance_variable_attribute_name_id(
  TiContext *context,
  pm_constant_id_t constant_id,
  uint16_t *attribute_name_id
) {

  if (!context)
    return 0;

  const pm_constant_t *constant = ti_get_constant(context, constant_id);

  if (!constant) {
    context->failed = 1;
    return 0;
  }

  if (constant->length < 2)
    return 0;

  if (
    !ti_intern_concatenated_bytes(
      constant->start + 1,
      constant->length - 1,
      NULL,
      0,
      attribute_name_id
    )
  ) {

    context->failed = 1;
    return 0;
  }

  return 1;
}

const pm_constant_t *
ti_get_constant(const TiContext *context, pm_constant_id_t constant_id) {
  if (!context || !context->parser || constant_id == 0)
    return NULL;

  return pm_constant_pool_id_to_constant(
    &context->parser->constant_pool,
    constant_id
  );
}

uint16_t
ti_calculate_row(const TiContext *context, const uint8_t *location) {

  uint16_t row = 1;

  for (const uint8_t *cursor = context->source;
       cursor < location && cursor < context->source + context->source_length;
       cursor++) {
    if (*cursor == '\n' && row < UINT16_MAX)
      row++;
  }

  return row;
}

typedef struct {
  const uint8_t *cursor;
  const pm_class_node_t *enclosing_class_node;
  size_t enclosing_class_byte_length;
} EnclosingClassSearch;

static bool
find_enclosing_class_on_visit(const pm_node_t *node, void *data) {
  EnclosingClassSearch *search = data;

  if (PM_NODE_TYPE(node) != PM_CLASS_NODE)
    return true;

  if (node->location.start > search->cursor ||
      node->location.end < search->cursor) {

    return true;
  }

  size_t byte_length = (size_t)(node->location.end - node->location.start);

  if (
    !search->enclosing_class_node ||
    byte_length < search->enclosing_class_byte_length
  ) {

    search->enclosing_class_node = (const pm_class_node_t *)node;
    search->enclosing_class_byte_length = byte_length;
  }

  return true;
}

void
ti_set_context_class_at_cursor(
  TiContext *context,
  const pm_node_t *root,
  int cursor_byte_offset
) {

  context->current_class_name_id = 0;
  context->current_class_id = TI_CLASS_NONE;

  if (!root || cursor_byte_offset < 0)
    return;

  EnclosingClassSearch search = {
    .cursor = context->source + cursor_byte_offset,
    .enclosing_class_node = NULL,
    .enclosing_class_byte_length = 0,
  };

  pm_visit_node(root, find_enclosing_class_on_visit, &search);

  if (!search.enclosing_class_node)
    return;

  uint16_t class_name_id;

  if (
    !ti_convert_constant_id(
      context,
      search.enclosing_class_node->name,
      &class_name_id
    )
  ) {

    return;
  }

  context->current_class_name_id = class_name_id;
  context->current_class_id = ti_get_defined_class_id(class_name_id);
}
