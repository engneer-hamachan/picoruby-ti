#include "picoruby_ti_method_evaluator.h"
#include "picoruby_ti_builtin.h"
#include "picoruby_ti_diagnostic.h"
#include "picoruby_ti_eval.h"
#include "picoruby_ti_t.h"
#include "picoruby_ti_t_frame.h"
#include "picoruby_ti_type.h"
#include <stdio.h>
#include <string.h>

#define TI_CALL_ARGUMENT_CAPACITY 64

typedef struct {
  const pm_node_t *diagnostic_node;
  uint16_t t_node_index;
  const uint8_t *keyword_name;
  size_t keyword_name_length;
} TiCallArgument;

typedef struct {
  TiCallArgument positional_arguments[TI_CALL_ARGUMENT_CAPACITY];
  TiCallArgument keyword_arguments[TI_CALL_ARGUMENT_CAPACITY];
  int positional_argument_count;
  int keyword_argument_count;
  int has_positional_splat;
  int has_keyword_splat;
} TiCallArguments;

typedef struct {
  pm_location_t diagnostic_location;
  char diagnostic_message[TI_BUILTIN_METHOD_MISMATCH_MESSAGE_CAPACITY];
} TiBuiltinMethodMismatch;

static int
set_array_variant_t_node_index(
  const TiBuiltinMethod *builtin_method,
  uint16_t *array_variant_t_node_index
) {
  uint8_t class_identifiers[4];

  int class_identifier_count =
    ti_get_builtin_return_array_variant_classes(
      builtin_method,
      class_identifiers
    );

  *array_variant_t_node_index = 0;

  for (int index = 0; index < class_identifier_count; index++) {
    uint16_t t_node_index = ti_new_t(class_identifiers[index], 0, 0);

    *array_variant_t_node_index =
      ti_make_union(*array_variant_t_node_index, t_node_index);

    if (*array_variant_t_node_index == 0)
      return 0;
  }

  return 1;
}

static uint16_t
make_method_return_t_node_index(const TiBuiltinMethod *builtin_method) {
  uint8_t return_class_identifiers[4];

  int return_class_identifier_count =
    ti_get_builtin_return_classes(
      builtin_method,
      return_class_identifiers
    );

  uint16_t method_return_t_node_index = 0;

  for (int index = 0; index < return_class_identifier_count; index++) {
    uint16_t array_variant_t_node_index = 0;

    if (
        return_class_identifiers[index] == TI_CLASS_ARRAY &&
        !set_array_variant_t_node_index(
          builtin_method,
          &array_variant_t_node_index
        )
      ) {

      return 0;
    }

    uint16_t union_member_t_node_index =
      ti_new_t(
        return_class_identifiers[index],
        0,
        array_variant_t_node_index
      );

    method_return_t_node_index =
      ti_make_union(
        method_return_t_node_index,
        union_member_t_node_index
      );

    if (method_return_t_node_index == 0)
      return 0;
  }

  return method_return_t_node_index;
}

static void
append_call_argument(
  TiCallArgument destination_arguments[TI_CALL_ARGUMENT_CAPACITY],
  int *destination_argument_count,
  const pm_node_t *diagnostic_node,
  uint16_t t_node_index,
  const uint8_t *keyword_name,
  size_t keyword_name_length
) {

  if (*destination_argument_count >= TI_CALL_ARGUMENT_CAPACITY)
    return;

  TiCallArgument *call_argument =
    &destination_arguments[(*destination_argument_count)++];

  call_argument->diagnostic_node = diagnostic_node;
  call_argument->t_node_index = t_node_index;
  call_argument->keyword_name = keyword_name;
  call_argument->keyword_name_length = keyword_name_length;
}

