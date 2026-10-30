// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if !defined(PXA_POCKET_SYSTEM_TEXT) && !defined(PXA_POCKET_STUDIO)
#include "glyphs.hpp"
#include "ascii.hpp"
#endif

namespace pocket {
enum class Locale : uint8_t { ZH = 0, EN = 1 };
// C++17 inline storage is shared by the game and its runtime, including native tests.
inline Locale locale = Locale::ZH;
inline bool english() { return locale == Locale::EN; }
inline void set_english(bool value) { locale = value ? Locale::EN : Locale::ZH; }
inline const char *tr(const char *en, const char *zh) { return english() ? en : zh; }
}

namespace pocket_ui {
// Invalid sequences consume one byte and draw the visible replacement glyph.
inline uint32_t next_utf8(const char *&s) {
    const auto *p = reinterpret_cast<const unsigned char *>(s);
    if (!*p) return 0;
    uint32_t cp = *p;
    unsigned length = cp < 0x80 ? 1 : cp >= 0xc2 && cp <= 0xdf ? 2 :
        cp >= 0xe0 && cp <= 0xef ? 3 : cp >= 0xf0 && cp <= 0xf4 ? 4 : 0;
    if (!length) { ++s; return 0xfffd; }
    if (length == 1) { ++s; return cp; }
    cp &= (1u << (7 - length)) - 1;
    for (unsigned i = 1; i < length; ++i) {
        if (!p[i] || (p[i] & 0xc0) != 0x80) { ++s; return 0xfffd; }
        cp = (cp << 6) | (p[i] & 0x3f);
    }
    if ((length == 2 && cp < 0x80) || (length == 3 && cp < 0x800) ||
        (length == 4 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
        ++s; return 0xfffd;
    }
    s += length; return cp;
}
// Ten rows, with the left pixel in bit 9. The generated table is sorted.
#if !defined(PXA_POCKET_SYSTEM_TEXT) && !defined(PXA_POCKET_STUDIO)
inline const uint16_t *glyph(uint32_t cp) {
    size_t first = 0, last = sizeof(cjk_glyphs) / sizeof(cjk_glyphs[0]);
    while (first < last) {
        size_t middle = first + (last-first)/2;
        if (cjk_glyphs[middle].codepoint < cp) first = middle + 1;
        else last = middle;
    }
    return first < sizeof(cjk_glyphs)/sizeof(cjk_glyphs[0]) && cjk_glyphs[first].codepoint == cp
        ? cjk_glyphs[first].rows : nullptr;
}
#endif
inline int advance(uint32_t cp) { return cp < 0x80 ? 6 : 10; }
inline int text_width(const char *s, int scale = 1) {
    if (!s || scale < 1 || scale > 4) return 0;
    int current = 0, width = 0;
    while (*s) {
        uint32_t cp = next_utf8(s);
        if (cp == '\n') { if (current > width) width = current; current = 0; }
        else current += advance(cp) * scale;
    }
    return current > width ? current : width;
}
// Pixel callback receives only x/y; callers capture their native color/context.
#if !defined(PXA_POCKET_SYSTEM_TEXT) && !defined(PXA_POCKET_STUDIO)
template<class Pixel>
inline void draw_text(int x, int y, const char *s, int scale, Pixel plot) {
    if (!s || scale < 1 || scale > 4) return;
    const int start = x;
    while (*s) {
        uint32_t cp = next_utf8(s);
        if (cp == '\n') { x = start; y += 10*scale; continue; }
        if (cp >= 'a' && cp <= 'z') cp -= 'a'-'A';
        const char *found = cp && cp < 0x80 ? strchr(ascii_names, int(cp)) : nullptr;
        const uint16_t *rows = cp >= 0x80 ? glyph(cp) : nullptr;
        int w = cp < 0x80 ? 5 : 10, h = cp < 0x80 ? 7 : 10;
        for (int row = 0; row < h; ++row) for (int col = 0; col < w; ++col) {
            bool ink = found ? (ascii_glyphs[found-ascii_names][row] & (16u >> col)) != 0 :
                rows ? (rows[row] & (512u >> col)) != 0 :
                cp != ' ' && (row == 0 || row == h-1 || col == 0 || col == w-1);
            if (ink) for (int sy = 0; sy < scale; ++sy) for (int sx = 0; sx < scale; ++sx)
                plot(x+col*scale+sx, y+row*scale+sy);
        }
        x += advance(cp)*scale;
    }
}
#endif
}
