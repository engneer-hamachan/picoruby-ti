#ifndef PICORUBY_TI_CONTEXT_H
#define PICORUBY_TI_CONTEXT_H

#include <prism.h>

#include "picoruby_ti_diagnostic.h"

typedef struct TiContext {
  pm_parser_t *parser;
  const uint8_t *source;
  size_t source_length;
  uint16_t current_class_name_id;
  uint8_t current_class_id;
  uint16_t return_t_node_index;
  int round;
  int failed;
  TiDiagnosticList *diagnostics;
} TiContext;

int ti_convert_constant_id(
  TiContext *context,
  pm_constant_id_t constant_id,
  uint16_t *name_id
);
int ti_convert_instance_variable_attribute_name_id(
  TiContext *context,
  pm_constant_id_t constant_id,
  uint16_t *attribute_name_id
);
const pm_constant_t *
ti_get_constant(const TiContext *context, pm_constant_id_t constant_id);
uint16_t ti_calculate_row(const TiContext *context, const uint8_t *location);
void ti_set_context_class_at_cursor(
  TiContext *context,
  const pm_node_t *root,
  int cursor_byte_offset
);

#endif
