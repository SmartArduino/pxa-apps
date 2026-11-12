// MIT. The PXA adapter calls the entire engine synchronously on one task.
#pragma once
#ifndef __wasm__
#include_next <pthread.h>
#else
#include <stdlib.h>
#include <sys/types.h>
typedef int pthread_once_t;
#define PTHREAD_MUTEX_INITIALIZER {}
#define PTHREAD_COND_INITIALIZER {}
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_RECURSIVE 1
static inline int pthread_mutex_lock(pthread_mutex_t *) { return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *) { return 0; }
static inline int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *) { return 0; }
static inline int pthread_mutexattr_init(pthread_mutexattr_t *) { return 0; }
static inline int pthread_mutexattr_settype(pthread_mutexattr_t *, int) { return 0; }
static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *) { return 0; }
static inline int pthread_cond_broadcast(pthread_cond_t *) { return 0; }
static inline int pthread_once(pthread_once_t *once, void (*fn)()) { if (!*once) { *once = 1; fn(); } return 0; }
[[noreturn]] static inline void pthread_exit(void *) { abort(); }
#endif
