package main

/*
#cgo CFLAGS: -I${SRCDIR}/../include
#cgo CFLAGS: -I${SRCDIR}/../src/hover
#cgo CFLAGS: -I${SRCDIR}/../src/suggest
#cgo CFLAGS: -I${SRCDIR}/../src/diagnostic
#cgo CFLAGS: -I${SRCDIR}/../src/base
#cgo CFLAGS: -I${SRCDIR}/../src/builtin
#cgo CFLAGS: -I${SRCDIR}/../src/context
#cgo CFLAGS: -I${SRCDIR}/../src/eval
#cgo CFLAGS: -I${SRCDIR}/../src/generated
#cgo CFLAGS: -I${SRCDIR}/../lib/prism/include
#cgo LDFLAGS: ${SRCDIR}/build/libpicoruby_ti.a
#cgo LDFLAGS: ${SRCDIR}/../lib/prism/build/libprism.a

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "picoruby_ti_arena.h"
#include "picoruby_ti_builtin.h"
#include "picoruby_ti_define_info.h"
#include "picoruby_ti_eval.h"
#include "picoruby_ti_hover.h"
#include "picoruby_ti_name.h"
#include "picoruby_ti_suggest.h"
#include "picoruby_ti_diagnostic.h"
#include "picoruby_ti_type.h"

typedef struct {
  const char *name;
  int name_byte_length;
} PicoRubyTiClass;

typedef struct {
  const char *name;
  int name_byte_length;
  const char *signature;
  const char *document;
  int is_static;
} PicoRubyTiMethod;

static inline TiSourceList
picoruby_ti_single_source_list(
  TiSource *source_item,
  const char *source,
  int source_byte_length
) {
  source_item->source = source;
  source_item->source_byte_length = source_byte_length;

  TiSourceList sources = {
    .items = source_item,
    .count = 1,
  };

  return sources;
}

static inline int
picoruby_ti_fill_diagnostics(
  const char *source,
  int source_byte_length,
  TiDiagnosticList *diagnostics
) {
  TiSource source_item;
  TiSourceList sources =
    picoruby_ti_single_source_list(
      &source_item,
      source,
      source_byte_length
    );

  return ti_fill_diagnostics(&sources, diagnostics);
}

static inline int
picoruby_ti_fill_suggestions_at_cursor(
  const char *source,
  int source_byte_length,
  int cursor_byte_offset,
  TiSuggestionList *suggestions
) {
  TiSource source_item;
  TiSourceList sources =
    picoruby_ti_single_source_list(
      &source_item,
      source,
      source_byte_length
    );

  return ti_fill_suggestions_at_cursor(
    &sources,
    cursor_byte_offset,
    suggestions
  );
}

static inline int
picoruby_ti_find_hover_at_cursor(
  const char *source,
  int source_byte_length,
  int cursor_byte_offset,
  TiHoverInfo *hover_info
) {
  TiSource source_item;
  TiSourceList sources =
    picoruby_ti_single_source_list(
      &source_item,
      source,
      source_byte_length
    );

  return ti_find_hover_at_cursor(
    &sources,
    cursor_byte_offset,
    hover_info
  );
}

static inline int
picoruby_ti_has_class(
  const PicoRubyTiClass *classes,
  int class_count,
  const char *class_name,
  int class_name_byte_length
) {
  for (int index = 0; index < class_count; index++) {
    if (
      classes[index].name_byte_length == class_name_byte_length &&
      memcmp(classes[index].name, class_name, class_name_byte_length) == 0
    ) {
      return 1;
    }
  }

  return 0;
}

static inline int
picoruby_ti_fill_classes(
  const char *source,
  int source_byte_length,
  PicoRubyTiClass *classes,
  int class_capacity
) {
  TiSource source_item;
  TiSourceList sources =
    picoruby_ti_single_source_list(
      &source_item,
      source,
      source_byte_length
    );

  if (!ti_evaluate_sources(&sources, NULL))
    return 0;

  int class_count = 0;

  for (
    uint16_t class_id = 1;
    class_id < ti_builtin_class_count && class_count < class_capacity;
    class_id++
  ) {
    const char *class_name = ti_get_builtin_class_name((uint8_t)class_id);

    classes[class_count].name = class_name;
    classes[class_count].name_byte_length = (int)strlen(class_name);
    class_count++;
  }

  for (
    int index = 0;
    index < ti_get_define_info_count() && class_count < class_capacity;
    index++
  ) {
    TiDefineInfo *define_info = ti_get_define_info(index);

    if (!define_info || !define_info->is_class)
      continue;

    const TiName *name = ti_get_name(define_info->name_id);
    const uint8_t *name_bytes = ti_get_name_bytes(name);

    if (
      !name || !name_bytes ||
      picoruby_ti_has_class(
        classes,
        class_count,
        (const char *)name_bytes,
        name->byte_length
      )
    ) {
      continue;
    }

    classes[class_count].name = (const char *)name_bytes;
    classes[class_count].name_byte_length = name->byte_length;
    class_count++;
  }

  return class_count;
}

static inline const char *
picoruby_ti_make_defined_method_signature(
  const TiDefineInfo *define_info,
  const TiName *method_name
) {
  char *signature = ti_allocate_from_arena(256);

  if (!signature)
    return NULL;

  int signature_byte_length = snprintf(
    signature,
    256,
    "%.*s(",
    method_name->byte_length,
    ti_get_name_bytes(method_name)
  );

  if (signature_byte_length < 0 || signature_byte_length >= 256)
    return NULL;

  for (int index = 0; index < define_info->define_arg_count; index++) {
    const TiName *argument_name =
      ti_get_name(define_info->define_arg_name_ids[index]);

    if (!argument_name)
      continue;

    int written_byte_length = snprintf(
      signature + signature_byte_length,
      256 - signature_byte_length,
      "%s%.*s",
      index == 0 ? "" : ", ",
      argument_name->byte_length,
      ti_get_name_bytes(argument_name)
    );

    if (
      written_byte_length < 0 ||
      written_byte_length >= 256 - signature_byte_length
    ) {
      return NULL;
    }

    signature_byte_length += written_byte_length;
  }

  char return_type[TI_TYPE_STRING_CAPACITY];

  if (
    !ti_type_to_string(
      define_info->return_t_node_index,
      return_type,
      sizeof(return_type)
    )
  ) {
    return NULL;
  }

  int written_byte_length = snprintf(
    signature + signature_byte_length,
    256 - signature_byte_length,
    ") -> %s",
    return_type
  );

  if (
    written_byte_length < 0 ||
    written_byte_length >= 256 - signature_byte_length
  ) {
    return NULL;
  }

  return signature;
}

static inline int
picoruby_ti_fill_methods(
  const char *source,
  int source_byte_length,
  const char *requested_class_name,
  int requested_class_name_byte_length,
  PicoRubyTiMethod *methods,
  int method_capacity,
  int *class_found
) {
  TiSource source_item;
  TiSourceList sources =
    picoruby_ti_single_source_list(
      &source_item,
      source,
      source_byte_length
    );

  *class_found = 0;

  if (!ti_evaluate_sources(&sources, NULL))
    return 0;

  int method_count = 0;
  uint8_t builtin_class_id = ti_get_builtin_class_id(
    (const uint8_t *)requested_class_name,
    requested_class_name_byte_length
  );

  if (builtin_class_id != TI_CLASS_NONE) {
    *class_found = 1;
    const TiBuiltinClass *builtin_class =
      &ti_builtin_classes[builtin_class_id];

    for (
      uint16_t index = 0;
      index < builtin_class->instance_method_count &&
      method_count < method_capacity;
      index++
    ) {
      const TiBuiltinMethod *method =
        &ti_builtin_methods[builtin_class->instance_method_start_index + index];
      const char *method_name = ti_get_builtin_method_name(method);

      methods[method_count].name = method_name;
      methods[method_count].name_byte_length = (int)strlen(method_name);
      methods[method_count].signature = ti_get_builtin_signature(method);
      methods[method_count].document = ti_get_builtin_document(method);
      methods[method_count].is_static = 0;
      method_count++;
    }

    for (
      uint16_t index = 0;
      index < builtin_class->static_method_count &&
      method_count < method_capacity;
      index++
    ) {
      const TiBuiltinMethod *method =
        &ti_builtin_methods[builtin_class->static_method_start_index + index];
      const char *method_name = ti_get_builtin_method_name(method);

      methods[method_count].name = method_name;
      methods[method_count].name_byte_length = (int)strlen(method_name);
      methods[method_count].signature = ti_get_builtin_signature(method);
      methods[method_count].document = ti_get_builtin_document(method);
      methods[method_count].is_static = 1;
      method_count++;
    }
  }

  uint16_t requested_class_name_id = 0;

  for (int index = 0; index < ti_get_define_info_count(); index++) {
    TiDefineInfo *define_info = ti_get_define_info(index);

    if (!define_info || !define_info->is_class)
      continue;

    const TiName *class_name = ti_get_name(define_info->name_id);
    const uint8_t *class_name_bytes = ti_get_name_bytes(class_name);

    if (
      class_name && class_name_bytes &&
      class_name->byte_length == requested_class_name_byte_length &&
      memcmp(
        class_name_bytes,
        requested_class_name,
        requested_class_name_byte_length
      ) == 0
    ) {
      requested_class_name_id = define_info->name_id;
      *class_found = 1;
      break;
    }
  }

  if (requested_class_name_id == 0)
    return method_count;

  for (
    int index = 0;
    index < ti_get_define_info_count() && method_count < method_capacity;
    index++
  ) {
    TiDefineInfo *define_info = ti_get_define_info(index);

    if (
      !define_info || define_info->is_class ||
      define_info->owner_class_name_id != requested_class_name_id
    ) {
      continue;
    }

    const TiName *method_name = ti_get_name(define_info->name_id);

    if (!method_name)
      continue;

    const char *signature =
      picoruby_ti_make_defined_method_signature(define_info, method_name);

    if (!signature)
      continue;

    methods[method_count].name =
      (const char *)ti_get_name_bytes(method_name);
    methods[method_count].name_byte_length = method_name->byte_length;
    methods[method_count].signature = signature;
    methods[method_count].document = "";
    methods[method_count].is_static = 0;
    method_count++;
  }

  return method_count;
}
*/
import "C"

