ROOT := $(CURDIR)
TI_GENERATED := $(ROOT)/src/generated

.PHONY: gendb gendb-test

gendb:
	ruby ./tidbgen/main.rb \
	  --sig-dir ./sig \
	  --out $(TI_GENERATED)

gendb-test: gendb
	ruby ./tidbgen/test/tidbgen_test.rb
	$(MAKE) -C ./host_test test
