#ifndef COROUTINE_C_CO_H
#define COROUTINE_C_CO_H

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

#endif //COROUTINE_C_CO_H
