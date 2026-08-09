#ifndef PICORUBY_TI_DIAGNOSTIC_H
#define PICORUBY_TI_DIAGNOSTIC_H

#include <prism.h>
#include "picoruby_ti_source.h"

#define TI_DIAGNOSTIC_CAPACITY 64
#define TI_BUILTIN_METHOD_MISMATCH_MESSAGE_CAPACITY 256

typedef struct {
  int start_byte_offset;
  int end_byte_offset;
  const char *message;
} TiDiagnostic;

typedef struct {
  TiDiagnostic items[TI_DIAGNOSTIC_CAPACITY];
  int count;
} TiDiagnosticList;

struct TiContext;

void ti_add_diagnostic(
  struct TiContext *context,
  pm_location_t location,
  const char *message
);

int ti_fill_diagnostics(
  const TiSourceList *sources,
  TiDiagnosticList *out
);

#endif
