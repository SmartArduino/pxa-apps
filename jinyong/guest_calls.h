/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
int jy_host_dispatch(void);
int jy_host_result_ready(void);
int jy_host_call_pending(void);
void jy_host_describe(uint32_t *kind,uint32_t *operation);
void jy_host_calls_reset(void);
unsigned jy_host_busy_count(void);
int32_t jy_host_last_error(void);
int32_t jy_host_last_result(void);
