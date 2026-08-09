#ifndef PICORUBY_TI_CASE_H
#define PICORUBY_TI_CASE_H

#include "picoruby_ti_context.h"
#include <stdint.h>

uint16_t
ti_eval_case(TiContext *context, const pm_case_node_t *case_node, int depth);

uint16_t ti_eval_case_match(
  TiContext *context,
  const pm_case_match_node_t *case_match_node,
  int depth
);

#endif
