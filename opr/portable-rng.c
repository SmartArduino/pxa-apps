/* SPDX-License-Identifier: MIT. Replayable host xorshift32, not cryptographic. */
#include "portable-rng.h"
static uint32_t state = 1;
void opr_rng_seed(uint32_t seed) { state = seed ? seed : UINT32_C(0x6d2b79f5); }
uint32_t opr_rng_state(void) { return state; }
int opr_rand(void) {
    uint32_t x = state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    state = x;
    return (int)(x >> 1);
}
