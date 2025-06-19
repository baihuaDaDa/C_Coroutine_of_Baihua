# C Coroutine of Baihua (CCB)

> A high-performance coroutine library based on a G-M-P model, supporting multi-core concurrency, user-space scheduling, and semaphore-based synchronization.

## ✨ Features

* 🧵 **Coroutine Scheduling**: Lightweight user-space coroutines with stackful context switching.
* ⚙️ **G-M-P Model**: Decouples coroutine logic (G) from physical threads (M) and processors (P) for concurrency and load balancing.
* 🚦 **User-space Semaphores**: Provides a native `co_sem` API for blocking synchronization.
* 🔀 **Multi-core Support**: Fully utilizes all CPU cores with `pthread`-based M (machine) threads.
* 🔁 **Coroutine Operations**: Support for yield, wait, and lifecycle management.

---

## 🔧 Build Instructions

### 🧩 Requirements

* GCC (x86-64 and i386 target support)
* POSIX-compliant system (e.g., Linux)
* `make`, `pthread`, `valgrind` (for testing)

### 🏗️ Build the Library

```bash
make lib      # Build both 64-bit and 32-bit shared libraries
```

### 🧪 Build and Run Tests

```bash
make test     # Compile and run all test programs under `test/`
```

---

## 📦 Library API Overview

Header: [`src/co.h`](src/co.h)

```c
void co_init();   // Initialize the coroutine runtime

struct co *co_start(const char *name, void (*func)(void *), void *arg);  // Create and enqueue a coroutine

void co_yield();   // Voluntarily yield execution to another coroutine

void co_wait(struct co *co);  // Block until a target coroutine finishes

// Semaphore APIs
struct co_sem *co_sem_create(unsigned int value);
void co_sem_wait(struct co_sem *sem);
void co_sem_post(struct co_sem *sem);
void co_sem_destroy(struct co_sem *sem);
```

---

## 🧠 Internal Design

### 🏗️ Architecture

* **G (Goroutine)**: Represents an executable context (a coroutine).
* **M (Machine)**: Backed by an OS thread (via `pthread`), responsible for executing coroutines.
* **P (Processor)**: Manages coroutine queues for scheduling and balancing.

### 📜 Scheduling Strategy

* Global + per-P run queues
* Work-stealing logic ensures balanced load
* Stackful context switch using `setjmp/longjmp` + manual stack pointer manipulation

### 🧵 Synchronization

* Coroutine-level blocking via semaphores (`co_sem_wait`, `co_sem_post`)
* Coroutine waiting handled via cooperative scheduling and `list` of waiters
* `main` coroutine uses `sem_t` to synchronize with non-main coroutines

---

## 🧪 Test Programs

Located in the `test/` directory:

| Test Name           | Purpose                                     |
| ------------------- | ------------------------------------------- |
| `massive_sum`       | Massive coroutine creation and join         |
| `matrix_transpose`  | Parallel matrix computation (data parallel) |
| `random_load`       | Load balancing with random tasks            |
| `unbalanced_load`   | Scheduling under skewed load                |
| `sem_basic`         | Basic semaphore synchronization             |
| `producer_consumer` | Classic producer-consumer with `co_sem`     |

To build and run, modify `test/Makefile` with:

```makefile
TESTS := <tests you want to run>
```

and then run:

```bash
make test
```

---

## 🔍 Debugging

Use `debug-64` or `debug-32` to start debugging with `gdb`:

```bash
make debug-64   # For 64-bit binaries
make debug-32   # For 32-bit binaries
```

---

## 🧹 Clean Build

```bash
make clean
```

---

## 📌 Notes

* This library does **not** rely on any OS-level thread pool or condition variables for coroutine execution.
* Manual memory management and synchronization are required; users must destroy semaphores explicitly to avoid leaks.
* The main coroutine is treated specially: it cannot yield and is woken up via `sem_post`.

---

## 📁 Project Structure

```
.
├── src/               # Coroutine library implementation
│   ├── co.h/c         # Core coroutine API and implementation
│   ├── lang_items.h   # Language-specific items (e.g., panic handler)
│   ├── list.h         # Doubly-linked list utils
│   └── Makefile       # Build coroutine library
├── test/              # Testing programs
│   ├── *.c            # Test source files
│   ├── only_leak.supp # Suppression file for valgrind
│   └── Makefile       # Build and test runner
├── .gitignore         # Git ignore file
├── Makefile           # Root Makefile for lib+test
└── README.md          # Project documentation
```