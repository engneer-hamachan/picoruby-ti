#ifndef PICORUBY_TI_HOVER_H
#define PICORUBY_TI_HOVER_H

#include "picoruby_ti_source.h"

#define TI_TYPE_NAME_CAPACITY 96
#define TI_VARIABLE_NAME_CAPACITY 64

typedef struct {
  char variable_name[TI_VARIABLE_NAME_CAPACITY];
  char type_name[TI_TYPE_NAME_CAPACITY];
  const char *method_signature;
  const char *method_document;
  int method_name_length;
  int is_method;
  int found;
} TiHoverInfo;

int ti_find_hover_at_cursor(
  const TiSourceList *sources,
  int cursor_byte_offset,
  TiHoverInfo *out
);

#endif
