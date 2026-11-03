/* SPDX-License-Identifier: MIT */
#ifndef JY_PROTECTED_BRIDGE_H
#define JY_PROTECTED_BRIDGE_H
#include <stdint.h>
enum {JY_IDLE=0,JY_WAIT=1,JY_CHECKPOINT=2,JY_THROW=3,JY_DONE=4,JY_FAULT=-1};
int jy_protected_checkpoint(void *identity,volatile int *status);
void jy_protected_release(void *identity);
void jy_protected_throw(void *identity,int status);
void jy_suspend(uint64_t us);
void jy_finish(int code);
int jy_finish_code(void);
int jy_coroutine_begin(void (*entry)(void));
int jy_coroutine_resume(void);
void jy_coroutine_cancel(void);
int jy_coroutine_phase(void);
int jy_coroutine_active(void);
uint64_t jy_wait_us(void);
unsigned jy_anchor_count(void);
unsigned jy_anchor_peak(void);
uint32_t jy_saved_peak(void);
uint32_t jy_anchor_bytes_peak(void);
int jy_fault_code(void);
#endif
