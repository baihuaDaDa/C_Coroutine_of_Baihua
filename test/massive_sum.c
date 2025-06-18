// test_massive_sum.c
#include <stdio.h>
#include <stdlib.h>
#include <co.h>

#define N 10000  // 协程数量大幅提升
#define RANGE 10  // 每协程计算小段区间，强调调度和并发

struct task_arg {
    int id;
    long start, end;
    long long result;
};

struct co *cos[N];

void worker(void *arg) {
    struct task_arg *targ = (struct task_arg *)arg;
    long long sum = 0;
    for (long i = targ->start; i <= targ->end; i++) {
        sum += (long long)i * i;
        if (i % 10 == 0) co_yield();
    }
    targ->result = sum;
}

int main() {
    co_init();

    struct task_arg *args[N];
    for (int i = 0; i < N; i++) {
        args[i] = malloc(sizeof(struct task_arg));
        args[i]->id = i;
        args[i]->start = i * RANGE + 1;
        args[i]->end = (i + 1) * RANGE;
        args[i]->result = 0;

        cos[i] = co_start("massive", worker, args[i]);
    }

    long long total = 0;
    for (int i = 0; i < N; i++) {
        co_wait(cos[i]);
        total += args[i]->result;
        free(args[i]);
    }

    printf("Total sum = %lld\n", total);
    return 0;
}
