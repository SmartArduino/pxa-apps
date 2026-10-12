#pragma once
// One Guest is entered serially by the PXA runtime. No host thread API is
// imported into the sandbox. Native engine tests retain real synchronization.
#if defined(__wasm__)
#define pthread_mutex_t ImeSingleOwnerMutex
using ImeSingleOwnerMutex = unsigned;
#define PTHREAD_MUTEX_INITIALIZER 0
inline int pthread_mutex_lock(pthread_mutex_t*) { return 0; }
inline int pthread_mutex_trylock(pthread_mutex_t*) { return 0; }
inline int pthread_mutex_unlock(pthread_mutex_t*) { return 0; }
#else
#include <pthread.h>
#endif
