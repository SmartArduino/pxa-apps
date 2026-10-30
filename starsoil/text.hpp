// SPDX-License-Identifier: MIT
#pragma once
#include "../common/pocket-ui/i18n.hpp"
#include "font.hpp"
namespace starsoil_text {
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
} // namespace starsoil_text
