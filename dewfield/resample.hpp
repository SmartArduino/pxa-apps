// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>
namespace dewfield_raster {
// Cover the viewport with one uniform scale. Pixel-centre sampling crops the
// surplus around the source centre; neither axis is stretched independently.
inline void cover(const uint16_t *source, int sw, int sh, uint16_t *destination,
                  int w, int h) {
  if (sw < 1 || sh < 1 || sw > 480 || sh > 480 || w < 1 || h < 1 || w > 480 ||
      h > 480)
    return;
  bool by_width = w * sh >= h * sw;
  int numerator = by_width ? sw : sh, denominator = by_width ? w : h;
  int crop_x = sw * denominator - w * numerator,
      crop_y = sh * denominator - h * numerator;
  uint16_t columns[480];
  for (int x = 0; x < w; ++x)
    columns[x] =
        uint16_t(((2 * x + 1) * numerator + crop_x) / (2 * denominator));
  int previous = -1;
  for (int y = 0; y < h; ++y) {
    int sy = ((2 * y + 1) * numerator + crop_y) / (2 * denominator);
    auto *out = destination + y * w;
    if (sy == previous)
      std::memcpy(out, out - w, unsigned(w) * sizeof(uint16_t));
    else {
      const auto *row = source + sy * sw;
      int x = 0;
      for (; x + 1 < w; x += 2) {
        const uint16_t pair[] = {row[columns[x]], row[columns[x + 1]]};
        std::memcpy(out + x, pair, sizeof(pair));
      }
      if (x < w)
        out[x] = row[columns[x]];
    }
    previous = sy;
  }
}
// Exact nearest-neighbour expansion. Reuse repeated rows and write pairs of
// native pixels together; the 480px display never needs per-pixel division.
inline void resample(const uint16_t *source, int sw, int sh,
                     uint16_t *destination, int w, int h) {
  if (sw < 1 || sh < 1 || w < 1 || h < 1 || w > 480 || h > 480)
    return;
  if (sw == w && sh == h) {
    std::memcpy(destination, source, unsigned(w * h) * sizeof(uint16_t));
    return;
  }
  if (w == sw * 2 && h == sh * 2) {
    for (int y = 0; y < sh; ++y) {
      const auto *row = source + y * sw;
      auto *out = destination + y * 2 * w;
      int x = 0;
      for (; x + 1 < sw; x += 2) {
        const uint16_t quad[] = {row[x], row[x], row[x + 1], row[x + 1]};
        std::memcpy(out + x * 2, quad, sizeof(quad));
      }
      if (x < sw) {
        out[x * 2] = row[x];
        out[x * 2 + 1] = row[x];
      }
      std::memcpy(out + w, out, unsigned(w) * sizeof(uint16_t));
    }
    return;
  }
  uint16_t columns[480];
  for (int x = 0; x < w; ++x)
    columns[x] = uint16_t(x * sw / w);
  int previous = -1;
  for (int y = 0; y < h; ++y) {
    const int sy = y * sh / h;
    auto *out = destination + y * w;
    if (sy == previous)
      std::memcpy(out, out - w, unsigned(w) * sizeof(uint16_t));
    else {
      const auto *row = source + sy * sw;
      int x = 0;
      for (; x + 1 < w; x += 2) {
        const uint16_t pair[] = {row[columns[x]], row[columns[x + 1]]};
        std::memcpy(out + x, pair, sizeof(pair));
      }
      if (x < w)
        out[x] = row[columns[x]];
    }
    previous = sy;
  }
}
} // namespace dewfield_raster
