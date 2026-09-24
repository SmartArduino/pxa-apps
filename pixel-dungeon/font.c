#include "font.h"

int pd_font_next(const char **cursor, pd_glyph_t *glyph) {
    const uint8_t *bytes = (const uint8_t *)*cursor;
    uint32_t codepoint = 0;
    int length = 0;
    if (*bytes == 0) return 0;
    if (bytes[0] < 0x80) {
        codepoint = bytes[0];
        length = 1;
    } else if ((bytes[0] & 0xE0) == 0xC0) {
        codepoint = bytes[0] & 0x1F;
        length = 2;
    } else if ((bytes[0] & 0xF0) == 0xE0) {
        codepoint = bytes[0] & 0x0F;
        length = 3;
    } else if ((bytes[0] & 0xF8) == 0xF0) {
        codepoint = bytes[0] & 0x07;
        length = 4;
    } else {
        *cursor += 1;
        return -1;
    }
    for (int index = 1; index < length; ++index) {
        if ((bytes[index] & 0xC0) != 0x80) {
            *cursor += 1;
            return -1;
        }
        codepoint = (codepoint << 6) | (uint32_t)(bytes[index] & 0x3F);
    }
    *cursor += length;
    if (codepoint < 0x80) {
        if (codepoint < 0x20 || codepoint > 0x7E) return -1;
        glyph->atlas = pd_font_ascii;
        glyph->atlas_width = PD_ASCII_ATLAS_W;
        glyph->cell_width = PD_ASCII_CELL_W;
        glyph->cell_height = PD_ASCII_CELL_H;
        glyph->slot = (uint16_t)(codepoint - 0x20);
        glyph->cjk = 0;
        return 1;
    }
    {
        const int slot = pd_font_cjk_slot(codepoint);
        if (slot < 0) return -1;
        glyph->atlas = slot >= PD_CJK_PAGE_GLYPHS ?
                       pd_font_cjk_extra : pd_font_cjk;
        glyph->atlas_width = PD_CJK_ATLAS_W;
        glyph->cell_width = PD_CJK_CELL;
        glyph->cell_height = PD_CJK_CELL;
        glyph->slot = (uint16_t)(slot % PD_CJK_PAGE_GLYPHS);
        glyph->cjk = slot >= PD_CJK_PAGE_GLYPHS ? 2 : 1;
    }
    return 1;
}

int pd_font_advance(const pd_glyph_t *glyph, int scale) {
    return (int)glyph->cell_width * scale;
}

int pd_font_width(const char *text, int scale) {
    int width = 0;
    const char *cursor = text;
    pd_glyph_t glyph;
    int result;
    while ((result = pd_font_next(&cursor, &glyph)) != 0) {
        if (result < 0) continue;
        width += pd_font_advance(&glyph, scale);
    }
    return width;
}

int pd_font_height(int scale) { return PD_CJK_CELL * scale; }