import (
	"strings"
	"sync"
	"unsafe"

	"github.com/owenrumney/go-lsp/lsp"
)

type picorubyTypeInference struct {
	typeInferenceMutex sync.Mutex
}

type picorubyDiagnostic struct {
	startByteOffset int
	endByteOffset   int
	message         string
}

type picorubyClass struct {
	Name string `json:"name"`
}

type picorubyMethod struct {
	Name      string `json:"name"`
	Signature string `json:"signature"`
	Document  string `json:"document"`
	Static    bool   `json:"static"`
}

func (typeInference *picorubyTypeInference) makeDiagnosticsByPicoRuby(
	rubyCode string,
) []picorubyDiagnostic {
	typeInference.typeInferenceMutex.Lock()
	defer typeInference.typeInferenceMutex.Unlock()

	rubyCodePointer := C.CString(rubyCode)
	defer C.free(unsafe.Pointer(rubyCodePointer))

	engineDiagnosticList := C.TiDiagnosticList{}

	diagnosticCount :=
		C.picoruby_ti_fill_diagnostics(
			rubyCodePointer,
			C.int(len(rubyCode)),
			&engineDiagnosticList,
		)

	if diagnosticCount <= 0 {
		return []picorubyDiagnostic{}
	}

	diagnostics :=
		make([]picorubyDiagnostic, int(diagnosticCount))

	for index := 0; index < int(diagnosticCount); index++ {
		engineDiagnostic := &engineDiagnosticList.items[index]

		diagnostics[index] = picorubyDiagnostic{
			startByteOffset: int(engineDiagnostic.start_byte_offset),
			endByteOffset:   int(engineDiagnostic.end_byte_offset),
			message:         C.GoString(engineDiagnostic.message),
		}
	}

	return diagnostics
}