static void
collect_keyword_arguments(
  TiContext *context,
  const pm_keyword_hash_node_t *keyword_hash_node,
  int evaluation_depth,
  TiCallArguments *call_arguments
) {

  for (size_t keyword_argument_index = 0;
       keyword_argument_index < keyword_hash_node->elements.size;
       keyword_argument_index++) {

    const pm_node_t *keyword_argument_node =
      keyword_hash_node->elements.nodes[keyword_argument_index];

    if (PM_NODE_TYPE(keyword_argument_node) == PM_ASSOC_SPLAT_NODE) {
      call_arguments->has_keyword_splat = 1;
      continue;
    }

    if (PM_NODE_TYPE(keyword_argument_node) == PM_ASSOC_NODE) {
      const pm_node_t *keyword_name_node =
        ((const pm_assoc_node_t *)keyword_argument_node)->key;

      const pm_node_t *keyword_value_node =
        ((const pm_assoc_node_t *)keyword_argument_node)->value;

      const uint8_t *keyword_name = NULL;
      size_t keyword_name_length = 0;

      if (
        keyword_name_node &&
        PM_NODE_TYPE(keyword_name_node) == PM_SYMBOL_NODE
      ) {

        keyword_name =
          pm_string_source(
            &((const pm_symbol_node_t *)keyword_name_node)->unescaped
          );
        keyword_name_length =
          pm_string_length(
            &((const pm_symbol_node_t *)keyword_name_node)->unescaped
          );
      }

      uint16_t t_node_index =
        ti_eval_expression(context, keyword_value_node, evaluation_depth + 1);

      const pm_node_t *diagnostic_node = keyword_argument_node;

      if (keyword_value_node)
        diagnostic_node = keyword_value_node;

      append_call_argument(
        call_arguments->keyword_arguments,
        &call_arguments->keyword_argument_count,
        diagnostic_node,
        t_node_index,
        keyword_name,
        keyword_name_length
      );

      continue;
    }

    append_call_argument(
      call_arguments->keyword_arguments,
      &call_arguments->keyword_argument_count,
      keyword_argument_node,
      0,
      NULL,
      0
    );
  }
}

static void
collect_call_arguments(
  TiContext *context,
  const pm_call_node_t *call_node,
  int evaluation_depth,
  TiCallArguments *call_arguments
) {

  memset(call_arguments, 0, sizeof(*call_arguments));

  if (!call_node->arguments)
    return;

  for (size_t call_argument_index = 0;
       call_argument_index < call_node->arguments->arguments.size;
       call_argument_index++) {

    const pm_node_t *call_argument_node =
      call_node->arguments->arguments.nodes[call_argument_index];

    if (PM_NODE_TYPE(call_argument_node) == PM_SPLAT_NODE) {
      call_arguments->has_positional_splat = 1;
      continue;
    }

    if (PM_NODE_TYPE(call_argument_node) == PM_KEYWORD_HASH_NODE) {
      collect_keyword_arguments(
        context,
        (const pm_keyword_hash_node_t *)call_argument_node,
        evaluation_depth,
        call_arguments
      );

      continue;
    }

    uint16_t t_node_index =
      ti_eval_expression(context, call_argument_node, evaluation_depth + 1);

    append_call_argument(
      call_arguments->positional_arguments,
      &call_arguments->positional_argument_count,
      call_argument_node,
      t_node_index,
      NULL,
      0
    );
  }
}

static int
contains_class_identifier(
  const uint8_t class_identifiers[4],
  int class_identifier_count,
  uint8_t class_identifier
) {

  for (int index = 0; index < class_identifier_count; index++) {
    if (class_identifiers[index] == class_identifier)
      return 1;
  }

  return 0;
}

static int
argument_type_matches(
  const TiBuiltinArgument *builtin_argument,
  uint16_t actual_t_node_index
) {

  if (actual_t_node_index == 0)
    return 1;

  uint8_t expected_class_identifiers[4];

  int expected_class_identifier_count =
    ti_get_builtin_argument_classes(
      builtin_argument,
      expected_class_identifiers
    );

  if (
    expected_class_identifier_count == 0 ||
    contains_class_identifier(
      expected_class_identifiers,
      expected_class_identifier_count,
      TI_CLASS_UNTYPED
    )
  ) {

    return 1;
  }


  for (
    const T *actual_t_node = ti_get_t(actual_t_node_index);
    actual_t_node;
    actual_t_node = ti_get_t(actual_t_node->union_next)
  ) {

    if (actual_t_node->object_class_id == TI_CLASS_UNTYPED)
      return 1;

    if (
      contains_class_identifier(
        expected_class_identifiers,
        expected_class_identifier_count,
        actual_t_node->object_class_id
      )
    ) {

      return 1;
    }
  }

  return 0;
}

static uint16_t
make_argument_t_node_index(const TiBuiltinArgument *builtin_argument) {
  uint8_t class_identifiers[4];

  int class_identifier_count =
    ti_get_builtin_argument_classes(builtin_argument, class_identifiers);

  uint16_t argument_t_node_index = 0;

  for (int index = 0; index < class_identifier_count; index++) {
    uint16_t union_member_t_node_index =
      ti_new_t(class_identifiers[index], 0, 0);

    argument_t_node_index =
      ti_make_union(argument_t_node_index, union_member_t_node_index);
  }

  return argument_t_node_index;
}

