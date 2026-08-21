#include "picoruby_ti_arena.h"
#include "picoruby_ti_builtin_database.h"
#include "picoruby_ti_diagnostic.h"
#include "picoruby_ti_hover.h"
#include "picoruby_ti_suggest.h"
#include "picoruby_ti_t.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
has_suggestion(const TiSuggestionList *suggestions, const char *contents) {
  for (int index = 0; index < suggestions->count; index++) {
    if (strcmp(suggestions->items[index].contents, contents) == 0)
      return 1;
  }

  return 0;
}

static TiSourceList
source_list_for(const char *source, TiSource *source_item) {
  source_item->source = source;
  source_item->source_byte_length = (int)strlen(source);

  TiSourceList sources = {
    .items = source_item,
    .count = 1,
  };

  return sources;
}

static int
find_hover(const char *source, int cursor_byte_offset, TiHoverInfo *hover_info) {
  TiSource source_item;
  TiSourceList sources = source_list_for(source, &source_item);

  return ti_find_hover_at_cursor(&sources, cursor_byte_offset, hover_info);
}

static TiSuggestionList
suggest_source(const char *source) {
  TiSuggestionList suggestions;
  int source_length = (int)strlen(source);
  TiSource source_item;
  TiSourceList sources = source_list_for(source, &source_item);

  ti_fill_suggestions_at_cursor(
    &sources,
    source_length,
    &suggestions
  );

  return suggestions;
}

static TiDiagnosticList
diagnose_source(const char *source) {
  TiDiagnosticList diagnostics;
  TiSource source_item;
  TiSourceList sources = source_list_for(source, &source_item);

  ti_fill_diagnostics(
    &sources,
    &diagnostics
  );

  return diagnostics;
}

static void
test_builtin_argument_type_diagnostic(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("\"x\".tr(1, \"a\")");

  assert(diagnostics.count == 1);
  assert(strcmp(
    diagnostics.items[0].message,
    "type mismatch: expected String, but got Integer for String.tr"
  ) == 0);
  assert(diagnostics.items[0].start_byte_offset == 7);
  assert(diagnostics.items[0].end_byte_offset == 8);

  diagnostics =
    diagnose_source("v = 1\nv = \"x\"\n\"x\".tr(v, \"a\")");
  assert(diagnostics.count == 0);

  diagnostics =
    diagnose_source("v = 1\nv = []\n\"x\".tr(v, \"a\")");
  assert(diagnostics.count == 1);
}

static void
test_unknown_argument_has_no_diagnostic(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("\"x\".tr(unknown, \"a\")");

  assert(diagnostics.count == 0);
}

static void
test_array_and_hash_contents_have_no_diagnostic(void) {
  TiDiagnosticList array_diagnostics =
    diagnose_source("[1] + [\"x\"]");
  TiDiagnosticList hash_diagnostics =
    diagnose_source("{a: 1}.merge({a: \"x\"})");

  assert(array_diagnostics.count == 0);
  assert(hash_diagnostics.count == 0);
}

static void
test_builtin_argument_count_diagnostic(void) {
  TiDiagnosticList diagnostics = diagnose_source("1.to_s(10, 2)");

  assert(diagnostics.count == 1);
  assert(strcmp(
    diagnostics.items[0].message,
    "too many arguments for Integer.to_s"
  ) == 0);
}

static void
test_splat_arguments_have_no_argument_count_diagnostic(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("a = [\"a\", \"b\"]\n\"x\".tr(*a)");
  assert(diagnostics.count == 0);

  diagnostics =
    diagnose_source("a = []\n\"x\".tr(\"a\", \"b\", *a)");
  assert(diagnostics.count == 0);

  diagnostics =
    diagnose_source("a = {}\n\"x\".tr(\"a\", \"b\", **a)");
  assert(diagnostics.count == 0);
}

static void
test_user_defined_method_arguments_have_no_diagnostic(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("def call(value)\nvalue\nend\ncall()\ncall(1, 2)");

  assert(diagnostics.count == 0);
}

static void
test_user_defined_method_return_type_is_scoped_by_class(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("class Hoge\n"
                    "  def test\n"
                    "    '1'\n"
                    "  end\n"
                    "end\n"
                    "h = Hoge.new\n"
                    "1 + h.test\n"
                    "def test\n"
                    "  1\n"
                    "end");

  assert(diagnostics.count == 1);
}

