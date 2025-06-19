#ifndef COROUTINE_C_CO_H
#define COROUTINE_C_CO_H

/// @brief Initialize the coroutine library.
void co_init();

/** @brief Create a new coroutine (but not execute it at once).
  * @param name The name of the coroutine.
  * @param func The function to be executed.
  * @param arg The argument to be passed to the function.
  * @return A pointer to the new coroutine, panic once failed.
  */
struct co *co_start(const char *name, void (*func)(void *), void *arg);

/// @brief Switch to another coroutine.
void co_yield();

/** @brief Wait for a coroutine to finish.
  * @param co The coroutine to wait for.
  */
void co_wait(struct co *co);

/** @brief Create a semaphore.
  * @param value The initial value of the semaphore.
  * @return A pointer to the initialized semaphore.
  */
struct co_sem *co_sem_create(unsigned int value);

/** @brief Wait on a semaphore. This function will block if the semaphore value is zero.
  * @param sem The semaphore to wait on.
  */
void co_sem_wait(struct co_sem *sem);

/** @brief Post (signal) a semaphore, releasing it. This function will wake up one coroutine waiting on the semaphore.
  * @param sem The semaphore to post.
  */
void co_sem_post(struct co_sem *sem);

/** @brief Destroy a semaphore, freeing its resources.
  *        User needs to free all the created semaphores before exiting the program, or memory leaks would occur.
  * @param sem The semaphore to destroy.
  */
void co_sem_destroy(struct co_sem *sem);

#endif //COROUTINE_C_CO_H
