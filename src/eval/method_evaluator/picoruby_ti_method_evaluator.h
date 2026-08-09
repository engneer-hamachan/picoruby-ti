#ifndef PICORUBY_TI_METHOD_EVALUATOR_H
#define PICORUBY_TI_METHOD_EVALUATOR_H

#include "picoruby_ti_context.h"
#include <stdint.h>

uint16_t
ti_eval_method(TiContext *context, const pm_call_node_t *call, int depth);

#endif
