#include "co.h"
#include "list.h"
#include "lang_items.h"

#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline void
stack_switch_call(void *sp, void *entry, uintptr_t arg) {
    asm volatile (
#if __x86_64__
        "movq %0, %%rsp; movq %2, %%rdi; jmp *%1"
            :
            : "b"((uintptr_t)sp - 8),
              "d"(entry),
              "a"(arg)
            : "memory"
#else
        "movl %0, %%esp; movl %2, 4(%0); jmp *%1"
          :
          : "b"((uintptr_t)sp - 8),
            "d"(entry),
            "a"(arg)
          : "memory"
#endif
    );
}

/* Coroutine */
enum co_status {
    CO_NEW = 1,
    CO_RUNNING,
    CO_WAITING,
    CO_DEAD,
};

typedef jmp_buf co_context;

#define CO_STACK_SIZE (1024 * 16) // 16KB
#define CO_RUNTIME_STACK_SIZE (1024 * 4) // 4KB

struct co {
    char *name;
    void (*func)(void *);
    void *arg;

    enum co_status status;
    struct co *waiter;
    co_context context;
    uint8_t *stack;
};

/* Debug */
__attribute__((unused))
void print_co_list(struct list *list) {
    if (!list_inited(list)) {
        panic("list has not been initialized");
        return;
    }
    struct node *node = list->head->next;
    while (node != list->tail) {
        printf("%s ", ((struct co *)node->data)->name);
        node = node->next;
    }
    printf("\n");
}

/* Runtime support */
static void co_wrapper(struct co *co);

static struct co *co_current = NULL;
static struct list co_running_list;
static struct list co_waiting_list;

static struct co *co_main = NULL;
static uint8_t co_runtime_stack[CO_RUNTIME_STACK_SIZE];

static void co_schedule() {
    co_current = list_pop_front(&co_running_list);
    list_push_back(&co_running_list, co_current);
    if (!co_current) {
        panic("no coroutine to schedule");
        return;
    }
    if (co_current->status == CO_NEW) {
        stack_switch_call(co_current->stack + CO_STACK_SIZE, co_wrapper, (uintptr_t) co_current);
    } else if (co_current->status == CO_RUNNING) {
        longjmp(co_current->context, 1);
    } else {
//        printf("coroutine status: %d\n", co_current->status);
        panic("invalid coroutine status");
    }
    panic("never reach here");
}

static void co_exit(struct co *co) {
    co->status = CO_DEAD;
    if (!list_erase(&co_running_list, co)) {
        panic("coroutine not in running list while exiting");
        return;
    }
    struct co *waiter = co->waiter;
    if (waiter) {
        waiter->status = CO_RUNNING;
        if (!list_erase(&co_waiting_list, waiter)) {
            panic("waiter not in waiting list while waiting");
            return;
        }
        list_push_back(&co_running_list, waiter);
    }
    free(co->stack);
    co->stack = NULL;
    co_schedule();
}

static void co_wrapper(struct co *co) {
    co->status = CO_RUNNING;
    co->func(co->arg);
    stack_switch_call(co_runtime_stack + CO_RUNTIME_STACK_SIZE, co_exit, (uintptr_t) co);
}

static void co_free(struct co *co) {
    if (co->stack) {
        free(co->stack);
        co->stack = NULL;
    }
    if (co->name) {
        free(co->name);
        co->name = NULL;
    }
    free(co);
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
    if (!name) {
        panic("name or func is NULL");
        return NULL;
    }
    struct co *co = (struct co *) malloc(sizeof(struct co));
    if (!co) {
        panic("malloc struct_co failed");
        return NULL;
    }
    co->name = (char *) malloc(strlen(name) + 1);
    if (!co->name) {
        panic("malloc data->name failed");
        return NULL;
    }
    strcpy(co->name, name);
    co->stack = (uint8_t *) malloc(CO_STACK_SIZE);
    if (!co->stack) {
        panic("malloc data->stack failed");
        return NULL;
    }
    co->func = func;
    co->arg = arg;
    co->status = CO_NEW;
    co->waiter = NULL;
    list_push_back(&co_running_list, co);
    return co;
}

void co_yield() {
    int val = setjmp(co_current->context);
    if (val == 0) { // suspend
        co_schedule();
    } else { // resume
        return;
    }
}

void co_wait(struct co *co) {
    if (!co) {
        panic("data is NULL");
        return;
    }
    if (co->status == CO_DEAD) {
        co_free(co); // free the resources left of coroutine
        return;
    }
    co->waiter = co_current;
    co_current->status = CO_WAITING;
    if (!list_erase(&co_running_list, co_current)) {
        panic("waiter not in running list before waiting");
        return;
    }
    list_push_back(&co_waiting_list, co_current);
//    print_co_list(&co_running_list);
//    print_co_list(&co_waiting_list);
    co_yield();
    co_free(co); // free the resources left of coroutine
}

__attribute__((constructor))
static void co_init() {
    list_init(&co_running_list);
    list_init(&co_waiting_list);
    co_main = co_start("main", NULL, NULL);
    co_current = co_main;
}

__attribute__((destructor))
static void co_destroy() {
    list_destroy(&co_running_list);
    list_destroy(&co_waiting_list);
    co_free(co_main);
}