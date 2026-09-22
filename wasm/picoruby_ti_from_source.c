#include "picoruby_ti_diagnostic.h"
#include "picoruby_ti_hover.h"
#include "picoruby_ti_source.h"
#include "picoruby_ti_suggest.h"
#include <stddef.h>

static TiSuggestionList suggestions;
static TiHoverInfo hover;
static TiDiagnosticList diagnostics;

static TiSourceList
make_source_list(TiSource *source, const char *bytes, int byte_length) {
  source->source = bytes;
  source->source_byte_length = byte_length;

  TiSourceList sources = {
    .items = source,
    .count = 1,
  };

  return sources;
}

int
ti_fill_suggestions_from_source(
  const char *source_bytes,
  int source_byte_length,
  int cursor_byte_offset
) {

  TiSource source;
  TiSourceList sources =
    make_source_list(&source, source_bytes, source_byte_length);

  return ti_fill_suggestions_at_cursor(
    &sources,
    cursor_byte_offset,
    &suggestions
  );
}

static const TiSuggestion *
get_suggestion(int index) {
  if (index < 0 || index >= suggestions.count)
    return NULL;

  return &suggestions.items[index];
}

const char *
ti_get_suggestion_contents(int index) {
  const TiSuggestion *suggestion = get_suggestion(index);
  return suggestion ? suggestion->contents : NULL;
}

const char *
ti_get_suggestion_detail(int index) {
  const TiSuggestion *suggestion = get_suggestion(index);
  return suggestion ? suggestion->detail : NULL;
}

const char *
ti_get_suggestion_document(int index) {
  const TiSuggestion *suggestion = get_suggestion(index);
  return suggestion ? suggestion->document : NULL;
}

const char *
ti_get_suggestion_class_name(int index) {
  const TiSuggestion *suggestion = get_suggestion(index);
  return suggestion ? suggestion->class_name : NULL;
}

int
ti_find_hover_from_source(
  const char *source_bytes,
  int source_byte_length,
  int cursor_byte_offset
) {

  TiSource source;
  TiSourceList sources =
    make_source_list(&source, source_bytes, source_byte_length);

  return ti_find_hover_at_cursor(&sources, cursor_byte_offset, &hover);
}

const char *
ti_get_hover_variable_name(void) {
  return hover.found ? hover.variable_name : NULL;
}

const char *
ti_get_hover_type_name(void) {
  return hover.found ? hover.type_name : NULL;
}

const char *
ti_get_hover_method_signature(void) {
  return hover.found ? hover.method_signature : NULL;
}

const char *
ti_get_hover_method_document(void) {
  return hover.found ? hover.method_document : NULL;
}

int
ti_get_hover_is_method(void) {
  return hover.found ? hover.is_method : 0;
}

int
ti_fill_diagnostics_from_source(
  const char *source_bytes,
  int source_byte_length
) {

  TiSource source;
  TiSourceList sources =
    make_source_list(&source, source_bytes, source_byte_length);

  return ti_fill_diagnostics(&sources, &diagnostics);
}

static const TiDiagnostic *
get_diagnostic(int index) {
  if (index < 0 || index >= diagnostics.count)
    return NULL;

  return &diagnostics.items[index];
}

int
ti_get_diagnostic_start_byte_offset(int index) {
  const TiDiagnostic *diagnostic = get_diagnostic(index);
  return diagnostic ? diagnostic->start_byte_offset : 0;
}

int
ti_get_diagnostic_end_byte_offset(int index) {
  const TiDiagnostic *diagnostic = get_diagnostic(index);
  return diagnostic ? diagnostic->end_byte_offset : 0;
}

const char *
ti_get_diagnostic_message(int index) {
  const TiDiagnostic *diagnostic = get_diagnostic(index);
  return diagnostic ? diagnostic->message : NULL;
}
