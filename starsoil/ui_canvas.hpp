// SPDX-License-Identifier: MIT
#pragma once
#include "color_math.hpp"
#include "pixel_fill.hpp"
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
  int origin_x = 0, origin_y = 0, region_width = 0, region_height = 0;
  int draw_width() const { return region_width ? region_width : width; }
  int draw_height() const { return region_height ? region_height : height; }
  static int ceil_div(int64_t n, int d) {
    return int(n >= 0 ? (n + d - 1) / d : n / d);
  }
  static int coordinate(int value, int extent, int layout) {
    if (value >= -4096 && value <= 4096 && extent > 0 && extent <= 480 && layout > 0 && layout <= 480) {
      int n = value * extent;
      return n >= 0 ? (n + layout - 1) / layout : n / layout;
    }
    return ceil_div(int64_t(value) * extent, layout);
  }
  int x(int v) const {
    return origin_x + coordinate(v, draw_width(), layout_width);
  }
  int y(int v) const {
    return origin_y + coordinate(v, draw_height(), layout_height);
  }
  int font_scale(int requested = 1) const {
    return std::clamp(
        (draw_width() * requested + layout_width / 2) / layout_width, 1, 16);
  }
  int text_width(const char *s, int requested = 1) const {
    return ceil_div(int64_t(starsoil_text::width(s)) * font_scale(requested) *
                        layout_width,
                    draw_width());
  }
  int text_height(int requested = 1) const {
    return ceil_div(int64_t(12 * font_scale(requested)) * layout_height,
                    draw_height());
  }
  int line_height() const {
    return ceil_div(int64_t(14 * font_scale()) * layout_height, draw_height());
  }
  void fill(int px, int py, int w, int h, uint16_t c) const {
    if (!pixels || w <= 0 || h <= 0)
      return;
    const int left = std::max(origin_x, px), top = std::max(origin_y, py);
    const int right = std::min(origin_x + draw_width(), px + w),
              bottom = std::min(origin_y + draw_height(), py + h);
    if (right <= left || bottom <= top)
      return;
    starsoil_pixels::rectangle(pixels, width, left, top, right, bottom, c);
  }
  void rect(int lx, int ly, int w, int h, uint16_t c) const {
    if (w > 0 && h > 0)
      fill(x(lx), y(ly), x(lx + w) - x(lx), y(ly + h) - y(ly), c);
  }
  void blend_rect(int lx, int ly, int w, int h, uint16_t c, unsigned alpha) const {
    if (!pixels || w<=0 || h<=0) return;
    int left=std::max(origin_x,x(lx)),right=std::min(origin_x+draw_width(),x(lx+w));
    int top=std::max(origin_y,y(ly)),bottom=std::min(origin_y+draw_height(),y(ly+h));
    for(int yy=top;yy<bottom;++yy)for(int xx=left;xx<right;++xx)
      pixels[yy*width+xx]=starsoil_color::mix(pixels[yy*width+xx],c,std::min(255u,alpha));
  }
  bool sprite(int lx, int ly, const uint16_t *source, int w, int h, bool flip,
              int light, int clip_top, int clip_bottom) const {
    if (w < 1 || h < 1 || w > 64 || h > 64)
      return false;
    // Map every shared pixel edge once, retaining rect()'s exact rounding.
    int columns[65], rows[65];
    for (int i = 0; i <= w; ++i)
      columns[i] = x(lx + i);
    for (int i = 0; i <= h; ++i)
      rows[i] = y(ly + i);
    for (int yy = 0; yy < h; ++yy) {
      if (ly + yy < clip_top || ly + yy >= clip_bottom)
        continue;
      for (int xx = 0; xx < w; ++xx) {
        uint16_t c = source[yy * w + (flip ? w - 1 - xx : xx)];
        if (!c)
          continue;
        if (light != 255)
          c = starsoil_color::dim(c, unsigned(light));
        fill(columns[xx], rows[yy], columns[xx + 1] - columns[xx],
             rows[yy + 1] - rows[yy], c);
      }
    }
    return true;
  }
  void text(int lx, int ly, const char *s, uint16_t c,
            int requested = 1) const {
    // draw() emits native font pixels once. Every one becomes an exact k x k
    // block, including the glyph advances and multi-line baseline positions.
    int step = font_scale(requested);
    const int ox = x(lx), oy = y(ly);
    starsoil_text::spans(0, 0, s, [&](int px, int py, int w, int h) {
      fill(ox + px * step, oy + py * step, w * step, h * step, c);
    });
  }
};
} // namespace starsoil_ui
