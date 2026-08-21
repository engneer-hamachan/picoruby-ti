#ifndef PICORUBY_TI_ATTRIBUTE_H
#define PICORUBY_TI_ATTRIBUTE_H

#include "picoruby_ti_context.h"

void ti_register_attribute_methods(
  TiContext *context,
  const pm_call_node_t *call_node
);

#endif
