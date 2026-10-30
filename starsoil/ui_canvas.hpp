// SPDX-License-Identifier: MIT
#pragma once
#include "text.hpp"
#include <algorithm>
#include <cstdint>

namespace starsoil_ui {
// UI primitives retain the layout's coordinates. Fonts are painted directly
// into the output surface, with one integral square size for every font pixel.
// Later primitives paint over earlier text normally; there is no text overlay
// queue that could leak through a modal or notification.
struct Canvas {
  uint16_t *pixels = nullptr;
  int width = 288, height = 224, layout_width = 288, layout_height = 224;
  static int ceil_div(int64_t n, int d) {
    return int(n >= 0 ? (n + d - 1) / d : n / d);
  }
  int x(int v) const { return ceil_div(int64_t(v) * width, layout_width); }
  int y(int v) const { return ceil_div(int64_t(v) * height, layout_height); }
  int font_scale(int requested = 1) const {
    return std::clamp((width * requested + layout_width / 2) / layout_width, 1,
                      16);
  }
  int text_width(const char *s, int requested = 1) const {
    return ceil_div(int64_t(starsoil_text::width(s)) * font_scale(requested) *
                        layout_width,
                    width);
  }
  int text_height(int requested = 1) const {
    return ceil_div(int64_t(12 * font_scale(requested)) * layout_height,
                    height);
  }
  int line_height() const {
    return ceil_div(int64_t(14 * font_scale()) * layout_height, height);
  }
  void fill(int px, int py, int w, int h, uint16_t c) const {
    if (!pixels || w <= 0 || h <= 0)
      return;
    const int left = std::max(0, px), top = std::max(0, py);
    const int right = std::min(width, px + w),
              bottom = std::min(height, py + h);
    if (right <= left || bottom <= top)
      return;
    for (int yy = top; yy < bottom; ++yy)
      std::fill(pixels + yy * width + left, pixels + yy * width + right, c);
  }
  void rect(int lx, int ly, int w, int h, uint16_t c) const {
    if (w > 0 && h > 0)
      fill(x(lx), y(ly), x(lx + w) - x(lx), y(ly + h) - y(ly), c);
  }
  void text(int lx, int ly, const char *s, uint16_t c,
            int requested = 1) const {
    // draw() emits native font pixels once. Every one becomes an exact k x k
    // block, including the glyph advances and multi-line baseline positions.
    int step = font_scale(requested);
    const int ox = x(lx), oy = y(ly);
    starsoil_text::draw(0, 0, s, 1, [&](int px, int py) {
      fill(ox + px * step, oy + py * step, step, step, c);
    });
  }
};
} // namespace starsoil_ui
