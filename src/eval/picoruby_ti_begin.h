#ifndef PICORUBY_TI_BEGIN_H
#define PICORUBY_TI_BEGIN_H

#include "picoruby_ti_context.h"
#include <stdint.h>

uint16_t ti_eval_begin(
  TiContext *context,
  const pm_begin_node_t *begin_node,
  int depth
);

uint16_t ti_eval_rescue(
  TiContext *context,
  const pm_rescue_node_t *rescue_node,
  int depth
);

#endif
