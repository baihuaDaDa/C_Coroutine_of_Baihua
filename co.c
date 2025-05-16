#include "co.h"

#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Debug */
#define panic(msg) panic_handler(__FILE__, __LINE__, msg)

static void panic_handler(const char *file, int line, const char *msg) {
    fprintf(stderr, "Panic: %s:%d: %s\n", file, line, msg);
    exit(EXIT_FAILURE);
}

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

/* List */
struct co_node {
    struct co *co;
    struct co_node *next;
    struct co_node *prev;
};

struct co_list {
    struct co_node *head;
    struct co_node *tail;
};

static void co_list_init(struct co_list *list) {
    list->head = (struct co_node *) malloc(sizeof(struct co_node));
    list->tail = (struct co_node *) malloc(sizeof(struct co_node));
    if (!list->head || !list->tail) {
        panic("malloc struct_co_node failed");
        return;
    }
    list->head->next = list->tail;
    list->head->prev = NULL;
    list->tail->next = NULL;
    list->tail->prev = list->head;
}

static int co_list_inited(struct co_list *list) {
    if (!list || !list->head || !list->tail) {
        return 0;
    }
    return 1;
}

static void co_list_destroy(struct co_list *list) {
    if (!list) {
        panic("list is NULL");
        return;
    }
    struct co_node *node = list->head;
    while (node) {
        struct co_node *next = node->next;
        free(node);
        node = next;
    }
}

static void co_list_push_back(struct co_list *list, struct co *co) {
    if (!co_list_inited(list)) {
        panic("list has not been initialized");
        return;
    }
    struct co_node *node = (struct co_node *) malloc(sizeof(struct co_node));
    if (!node) {
        panic("malloc struct_co_node failed");
        return;
    }
    node->co = co;
    node->next = list->tail;
    node->prev = list->tail->prev;
    list->tail->prev->next = node;
}

static struct co* co_list_pop_front(struct co_list *list) {
    if (!co_list_inited(list)) {
        panic("list has not been initialized");
        return NULL;
    }
    struct co_node *node = list->head->next;
    if (node == list->tail) {
        return NULL;
    }
    list->head->next = node->next;
    node->next->prev = list->head;
    struct co *co = node->co;
    free(node);
    return co;
}

static int co_list_erase(struct co_list *list, struct co *co) {
    if (!co_list_inited(list)) {
        panic("list has not been initialized");
        return 0;
    }
    struct co_node *node = list->head->next;
    while (1)  {
        if (node->co == NULL) return 0;
        if (node->co == co) {
            free(node);
            return 1;
        }
        node = node->next;
    }
}

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

static struct co *co_current = NULL;
static struct co_list co_running_list;
static struct co_list co_waiting_list;

static struct co *co_main = NULL;
static uint8_t co_runtime_stack[CO_RUNTIME_STACK_SIZE];

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

static void co_schedule() {
    co_current = co_list_pop_front(&co_running_list);
    if (!co_current) {
        panic("no coroutine to schedule");
        return;
    }
    if (co_current->status == CO_NEW) {
        stack_switch_call(co_current->stack + CO_STACK_SIZE, co_wrapper, (uintptr_t) co_current);
    } else if (co_current->status == CO_RUNNING) {
        longjmp(co_current->context, 1);
    } else {
        panic("invalid coroutine status");
    }
    panic("never reach here");
}

static void co_exit(struct co *co) {
    co->status = CO_DEAD;
    struct co *waiter = co->waiter;
    if (waiter) {
        waiter->status = CO_RUNNING;
        if (!co_list_erase(&co_waiting_list, waiter)) {
            panic("waiter not in waiting list while waiting");
            return;
        }
        co_list_push_back(&co_running_list, waiter);
    }
    free(co->stack);
    co_schedule();
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
    if (!name || !func) {
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
        panic("malloc co->name failed");
        return NULL;
    }
    strcpy(co->name, name);
    co->stack = (uint8_t *) malloc(CO_STACK_SIZE);
    if (!co->stack) {
        panic("malloc co->stack failed");
        return NULL;
    }
    co->func = func;
    co->arg = arg;
    co->status = CO_NEW;
    co->waiter = NULL;
    co_list_push_back(&co_running_list, co);
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
        panic("co is NULL");
        return;
    }
    if (co->status == CO_DEAD) {
        co_free(co); // free the resources left of coroutine
        return;
    }
    co->waiter = co_current;
    co_current->status = CO_WAITING;
    if (!co_list_erase(&co_running_list, co_current)) {
        panic("waiter not in running list before waiting");
        return;
    }
    co_list_push_back(&co_waiting_list, co_current);
    co_yield();
    co_free(co); // free the resources left of coroutine
}

__attribute__((constructor))
static void co_init() {
    co_list_init(&co_running_list);
    co_list_init(&co_waiting_list);
    co_main = co_start("main", NULL, NULL);
    co_current = co_main;
}

__attribute__((destructor))
static void co_destroy() {
    co_list_destroy(&co_running_list);
    co_list_destroy(&co_waiting_list);
    co_free(co_main);
}