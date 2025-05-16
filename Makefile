LIB_PATH := src
TEST_PATH := test

.PHONY: lib test clean

all: lib test

lib:
	@$(MAKE) -C $(LIB_PATH)

test: lib
	@$(MAKE) -C $(TEST_PATH)

clean:
	@$(MAKE) -C $(LIB_PATH) clean
	@$(MAKE) -C $(TEST_PATH) clean