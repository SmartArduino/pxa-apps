#ifndef PD_FONT_H
#define PD_FONT_H

#include <stdint.h>

#include "font_data.h"

/* Anti-aliased text: ASCII glyphs are 8x14, CJK glyphs 16x16, and every
 * texel is a coverage value drawn with the Host's per-texel alpha path. */
typedef struct {
    const uint8_t *atlas;
    uint16_t atlas_width;
    uint16_t cell_width;
    uint16_t cell_height;
    uint16_t slot;
    uint8_t cjk;
} pd_glyph_t;

/* Decodes one UTF-8 glyph. Returns 0 at the end of the string, or when the
 * codepoint has no baked-in glyph. */
int pd_font_next(const char **cursor, pd_glyph_t *glyph);
int pd_font_advance(const pd_glyph_t *glyph, int scale);
int pd_font_width(const char *text, int scale);
int pd_font_height(int scale);

#endif
