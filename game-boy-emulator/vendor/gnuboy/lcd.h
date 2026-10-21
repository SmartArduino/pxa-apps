#pragma once

#include "gnuboy.h"

void gb_lcd_init(void);
void gb_lcd_reset(bool hard);
void gb_lcd_emulate(int cycles);
void gb_lcd_stat_trigger(void);
void gb_lcd_lcdc_change(byte b);
void gb_lcd_pal_dirty(void);

static inline uint16_t gnuboy_rgb565(uint16_t color)
{
    unsigned green = (color >> 5) & 31;
    return ((color & 31) << 11) | ((green * 2 + green / 16) << 5) | ((color >> 10) & 31);
}