static void
set_type_mismatch(
  const char *class_name,
  const char *method_name,
  const TiBuiltinArgument *builtin_argument,
  const TiCallArgument *call_argument,
  TiBuiltinMethodMismatch *builtin_method_mismatch
) {

  char expected_type_string[TI_TYPE_STRING_CAPACITY];
  char actual_type_string[TI_TYPE_STRING_CAPACITY];

  ti_type_to_string(
    make_argument_t_node_index(builtin_argument),
    expected_type_string,
    sizeof(expected_type_string)
  );

  ti_type_to_string(
    call_argument->t_node_index,
    actual_type_string,
    sizeof(actual_type_string)
  );

  builtin_method_mismatch->diagnostic_location =
    call_argument->diagnostic_node->location;

  snprintf(
    builtin_method_mismatch->diagnostic_message,
    sizeof(builtin_method_mismatch->diagnostic_message),
    "type mismatch: expected %s, but got %s for %s.%s",
    expected_type_string,
    actual_type_string,
    class_name,
    method_name
  );
}

static void
set_argument_count_mismatch(
  const pm_call_node_t *call_node,
  const char *class_name,
  const char *method_name,
  const char *argument_count_description,
  TiBuiltinMethodMismatch *builtin_method_mismatch
) {

  builtin_method_mismatch->diagnostic_location = call_node->base.location;

  snprintf(
    builtin_method_mismatch->diagnostic_message,
    sizeof(builtin_method_mismatch->diagnostic_message),
    "%s arguments for %s.%s",
    argument_count_description,
    class_name,
    method_name
  );
}

static int
count_remaining_required_positional_arguments(
  const TiBuiltinMethod *builtin_method,
  int builtin_argument_index
) {

  int required_positional_argument_count = 0;

  for (
    int remaining_argument_index = builtin_argument_index;
     remaining_argument_index < builtin_method->argument_count;
     remaining_argument_index++
   ) {

    const TiBuiltinArgument *builtin_argument =
      ti_get_builtin_argument(builtin_method, remaining_argument_index);

    if (
      builtin_argument &&
      builtin_argument->kind == TI_BUILTIN_ARGUMENT_REQUIRED
    ) {
      required_positional_argument_count++;
    }
  }

  return required_positional_argument_count;
}

static int
keyword_name_matches(
  const TiBuiltinArgument *builtin_argument,
  const TiCallArgument *call_argument
) {

  const char *expected_keyword_name =
    ti_get_builtin_argument_name(builtin_argument);

  return expected_keyword_name &&
         strlen(expected_keyword_name) == call_argument->keyword_name_length &&
         call_argument->keyword_name &&
         memcmp(
           expected_keyword_name,
           call_argument->keyword_name,
           call_argument->keyword_name_length
         ) == 0;
}

static int
match_argument_type_or_set_mismatch(
  const char *class_name,
  const char *method_name,
  const TiBuiltinArgument *builtin_argument,
  const TiCallArgument *call_argument,
  TiBuiltinMethodMismatch *builtin_method_mismatch
) {

  if (
    argument_type_matches(
      builtin_argument,
      call_argument->t_node_index
    )
  ) {

    return 1;
  }

  set_type_mismatch(
    class_name,
    method_name,
    builtin_argument,
    call_argument,
    builtin_method_mismatch
  );

  return 0;
}

