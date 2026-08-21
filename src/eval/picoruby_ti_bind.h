#ifndef PICORUBY_TI_BIND_H
#define PICORUBY_TI_BIND_H

#include "picoruby_ti_context.h"

uint16_t ti_bind_scalar_assignment(
  TiContext *context,
  pm_constant_id_t constant_id,
  const pm_node_t *value,
  int depth
);
uint16_t ti_bind_instance_variable_assignment(
  TiContext *context,
  pm_constant_id_t constant_id,
  const pm_node_t *value,
  int depth
);

#endif
