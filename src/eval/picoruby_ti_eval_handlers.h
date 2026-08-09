#ifndef PICORUBY_TI_EVAL_HANDLERS_H
#define PICORUBY_TI_EVAL_HANDLERS_H

#include "picoruby_ti_context.h"
#include <stdint.h>

uint16_t ti_handle_identifier(
  TiContext *context,
  pm_constant_id_t constant_id
);
uint16_t ti_handle_const_evaluation(
  TiContext *context,
  const pm_constant_read_node_t *constant_read
);

#endif
