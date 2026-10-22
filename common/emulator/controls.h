/* GPL-2.0-or-later. NES-compatible layout with RGB565 control caches. */
#pragma once
#include <stdint.h>
#include "core.h"
#include "display.h"
enum {EMU_CONTROL_LEFT,EMU_CONTROL_RIGHT,EMU_CONTROL_UP,EMU_CONTROL_DOWN,
      EMU_CONTROL_SELECT,EMU_CONTROL_START,EMU_CONTROL_MENU,EMU_CONTROL_B,EMU_CONTROL_A,
      EMU_CONTROL_X,EMU_CONTROL_Y,EMU_CONTROL_L,EMU_CONTROL_R,
      EMU_CONTROLS_COUNT,EMU_CONTROL_STYLES=1+EMU_CONTROLS_COUNT*3};
#define EMU_CONTROLS_MASK_BYTES 131072u

typedef struct {
    int x,y,w,h;
    uint16_t bit;
    uint16_t tint;
    const char *label;
    unsigned mask_offset;
} emu_control_button_t;
typedef struct {
    unsigned width,height,stride,alpha,controller_hidden;
    unsigned cached_alpha,cached_pressed,mask_used;
    emu_control_button_t buttons[EMU_CONTROLS_COUNT];
    uint8_t mask[EMU_CONTROLS_MASK_BYTES];
    uint16_t red[EMU_CONTROL_STYLES][32],green[EMU_CONTROL_STYLES][64],blue[EMU_CONTROL_STYLES][32];
    emu_display_rect_t safe;
} emu_controls_t;
void emu_controls_init(emu_controls_t *c,unsigned width,unsigned height);
void emu_controls_set_labels(emu_controls_t *c,const char *select_label,const char *start_label,const char *menu_label);
unsigned emu_controls_cycle(emu_controls_t *c);
int emu_controls_visible(const emu_controls_t *c);
int emu_controls_use_controller(const emu_controls_t *c);
int emu_controls_use_touch(const emu_controls_t *c);
/* Pad bitmask, -1 for MENU, zero for the D-pad dead zone or empty screen. */
int emu_controls_hit(const emu_controls_t *c,int x,int y);
/* Restore black bars covered by last frame's controls; the game rectangle is
 * repainted separately. No full-canvas clearing or background copy is needed. */
void emu_controls_clear_bars(const emu_controls_t *c,uint16_t *out,const emu_display_rect_t *view);
void emu_controls_draw(emu_controls_t *c,uint16_t *out,unsigned pressed);
void emu_controls_clear_bars_rows(const emu_controls_t *c,uint16_t *out,const emu_display_rect_t *view,const uint8_t *dirty);
void emu_controls_draw_rows(emu_controls_t *c,uint16_t *out,unsigned pressed,const uint8_t *dirty);
