#pragma once
#include <stdint.h>
#include "core.h"
/* Debt is in millionths of a frame. Keep fractional time and retain unfinished
 * CPU work across callbacks. Display backpressure never changes this clock. */
typedef struct { uint64_t previous, debt; } emu_schedule_t;
static inline void emu_schedule_reset(emu_schedule_t *s) { s->previous = s->debt = 0; }
static inline void emu_schedule_tick(emu_schedule_t *s, uint64_t now, unsigned hz) {
    (void)hz;
    /* Hardware-ratio clock, with no per-tick rounding or floating point. */
    if (s->previous && now >= s->previous) s->debt += (now - s->previous) * EMU_CLOCK_HZ;
    s->previous = now;
}
static inline int emu_schedule_due(const emu_schedule_t *s) { return s->debt >= UINT64_C(1000000)*EMU_FRAME_CYCLES; }
static inline void emu_schedule_step(emu_schedule_t *s) { s->debt -= UINT64_C(1000000)*EMU_FRAME_CYCLES; }
