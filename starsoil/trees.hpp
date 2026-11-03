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
template <class Sample> Crown inspect(int xx, int yy, Sample sample) {
  using namespace starsoil;
  if (sample(xx, yy) != Log) return {};
  int top = yy, bottom = yy;
  for (int n = 0; n < 12 && sample(xx, top - 1) == Log; ++n) --top;
  for (int n = 0; n < 12 && sample(xx, bottom + 1) == Log; ++n) ++bottom;
  if (bottom - top < 3) return {};
  int leaves = 0, first = top;
  for (int oy = -3; oy <= 2; ++oy)
    for (int ox = -3; ox <= 3; ++ox)
      if (sample(xx + ox, top + oy) == Leaves) {
        ++leaves; first = std::min(first, top + oy);
      }
  if (leaves < 4) return {};
  int rows = first <= top - 3 ? 6 : 5;
  return {xx, top, bottom, top - (rows - 3), rows, true};
}
template <class Sample> Crown find(int x, int y, Sample sample) {
  using namespace starsoil;
  Crown best;
  int distance = 100;
  for (int dx = -4; dx <= 4; ++dx) {
    if (std::abs(dx) > distance)
      continue;
    for (int dy = -3; dy <= 5; ++dy) {
      int xx = x + dx, yy = y + dy;
      Crown candidate = inspect(xx, yy, sample);
      if (!candidate.valid) continue;
      // Bind the complete artwork to the trunk, so camera motion cannot alter
      // it.
      best = candidate;
      distance = std::abs(dx);
      break;
    }
  }
  return best;
}
// Index the visible neighborhood once. A row bitset finds the first eligible
// trunk in each column while preserving find()'s distance and tie ordering.
// Bounded storage; unusually dense player-built forests use the exact fallback.
class CrownCache {
  static constexpr int Capacity = 42, Records = 256;
  uint64_t rows[Capacity]{};
  uint16_t ids[Capacity * Capacity]{};
  Crown crowns[Records]{};
  int left = 0, top = 0, width = 0, height = 0;
public:
  template <class Sample>
  void prepare(int x, int y, int w, int h, Sample sample) {
    left = x - 4; top = y - 3;
    width = w > 0 && w + 8 <= Capacity ? w + 8 : 0;
    height = h > 0 && h + 8 <= Capacity ? h + 8 : 0;
    std::fill(rows, rows + Capacity, uint64_t(0));
    if (!width || !height) return;
    int count = 0;
    for (int yy = 0; yy < height; ++yy)
      for (int xx = 0; xx < width; ++xx) {
        if (sample(left + xx, top + yy) != starsoil::Log) continue;
        Crown c = inspect(left + xx, top + yy, sample);
        if (!c.valid) continue;
        rows[xx] |= uint64_t(1) << yy;
        uint16_t id = UINT16_MAX;
        if (count < Records) { crowns[count] = c; id = uint16_t(++count); }
        ids[yy * width + xx] = id;
      }
  }
  template <class Sample> Crown find(int x, int y, Sample sample) const {
    if (x - 4 < left || x + 4 >= left + width ||
        y - 3 < top || y + 5 >= top + height)
      return starsoil_tree::find(x, y, sample);
    uint64_t mask = ((uint64_t(1) << (y + 6 - top)) - 1) &
                    ~((uint64_t(1) << (y - 3 - top)) - 1);
    Crown best;
    int distance = 100;
    for (int dx = -4; dx <= 4; ++dx) {
      if (std::abs(dx) > distance) continue;
      int col = x + dx - left;
      uint64_t candidates = rows[col] & mask;
      if (!candidates) continue;
      int row = __builtin_ctzll(candidates);
      uint16_t id = ids[row * width + col];
      best = id == UINT16_MAX ? inspect(x + dx, top + row, sample) : crowns[id - 1];
      distance = std::abs(dx);
    }
    return best;
  }
};
template <class Sample, class Plot>
bool draw_natural(int tx, int ty, int sx, int sy, int biome, int light,
                  Sample sample, Plot plot, const Crown *cached = nullptr) {
  using namespace starsoil;
  int t = sample(tx, ty);
  if (t != Leaves && t != Log && t != Air)
    return false;
  Crown tree = cached ? *cached : find(tx, ty, sample);
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
  const int total = (tree.bottom - tree.top + 1) * 16;
  const int bend = int((uint32_t(tree.x) * 2654435761u >> 28) % 3) - 1;
  for (int yy = 0; yy < 16; ++yy) {
    // Crown resampling and the curved trunk depend on the row, not on each
    // pixel. In particular, avoid repeating S3's software divisions 16 times.
    const int py = ty * 16 + yy - top;
    const bool crown_row = py >= 0 && py < height;
    const uint16_t *source = crown_row
        ? tree_art::oak[variant] + (py * tree_art::Height / height) * tree_art::Width
        : nullptr;
    int center = 0, half = 0;
    if (t == Log) {
      const int u = (ty - tree.top) * 16 + yy;
      center = 8 + bend * 8 * u * (total - u) / (total * total);
      half = 3 + u / std::max(1, total / 2);
      if (u > total - 9)
        half += (u - total + 9) / 3;
    }
    for (int xx = 0; xx < 16; ++xx) {
      int px = tx * 16 + xx - left;
      uint16_t c = 0;
      if (px >= 0 && px < tree_art::Width && crown_row)
        c = source[px];
      else if (t == Log) {
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
