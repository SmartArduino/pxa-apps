// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Thin accessors compiled beside the unmodified upstream core.
void crisp_core_init(void);
void crisp_core_start(int index, int best, uint32_t seed);
int crisp_core_state(void); // 0 title, 1 play, 2 game over
void crisp_core_best(int best);
#ifdef __cplusplus
}
#endif
