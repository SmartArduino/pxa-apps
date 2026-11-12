// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <stdint.h>

namespace overlay_raster {
struct Surface {
  uint16_t *pixels;
  int width, height, top, bottom;
};
inline uint16_t blend(uint16_t a, uint16_t b, unsigned n) {
  return uint16_t(
      ((((a >> 11) * (256 - n) + (b >> 11) * n) >> 8) << 11) |
      (((((a >> 5) & 63) * (256 - n) + ((b >> 5) & 63) * n) >> 8) << 5) |
      (((a & 31) * (256 - n) + (b & 31) * n) >> 8));
}
inline void mask(Surface s, int x, int y, int w, int h, const uint8_t *alpha,
                 int sw, int sh, uint16_t color, int opacity) {
  if (!s.pixels || !alpha || w <= 0 || h <= 0 || sw <= 0 || sh <= 0)
    return;
  unsigned strength = std::clamp(opacity, 0, 256);
  int left = std::max(0, x), right = std::min(s.width, x + w),
      top = std::max({0, s.top, y}),
      bottom = std::min({s.height, s.bottom, y + h});
  for (int yy = top; yy < bottom; ++yy) {
    int row = (yy - y) * sh / h * sw;
    for (int xx = left; xx < right; ++xx) {
      unsigned a = alpha[row + (xx - x) * sw / w] * strength / 255;
      if (a) {
        auto &pixel = s.pixels[yy * s.width + xx];
        pixel = a == 256 ? color : blend(pixel, color, a);
      }
    }
  }
}
inline void stroke(Surface s, int x, int y, int x2, int y2, int thickness,
                   uint16_t color) {
  if (!s.pixels || thickness <= 0)
    return;
  int dx = x2 > x ? x2 - x : x - x2, sx = x < x2 ? 1 : -1,
      dy = y2 > y ? y - y2 : y2 - y, sy = y < y2 ? 1 : -1, error = dx + dy;
  for (;;) {
    int left = std::max(0, x - thickness / 2),
        right = std::min(s.width, x - thickness / 2 + thickness),
        top = std::max({0, s.top, y - thickness / 2}),
        bottom = std::min({s.height, s.bottom, y - thickness / 2 + thickness});
    if (left < right)
      for (int yy = top; yy < bottom; ++yy)
        std::fill(s.pixels + yy * s.width + left,
                  s.pixels + yy * s.width + right, color);
    if (x == x2 && y == y2)
      break;
    int twice = error * 2;
    if (twice >= dy) {
      error += dy;
      x += sx;
    }
    if (twice <= dx) {
      error += dx;
      y += sy;
    }
  }
}
} // namespace overlay_raster
