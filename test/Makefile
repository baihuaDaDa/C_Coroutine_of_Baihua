LIB_PATH := ../src

.PHONY: test libco

all: libco-test-64 libco-test-32

test: libco all
	@echo "==== TEST 64 bit ===="
	@LD_LIBRARY_PATH=$(LIB_PATH) ./libco-test-64
	@echo "==== TEST 64 bit with valgrind ===="
	@LD_LIBRARY_PATH=$(LIB_PATH) valgrind --leak-check=summary --show-leak-kinds=definite --errors-for-leak-kinds=definite --suppressions=only_leak.supp ./libco-test-64
	@echo "==== TEST 32 bit ===="
	@LD_LIBRARY_PATH=$(LIB_PATH) ./libco-test-32

debug-32: libco all
	@LD_LIBRARY_PATH=$(LIB_PATH) gdb libco-test-32

debug-64: libco all
	@LD_LIBRARY_PATH=$(LIB_PATH) gdb libco-test-64

libco:
	@cd $(LIB_PATH) && make -s

libco-test-64: main.c
	gcc -I$(LIB_PATH) -L$(LIB_PATH) -m64 main.c -o libco-test-64 -lco-64

libco-test-32: main.c
	gcc -I$(LIB_PATH) -L$(LIB_PATH) -m32 main.c -o libco-test-32 -lco-32

clean:
	rm -f libco-test-*
