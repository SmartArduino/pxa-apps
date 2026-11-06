// SPDX-License-Identifier: MIT
#pragma once
#include "../common/pocket-ui/i18n.hpp"
#include "font.hpp"
namespace dewfield_text {
inline const font::Glyph *glyph(uint32_t cp) {
  size_t lo = 0, hi = sizeof(font::glyphs) / sizeof(font::glyphs[0]);
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (font::glyphs[mid].cp < cp)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo < sizeof(font::glyphs) / sizeof(font::glyphs[0]) &&
                 font::glyphs[lo].cp == cp
             ? font::glyphs + lo
             : nullptr;
}
inline int advance(uint32_t cp) {
  const auto *g = glyph(cp);
  return g ? g->advance : 12;
}
inline int width(const char *value, int scale = 1) {
  if (!value || scale < 1 || scale > 4)
    return 0;
  int row = 0, best = 0;
  while (*value) {
    uint32_t cp = pocket_ui::next_utf8(value);
    if (cp == '\n') {
      if (row > best)
        best = row;
      row = 0;
    } else
      row += advance(cp) * scale;
  }
  return row > best ? row : best;
}
template <class Plot>
void draw(int x, int y, const char *value, int scale, Plot plot) {
  if (!value || scale < 1 || scale > 4)
    return;
  const int left = x;
  while (*value) {
    uint32_t cp = pocket_ui::next_utf8(value);
    if (cp == '\n') {
      x = left;
      y += 13 * scale;
      continue;
    }
    const auto *g = glyph(cp);
    for (int yy = 0; yy < 12; ++yy)
      for (int xx = 0; xx < 12; ++xx) {
        bool ink =
            g ? (g->rows[yy] & (1u << (11 - xx)))
              : cp != ' ' && (xx == 0 || xx == 10 || yy == 1 || yy == 10);
        if (ink)
          for (int sy = 0; sy < scale; ++sy)
            for (int sx = 0; sx < scale; ++sx)
              plot(x + xx * scale + sx, y + yy * scale + sy);
      }
    x += g ? g->advance * scale : 12 * scale;
  }
}
// Emit horizontal ink runs in native glyph coordinates. The original point
// renderer remains the reference used by font/layout regression tests.
template <class Plot> void spans(int x, int y, const char *value, Plot plot) {
  if (!value)
    return;
  const int left = x;
  while (*value) {
    uint32_t cp = pocket_ui::next_utf8(value);
    if (cp == '\n') {
      x = left;
      y += 13;
      continue;
    }
    const auto *g = glyph(cp);
    for (int yy = 0; yy < 12; ++yy) {
      uint32_t bits = g                     ? g->rows[yy]
                      : cp == ' '           ? 0
                      : yy == 1 || yy == 10 ? 0xfff
                                            : 0x802;
      while (bits) {
        int start = __builtin_clz(bits) - 20, end = start + 1;
        while (end < 12 && (bits & (1u << (11 - end))))
          ++end;
        plot(x + start, y + yy, end - start, 1);
        bits &= (1u << (12 - end)) - 1;
      }
    }
    x += g ? g->advance : 12;
  }
}
} // namespace dewfield_text
