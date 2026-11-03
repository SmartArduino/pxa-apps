/* SPDX-License-Identifier: MIT
 * Independent adapter for Binaryen's public Asyncify intrinsic ABI.
 * The original engine is called on its own linear-memory C stack. This keeps
 * its suspended local arrays safe while PXA callbacks and malloc use the host
 * stack. HOME/cancel discard a suspended continuation; they never resume it.
 */
#include "asyncify-driver.h"
#include <stddef.h>
#include <stdint.h>
#define DIGGER_NOINLINE __attribute__((noinline))
#define DIGGER_GAME_STACK_BYTES (512u * 1024u)
#define DIGGER_ASYNC_STACK_BYTES (1024u * 1024u)

__attribute__((import_module("asyncify"), import_name("start_unwind")))
void digger_async_start_unwind(void *);
__attribute__((import_module("asyncify"), import_name("stop_unwind")))
void digger_async_stop_unwind(void);
__attribute__((import_module("asyncify"), import_name("start_rewind")))
void digger_async_start_rewind(void *);
__attribute__((import_module("asyncify"), import_name("stop_rewind")))
void digger_async_stop_rewind(void);

extern uintptr_t digger_stack_enter(uintptr_t);
/* WAMR checks auxiliary stack writes against the linker-defined C stack.
 * Keep the suspended game in its lower half, separate from Host callbacks. */
extern unsigned char __stack_low[], __stack_high[];
static unsigned char *game_stack;
static _Alignas(16) unsigned char saved_stack[DIGGER_ASYNC_STACK_BYTES];
static struct { uint32_t cursor, end; } async_data;
static int engine_started;
static uintptr_t game_sp;
static int phase, active, rewinding, yielded;
static uint32_t requested_wait_us, max_saved_bytes;

static void init_buffers(void)
{
  game_stack = __stack_low + 64u;
  async_data.cursor = (uint32_t)(uintptr_t)saved_stack;
  async_data.end = (uint32_t)(uintptr_t)(saved_stack + sizeof saved_stack);
  game_sp = (uintptr_t)(game_stack + DIGGER_GAME_STACK_BYTES);
  max_saved_bytes = 0;
}

/* Uninstrumented scheduler entry; listed in the Asyncify removelist. */
DIGGER_NOINLINE int digger_coroutine_drive(void)
{
  uint32_t used;
  uintptr_t low = (uintptr_t)game_stack;
  uintptr_t high = (uintptr_t)(game_stack + DIGGER_GAME_STACK_BYTES);
  active = 1;
  game_sp = digger_stack_enter(game_sp);
  active = 0;
  if (yielded) {
    digger_async_stop_unwind();
    used = async_data.cursor - (uint32_t)(uintptr_t)saved_stack;
    if (used > max_saved_bytes) max_saved_bytes = used;
    if (async_data.cursor > async_data.end || game_sp < low || game_sp > high) {
      phase = DIGGER_COROUTINE_FAULT;
      return phase;
    }
    phase = DIGGER_COROUTINE_SUSPENDED;
  } else {
    phase = DIGGER_COROUTINE_FINISHED;
  }
  return phase;
}

DIGGER_NOINLINE int digger_coroutine_start(void)
{
  if (active || engine_started)
    return DIGGER_COROUTINE_ACTIVE_MISUSE;
  if ((uintptr_t)__stack_high - (uintptr_t)__stack_low <
      DIGGER_GAME_STACK_BYTES + 64u + 512u * 1024u)
    return DIGGER_COROUTINE_FAULT;
  init_buffers();
  engine_started = 1;
  requested_wait_us = 0;
  rewinding = yielded = 0;
  return digger_coroutine_drive();
}

DIGGER_NOINLINE int digger_coroutine_resume(void)
{
  if (active || phase != DIGGER_COROUTINE_SUSPENDED)
    return DIGGER_COROUTINE_ACTIVE_MISUSE;
  yielded = 0;
  rewinding = 1;
  digger_async_start_rewind(&async_data);
  return digger_coroutine_drive();
}

/* This leaf MUST remain instrumented and noinline. */
DIGGER_NOINLINE void digger_coroutine_yield(uint32_t wait_us)
{
  if (rewinding) {
    rewinding = 0;
    digger_async_stop_rewind();
    return;
  }
  requested_wait_us = wait_us;
  yielded = 1;
  digger_async_start_unwind(&async_data);
}

DIGGER_NOINLINE int digger_coroutine_cancel(void)
{
  if (active) return DIGGER_COROUTINE_ACTIVE_MISUSE;
  /* The frontend destroys engine only after this function returns. */
  rewinding = yielded = 0;
  requested_wait_us = 0;
  init_buffers();
  phase = DIGGER_COROUTINE_IDLE;
  return phase;
}

int digger_coroutine_is_suspended(void)
{
  return phase == DIGGER_COROUTINE_SUSPENDED;
}
uint32_t digger_coroutine_get_wait_us(void) { return requested_wait_us; }
uint32_t digger_coroutine_saved_bytes(void) { return max_saved_bytes; }
int digger_coroutine_status(void) { return phase; }

