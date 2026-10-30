// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include "items.hpp"
#include <cstdint>
namespace starsoil_terrain {
// Different opaque materials share one contour, so no transparency is
// introduced between soil, stone, planks or ores.
constexpr bool terrain(int t) {
  return (t >= starsoil::Dirt && t <= starsoil::Ice) ||
         (t >= starsoil::Plank && t <= starsoil::Moss);
}
struct Neighbors {
  uint8_t sides, diagonals;
};
template <class Sample> Neighbors neighbors(int x, int y, Sample sample) {
  return {uint8_t((terrain(sample(x, y - 1)) ? 1 : 0) |
                  (terrain(sample(x + 1, y)) ? 2 : 0) |
                  (terrain(sample(x, y + 1)) ? 4 : 0) |
                  (terrain(sample(x - 1, y)) ? 8 : 0)),
          uint8_t((terrain(sample(x - 1, y - 1)) ? 1 : 0) |
                  (terrain(sample(x + 1, y - 1)) ? 2 : 0) |
                  (terrain(sample(x - 1, y + 1)) ? 4 : 0) |
                  (terrain(sample(x + 1, y + 1)) ? 8 : 0))};
}
// Three solid quadrants around a vertex round the EMPTY quadrant by adding
// pixels. Cutting the opposite solid quadrant instead creates an isolated hole.
constexpr uint8_t fill_corners(uint8_t sides, uint8_t diagonals) {
  return uint8_t(((sides & 9) == 9 && (diagonals & 1) ? 1 : 0) |
                 ((sides & 3) == 3 && (diagonals & 2) ? 2 : 0) |
                 ((sides & 12) == 12 && (diagonals & 4) ? 4 : 0) |
                 ((sides & 6) == 6 && (diagonals & 8) ? 8 : 0));
}
inline uint16_t dim(uint16_t c, int light) {
  return uint16_t(((((c >> 11) & 31) * light / 255) << 11) |
                  ((((c >> 5) & 63) * light / 255) << 5) |
                  ((c & 31) * light / 255));
}
template <class Plot>
void draw(int x, int y, const uint16_t *data, int light, Plot plot) {
  for (int yy = 0; yy < 16; ++yy)
    for (int xx = 0; xx < 16; ++xx) {
      uint16_t c = data[yy * 16 + xx];
      if (c)
        plot(x + xx, y + yy, dim(c, light));
    }
}
// Both the game and composite topology tests call these production functions.
template <class Sample, class Plot>
bool draw_terrain(int tx, int ty, int sx, int sy, int light, Sample sample,
                  Plot plot) {
  int t = sample(tx, ty);
  if (!terrain(t))
    return false;
  Neighbors n = neighbors(tx, ty, sample);
  draw(sx, sy, art::tiles[t * 16 + n.sides], light, plot);
  return true;
}
template <class Sample, class Plot>
void draw_fillet(int tx, int ty, int sx, int sy, int light, Sample sample,
                 Plot plot) {
  if (terrain(sample(tx, ty)))
    return;
  Neighbors n = neighbors(tx, ty, sample);
  uint8_t corners = fill_corners(n.sides, n.diagonals);
  for (int i = 0; i < 4; ++i) {
    if (!(corners & (1 << i)))
      continue;
    int dx = i & 1 ? 1 : -1, dy = i & 2 ? 1 : -1;
    int t = sample(tx + dx, ty + dy);
    int px = dx > 0 ? 0 : 15, py = dy > 0 ? 0 : 15;
    // The diagonal solid tile necessarily has its two incident sides joined;
    // its shared vertex is opaque and provides the correct material color.
    Neighbors diagonal = neighbors(tx + dx, ty + dy, sample);
    uint16_t color = art::tiles[t * 16 + diagonal.sides][py * 16 + px];
    int x = dx > 0 ? 15 : 0, y = dy > 0 ? 15 : 0;
    plot(sx + x, sy + y, dim(color, light));
    plot(sx + x - dx, sy + y, dim(color, light));
    plot(sx + x, sy + y - dy, dim(color, light));
  }
}
} // namespace starsoil_terrain
