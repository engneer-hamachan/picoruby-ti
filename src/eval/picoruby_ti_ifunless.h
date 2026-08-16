#ifndef PICORUBY_TI_IFUNLESS_H
#define PICORUBY_TI_IFUNLESS_H

#include "picoruby_ti_context.h"
#include <stdint.h>

uint16_t
ti_eval_ifunless(TiContext *context, const pm_if_node_t *if_node, int depth);

uint16_t ti_eval_unless(
  TiContext *context,
  const pm_unless_node_t *unless_node,
  int depth
);

#endif
