#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <co.h>

// 可配置参数
#define DEFAULT_NUM_COROUTINES 10000  // 已确定为10000个协程
#define DEFAULT_MIN_WORK 1000          // 最小工作量
#define DEFAULT_MAX_WORK 500000        // 最大工作量
#define BATCH_COUNT 20                // 增加批次数以平滑进度报告
#define NUMBERS_PER_ITERATION 5       // 保持不变
#define YIELD_FREQUENCY 10            // 增大以减少上下文切换频率
#define TASK_ID_MULTIPLIER 10         // 保持不变

typedef struct {
    int id;
    int work_amount;     // 实际工作量
    int completed;       // 完成标志
    long long expected;  // 期望结果
    long long result;    // 实际结果
} Task;

// 可验证的计算函数 - 计算数字序列的立方和
long long compute_cube_sum(int start, int count) {
    long long sum = 0;
    for (int i = 0; i < count; i++) {
        int value = start + i;
        sum += (long long) value * value * value;
    }
    return sum;
}

void stress_task(void *arg) {
    Task *task = (Task *) arg;

    // 执行实际工作 - 计算立方和
    int start_num = task->id * TASK_ID_MULTIPLIER + 1;
    long long result = 0;

    for (int i = 0; i < task->work_amount; i++) {
        // 分批计算以支持yield
        result += compute_cube_sum(start_num + i * NUMBERS_PER_ITERATION, NUMBERS_PER_ITERATION);

        if (i % YIELD_FREQUENCY == 0) {
            co_yield(); // 定期让出CPU
        }
    }

    // 存储结果
    task->result = result;
    task->completed = 1;
}

int main(int argc, char *argv[]) {
    co_init();

    // 默认参数
    int num_coroutines = DEFAULT_NUM_COROUTINES;
    int min_work = DEFAULT_MIN_WORK;
    int max_work = DEFAULT_MAX_WORK;

    // 解析命令行参数
    if (argc > 1) num_coroutines = atoi(argv[1]);
    if (argc > 2) min_work = atoi(argv[2]);
    if (argc > 3) max_work = atoi(argv[3]);

    // 安全检查
    if (num_coroutines <= 0) num_coroutines = DEFAULT_NUM_COROUTINES;
    if (min_work <= 0) min_work = DEFAULT_MIN_WORK;
    if (max_work <= min_work) max_work = min_work + DEFAULT_MAX_WORK / 4;

    printf("压力测试开始 - 创建%d个协程 (工作量范围: %d-%d)\n",
           num_coroutines, min_work, max_work);

    // 初始化随机数生成器
    srand(time(NULL));

    Task *tasks = malloc(sizeof(Task) * num_coroutines);
    struct co **coroutines = malloc(sizeof(struct co *) * num_coroutines);

    // 初始化任务
    for (int i = 0; i < num_coroutines; i++) {
        tasks[i].id = i;
        tasks[i].work_amount = min_work + rand() % (max_work - min_work + 1);
        tasks[i].completed = 0;

        // 预先计算期望结果
        int start_num = tasks[i].id * TASK_ID_MULTIPLIER + 1;
        long long expected = 0;
        for (int j = 0; j < tasks[i].work_amount; j++) {
            expected += compute_cube_sum(start_num + j * NUMBERS_PER_ITERATION, NUMBERS_PER_ITERATION);
        }
        tasks[i].expected = expected;
        tasks[i].result = 0;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // 创建所有协程
    for (int i = 0; i < num_coroutines; i++) {
        char name[32];
        sprintf(name, "stress-%d", i);
        coroutines[i] = co_start(name, stress_task, &tasks[i]);
    }

    // 分批等待协程完成
    int batch_size = num_coroutines / BATCH_COUNT;
    if (batch_size == 0) batch_size = 1;

    for (int batch = 0; batch < BATCH_COUNT; batch++) {
        int start = batch * batch_size;
        int end = (batch == BATCH_COUNT - 1) ? num_coroutines : (batch + 1) * batch_size;

        for (int i = start; i < end; i++) {
            if (i < num_coroutines) {
                co_wait(coroutines[i]);
            }
        }

        printf("批次 %d/%d 完成\n", batch + 1, BATCH_COUNT);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // 验证所有协程都已完成并且计算正确
    int success = 1;
    int completed_count = 0;
    int correct_count = 0;

    for (int i = 0; i < num_coroutines; i++) {
        if (!tasks[i].completed) {
            printf("错误: 协程 %d 未完成\n", i);
            success = 0;
        } else {
            completed_count++;

            if (tasks[i].result != tasks[i].expected) {
                printf("错误: 协程 %d 计算结果错误 (得到: %lld, 期望: %lld)\n",
                       i, tasks[i].result, tasks[i].expected);
                success = 0;
            } else {
                correct_count++;
            }
        }
    }

    printf("测试统计: %d/%d 协程完成, %d/%d 计算正确\n",
           completed_count, num_coroutines,
           correct_count, num_coroutines);

    assert(success);

    free(tasks);
    free(coroutines);

    printf("压力测试通过!\n");

    printf("Total time: %.6f s\n", sec);
    return 0;
}