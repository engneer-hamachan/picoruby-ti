#ifndef PICORUBY_TI_LOOP_H
#define PICORUBY_TI_LOOP_H

#include "picoruby_ti_context.h"
#include <stdint.h>

uint16_t ti_eval_while(
  TiContext *context,
  const pm_while_node_t *while_node,
  int depth
);

uint16_t ti_eval_until(
  TiContext *context,
  const pm_until_node_t *until_node,
  int depth
);

uint16_t ti_eval_for(
  TiContext *context,
  const pm_for_node_t *for_node,
  int depth
);

#endif
