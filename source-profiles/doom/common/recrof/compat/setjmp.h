#pragma once
/* The current PXA WAMR build has no Wasm EH. Engine error recovery therefore
 * ends the current session visibly; the launcher can start a fresh instance. */
#include "port.h"
typedef int jmp_buf[1];
#define setjmp(env) ((void)(env), 0)
#define longjmp(env, value)                                                    \
  ((void)(env), (void)(value),                                                 \
   port_error("Engine aborted this session. Reopen the game to restart."))
