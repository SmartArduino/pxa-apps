/* GPL-2.0-or-later; embedded bitmap data remains SIL OFL-1.1. */
#pragma once
#include <stdint.h>
#define GB_UI_FONT_HEIGHT 16
uint32_t gb_ui_next(const char **text);
unsigned gb_ui_glyph(uint32_t codepoint,uint16_t rows[GB_UI_FONT_HEIGHT]);
unsigned gb_ui_width(const char *text);
void gb_ui_draw(uint16_t *out,unsigned width,unsigned height,int x,int y,
                 const char *text,uint16_t color,unsigned max_width);
