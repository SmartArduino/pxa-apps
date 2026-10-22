/* GPL-2.0-or-later. NES-compatible layout with RGB565 control caches. */
#pragma once
#include <stdint.h>
#include "core.h"
#include "display.h"
enum {GB_CONTROL_LEFT,GB_CONTROL_RIGHT,GB_CONTROL_UP,GB_CONTROL_DOWN,
      GB_CONTROL_SELECT,GB_CONTROL_START,GB_CONTROL_MENU,GB_CONTROL_B,GB_CONTROL_A,
      GB_CONTROLS_COUNT,GB_CONTROL_STYLES=1+GB_CONTROLS_COUNT*3};
#define GB_CONTROLS_MASK_BYTES 49152u

typedef struct {
    int x,y,w,h;
    uint8_t bit;
    uint16_t tint;
    const char *label;
    unsigned mask_offset;
} gb_control_button_t;
typedef struct {
    unsigned width,height,stride,alpha,controller_hidden;
    unsigned cached_alpha,cached_pressed,mask_used;
    gb_control_button_t buttons[GB_CONTROLS_COUNT];
    uint8_t mask[GB_CONTROLS_MASK_BYTES];
    uint16_t red[GB_CONTROL_STYLES][32],green[GB_CONTROL_STYLES][64],blue[GB_CONTROL_STYLES][32];
    gb_display_rect_t safe;
} gb_controls_t;
void gb_controls_init(gb_controls_t *c,unsigned width,unsigned height);
void gb_controls_set_labels(gb_controls_t *c,const char *select_label,const char *start_label,const char *menu_label);
unsigned gb_controls_cycle(gb_controls_t *c);
int gb_controls_visible(const gb_controls_t *c);
int gb_controls_use_controller(const gb_controls_t *c);
int gb_controls_use_touch(const gb_controls_t *c);
/* Pad bitmask, -1 for MENU, zero for the D-pad dead zone or empty screen. */
int gb_controls_hit(const gb_controls_t *c,int x,int y);
/* Restore black bars covered by last frame's controls; the game rectangle is
 * repainted separately. No full-canvas clearing or background copy is needed. */
void gb_controls_clear_bars(const gb_controls_t *c,uint16_t *out,const gb_display_rect_t *view);
void gb_controls_draw(gb_controls_t *c,uint16_t *out,unsigned pressed);
void gb_controls_clear_bars_rows(const gb_controls_t *c,uint16_t *out,const gb_display_rect_t *view,const uint8_t *dirty);
void gb_controls_draw_rows(gb_controls_t *c,uint16_t *out,unsigned pressed,const uint8_t *dirty);
void gb_controls_mark_rows(const gb_controls_t *c,unsigned changed_buttons,uint8_t *rows);
