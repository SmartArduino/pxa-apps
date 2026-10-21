/* GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include "display.h"
enum { NES_CONTROLS_COUNT=9, NES_CONTROL_STYLES=1+NES_CONTROLS_COUNT*3 };
typedef struct {
    int x,y,w,h;
    uint8_t bit;
    uint16_t tint;
    const char *label;
} nes_control_button_t;
typedef struct {unsigned left,top,right,bottom;} nes_control_bounds_t;
typedef struct {
    unsigned width,height,alpha,controller_hidden;
    unsigned cached_alpha,cached_pressed;
    nes_control_button_t buttons[NES_CONTROLS_COUNT];
    nes_control_bounds_t paint_bounds[NES_CONTROLS_COUNT];
    uint8_t mask[NES_DISPLAY_MAX_WIDTH*NES_DISPLAY_MAX_HEIGHT],row_used[NES_DISPLAY_MAX_HEIGHT];
    uint16_t palette[64],lut[NES_CONTROL_STYLES][64],black_lut[NES_CONTROL_STYLES];
    nes_display_rect_t safe;
} nes_controls_t;
void nes_controls_init(nes_controls_t *c,unsigned width,unsigned height,const uint16_t *palette);
void nes_controls_set_labels(nes_controls_t *c,const char *select_label,const char *start_label,const char *menu_label);
void nes_controls_update(nes_controls_t *c,unsigned pressed);
void nes_controls_cycle(nes_controls_t *c);
int nes_controls_visible(const nes_controls_t *c);
int nes_controls_use_controller(nes_controls_t *c);
int nes_controls_use_touch(nes_controls_t *c);
/* Positive values are NES pad bits, -1 opens the pause menu, 0 is empty. */
int nes_controls_hit(const nes_controls_t *c,int x,int y);
