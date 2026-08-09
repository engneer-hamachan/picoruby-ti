#include "picoruby_ti_context.h"
#include "picoruby_ti_name.h"

int
ti_convert_constant_id(
  TiContext *context,
  pm_constant_id_t constant_id,
  uint16_t *name_id
) {

  if (!context)
    return 0;

  if (!ti_intern_constant(context->parser, constant_id, name_id)) {
    context->failed = 1;
    return 0;
  }

  return 1;
}

const pm_constant_t *
ti_get_constant(const TiContext *context, pm_constant_id_t constant_id) {
  if (!context || !context->parser || constant_id == 0)
    return NULL;

  return pm_constant_pool_id_to_constant(
    &context->parser->constant_pool,
    constant_id
  );
}

uint16_t
ti_calculate_row(const TiContext *context, const uint8_t *location) {

  uint16_t row = 1;

  for (const uint8_t *cursor = context->source;
       cursor < location && cursor < context->source + context->source_length;
       cursor++) {
    if (*cursor == '\n' && row < UINT16_MAX)
      row++;
  }

  return row;
}
