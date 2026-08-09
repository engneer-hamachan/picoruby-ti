#ifndef PICORUBY_TI_SUGGEST_H
#define PICORUBY_TI_SUGGEST_H

#include "picoruby_ti_source.h"
#include "picoruby_ti_context.h"

#define TI_SUGGESTION_CAPACITY 64

typedef struct {
  const char *contents;
  int contents_length;
  const char *detail;
  const char *document;
  const char *class_name;
} TiSuggestion;

typedef struct {
  TiSuggestion items[TI_SUGGESTION_CAPACITY];
  int count;
} TiSuggestionList;

int ti_collect_suggestions_at_cursor(
  TiContext *context,
  const pm_node_t *root,
  int cursor_byte_offset,
  TiSuggestionList *out
);

int ti_fill_suggestions_at_cursor(
  const TiSourceList *sources,
  int cursor_byte_offset,
  TiSuggestionList *out
);

#endif
