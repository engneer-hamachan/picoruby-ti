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
const pm_constant_t *
ti_get_constant(const TiContext *context, pm_constant_id_t constant_id);
uint16_t ti_calculate_row(const TiContext *context, const uint8_t *location);

#endif