static int
match_builtin_method(
  const pm_call_node_t *call_node,
  const char *class_name,
  const char *method_name,
  const TiBuiltinMethod *builtin_method,
  const TiCallArguments *call_arguments,
  TiBuiltinMethodMismatch *builtin_method_mismatch
) {

  int positional_argument_index = 0;
  uint8_t keyword_argument_matched_flags[TI_CALL_ARGUMENT_CAPACITY] = {0};
  int has_rest_keyword_argument = 0;

  for (
    int builtin_argument_index = 0;
    builtin_argument_index < builtin_method->argument_count;
    builtin_argument_index++
  ) {

    const TiBuiltinArgument *builtin_argument =
      ti_get_builtin_argument(builtin_method, builtin_argument_index);

    if (!builtin_argument)
      continue;

    switch (builtin_argument->kind) {
    case TI_BUILTIN_ARGUMENT_REQUIRED:
      if (
        positional_argument_index >=
        call_arguments->positional_argument_count
      ) {

        if (call_arguments->has_positional_splat)
          break;

        set_argument_count_mismatch(
          call_node,
          class_name,
          method_name,
          "too few",
          builtin_method_mismatch
        );

        return 0;
      }

      if (
        !match_argument_type_or_set_mismatch(
          class_name,
          method_name,
          builtin_argument,
          &call_arguments->positional_arguments[positional_argument_index],
          builtin_method_mismatch
        )
      ) {

        return 0;
      }

      positional_argument_index++;

      break;

    case TI_BUILTIN_ARGUMENT_OPTIONAL: {
      int remaining_call_positional_argument_count =
        call_arguments->positional_argument_count - positional_argument_index;

      int remaining_required_positional_argument_count =
        count_remaining_required_positional_arguments(
          builtin_method,
          builtin_argument_index + 1
        );

      if (
        remaining_call_positional_argument_count <=
        remaining_required_positional_argument_count
      ) {

        break;
      }

      if (
        !match_argument_type_or_set_mismatch(
          class_name,
          method_name,
          builtin_argument,
          &call_arguments->positional_arguments[positional_argument_index],
          builtin_method_mismatch
        )
      ) {

        return 0;
      }

      positional_argument_index++;

      break;
    }

    case TI_BUILTIN_ARGUMENT_REST: {
      int rest_positional_end_index =
        call_arguments->positional_argument_count -
        count_remaining_required_positional_arguments(
          builtin_method,
          builtin_argument_index + 1
        );

      while (positional_argument_index < rest_positional_end_index) {
        if (
          !match_argument_type_or_set_mismatch(
            class_name,
            method_name,
            builtin_argument,
            &call_arguments->positional_arguments[positional_argument_index],
            builtin_method_mismatch
          )
        ) {

          return 0;
        }

        positional_argument_index++;
      }

      break;
    }

    case TI_BUILTIN_ARGUMENT_REQUIRED_KEYWORD:
    case TI_BUILTIN_ARGUMENT_OPTIONAL_KEYWORD: {
      int matching_keyword_argument_index = -1;

      for (
        int keyword_argument_index = 0;
        keyword_argument_index < call_arguments->keyword_argument_count;
        keyword_argument_index++
      ) {

        if (
          !keyword_argument_matched_flags[keyword_argument_index] &&
          keyword_name_matches(
            builtin_argument,
            &call_arguments->keyword_arguments[keyword_argument_index]
          )
        ) {

          matching_keyword_argument_index = keyword_argument_index;
          break;
        }
      }

      if (matching_keyword_argument_index < 0) {
        if (
          builtin_argument->kind == TI_BUILTIN_ARGUMENT_REQUIRED_KEYWORD &&
          !call_arguments->has_keyword_splat
        ) {

          set_argument_count_mismatch(
            call_node,
            class_name,
            method_name,
            "too few",
            builtin_method_mismatch
          );

          return 0;
        }

        break;
      }

      const TiCallArgument *call_argument =
        &call_arguments->keyword_arguments[matching_keyword_argument_index];

      if (
        !match_argument_type_or_set_mismatch(
          class_name,
          method_name,
          builtin_argument,
          call_argument,
          builtin_method_mismatch
        )
      ) {

        return 0;
      }

      keyword_argument_matched_flags[matching_keyword_argument_index] = 1;

      break;
    }

    case TI_BUILTIN_ARGUMENT_REST_KEYWORD:
      has_rest_keyword_argument = 1;

      for (
        int keyword_argument_index = 0;
        keyword_argument_index < call_arguments->keyword_argument_count;
        keyword_argument_index++
      ) {

        if (keyword_argument_matched_flags[keyword_argument_index])
          continue;

        const TiCallArgument *call_argument =
          &call_arguments->keyword_arguments[keyword_argument_index];

        if (
          !match_argument_type_or_set_mismatch(
            class_name,
            method_name,
            builtin_argument,
            call_argument,
            builtin_method_mismatch
          )
        ) {

          return 0;
        }

        keyword_argument_matched_flags[keyword_argument_index] = 1;
      }

      break;
    }
  }

  if (
    positional_argument_index < call_arguments->positional_argument_count &&
    !call_arguments->has_positional_splat
  ) {

    set_argument_count_mismatch(
      call_node,
      class_name,
      method_name,
      "too many",
      builtin_method_mismatch
    );

    return 0;
  }

  if (
    !has_rest_keyword_argument &&
    !call_arguments->has_keyword_splat
  ) {
    for (
      int keyword_argument_index = 0;
      keyword_argument_index < call_arguments->keyword_argument_count;
      keyword_argument_index++
    ) {

      if (!keyword_argument_matched_flags[keyword_argument_index]) {
        set_argument_count_mismatch(
          call_node,
          class_name,
          method_name,
          "too many",
          builtin_method_mismatch
        );

        return 0;
      }
    }
  }

  return 1;
}

