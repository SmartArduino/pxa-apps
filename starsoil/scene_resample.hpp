// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>
namespace starsoil_scene {
// Reuse exact nearest-neighbor coordinates across rows and write adjacent
// output pixels in groups. Exact-width rows can be copied whole. No filtering
// or interpolation is added.
class Resampler {
  static constexpr int Capacity = 480;
  uint16_t columns[Capacity], rows[Capacity];
  int source_width = 0, source_height = 0, width = 0, height = 0;
  void prepare(int sw, int sh, int w, int h) {
    source_width = sw; source_height = sh; width = w; height = h;
    for (int x = 0; x < w; ++x) columns[x] = uint16_t(x * sw / w);
    for (int y = 0; y < h; ++y) rows[y] = uint16_t(y * sh / h);
  }
public:
  void draw(const uint16_t *source, int sw, int sh,
            uint16_t *destination, int w, int h) {
    if (sw < 1 || sh < 1 || w < 1 || h < 1 || sw > Capacity ||
        sh > Capacity || w > Capacity || h > Capacity) return;
    if (sw == w && sh == h) {
      __builtin_memcpy(destination, source, unsigned(w * h) * sizeof(uint16_t));
      return;
    }
    if (sw != source_width || sh != source_height || w != width || h != height)
      prepare(sw, sh, w, h);
    int previous = -1;
    for (int y = 0; y < h; ++y) {
      const int sy = rows[y];
      auto *out = destination + y * w;
      if (sy == previous) {
        __builtin_memcpy(out, out - w, unsigned(w) * sizeof(uint16_t));
      } else {
        const auto *row = source + sy * sw;
        if (sw == w) {
          __builtin_memcpy(out, row, unsigned(w) * sizeof(uint16_t));
        } else {
          int x = 0;
          for (; x + 3 < w; x += 4) {
            const uint16_t quad[] = {row[columns[x]], row[columns[x + 1]],
                                     row[columns[x + 2]], row[columns[x + 3]]};
            __builtin_memcpy(out + x, quad, sizeof(quad));
          }
          for (; x + 1 < w; x += 2) {
            // A fixed-size builtin becomes one unaligned-safe 32-bit write.
            // The two native uint16_t values retain their exact byte order.
            const uint16_t pair[] = {row[columns[x]], row[columns[x + 1]]};
            __builtin_memcpy(out + x, pair, sizeof(pair));
          }
          if (x < w) out[x] = row[columns[x]];
        }
      }
      previous = sy;
    }
  }
};
// The renderer calls this on its single application thread. Source and output
// occupy separate regions of the fixed render arena, including after a resize.
// Keep the coordinate maps between frames; rebuild only when geometry changes.
inline void resample(const uint16_t *source, int sw, int sh,
                     uint16_t *destination, int w, int h) {
  static Resampler sampler;
  sampler.draw(source, sw, sh, destination, w, h);
}
} // namespace starsoil_scene
