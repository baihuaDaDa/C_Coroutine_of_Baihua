#include "co.h"
#include "list.h"
#include "lang_items.h"

#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

static inline void
stack_switch_call(void *sp, void *entry, uintptr_t arg) {
    asm volatile (
#if __x86_64__
            "movq %0, %%rsp; movq %2, %%rdi; jmp *%1"
            :
            : "b"((uintptr_t) sp - 8),
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

/* config */
#define CO_STACK_SIZE (1024 * 16) // 16KB
#define CO_RUNTIME_STACK_SIZE (1024 * 4) // 4KB
#define RUN_QUEUE_SIZE 256
#define M_NUM 16

/* Coroutine */
enum co_status {
    CO_NEW = 1,
    CO_RUNNING,
    CO_WAITING,
    CO_DEAD,
};

typedef jmp_buf co_context;

struct loop_queue {
    struct g *inner[RUN_QUEUE_SIZE];
    uint64_t head;
    uint64_t tail;
};

struct mutex_queue {
    pthread_mutex_t mutex;
    struct list queue;
};

struct co {
    struct g *g;
    char *name;
    void (*func)(void *);
    void *arg;
    pthread_mutex_t status_mutex;
    enum co_status status;
    struct list waiters;
    co_context context;
    uint8_t *stack;
};

/* G-M-P model */
struct g {
    struct m *m;
    struct co *co;
};

struct m {
    struct g *g0;
    struct p *p;
    pthread_t thread_id;
};

struct p {
    struct co *to_be_waited;
    struct loop_queue all_queue;
    struct loop_queue running_queue;
    struct loop_queue dead_queue;
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
        printf("%s ", ((struct co *) node->data)->name);
        node = node->next;
    }
    printf("\n");
}

/* Runtime support */
static struct m m_set[M_NUM];
static struct p p_set[M_NUM]; // loop_queue and m are one-to-one relationship
static struct mutex_queue global_queue;
static pthread_key_t tls_key_g_current;
static struct co *co_main = NULL;
static sem_t co_main_sem;
static int exit_signal = 0;
/* ----------------------------- */

static void g_destroy(struct g *g);
static struct g *g_get_current();
static struct m *m_get_current();
static void *m_run_coroutine(void *ptr);
static void p_init(struct p *p);
static void p_destroy(struct p *p);
static void mq_init(struct mutex_queue *mq);
static void mq_destroy(struct mutex_queue *mq);
static struct list *mq_get(struct mutex_queue *mq);
static void mq_free(struct mutex_queue *mq);
static int queue_push(struct loop_queue *q, struct g *g);
static struct g *queue_pop(struct loop_queue *q);
static int queue_is_empty(struct loop_queue *q);
static int queue_erase(struct loop_queue *q, struct g *g);

static struct co *co_new(const char *name, void (*func)(void *), void *arg, struct g *g);
static void co_wrapper(struct co *co);
static void co_free(struct co *co);

static int queue_push(struct loop_queue *q, struct g *g) {
    uint64_t new_tail = (q->tail + 1) % RUN_QUEUE_SIZE;
    if (new_tail == q->head) return 0;
    q->inner[q->tail] = g;
    q->tail = new_tail;
    return 1;
}

static struct g *queue_pop(struct loop_queue *q) {
    if (q->head == q->tail) return NULL; // empty queue
    struct g *g = q->inner[q->head++];
    q->head %= RUN_QUEUE_SIZE;
    return g;
}

static int queue_is_empty(struct loop_queue *q) {
    return q->head == q->tail;
}

static int queue_erase(struct loop_queue *q, struct g *g) {
    for (uint64_t i = q->head; i != q->tail; i = (i + 1) % RUN_QUEUE_SIZE) {
        if (q->inner[i] == g) {
            for (uint64_t j = i; j != q->tail;) {
                uint64_t j_next = (j + 1) % RUN_QUEUE_SIZE;
                q->inner[j] = q->inner[j_next];
                j = j_next;
            }
            return 1;
        }
    }
    return 0;
}

static void g_destroy(struct g *g) {
    co_free(g->co);
    free(g);
}

static void p_init(struct p *p) {
    p->all_queue.head = 0;
    p->all_queue.tail = 0;
    p->running_queue.head = 0;
    p->running_queue.tail = 0;
    p->dead_queue.head = 0;
    p->dead_queue.tail = 0;
}

static void p_destroy(struct p *p) {
    struct loop_queue *q = &p->all_queue;
    for (uint64_t i = q->head; i != q->tail; i = (i + 1) % RUN_QUEUE_SIZE) {
        g_destroy(q->inner[i]);
    }
}

static void mq_init(struct mutex_queue *mq) {
    list_init(&mq->queue);
    mq->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    int result = pthread_mutex_init(&mq->mutex, NULL);
    if (result != 0) {
        panic("init global queue mutex failed");
    }
}

static void mq_destroy(struct mutex_queue *mq) {
    list_destroy(&mq->queue);
    pthread_mutex_destroy(&mq->mutex);
}

static struct list *mq_get(struct mutex_queue *mq) {
    pthread_mutex_lock(&mq->mutex);
    return &mq->queue;
}

static void mq_free(struct mutex_queue *mq) {
    pthread_mutex_unlock(&mq->mutex);
}

static struct g *g_get_current() {
    struct g **tls_data_g_current = (struct g **)pthread_getspecific(tls_key_g_current);
    return *tls_data_g_current;
}

static struct m *m_get_current() {
    struct g **tls_data_g_current = (struct g **)pthread_getspecific(tls_key_g_current);
    struct g *g_current = *tls_data_g_current;
    return g_current->m;
}

static void *m_run_coroutine(void *ptr) {
    struct g *g0 = (struct g *) ptr;
    // init TLS data
    struct g **tls_data_g_current = malloc(sizeof(struct g *));
    pthread_setspecific(tls_key_g_current, tls_data_g_current);
    *tls_data_g_current = g0;
    // current m, loop_queue
    struct m *m_current = g0->m;
    struct p *p_current = m_current->p;
    // schedule
    int val = 0;
    while (!atomic_load_explicit(&exit_signal, memory_order_acquire)) {
        if (val == 0) { // run next
            struct g *g_next = NULL;
            if (!queue_is_empty(&p_current->running_queue)) {
                g_next = queue_pop(&p_current->running_queue);
            } else {
                struct list *gq_inner = mq_get(&global_queue);
                if (!list_is_empty(gq_inner)) {
                    g_next = (struct g *) list_pop_front(gq_inner);
                }
                mq_free(&global_queue);
            }
            if (g_next) {
                g_next->m = m_current; // set current m
                *tls_data_g_current = g_next;
                struct co *co_current = g_next->co;
                if (co_current->status == CO_NEW) {
                    stack_switch_call(co_current->stack + CO_STACK_SIZE, co_wrapper, (uintptr_t) co_current);
                } else if (co_current->status == CO_RUNNING) {
                    longjmp(co_current->context, 1);
                } else {
//                    printf("coroutine status: %d\n", co_current->status);
                    panic("invalid coroutine status");
                }
                val = setjmp(g0->co->context);
            }
        } else if (val == 1) { // suspend
            struct g *g_current = *tls_data_g_current;
            if (!queue_push(&p_current->running_queue, g_current)) {
                struct list *gq_inner = mq_get(&global_queue);
                list_push_back(gq_inner, g_current);
                g_current->m = NULL;
                mq_free(&global_queue);
            }
            *tls_data_g_current = g0;
        } else if (val == 2) { // exit
            struct g *g_current = *tls_data_g_current;
            struct co *co = g_current->co;
            // erase from running list and free stack
            if (!queue_erase(&p_current->running_queue, g_current)) {
                struct list *gq_inner = mq_get(&global_queue);
                if (!list_erase(gq_inner, g_current)) {
                    mq_free(&global_queue);
                    panic("current coroutine not running while exiting");
                    return NULL;
                }
                mq_free(&global_queue);
            }
            queue_push(&p_current->dead_queue, g_current);
            free(co->stack);
            co->stack = NULL;
            // set status to CO_DEAD and wake up all waiters
            pthread_mutex_lock(&co->status_mutex);
            co->status = CO_DEAD;
            struct list *waiters = &co->waiters;
            while (!list_is_empty(waiters)) {
                struct co *waiter = (struct co *) list_pop_front(waiters);
                if (waiter == co_main) {
                    sem_post(&co_main_sem); // wake up main coroutine
                    pthread_mutex_unlock(&co->status_mutex);
                    continue;
                }
                pthread_mutex_lock(&waiter->status_mutex);
                if (waiter->status == CO_WAITING) {
                    waiter->status = CO_RUNNING;
                    struct list *gq_inner = mq_get(&global_queue);
                    list_push_back(gq_inner, waiter->g);
                    mq_free(&global_queue);
                } else {
                    pthread_mutex_unlock(&waiter->status_mutex);
                    panic("waiter status is not CO_WAITING");
                }
                pthread_mutex_unlock(&waiter->status_mutex);
            }
            pthread_mutex_unlock(&co->status_mutex);
        } else { // wait
            struct g *g_current = *tls_data_g_current;
            struct co *co_current = g_current->co, *to_be_waited = p_current->to_be_waited;
            // add to waiters
            pthread_mutex_lock(&to_be_waited->status_mutex);
            if (to_be_waited->status == CO_DEAD) {
                pthread_mutex_unlock(&to_be_waited->status_mutex);
                return NULL;
            }
            list_push_back(&to_be_waited->waiters, co_current);
            g_current->m = NULL;
            if (!queue_erase(&p_current->running_queue, co_current->g)) {
                panic("waiters not in running list before waiting");
                return NULL;
            }
            // set co_current's status to CO_WAITING
            // do not free to_be_waited mutex here
            pthread_mutex_lock(&co_current->status_mutex);
            co_current->status = CO_WAITING;
            pthread_mutex_unlock(&co_current->status_mutex);
            pthread_mutex_unlock(&to_be_waited->status_mutex);
        }
    }
    return NULL;
}

static void co_wrapper(struct co *co) {
    co->status = CO_RUNNING;
    co->func(co->arg);
//    stack_switch_call(co_runtime_stack + CO_RUNTIME_STACK_SIZE, co_exit, (uintptr_t) co);
    longjmp(m_get_current()->g0->co->context, 2); // exit coroutine
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
    list_destroy(&co->waiters);
    free(co);
}

struct co *co_new(const char *name, void (*func)(void *), void *arg, struct g *g) {
    if (!name) {
        panic("name or func is NULL");
        return NULL;
    }
    struct co *co = (struct co *) malloc(sizeof(struct co));
    if (!co) {
        panic("malloc struct_co failed");
        return NULL;
    }
    co->g = g;
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
    co->status_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    int result = pthread_mutex_init(&co->status_mutex, NULL);
    if (result != 0) {
        panic("init global queue mutex failed");
        return NULL;
    }
    co->status = CO_NEW;
    list_init(&co->waiters);
    return co;
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
    struct m *m_current = m_get_current();
    struct g *g = malloc(sizeof(struct g));
    struct co * co = co_new(name, func, arg, g);
    g->co = co;
    if (m_current == &m_set[0]) { // main thread
        struct list *gq_inner = mq_get(&global_queue);
        list_push_back(gq_inner, g);
        mq_free(&global_queue);
    } else { // other thread
        g->m = m_current;
        queue_push(&m_current->p->running_queue, g);
    }
    return co;
}

void co_yield() {
    struct g *g_current = g_get_current();
    if (g_current->co == co_main) return; // do not yield in main coroutine
    int val = setjmp(g_current->co->context);
    if (val == 0) { // suspend
        longjmp(g_current->m->g0->co->context, 1); // jump to scheduler
    } else { // resume
        return;
    }
}

void co_wait(struct co *co) {
    if (!co || co == co_main) {
        panic("co is NULL or main coroutine");
        return;
    }
    struct g *g_current = g_get_current();
    struct m *m_current = g_current->m;
    if (g_current->co == co_main) { // main coroutine waits others
        pthread_mutex_lock(&co->status_mutex);
        if (co->status == CO_DEAD) {
            pthread_mutex_unlock(&co->status_mutex);
            return;
        }
        list_push_back(&co->waiters, co_main);
        pthread_mutex_unlock(&co->status_mutex);
        sem_wait(&co_main_sem);
        return;
    }
    m_current->p->to_be_waited = co;
    int val = setjmp(g_current->co->context);
    if (val == 0) {
        longjmp(m_current->g0->co->context, 3); // jump to scheduler
    } else { // resume
        return;
    }
}

static void tls_destructor(void *ptr) {
    free(ptr);
}

void co_init() {
    // init TLS key
    pthread_key_create(&tls_key_g_current, tls_destructor);
    // init global queue
    mq_init(&global_queue);
    // init semaphore of main
    sem_init(&co_main_sem, 0, 0);
    // main coroutine occupies main thread
    for (int i = 0; i < M_NUM; i++) {
        p_init(&p_set[i]);
        m_set[i].g0 = malloc(sizeof(struct g));
        m_set[i].p = &p_set[i];
        m_set[i].g0->m = &m_set[i];
    }
    m_set[0].thread_id = pthread_self();
    co_main = co_new("co_main", NULL, NULL, m_set[0].g0);
    m_set[0].g0->co = co_main;
    // init TLS data of main
    struct g **tls_data_g_current = malloc(sizeof(struct g *));
    pthread_setspecific(tls_key_g_current, tls_data_g_current);
    *tls_data_g_current = co_main->g;
    // other coroutines
    for (int i = 1; i < M_NUM; i++) {
        m_set[i].g0->co = co_new("co_run_coroutine", NULL, NULL, m_set[i].g0);
        pthread_create(&m_set[i].thread_id, NULL, m_run_coroutine, m_set[i].g0);
    }
}

__attribute__((destructor))
static void co_destroy() {
    atomic_store_explicit(&exit_signal, 1, memory_order_release);
    for (int i = 0; i < M_NUM; i++) {
        pthread_join(m_set[i].thread_id, NULL);
        g_destroy(m_set[i].g0);
        p_destroy(&p_set[i]);
    }
    // destroy semaphore of main
    sem_destroy(&co_main_sem);
    // destroy global queue
    mq_destroy(&global_queue);
    // destroy TLS key
    pthread_key_delete(tls_key_g_current);
}