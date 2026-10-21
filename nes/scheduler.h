#pragma once
#include <stdint.h>
/* Debt is in millionths of a frame. Keep fractional time and retain unfinished
 * CPU work across callbacks. Display backpressure never changes this clock. */
typedef struct { uint64_t previous, debt; } nes_schedule_t;
static inline void nes_schedule_reset(nes_schedule_t *s) { s->previous = s->debt = 0; }
static inline void nes_schedule_tick(nes_schedule_t *s, uint64_t now, unsigned hz) {
    if (s->previous && now >= s->previous) s->debt += (now - s->previous) * hz;
    s->previous = now;
}
static inline int nes_schedule_due(const nes_schedule_t *s) { return s->debt >= 1000000; }
static inline void nes_schedule_step(nes_schedule_t *s) { s->debt -= 1000000; }
