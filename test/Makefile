LIB_PATH := ../src
TESTS := random_load

CFLAGS := -I$(LIB_PATH)
LDFLAGS_64 := -L$(LIB_PATH) -lco-64
LDFLAGS_32 := -L$(LIB_PATH) -lco-32

.PHONY: all test clean debug-32 debug-64 libco

all: $(addprefix bin/,$(addsuffix -64,$(TESTS))) $(addprefix bin/,$(addsuffix -32,$(TESTS)))

bin:
	mkdir -p bin

bin/%-64: %.c | bin libco
	gcc $(CFLAGS) -m64 $< -o $@ $(LDFLAGS_64)

bin/%-32: %.c | bin libco
	gcc $(CFLAGS) -m32 $< -o $@ $(LDFLAGS_32)

libco:
	@cd $(LIB_PATH) && make -s

test: all
	@for t in $(TESTS); do \
		echo "==== TEST $$t 64-bit ===="; \
		LD_LIBRARY_PATH=$(LIB_PATH) ./bin/$$t-64; \
		echo "==== VALGRIND $$t 64-bit ===="; \
		LD_LIBRARY_PATH=$(LIB_PATH) valgrind --leak-check=summary --show-leak-kinds=definite --errors-for-leak-kinds=definite --suppressions=only_leak.supp ./bin/$$t-64; \
		echo "==== TEST $$t 32-bit ===="; \
		LD_LIBRARY_PATH=$(LIB_PATH) ./bin/$$t-32; \
	done

debug-64: libco all
	@echo "Choose binary to debug (one of: $(TESTS))"
	@read BIN && LD_LIBRARY_PATH=$(LIB_PATH) gdb bin/$$BIN-64

debug-32: libco all
	@echo "Choose binary to debug (one of: $(TESTS))"
	@read BIN && LD_LIBRARY_PATH=$(LIB_PATH) gdb bin/$$BIN-32

clean:
	rm -rf bin
