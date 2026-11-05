PROJECT_ROOT := ..
TYPE_INFERENCE_ROOT := $(PROJECT_ROOT)
PRISM_ROOT := $(PROJECT_ROOT)/lib/prism
BUILD_DIRECTORY := build

TYPE_INFERENCE_C_DIRECTORIES := \
	base \
	builtin \
	context \
	diagnostic \
	eval \
	eval/method_evaluator \
	generated \
	hover \
	suggest

TYPE_INFERENCE_C_FILES := \
	$(foreach type_inference_directory,$(TYPE_INFERENCE_C_DIRECTORIES),\
		$(wildcard $(TYPE_INFERENCE_ROOT)/src/$(type_inference_directory)/*.c))

TYPE_INFERENCE_OBJECT_FILES := \
	$(patsubst $(TYPE_INFERENCE_ROOT)/src/%.c,\
		$(BUILD_DIRECTORY)/ti/%.o,$(TYPE_INFERENCE_C_FILES))

PRISM_C_FILES := $(wildcard $(PRISM_ROOT)/src/*.c $(PRISM_ROOT)/src/util/*.c)

PRISM_OBJECT_FILES := \
	$(patsubst $(PRISM_ROOT)/src/%.c,\
		$(BUILD_DIRECTORY)/prism/%.o,$(PRISM_C_FILES))

FROM_SOURCE_OBJECT_FILE := $(BUILD_DIRECTORY)/picoruby_ti_from_source.o

DEPENDENCY_FILES := \
	$(TYPE_INFERENCE_OBJECT_FILES:.o=.d) \
	$(PRISM_OBJECT_FILES:.o=.d) \
	$(FROM_SOURCE_OBJECT_FILE:.o=.d)

PRISM_GENERATED_HEADER := $(PRISM_ROOT)/include/prism/diagnostic.h
WASM := picoruby_ti.wasm

EXPORTED_FUNCTIONS := \
	_malloc \
	_free \
	_ti_fill_suggestions_from_source \
	_ti_get_suggestion_contents \
	_ti_get_suggestion_detail \
	_ti_get_suggestion_document \
	_ti_get_suggestion_class_name \
	_ti_find_hover_from_source \
	_ti_get_hover_variable_name \
	_ti_get_hover_type_name \
	_ti_get_hover_method_signature \
	_ti_get_hover_method_document \
	_ti_get_hover_is_method \
	_ti_fill_diagnostics_from_source \
	_ti_get_diagnostic_start_byte_offset \
	_ti_get_diagnostic_end_byte_offset \
	_ti_get_diagnostic_message

empty :=
space := $(empty) $(empty)
comma := ,

INCLUDE_FLAGS := \
	$(foreach type_inference_directory,$(TYPE_INFERENCE_C_DIRECTORIES),\
		-I$(TYPE_INFERENCE_ROOT)/src/$(type_inference_directory)) \
	-I$(TYPE_INFERENCE_ROOT)/include -I$(PRISM_ROOT)/include

CC := emcc
CFLAGS := -O2 -MMD -MP -DPRISM_BUILD_MINIMAL $(INCLUDE_FLAGS)
TYPE_INFERENCE_CFLAGS := -std=c11 -Wall -Wextra -Werror
PRISM_CFLAGS := -std=c99

LDFLAGS := \
	-O2 \
	-sSTANDALONE_WASM \
	--no-entry \
	-sEXPORTED_FUNCTIONS=$(subst $(space),$(comma),$(strip $(EXPORTED_FUNCTIONS)))

.PHONY: build gendb clean

build: gendb
	$(MAKE) $(WASM)

gendb:
	$(MAKE) -C $(PROJECT_ROOT) gendb

$(WASM): \
	$(TYPE_INFERENCE_OBJECT_FILES) \
	$(PRISM_OBJECT_FILES) \
	$(FROM_SOURCE_OBJECT_FILE) \
	Makefile
	$(CC) $(LDFLAGS) \
		$(TYPE_INFERENCE_OBJECT_FILES) \
		$(PRISM_OBJECT_FILES) \
		$(FROM_SOURCE_OBJECT_FILE) \
		-o $@

$(BUILD_DIRECTORY)/ti/%.o: $(TYPE_INFERENCE_ROOT)/src/%.c | $(PRISM_GENERATED_HEADER)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(TYPE_INFERENCE_CFLAGS) -c $< -o $@

$(BUILD_DIRECTORY)/prism/%.o: $(PRISM_ROOT)/src/%.c | $(PRISM_GENERATED_HEADER)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(PRISM_CFLAGS) -c $< -o $@

$(FROM_SOURCE_OBJECT_FILE): picoruby_ti_from_source.c | $(PRISM_GENERATED_HEADER)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(TYPE_INFERENCE_CFLAGS) -c $< -o $@

$(PRISM_GENERATED_HEADER): \
	$(PRISM_ROOT)/templates/template.rb \
	$(PRISM_ROOT)/config.yml
	cd $(PRISM_ROOT) && ruby templates/template.rb

clean:
	$(RM) -r $(BUILD_DIRECTORY) $(WASM)

-include $(DEPENDENCY_FILES)
