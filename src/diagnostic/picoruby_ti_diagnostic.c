#include "picoruby_ti_diagnostic.h"
#include "picoruby_ti_arena.h"
#include "picoruby_ti_context.h"
#include "picoruby_ti_eval.h"
#include <stddef.h>
#include <string.h>

void
ti_add_diagnostic(
  TiContext *context,
  pm_location_t location,
  const char *message
) {

  if (
    !context ||
    context->round != 2 ||
    !context->diagnostics ||
    !message ||
    context->diagnostics->count >= TI_DIAGNOSTIC_CAPACITY
  ) {
    return;
  }

  TiDiagnostic *diagnostic =
    &context->diagnostics->items[context->diagnostics->count++];

  diagnostic->start_byte_offset =
    (int)(location.start - context->source);
  diagnostic->end_byte_offset =
    (int)(location.end - context->source);

  size_t message_byte_length = strlen(message);

  char *message_copy =
    ti_allocate_from_arena(message_byte_length + 1);

  if (!message_copy) {
    context->diagnostics->count--;
    context->failed = 1;
    return;
  }

  memcpy(message_copy, message, message_byte_length + 1);
  diagnostic->message = message_copy;
}

int
ti_fill_diagnostics(
  const TiSourceList *sources,
  TiDiagnosticList *out
) {

  if (out)
    memset(out, 0, sizeof(*out));

  if (!sources || !out)
    return 0;

  if (!ti_evaluate_sources(sources, out)) {
    memset(out, 0, sizeof(*out));
    return 0;
  }

  return out->count;
}
