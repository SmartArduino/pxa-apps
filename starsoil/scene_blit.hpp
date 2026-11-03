// SPDX-License-Identifier: MIT
#pragma once
#include "color_math.hpp"
#include <algorithm>
#include <cstdint>
namespace starsoil_scene {
// Clip once per sprite, then write complete rows without a call per pixel.
inline void blit(uint16_t *dst, int width, int height, int x, int y,
                 const uint16_t *src, int sw, int sh, bool flip, int light,
                 int clip_top, int clip_bottom) {
  int left = std::max(0, -x), right = std::min(sw, width - x);
  int top = std::max({0, -y, clip_top - y}),
      bottom = std::min({sh, height - y, clip_bottom - y});
  if (left >= right || top >= bottom)
    return;
  for (int yy = top; yy < bottom; ++yy) {
    auto *row = dst + (y + yy) * width + x + left;
    const auto *source = src + yy * sw;
    for (int xx = left; xx < right; ++xx) {
      uint16_t c = source[flip ? sw - 1 - xx : xx];
      if (!c)
        continue;
      if (light != 255)
        c = starsoil_color::dim(c, unsigned(light));
      row[xx - left] = c;
    }
  }
}
} // namespace starsoil_scene
