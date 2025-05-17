# Coroutine_C

## 简介
`Coroutine_C` 是一个用 C 语言实现的协程库，支持创建、切换和等待协程。该库提供了简单的 API，用于实现协程的基本功能，并包含测试用例和示例代码。

## 文件结构
- `src/`: 包含协程库的源代码和头文件。
- `test/`: 包含测试程序的代码。
- `Makefile`: 顶层构建文件，用于构建库和测试程序。

## 构建
在项目根目录下运行以下命令：

### 构建库和测试程序
```bash
make
```

### 仅构建库
```bash
make lib
```

### 清理生成文件
```bash
make clean
```

## API
以下是库中提供的主要 API：

### 函数

#### `co_start`
```c
struct co *co_start(const char *name, void (*func)(void *), void *arg);
```
- **描述**: 创建一个新的协程，但不会立即执行。
- **参数**:
  - `name`: 协程的名称。
  - `func`: 协程执行的函数。
  - `arg`: 传递给协程函数的参数。
- **返回值**: 返回新创建的协程指针，失败时会触发 panic。

#### `co_yield`
```c
void co_yield();
```
- **描述**: 切换到另一个协程。

#### `co_wait`
```c
void co_wait(struct co *co);
```
- **描述**: 等待指定的协程执行完成。
- **参数**:
  - `co`: 要等待的协程。

## 示例
以下是如何使用该库的示例代码：

### 示例代码
```c
#include <co.h>
#include <stdio.h>

void work(void *arg) {
    const char *msg = (const char *)arg;
    for (int i = 0; i < 5; ++i) {
        printf("%s %d\n", msg, i);
        co_yield();
    }
}

int main() {
    struct co *co1 = co_start("worker1", work, "Hello from co1");
    struct co *co2 = co_start("worker2", work, "Hello from co2");

    co_wait(co1);
    co_wait(co2);

    return 0;
}
```

## 测试
测试程序位于 `test/` 目录中，支持 32 位和 64 位测试。

### 运行测试
在项目根目录下运行以下命令：
```bash
make test
```

测试程序会自动运行 64 位和 32 位的测试，并输出结果。

## 注意事项
- 确保系统安装了支持 32 位和 64 位编译的 GCC。
- 如果遇到问题，请检查 `Makefile` 配置是否正确。