#ifndef COROUTINE_C_LANG_ITEMS_H
#define COROUTINE_C_LANG_ITEMS_H

#include <stdio.h>
#include <stdlib.h>

/* Debug */
#define panic(msg) panic_handler(__FILE__, __LINE__, msg)

__attribute__((unused))
static void panic_handler(const char *file, int line, const char *msg) {
    fprintf(stderr, "Panic: %s:%d: %s\n", file, line, msg);
    exit(EXIT_FAILURE);
}

#endif //COROUTINE_C_LANG_ITEMS_H
