// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include "terrain.hpp"
#include "tree_art.hpp"
#include <algorithm>
namespace starsoil_tree {
struct Crown {
  int x = 0, top = 0, bottom = 0, leaf_top = 0, rows = 0;
  bool valid = false;
};
template <class Sample> Crown find(int x, int y, Sample sample) {
  using namespace starsoil;
  Crown best;
  int distance = 100;
  for (int dx = -4; dx <= 4; ++dx) {
    if (std::abs(dx) > distance)
      continue;
    for (int dy = -3; dy <= 5; ++dy) {
      int xx = x + dx, yy = y + dy;
      if (sample(xx, yy) != Log)
        continue;
      int top = yy, bottom = yy;
      for (int n = 0; n < 12 && sample(xx, top - 1) == Log; ++n)
        --top;
      for (int n = 0; n < 12 && sample(xx, bottom + 1) == Log; ++n)
        ++bottom;
      if (bottom - top < 3)
        continue;
      int leaves = 0, first = top;
      for (int oy = -3; oy <= 2; ++oy)
        for (int ox = -3; ox <= 3; ++ox)
          if (sample(xx + ox, top + oy) == Leaves) {
            ++leaves;
            first = std::min(first, top + oy);
          }
      if (leaves < 4)
        continue;
      // Bind the complete artwork to the trunk, so camera motion cannot alter
      // it.
      int rows = first <= top - 3 ? 6 : 5;
      best = {xx, top, bottom, top - (rows - 3), rows, true};
      distance = std::abs(dx);
      break;
    }
  }
  return best;
}
template <class Sample, class Plot>
bool draw_natural(int tx, int ty, int sx, int sy, int biome, int light,
                  Sample sample, Plot plot) {
  using namespace starsoil;
  int t = sample(tx, ty);
  if (t != Leaves && t != Log && t != Air)
    return false;
  Crown tree = find(tx, ty, sample);
  if (!tree.valid)
    return false;
  int variant = biome == Tundra ? 1 : biome == Jungle ? 2 : 0;
  int left = tree.x * 16 + 8 - tree_art::Anchor;
  int top = tree.leaf_top * 16, height = (tree.rows + 1) * 16;
  bool in_crown = tx * 16 + 15 >= left && tx * 16 < left + tree_art::Width &&
                  ty * 16 + 15 >= top && ty * 16 < top + height;
  if (t == Air) {
    if (!in_crown)
      return false;
    int dx = tx - tree.x, dy = ty - tree.top;
    // Decorative lobe tips may extend into the original empty outside corners.
    // A removed leaf cell inside the original crown remains visibly removed.
    bool original = dy >= -(tree.rows - 3) && dy <= 2 &&
                    dx * dx + dy * dy < (tree.rows == 6 ? 13 : 12);
    if (original)
      return false;
  }
  if (!in_crown && t != Log)
    return false;
  for (int yy = 0; yy < 16; ++yy)
    for (int xx = 0; xx < 16; ++xx) {
      int px = tx * 16 + xx - left, py = ty * 16 + yy - top;
      uint16_t c = 0;
      if (px >= 0 && px < tree_art::Width && py >= 0 && py < height)
        c = tree_art::oak[variant]
                         [(py * tree_art::Height / height) * tree_art::Width +
                          px];
      else if (t == Log) {
        int total = (tree.bottom - tree.top + 1) * 16,
            u = (ty - tree.top) * 16 + yy;
        int bend = int((uint32_t(tree.x) * 2654435761u >> 28) % 3) - 1;
        int center = 8 + bend * 8 * u * (total - u) / (total * total);
        int half = 3 + u / std::max(1, total / 2);
        if (u > total - 9)
          half += ((u - total + 9) / 3);
        if (std::abs(xx - center) <= half) {
          int tex = std::clamp(tree_art::Anchor + xx - center,
                               tree_art::Anchor - 4, tree_art::Anchor + 4);
          c = tree_art::oak[0][(tree_art::Height - 5) * tree_art::Width + tex];
          // Long broken bark strokes, without a repeated horizontal tile seam.
          if ((yy + ty * 16 + xx * 3) % 29 < 3)
            c = starsoil_terrain::dim(c, 205);
        }
      }
      if (c)
        plot(sx + xx, sy + yy, starsoil_terrain::dim(c, light));
    }
  return true;
}
template <class Sample>
const uint16_t *art_for(int t, int x, int y, int biome, Sample sample) {
  using namespace starsoil;
  if (t == Log) {
    int branches = (sample(x - 1, y) == Leaves ? 1 : 0) |
                   (sample(x + 1, y) == Leaves ? 2 : 0) |
                   (starsoil_terrain::terrain(sample(x, y + 1)) ? 4 : 0);
    return art::logs[branches];
  }
  if (t != Leaves)
    return nullptr;
  auto joins = [](int n) { return n == Leaves || n == Log; };
  int mask =
      (joins(sample(x, y - 1)) ? 1 : 0) | (joins(sample(x + 1, y)) ? 2 : 0) |
      (joins(sample(x, y + 1)) ? 4 : 0) | (joins(sample(x - 1, y)) ? 8 : 0);
  return biome == Tundra || biome == Jungle
             ? art::foliage[(biome == Tundra ? 0 : 16) + mask]
             : art::tiles[Leaves * 16 + mask];
}
} // namespace starsoil_tree
