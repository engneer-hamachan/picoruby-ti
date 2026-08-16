#ifndef PICORUBY_TI_EVAL_H
#define PICORUBY_TI_EVAL_H

#include "picoruby_ti_context.h"
#include "picoruby_ti_source.h"
#include <stdint.h>

#define TI_EVAL_DEPTH_LIMIT 8

int ti_evaluate_sources(
  const TiSourceList *sources,
  TiDiagnosticList *diagnostics
);
uint16_t ti_eval_statements(
  TiContext *context,
  const pm_statements_node_t *statements,
  int depth
);
uint16_t ti_eval_statements_or_nil(
  TiContext *context,
  const pm_statements_node_t *statements,
  int depth
);
uint16_t
ti_eval_expression(TiContext *context, const pm_node_t *node, int depth);
void ti_eval_node(TiContext *context, const pm_node_t *node);

#endif
