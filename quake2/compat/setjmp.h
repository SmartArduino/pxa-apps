#pragma once
/* WAMR has no native Wasm exception recovery. Retain the engine's original
 * error before it clears state, then show it at the existing abort boundary. */
#include "port.h"
typedef int jmp_buf[1];
void port_quake2_abort(void);
#define setjmp(env) ((void)(env), 0)
#define longjmp(env, value) \
  ((void)(env), (void)(value), port_quake2_abort())
