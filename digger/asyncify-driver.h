/* SPDX-License-Identifier: MIT
 * Independent PXA cooperative driver. This file contains no upstream game data.
 */
#ifndef DIGGER_ASYNCIFY_DRIVER_H
#define DIGGER_ASYNCIFY_DRIVER_H
#include <stdint.h>
enum {
  DIGGER_COROUTINE_IDLE = 0,
  DIGGER_COROUTINE_SUSPENDED = 1,
  DIGGER_COROUTINE_FINISHED = 2,
  DIGGER_COROUTINE_ACTIVE_MISUSE = -1,
  DIGGER_COROUTINE_FAULT = -2
};
int digger_coroutine_start(void);
int digger_coroutine_resume(void);
void digger_coroutine_yield(uint32_t wait_us);
int digger_coroutine_cancel(void);
int digger_coroutine_is_suspended(void);
uint32_t digger_coroutine_get_wait_us(void);
uint32_t digger_coroutine_saved_bytes(void);
int digger_coroutine_status(void);
#endif

