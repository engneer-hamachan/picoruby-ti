ROOT := $(CURDIR)
TI_GENERATED := $(ROOT)/src/generated
WASM_DOCKER_IMAGE := picoruby-ti-wasm

.PHONY: gendb gendb-test wasm

gendb:
	ruby ./tidbgen/main.rb \
	  --sig-dir ./sig \
	  --out $(TI_GENERATED)

gendb-test: gendb
	ruby ./tidbgen/test/tidbgen_test.rb
	$(MAKE) -C ./host_test test

wasm:
	docker build -t $(WASM_DOCKER_IMAGE) ./wasm;
	docker run --rm \
	  -v $(ROOT):/work \
	  -u $$(id -u):$$(id -g) \
	  $(WASM_DOCKER_IMAGE)
