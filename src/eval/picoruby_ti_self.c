#include "picoruby_ti_self.h"
#include "picoruby_ti_t.h"

uint16_t
ti_eval_self(TiContext *context) {
  return ti_new_t(context->current_class_id, TI_T_FLAG_DEFINED_CLASS, 0);
}