func (typeInference *picorubyTypeInference) makeCompletionItemsByPicoRuby(
	rubyCode string,
	rubyCodeCursorByteOffset int,
) []lsp.CompletionItem {
	typeInference.typeInferenceMutex.Lock()
	defer typeInference.typeInferenceMutex.Unlock()

	rubyCodePointer := C.CString(rubyCode)
	defer C.free(unsafe.Pointer(rubyCodePointer))

	engineCompletionList := C.TiSuggestionList{}

	completionItemCount :=
		C.picoruby_ti_fill_suggestions_at_cursor(
			rubyCodePointer,
			C.int(len(rubyCode)),
			C.int(rubyCodeCursorByteOffset),
			&engineCompletionList,
		)

	if completionItemCount <= 0 {
		return []lsp.CompletionItem{}
	}

	completionItems :=
		make([]lsp.CompletionItem, int(completionItemCount))

	for index := 0; index < int(completionItemCount); index++ {
		engineCompletionItem := &engineCompletionList.items[index]

		completionItem := lsp.CompletionItem{
			Label: C.GoStringN(
				engineCompletionItem.contents,
				C.int(engineCompletionItem.contents_length),
			),
			Detail: C.GoString(engineCompletionItem.detail),
		}

		if engineCompletionItem.document != nil {
			engineDocumentation :=
				C.GoString(engineCompletionItem.document)

			if engineDocumentation != "" {
				completionItem.Documentation = &lsp.MarkupContent{
					Kind:  lsp.Markdown,
					Value: engineDocumentation,
				}
			}
		}

		completionItems[index] = completionItem
	}

	return completionItems
}