static void
test_user_defined_method_does_not_share_type_with_local_variable(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("def test\n"
                    "  '1'\n"
                    "end\n"
                    "test = 1\n"
                    "1 + test()");

  assert(diagnostics.count == 1);
}

static void
test_literal_bindings(void) {
  TiSuggestionList integer_suggestions = suggest_source("a = 1\na.");
  assert(has_suggestion(&integer_suggestions, "abs"));

  TiSuggestionList string_suggestions = suggest_source("s = \"x\"\ns.sp");
  assert(has_suggestion(&string_suggestions, "split"));

  TiSuggestionList hash_suggestions = suggest_source("h = {}\nh.ke");
  assert(has_suggestion(&hash_suggestions, "key"));
}

static void
test_binding_lookup(void) {
  TiSuggestionList suggestions = suggest_source("a = 1\nb = a\nb.ab");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_method_chain(void) {
  TiSuggestionList suggestions =
    suggest_source("s = \"abc\".tr(\"a\", \"b\")\ns.le");
  assert(has_suggestion(&suggestions, "length"));
}

static void
test_instance_variable(void) {
  TiSuggestionList suggestions = suggest_source("@x = 1\n@x.ab");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_definition_return(void) {
  TiSuggestionList suggestions =
    suggest_source("a = 1\ndef plus_one(a) = a + 1\nplus_one().ab");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_definition_binding_return(void) {
  TiSuggestionList suggestions = suggest_source("def message\n"
                                                "  value = \"x\"\n"
                                                "  value\n"
                                                "end\n"
                                                "message().sp");
  assert(has_suggestion(&suggestions, "split"));
}

static void
test_if_return(void) {
  TiSuggestionList suggestions = suggest_source("def mixed\n"
                                                "  if condition\n"
                                                "    1\n"
                                                "  else\n"
                                                "    \"x\"\n"
                                                "  end\n"
                                                "end\n"
                                                "mixed().");
  assert(has_suggestion(&suggestions, "abs"));
  assert(has_suggestion(&suggestions, "bytes"));
}

static void
test_case_return(void) {
  TiSuggestionList suggestions = suggest_source("def mixed\n"
                                                "  case value\n"
                                                "  when 1, 2\n"
                                                "    1\n"
                                                "  else\n"
                                                "    \"x\"\n"
                                                "  end\n"
                                                "end\n"
                                                "mixed().");
  assert(has_suggestion(&suggestions, "abs"));
  assert(has_suggestion(&suggestions, "bytes"));
}

static void
test_case_without_else_return(void) {
  const char *source = "result = case value\n"
                       "when 1\n"
                       "  1\n"
                       "end\n"
                       "result\n";
  const char *target = strrchr(source, 'r');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(strcmp(hover_info.type_name, "Union<Integer NilClass>") == 0);
}

static void
test_case_condition_evaluation(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("result = case value\n"
                    "when \"x\".tr(1, \"a\")\n"
                    "  1\n"
                    "end");
  assert(diagnostics.count == 1);

  TiSuggestionList suggestions =
    suggest_source("result = case\n"
                   "when assigned = 1\n"
                   "  nil\n"
                   "end\n"
                   "assigned.");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_case_match_return(void) {
  TiSuggestionList suggestions = suggest_source("def mixed\n"
                                                "  case value\n"
                                                "  in Integer\n"
                                                "    1\n"
                                                "  else\n"
                                                "    \"x\"\n"
                                                "  end\n"
                                                "end\n"
                                                "mixed().");
  assert(has_suggestion(&suggestions, "abs"));
  assert(has_suggestion(&suggestions, "bytes"));
}

static void
test_case_match_without_else_return(void) {
  const char *source = "result = case value\n"
                       "in Integer\n"
                       "  1\n"
                       "end\n"
                       "result\n";
  const char *target = strrchr(source, 'r');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(strcmp(hover_info.type_name, "Integer") == 0);
}

static void
test_case_match_pattern_evaluation(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("result = case value\n"
                    "in ^(\"x\".tr(1, \"a\"))\n"
                    "  1\n"
                    "else\n"
                    "  2\n"
                    "end");
  assert(diagnostics.count == 1);

  TiSuggestionList suggestions =
    suggest_source("result = case value\n"
                   "in ^(assigned = 1)\n"
                   "  nil\n"
                   "else\n"
                   "  nil\n"
                   "end\n"
                   "assigned.");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_case_match_pattern_variable_has_no_binding(void) {
  TiSuggestionList suggestions =
    suggest_source("result = case value\n"
                   "in captured\n"
                   "  nil\n"
                   "else\n"
                   "  nil\n"
                   "end\n"
                   "captured.");
  assert(suggestions.count == 0);
}

static void
test_explicit_return(void) {
  TiSuggestionList suggestions = suggest_source("def mixed\n"
                                                "  return 1\n"
                                                "  \"x\"\n"
                                                "end\n"
                                                "mixed().");
  assert(has_suggestion(&suggestions, "abs"));
  assert(has_suggestion(&suggestions, "bytes"));
}

static void
test_type_at_cursor(void) {
  const char *source = "value = 1\nvalue\n";
  const char *target = strrchr(source, 'v');
  assert(target);

  TiHoverInfo hover_info;
  int found = find_hover(
    source,
    (int)(target - source),
    &hover_info
  );

  assert(found);
  assert(!hover_info.is_method);
  assert(strcmp(hover_info.variable_name, "value") == 0);
  assert(strcmp(hover_info.type_name, "Integer") == 0);
}

static void
test_union_type_at_cursor(void) {
  const char *source = "value = 1\nvalue = \"x\"\nvalue\n";
  const char *target = strrchr(source, 'v');
  assert(target);

  TiHoverInfo hover_info;
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(strcmp(hover_info.type_name, "Union<Integer String>") == 0);
}

static void
test_builtin_method_at_cursor(void) {
  const char *source = "\"x\".bytes";
  const char *target = strstr(source, "bytes");
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(hover_info.is_method);
  assert(hover_info.method_name_length == 5);
  assert(hover_info.method_document);
  assert(strncmp(hover_info.method_signature, "bytes:", 6) == 0);
}

static void
test_defined_method_at_cursor(void) {
  const char *source = "def answer(value) = 1\nanswer(1)";
  const char *target = strrchr(source, 'a');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(hover_info.is_method);
  assert(hover_info.method_name_length == 6);
  assert(strcmp(hover_info.method_document, "") == 0);
  assert(strcmp(hover_info.method_signature, "answer(value) -> Integer") == 0);
}

static void
test_forward_definition(void) {
  TiSuggestionList suggestions =
    suggest_source("x = my_method()\ndef my_method() = \"x\"\nx.sp");
  assert(has_suggestion(&suggestions, "split"));
}

static void
test_preload_source_definition(void) {
  const char *preload_source = "def answer() = 1";
  const char *source = "answer().";
  TiSource source_items[] = {
    {
      .source = preload_source,
      .source_byte_length = (int)strlen(preload_source),
    },
    {
      .source = source,
      .source_byte_length = (int)strlen(source),
    },
  };
  TiSourceList sources = {
    .items = source_items,
    .count = 2,
  };
  TiSuggestionList suggestions;

  ti_fill_suggestions_at_cursor(
    &sources,
    source_items[1].source_byte_length,
    &suggestions
  );

  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_union_binding(void) {
  TiSuggestionList suggestions = suggest_source("v = 1\nv = \"s\"\nv.");
  assert(has_suggestion(&suggestions, "abs"));
  assert(has_suggestion(&suggestions, "bytes"));
}

static void
test_union_capacity(void) {
  uint8_t class_identifiers[TI_UNION_CAPACITY] = {
    TI_CLASS_INTEGER,
    TI_CLASS_FLOAT,
    TI_CLASS_STRING,
    TI_CLASS_SYMBOL,
    TI_CLASS_TRUE,
  };

  ti_reset_arena();
  assert(ti_initialize_t());

  uint16_t union_t_node_index = 0;

  for (int index = 0; index < TI_UNION_CAPACITY; index++) {
    uint16_t t_node_index = ti_new_t(class_identifiers[index], 0, 0);
    union_t_node_index = ti_make_union(union_t_node_index, t_node_index);
  }

  int union_member_count = 0;

  for (const T *union_t = ti_get_t(union_t_node_index);
       union_t;
       union_t = ti_get_t(union_t->union_next)) {
    union_member_count++;
  }

  assert(union_member_count == TI_UNION_CAPACITY);

  uint16_t t_node_index = ti_new_t(TI_CLASS_FALSE, 0, 0);
  union_t_node_index = ti_make_union(union_t_node_index, t_node_index);

  const T *union_t = ti_get_t(union_t_node_index);
  assert(union_t);
  assert(union_t->object_class_id == TI_CLASS_UNTYPED);
  assert(union_t->union_next == 0);
}

static void
test_unknown_return(void) {
  TiSuggestionList suggestions =
    suggest_source("def orphan(x) = x.foo\norphan().");
  assert(suggestions.count == 0);
}

static void
test_while_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run\n"
                                                "  while true\n"
                                                "    assigned = 1\n"
                                                "  end\n"
                                                "  assigned\n"
                                                "end\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_while_return(void) {
  const char *source = "result = while true\n"
                       "end\n"
                       "result\n";
  const char *target = strrchr(source, 'r');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.type_name, "NilClass") == 0);
}

static void
test_until_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run\n"
                                                "  until false\n"
                                                "    assigned = \"x\"\n"
                                                "  end\n"
                                                "  assigned\n"
                                                "end\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "bytes"));
}

static void
test_for_return(void) {
  const char *source = "result = for item in [1]\n"
                       "end\n"
                       "result\n";
  const char *target = strrchr(source, 'r');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.type_name, "Array<Integer>") == 0);
}

