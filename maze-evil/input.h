#ifndef MAZE_EVIL_INPUT_H
#define MAZE_EVIL_INPUT_H

#include <stdint.h>

#include "world.h"

/* Left half: virtual stick. The first finger down defines the centre; forward
 * and strafe come from its displacement. Right half: dragging turns and a
 * short tap fires once. */
typedef struct {
    int down;
    uint32_t id;
    int x;
    int y;
    int origin_x;
    int origin_y;
    uint64_t down_at_us;
    int travel;
} touch_finger_t;

typedef struct {
    int half_width;
    touch_finger_t stick;
    touch_finger_t look;
    float turn_accum;
    int tap_fired;
} touch_controls_t;

void touch_init(touch_controls_t *controls, int view_width);
void touch_on_down(touch_controls_t *controls, uint32_t id, int x, int y,
                   uint64_t now_us);
void touch_on_move(touch_controls_t *controls, uint32_t id, int x, int y,
                   uint64_t now_us);
void touch_on_up(touch_controls_t *controls, uint32_t id, uint64_t now_us);
/* UP carries the final position even if its preceding MOVE was coalesced.
 * Inline event adapters reuse the move/up ABI without new Wasm function types. */
[[gnu::always_inline]] inline void touch_on_release(
    touch_controls_t *controls, uint32_t id, int x, int y, uint64_t now_us) {
    touch_on_move(controls, id, x, y, now_us);
    touch_on_up(controls, id, now_us);
}
/* Cancellation releases ownership without interpreting the gesture as a tap. */
[[gnu::always_inline]] inline void touch_on_cancel(touch_controls_t *controls, uint32_t id) {
    if (controls->stick.id == id) controls->stick.down = 0;
    if (controls->look.id == id) controls->look.down = 0;
}
controls_t touch_consume(touch_controls_t *controls, uint64_t now_us);

#endif
