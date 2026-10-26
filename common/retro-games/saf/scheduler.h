/* SPDX-License-Identifier: MIT. Unsigned wrap-safe clock, maximum four frames. */
#ifndef PXA_SAF_SCHEDULER_H
#define PXA_SAF_SCHEDULER_H
#include <stdint.h>
typedef struct { uint32_t last_ms,remainder,dropped;uint8_t initialized; } saf_clock_t;
static inline void saf_clock_pause(saf_clock_t *clock) { clock->initialized=0;clock->remainder=0; }
static inline unsigned saf_clock_due(saf_clock_t *clock,uint32_t now) {
    if(!clock->initialized){clock->last_ms=now;clock->initialized=1;return 0;}
    uint32_t delta=now-clock->last_ms;clock->last_ms=now;
    uint64_t total=(uint64_t)delta+clock->remainder;
    uint32_t due=(uint32_t)(total/40);clock->remainder=(uint32_t)(total%40);
    if(due>4){clock->dropped+=due-4;due=4;}return due;
}
#endif