static void
test_for_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run\n"
                                                "  for item in [1]\n"
                                                "    assigned = 1\n"
                                                "  end\n"
                                                "  assigned\n"
                                                "end\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_begin_rescue_return(void) {
  const char *source = "result = begin\n"
                       "  1\n"
                       "rescue\n"
                       "  \"x\"\n"
                       "end\n"
                       "result\n";
  const char *target = strrchr(source, 'r');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.type_name, "Union<Integer String>") == 0);
}

static void
test_begin_else_return(void) {
  const char *source = "result = begin\n"
                       "  1\n"
                       "rescue\n"
                       "  \"x\"\n"
                       "else\n"
                       "  :done\n"
                       "end\n"
                       "result\n";
  const char *target = strrchr(source, 'r');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.type_name, "Union<Symbol String>") == 0);
}

static void
test_rescue_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run\n"
                                                "  begin\n"
                                                "    nil\n"
                                                "  rescue\n"
                                                "    assigned = 1\n"
                                                "  end\n"
                                                "  assigned\n"
                                                "end\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_ensure_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run\n"
                                                "  begin\n"
                                                "    nil\n"
                                                "  ensure\n"
                                                "    assigned = \"x\"\n"
                                                "  end\n"
                                                "  assigned\n"
                                                "end\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "bytes"));
}

