#ifndef TOMB_INPUT_H
#define TOMB_INPUT_H

#include <stdint.h>

#include "player.h"
#include "pxa.h"
#include "pxa_ui.h"

/* Left half of the panel: virtual stick, the first touch fixes its centre.
 * Right half: dragging orbits (horizontal) and tilts (vertical) the camera; a
 * short tap jumps. The controller A button also jumps. Ported from micropixel
 * input/touch_controls.cpp. Coordinates are panel pixels. */

typedef struct {
    int stick_active;
    int origin_x;
    int origin_y;
    int x;
    int y;
} tomb_stick_overlay_t;

void tomb_input_init(int panel_width);
void tomb_input_pointer(const pxa_ui_pointer_data_t *pointer);
void tomb_input_button(uint32_t buttons);
/* Drains the deltas accumulated since the previous call. */
tomb_controls_t tomb_input_consume(void);
tomb_stick_overlay_t tomb_input_overlay(void);

#endif
