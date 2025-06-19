#include <stdio.h>
#include <co.h>

// 全局计数器
static int counter = 0;

// 信号量用于保护计数器
static struct co_sem *sem;

// 协程函数
void counter_task(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < 10; i++) {
        // 获取信号量（进入临界区）
        co_sem_wait(sem);

        // 打印并修改共享计数器
        printf("协程 %d: counter = %d\n", id, counter);
        counter++;

        // 释放信号量（退出临界区）
        co_sem_post(sem);

        // 让出CPU给其他协程
        co_yield();
    }

    printf("协程 %d 完成\n", id);
}

int main() {
    co_init();
    // 创建互斥信号量（初始值为1）
    sem = co_sem_create(1);

    int id1 = 1, id2 = 2;

    // 创建两个协程
    struct co *co1 = co_start("counter-1", counter_task, &id1);
    struct co *co2 = co_start("counter-2", counter_task, &id2);

    // 等待两个协程完成
    co_wait(co1);
    co_wait(co2);

    // 销毁信号量
    co_sem_destroy(sem);

    // 打印最终结果
    printf("最终计数: %d\n", counter);

    return 0;
}