static void
test_unless_return(void) {
  const char *source = "result = unless value\n"
                       "  1\n"
                       "else\n"
                       "  \"x\"\n"
                       "end\n"
                       "result\n";
  const char *target = strrchr(source, 'r');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.type_name, "Union<Integer String>") == 0);
}

static void
test_unless_without_else_return(void) {
  const char *source = "result = unless value\n"
                       "  1\n"
                       "end\n"
                       "result\n";
  const char *target = strrchr(source, 'r');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.type_name, "Union<Integer NilClass>") == 0);
}

static void
test_unhandled_node_is_traversed(void) {
  TiDiagnosticList diagnostics = diagnose_source("def run\n"
                                                 "  true && \"x\".tr(1, \"a\")\n"
                                                 "end");
  assert(diagnostics.count == 1);
}

static void
test_nested_definition_in_definition(void) {
  TiSuggestionList suggestions = suggest_source("def outer\n"
                                                "  def inner\n"
                                                "    1\n"
                                                "  end\n"
                                                "  inner()\n"
                                                "end\n"
                                                "outer().");
  assert(has_suggestion(&suggestions, "abs"));
}

static void
test_binding_overflow(void) {
  size_t capacity = 24000;
  char *source = malloc(capacity);
  assert(source);

  size_t offset = 0;
  for (int index = 0; index < 700; index++) {
    int written = snprintf(
      source + offset,
      capacity - offset,
      "value_%d = %d\n",
      index,
      index
    );
    assert(written > 0);
    offset += (size_t)written;
    assert(offset < capacity - 16);
  }

  memcpy(source + offset, "value_0.\0", 9);
  offset += 8;

  TiSuggestionList suggestions;
  TiSource source_item;
  TiSourceList sources = source_list_for(source, &source_item);
  int count = ti_fill_suggestions_at_cursor(
    &sources,
    (int)offset,
    &suggestions
  );
  assert(count == 0);

  free(source);
}

