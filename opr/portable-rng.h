/* SPDX-License-Identifier: MIT. Host RNG contract for the unchanged rand() calls. */
#ifndef OPR_PORTABLE_RNG_H
#define OPR_PORTABLE_RNG_H
#include <stdint.h>
void opr_rng_seed(uint32_t seed);
uint32_t opr_rng_state(void);
int opr_rand(void);
#endif
