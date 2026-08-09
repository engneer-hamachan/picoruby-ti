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
		$(BUILD_DIRECTORY)/%.o,$(TYPE_INFERENCE_C_FILES))

DEPENDENCY_FILES := $(TYPE_INFERENCE_OBJECT_FILES:.o=.d)
TYPE_INFERENCE_LIBRARY := $(BUILD_DIRECTORY)/libpicoruby_ti.a
PRISM_LIBRARY := $(PRISM_ROOT)/build/libprism.a
PRISM_GENERATED_HEADER := $(PRISM_ROOT)/include/prism/diagnostic.h
LANGUAGE_SERVER := picoruby-ti-lsp
LANGUAGE_SERVER_GO_FILES := $(wildcard *.go)

INCLUDE_FLAGS := \
	$(foreach type_inference_directory,$(TYPE_INFERENCE_C_DIRECTORIES),\
		-I$(TYPE_INFERENCE_ROOT)/src/$(type_inference_directory)) \
	-I$(TYPE_INFERENCE_ROOT)/include -I$(PRISM_ROOT)/include

CFLAGS := -std=c11 -Wall -Wextra -Werror -MMD -MP $(INCLUDE_FLAGS)

.PHONY: build test

build: $(LANGUAGE_SERVER)

$(LANGUAGE_SERVER): \
	$(LANGUAGE_SERVER_GO_FILES) \
	$(TYPE_INFERENCE_LIBRARY) \
	$(PRISM_LIBRARY) \
	Makefile
	go build -a -o $@ .

$(TYPE_INFERENCE_LIBRARY): $(TYPE_INFERENCE_OBJECT_FILES)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(BUILD_DIRECTORY)/%.o: $(TYPE_INFERENCE_ROOT)/src/%.c | $(PRISM_LIBRARY)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(PRISM_GENERATED_HEADER): \
	$(PRISM_ROOT)/templates/template.rb \
	$(PRISM_ROOT)/config.yml
	cd $(PRISM_ROOT) && ruby templates/template.rb

$(PRISM_LIBRARY): $(PRISM_GENERATED_HEADER)
	$(MAKE) -C $(PRISM_ROOT) static

test: \
	$(TYPE_INFERENCE_LIBRARY) \
	$(PRISM_LIBRARY)
	go test ./...

-include $(DEPENDENCY_FILES)
