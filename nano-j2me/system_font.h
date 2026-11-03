/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "nj2me/nj_gfx.h"
int system_fonts_init(void);
const nj_font *const *system_fonts(void);
void system_text(uint16_t *pixels,int width,int height,int x,int y,int clip_width,const char *utf8);
int system_text_width(const char *utf8);
int system_text_height(void);
int system_text_pixels(void);
void system_text_tint(uint16_t *pixels,int width,int height,int x,int y,int clip_width,const char *utf8,uint16_t color);
