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
controls_t touch_consume(touch_controls_t *controls, uint64_t now_us);

#endif
