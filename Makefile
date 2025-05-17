LIB_PATH := src
TEST_PATH := test

.PHONY: lib test clean

all: lib test

lib:
	@$(MAKE) -C $(LIB_PATH)

test:
	@$(MAKE) -C $(TEST_PATH) test

debug-32:
	@$(MAKE) -C $(TEST_PATH) debug-32

debug-64:
	@$(MAKE) -C $(TEST_PATH) debug-64

clean:
	@$(MAKE) -C $(LIB_PATH) clean
	@$(MAKE) -C $(TEST_PATH) clean