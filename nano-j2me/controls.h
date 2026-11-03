/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "layout.h"
#include "settings.h"
#define NANO_ACTION_KEYBOARD (NANO_ACTION_MENU+1)
#define NANO_CONTROL_COUNT 22
#define NANO_CONTROL_MASK_BYTES 49152
typedef struct {
    nano_rect rect;
    unsigned offset;
    uint8_t key,pressed;
    uint16_t red[2][32],green[2][64],blue[2][32];
} nano_button;
typedef struct {
    nano_button buttons[NANO_CONTROL_COUNT];
    uint8_t mask[NANO_CONTROL_MASK_BYTES];
    nano_rect dpad,bounds;
    unsigned count,used,alpha,generation,cache_rebuilds;
    uint64_t blended_pixels;
} nano_controls;
typedef void (*nano_control_label)(uint16_t *pixels,int width,int height,const char *text);
void nano_controls_build(nano_controls *c,nano_rect safe,const nano_settings *settings,int visible,nano_control_label label);
uint64_t nano_controls_hit(const nano_controls *c,int x,int y,unsigned *action,int *button);
void nano_controls_draw(nano_controls *c,uint16_t *pixels,unsigned stride,uint64_t keys,uint32_t buttons);