static void
test_instance_variable_type_at_cursor(void) {
  const char *source = "class Holder\n"
                       "  def initialize\n"
                       "    @name = \"x\"\n"
                       "  end\n"
                       "end\n";
  const char *target = strstr(source, "@name");
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.variable_name, "@name") == 0);
  assert(strcmp(hover_info.type_name, "String") == 0);
}

static void
test_instance_variable_assignments_are_unioned(void) {
  const char *source = "class Holder\n"
                       "  def initialize\n"
                       "    @value = 1\n"
                       "  end\n"
                       "  def reset\n"
                       "    @value = \"x\"\n"
                       "  end\n"
                       "end\n";
  const char *target = strstr(source, "@value = \"x\"");
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.type_name, "Union<Integer String>") == 0);
}

static void
test_instance_variable_is_scoped_to_class(void) {
  const char *source = "class A\n"
                       "  def initialize\n"
                       "    @value = 1\n"
                       "  end\n"
                       "end\n"
                       "class B\n"
                       "  def initialize\n"
                       "    @value = \"x\"\n"
                       "  end\n"
                       "end\n";
  const char *target = strstr(source, "@value = \"x\"");
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(strcmp(hover_info.type_name, "String") == 0);
}

static void
test_attribute_reader_type_at_cursor(void) {
  const char *source = "class Holder\n"
                       "  attr_accessor :name\n"
                       "  def initialize\n"
                       "    @name = \"x\"\n"
                       "  end\n"
                       "end\n"
                       "holder = Holder.new\n"
                       "holder.name\n";
  const char *target = strstr(source, "holder.name");
  TiHoverInfo hover_info;

  assert(target);
  target = strstr(target, "name");

  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(hover_info.is_method);
  assert(strcmp(hover_info.method_signature, "name() -> String") == 0);
}

static void
test_self_method_type_at_cursor(void) {
  const char *source = "class Holder\n"
                       "  def name = \"x\"\n"
                       "  def run\n"
                       "    self.name\n"
                       "  end\n"
                       "end\n";
  const char *target = strstr(source, "self.name");
  TiHoverInfo hover_info;

  assert(target);
  target = strstr(target, "name");

  assert(find_hover(source, (int)(target - source), &hover_info));
  assert(hover_info.is_method);
  assert(strcmp(hover_info.method_signature, "name() -> String") == 0);
}

int
main(void) {
  test_builtin_argument_type_diagnostic();
  test_unknown_argument_has_no_diagnostic();
  test_array_and_hash_contents_have_no_diagnostic();
  test_builtin_argument_count_diagnostic();
  test_splat_arguments_have_no_argument_count_diagnostic();
  test_user_defined_method_arguments_have_no_diagnostic();
  test_user_defined_method_return_type_is_scoped_by_class();
  test_user_defined_method_does_not_share_type_with_local_variable();
  test_literal_bindings();
  test_binding_lookup();
  test_method_chain();
  test_instance_variable();
  test_instance_variable_type_at_cursor();
  test_instance_variable_assignments_are_unioned();
  test_instance_variable_is_scoped_to_class();
  test_attribute_reader_type_at_cursor();
  test_self_method_type_at_cursor();
  test_definition_return();
  test_definition_binding_return();
  test_if_return();
  test_case_return();
  test_case_without_else_return();
  test_case_condition_evaluation();
  test_case_match_return();
  test_case_match_without_else_return();
  test_case_match_pattern_evaluation();
  test_case_match_pattern_variable_has_no_binding();
  test_explicit_return();
  test_type_at_cursor();
  test_union_type_at_cursor();
  test_builtin_method_at_cursor();
  test_defined_method_at_cursor();
  test_forward_definition();
  test_preload_source_definition();
  test_union_binding();
  test_union_capacity();
  test_unknown_return();
  test_while_body_binding();
  test_while_return();
  test_until_body_binding();
  test_for_return();
  test_for_body_binding();
  test_begin_rescue_return();
  test_begin_else_return();
  test_rescue_body_binding();
  test_ensure_body_binding();
  test_unless_return();
  test_unless_without_else_return();
  test_unhandled_node_is_traversed();
  test_nested_definition_in_definition();
  test_binding_overflow();

  return 0;
}