func (typeInference *picorubyTypeInference) makeHoverResultByPicoRuby(
	rubyCode string,
	rubyCodeCursorByteOffset int,
) *lsp.Hover {
	typeInference.typeInferenceMutex.Lock()
	defer typeInference.typeInferenceMutex.Unlock()

	rubyCodePointer := C.CString(rubyCode)
	defer C.free(unsafe.Pointer(rubyCodePointer))

	engineHoverResult := C.TiHoverInfo{}

	found :=
		C.picoruby_ti_find_hover_at_cursor(
			rubyCodePointer,
			C.int(len(rubyCode)),
			C.int(rubyCodeCursorByteOffset),
			&engineHoverResult,
		)

	if found == 0 {
		return nil
	}

	var hoverContent string

	if engineHoverResult.is_method != 0 {
		hoverContent = C.GoString(engineHoverResult.method_signature)
		methodDocument := C.GoString(engineHoverResult.method_document)

		if methodDocument != "" {
			hoverContent += "\n\n" + methodDocument
		}
	} else {
		variableName := C.GoString(&engineHoverResult.variable_name[0])
		typeName := C.GoString(&engineHoverResult.type_name[0])
		hoverContent = variableName + ": " +
			strings.ReplaceAll(
				strings.ReplaceAll(typeName, "<", "&lt;"),
				">",
				"&gt;",
			)
	}

	return &lsp.Hover{
		Contents: lsp.MarkupContent{
			Kind:  lsp.Markdown,
			Value: hoverContent,
		},
	}
}

func (typeInference *picorubyTypeInference) makeClassesByPicoRuby(
	rubyCode string,
) []picorubyClass {

	typeInference.typeInferenceMutex.Lock()
	defer typeInference.typeInferenceMutex.Unlock()

	rubyCodePointer := C.CString(rubyCode)
	defer C.free(unsafe.Pointer(rubyCodePointer))

	engineClasses := make([]C.PicoRubyTiClass, 128)

	classCount :=
		C.picoruby_ti_fill_classes(
			rubyCodePointer,
			C.int(len(rubyCode)),
			&engineClasses[0],
			C.int(len(engineClasses)),
		)

	classes := make([]picorubyClass, int(classCount))

	for index := 0; index < int(classCount); index++ {
		classes[index] = picorubyClass{
			Name: C.GoStringN(
				engineClasses[index].name,
				C.int(engineClasses[index].name_byte_length),
			),
		}
	}

	return classes
}

func (typeInference *picorubyTypeInference) makeMethodsByPicoRuby(
	rubyCode string,
	className string,
) ([]picorubyMethod, bool) {

	typeInference.typeInferenceMutex.Lock()
	defer typeInference.typeInferenceMutex.Unlock()

	rubyCodePointer := C.CString(rubyCode)
	defer C.free(unsafe.Pointer(rubyCodePointer))

	classNamePointer := C.CString(className)
	defer C.free(unsafe.Pointer(classNamePointer))

	engineMethods := make([]C.PicoRubyTiMethod, 512)
	classFound := C.int(0)

	methodCount :=
		C.picoruby_ti_fill_methods(
			rubyCodePointer,
			C.int(len(rubyCode)),
			classNamePointer,
			C.int(len(className)),
			&engineMethods[0],
			C.int(len(engineMethods)),
			&classFound,
		)

	methods := make([]picorubyMethod, int(methodCount))

	for index := 0; index < int(methodCount); index++ {
		engineMethod := &engineMethods[index]

		methods[index] =
			picorubyMethod{
				Name: C.GoStringN(
					engineMethod.name,
					C.int(engineMethod.name_byte_length),
				),
				Signature: C.GoString(engineMethod.signature),
				Document:  C.GoString(engineMethod.document),
				Static:    engineMethod.is_static != 0,
			}
	}

	return methods, classFound != 0
}