static uint16_t
evaluate_builtin_method(
  TiContext *context,
  const pm_call_node_t *call_node,
  int evaluation_depth,
  uint8_t class_identifier,
  const TiBuiltinMethod *builtin_method
) {

  TiCallArguments call_arguments;

  collect_call_arguments(
    context,
    call_node,
    evaluation_depth,
    &call_arguments
  );

  const char *class_name = ti_get_builtin_class_name(class_identifier);
  TiBuiltinMethodMismatch builtin_method_mismatch = {0};

  if (
    !match_builtin_method(
      call_node,
      class_name,
      ti_get_builtin_method_name(builtin_method),
      builtin_method,
      &call_arguments,
      &builtin_method_mismatch
    )
  ) {

    ti_add_diagnostic(
      context,
      builtin_method_mismatch.diagnostic_location,
      builtin_method_mismatch.diagnostic_message
    );
  }

  return make_method_return_t_node_index(builtin_method);
}

uint16_t
ti_eval_method(
  TiContext *context,
  const pm_call_node_t *call_node,
  int evaluation_depth
) {

  uint16_t method_name_identifier;

  if (
    !ti_convert_constant_id(
      context,
      call_node->name,
      &method_name_identifier
    )
  ) {

    return 0;
  }

  const pm_constant_t *method_name_constant =
    ti_get_constant(context, call_node->name);

  if (!method_name_constant)
    return 0;

  if (!call_node->receiver) {
    uint16_t defined_return_t_node_index =
      ti_get_method_t(context->current_class_id, method_name_identifier);

    if (defined_return_t_node_index != 0)
      return defined_return_t_node_index;

    const TiBuiltinMethod *kernel_builtin_method =
      ti_get_builtin_instance_method(
        TI_CLASS_KERNEL,
        method_name_constant->start,
        method_name_constant->length
      );

    if (kernel_builtin_method) {
      return evaluate_builtin_method(
        context,
        call_node,
        evaluation_depth,
        TI_CLASS_KERNEL,
        kernel_builtin_method
      );
    }

    return 0;
  }

  uint16_t receiver_t_node_index =
    ti_eval_expression(context, call_node->receiver, evaluation_depth + 1);

  const T *receiver_t_node = ti_get_t(receiver_t_node_index);

  if (!receiver_t_node || receiver_t_node->union_next != 0)
    return 0;

  if ((receiver_t_node->t_flags & TI_T_FLAG_DEFINED_CLASS) != 0) {
    if (
      method_name_constant->length == 3 &&
      memcmp(method_name_constant->start, "new", 3) == 0 &&
      (receiver_t_node->t_flags & TI_T_FLAG_STATIC) != 0
    ) {

      return ti_new_t(
        receiver_t_node->object_class_id,
        receiver_t_node->t_flags & TI_T_FLAG_DEFINED_CLASS,
        receiver_t_node->variants
      );
    }

    return ti_get_method_t(
      receiver_t_node->object_class_id,
      method_name_identifier
    );
  }

  const TiBuiltinMethod *builtin_method;
  int is_static_method =
    (receiver_t_node->t_flags & TI_T_FLAG_STATIC) != 0;

  if (is_static_method) {
    builtin_method =
      ti_get_builtin_static_method(
        receiver_t_node->object_class_id,
        method_name_constant->start,
        method_name_constant->length
      );
  } else {
    builtin_method =
      ti_get_builtin_instance_method(
        receiver_t_node->object_class_id,
        method_name_constant->start,
        method_name_constant->length
      );
  }

  if (!builtin_method)
    return 0;

  return evaluate_builtin_method(
    context,
    call_node,
    evaluation_depth,
    receiver_t_node->object_class_id,
    builtin_method
  );
}
