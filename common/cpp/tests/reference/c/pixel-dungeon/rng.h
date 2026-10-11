#ifndef PD_RNG_H
#define PD_RNG_H

#include <stdint.h>

/* xorshift32: deterministic, cheap, good enough for level generation and
 * combat rolls. The state is a plain u32 so it can live directly in the saved
 * game state. */
static inline uint32_t pd_rng_next(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static inline void pd_rng_seed(uint32_t *state, uint32_t seed) {
    *state = seed == 0 ? UINT32_C(0x9e3779b9) : seed;
    for (int index = 0; index < 4; ++index) (void)pd_rng_next(state);
}

/* Uniform in [0, bound). */
static inline uint32_t pd_rng_below(uint32_t *state, uint32_t bound) {
    return bound == 0 ? 0 : pd_rng_next(state) % bound;
}

/* Uniform in [low, high]. */
static inline int pd_rng_range(uint32_t *state, int low, int high) {
    if (high <= low) return low;
    return low + (int)pd_rng_below(state, (uint32_t)(high - low + 1));
}

static inline uint32_t pd_rng_mix(uint32_t seed, uint32_t salt) {
    uint32_t value = seed + salt * UINT32_C(0x9e3779b9);
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return value;
}

#endif
