// test_matrix_transpose.c
#include <stdio.h>
#include <stdlib.h>
#include <co.h>

#define N 16  // 协程数量
#define SIZE 512  // 矩阵规模

int A[SIZE][SIZE];
int B[SIZE][SIZE];

struct task_arg {
    int row_start;
    int row_end;
};

struct co *cos[N];

void transpose_worker(void *arg) {
    struct task_arg *targ = (struct task_arg *)arg;
    for (int i = targ->row_start; i < targ->row_end; i++) {
        for (int j = 0; j < SIZE; j++) {
            B[j][i] = A[i][j];
            if ((i * SIZE + j) % 50000 == 0) co_yield();
        }
    }
}

int main() {
    co_init();

    // 初始化矩阵
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            A[i][j] = i * SIZE + j;

    struct task_arg *args[N];
    int rows_per_co = SIZE / N;

    for (int i = 0; i < N; i++) {
        args[i] = malloc(sizeof(struct task_arg));
        args[i]->row_start = i * rows_per_co;
        args[i]->row_end = (i + 1) * rows_per_co;
        cos[i] = co_start("transpose", transpose_worker, args[i]);
    }

    for (int i = 0; i < N; i++) {
        co_wait(cos[i]);
        free(args[i]);
    }

    printf("Matrix transpose done.\n");

    // 可选校验
    int passed = 1;
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            if (B[j][i] != A[i][j])
                passed = 0;

    printf("Transpose %s\n", passed ? "PASSED" : "FAILED");
    return 0;
}
