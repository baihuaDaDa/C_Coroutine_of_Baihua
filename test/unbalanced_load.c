// test_unbalanced_load.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <co.h>

#define N 1000

struct task_arg {
    int id;
    int workload;  // 每个协程的工作量
};

struct co *cos[N];

void worker(void *arg) {
    struct task_arg *targ = (struct task_arg *)arg;
    volatile long dummy = 0;
    for (int i = 0; i < targ->workload; i++) {
        dummy += i % 7;
        if (i % 10000 == 0) co_yield();
    }
    printf("Coroutine %d finished, workload %d\n", targ->id, targ->workload);
}

int main() {
    co_init();
    struct task_arg *args[N];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < N; i++) {
        args[i] = malloc(sizeof(struct task_arg));
        args[i]->id = i;
        args[i]->workload = (i + 1) * 100000;  // 越后面越重
        cos[i] = co_start("unbalanced", worker, args[i]);
    }

    for (int i = 0; i < N; i++) {
        co_wait(cos[i]);
        free(args[i]);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("All unbalanced coroutines completed.\n");
    printf("Time: %.6f s\n", sec);
    return 0;
}
