#include "picoruby_ti_hover.h"
#include "picoruby_ti_arena.h"
#include "picoruby_ti_context.h"
#include "picoruby_ti_eval.h"
#include "picoruby_ti_suggest.h"
#include "picoruby_ti_t_frame.h"
#include "picoruby_ti_type.h"
#include <prism.h>
#include <string.h>

typedef struct {
  const uint8_t *source;
  int cursor_byte_offset;
  pm_constant_id_t name;
  pm_location_t location;
  const pm_call_node_t *call;
} TiHoverSearch;

static bool
find_hover_target_on_visit(const pm_node_t *node, void *data) {
  TiHoverSearch *search = data;
  pm_constant_id_t name = 0;
  pm_location_t location = node->location;

  switch (PM_NODE_TYPE(node)) {
  case PM_CALL_NODE: {
    const pm_call_node_t *call = (const pm_call_node_t *)node;
    name = call->name;
    location = call->message_loc;
    break;
  }

  case PM_LOCAL_VARIABLE_READ_NODE:
    name = ((const pm_local_variable_read_node_t *)node)->name;
    break;
  case PM_INSTANCE_VARIABLE_READ_NODE:
    name = ((const pm_instance_variable_read_node_t *)node)->name;
    break;
  case PM_GLOBAL_VARIABLE_READ_NODE:
    name = ((const pm_global_variable_read_node_t *)node)->name;
    break;
  case PM_CONSTANT_READ_NODE:
    name = ((const pm_constant_read_node_t *)node)->name;
    break;
  case PM_LOCAL_VARIABLE_WRITE_NODE: {
    const pm_local_variable_write_node_t *write =
      (const pm_local_variable_write_node_t *)node;
    name = write->name;
    location = write->name_loc;
    break;
  }

  case PM_INSTANCE_VARIABLE_WRITE_NODE: {
    const pm_instance_variable_write_node_t *write =
      (const pm_instance_variable_write_node_t *)node;

    name = write->name;
    location = write->name_loc;

    break;
  }

  case PM_GLOBAL_VARIABLE_WRITE_NODE: {
    const pm_global_variable_write_node_t *write =
      (const pm_global_variable_write_node_t *)node;

    name = write->name;
    location = write->name_loc;

    break;
  }

  case PM_CONSTANT_WRITE_NODE: {
    const pm_constant_write_node_t *write =
      (const pm_constant_write_node_t *)node;

    name = write->name;
    location = write->name_loc;

    break;
  }

  default:
    return true;
  }

  int start = (int)(location.start - search->source);
  int end = (int)(location.end - search->source);

  if (search->cursor_byte_offset < start || search->cursor_byte_offset >= end)
    return true;

  search->name = name;
  search->location = location;
  search->call = NULL;

  if (PM_NODE_TYPE(node) == PM_CALL_NODE)
    search->call = (const pm_call_node_t *)node;

  return false;
}

int
ti_find_hover_at_cursor(
  const TiSourceList *sources,
  int cursor_byte_offset,
  TiHoverInfo *out
) {

  if (out)
    memset(out, 0, sizeof(*out));

  if (
    !sources || !sources->items || sources->count <= 0 || !out
  ) {
    return 0;
  }

  const TiSource *source = &sources->items[sources->count - 1];
  const char *source_bytes = "";

  if (source->source)
    source_bytes = source->source;

  if (
    (!source->source && source->source_byte_length > 0) ||
    cursor_byte_offset < 0 ||
    cursor_byte_offset > source->source_byte_length ||
    !ti_evaluate_sources(sources, NULL)
  ) {

    return 0;
  }

  pm_parser_t parser;

  pm_parser_init(
    &parser,
    (const uint8_t *)source_bytes,
    (size_t)source->source_byte_length,
    NULL
  );

  pm_node_t *root = pm_parse(&parser);
  if (!root) {
    pm_parser_free(&parser);
    return 0;
  }

  TiContext context = {
    .parser = &parser,
    .source = (const uint8_t *)source_bytes,
    .source_length = (size_t)source->source_byte_length,
  };

  TiHoverSearch search = {
    .source = (const uint8_t *)source_bytes,
    .cursor_byte_offset = cursor_byte_offset,
  };

  if (!ti_did_arena_overflow())
    pm_visit_node(root, find_hover_target_on_visit, &search);

  if (search.call) {
    TiSuggestionList suggestions;
    memset(&suggestions, 0, sizeof(suggestions));

    int method_name_end_byte_offset =
      (int)(search.call->message_loc.end - context.source);

    ti_collect_suggestions_at_cursor(
      &context,
      root,
      method_name_end_byte_offset,
      &suggestions
    );

    const pm_constant_t *method_name =
      ti_get_constant(&context, search.call->name);

    if (method_name) {
      for (int index = 0; index < suggestions.count; index++) {
        const TiSuggestion *suggestion = &suggestions.items[index];

        if (
          suggestion->contents_length == (int)method_name->length &&
          memcmp(
            suggestion->contents,
            method_name->start,
            method_name->length
          ) == 0
        ) {

          out->method_signature = suggestion->detail;
          out->method_document = suggestion->document;
          out->method_name_length = suggestion->contents_length;
          out->is_method = 1;
          out->found = 1;

          break;
        }
      }
    }
  }

  uint16_t name_id;
  if (!search.call && search.name != 0 &&
      ti_convert_constant_id(&context, search.name, &name_id)) {
    const pm_constant_t *constant = ti_get_constant(&context, search.name);
    uint16_t t_node_index = ti_get_value_t(name_id);

    if (
      constant &&
      t_node_index != 0 &&
      ti_type_to_string(
        t_node_index,
        out->type_name,
        sizeof(out->type_name)
      ) > 0
    ) {

      size_t name_length = constant->length;

      if (name_length >= sizeof(out->variable_name))
        name_length = sizeof(out->variable_name) - 1;

      memcpy(out->variable_name, constant->start, name_length);

      out->variable_name[name_length] = '\0';
      out->found = 1;
    }
  }

  pm_node_destroy(&parser, root);
  pm_parser_free(&parser);

  return out->found;
}
