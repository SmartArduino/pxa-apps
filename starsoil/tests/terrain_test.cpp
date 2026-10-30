// SPDX-License-Identifier: MIT
// Check composite coverage, not just per-tile opacity: transparent pockets in a
// solid tile must have a real orthogonal opening to an empty world cell.
#include "../terrain.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace starsoil;
namespace {
constexpr int Size = 80;
using Mask = std::array<uint8_t, Size * Size>;
void connected_to_cells(const Mask &painted, int pattern, bool solid) {
  Mask seen{};
  std::vector<int> queue;
  for (int y = -1; y <= 3; ++y)
    for (int x = -1; x <= 3; ++x) {
      bool occupied =
          x >= 0 && x < 3 && y >= 0 && y < 3 && (pattern & (1 << (y * 3 + x)));
      if (occupied != solid)
        continue;
      int at = ((y + 1) * 16 + 8) * Size + (x + 1) * 16 + 8;
      assert(bool(painted[at]) == solid);
      seen[at] = 1;
      queue.push_back(at);
    }
  for (size_t i = 0; i < queue.size(); ++i) {
    int x = queue[i] % Size, y = queue[i] / Size;
    for (auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1},
                          std::pair{0, -1}}) {
      int xx = x + dx, yy = y + dy;
      if (xx < 0 || yy < 0 || xx >= Size || yy >= Size)
        continue;
      int next = yy * Size + xx;
      if (!seen[next] && bool(painted[next]) == solid) {
        seen[next] = 1;
        queue.push_back(next);
      }
    }
  }
  for (int i = 0; i < Size * Size; ++i)
    if (bool(painted[i]) == solid && !seen[i]) {
      std::fprintf(stderr, "FAIL pattern=%03x pixel=%d,%d: %s\n", pattern,
                   i % Size, i / Size,
                   solid ? "floating fill" : "isolated transparent pocket");
      assert(false);
    }
}
} // namespace
int main() {
  const int materials[] = {Grass, Dirt, Stone, CopperOre, IronOre,
                           Snow,  Ice,  Plank, Moss};
  for (int mode = 0; mode < 4; ++mode)
    for (int pattern = 0; pattern < 512; ++pattern) {
      Mask painted{};
      auto sample = [&](int x, int y) {
        if (x < 0 || x >= 3 || y < 0 || y >= 3 ||
            !(pattern & (1 << (y * 3 + x))))
          return int(Air);
        return mode ? int(Leaves) : materials[y * 3 + x];
      };
      auto plot = [&](int x, int y, uint16_t color) {
        assert(x >= 0 && x < Size && y >= 0 && y < Size);
        assert(color);
        painted[y * Size + x] = 1;
      };
      for (int y = -1; y <= 3; ++y)
        for (int x = -1; x <= 3; ++x) {
          int sx = (x + 1) * 16, sy = (y + 1) * 16;
          if (!starsoil_terrain::draw_terrain(x, y, sx, sy, 255, sample,
                                              plot) &&
              sample(x, y) == Leaves) {
            int sides = (sample(x, y - 1) == Leaves ? 1 : 0) |
                        (sample(x + 1, y) == Leaves ? 2 : 0) |
                        (sample(x, y + 1) == Leaves ? 4 : 0) |
                        (sample(x - 1, y) == Leaves ? 8 : 0);
            auto data = mode == 1 ? art::tiles[Leaves * 16 + sides]
                                  : art::foliage[(mode == 2 ? 0 : 16) + sides];
            starsoil_terrain::draw(sx, sy, data, 255, plot);
          }
          starsoil_terrain::draw_fillet(x, y, sx, sy, 255, sample, plot);
        }
      connected_to_cells(painted, pattern, false);
      connected_to_cells(painted, pattern, true);
      // A fully occupied 2x2 vertex must remain filled even with other
      // neighbors absent. This catches both material-seam holes and four-leaf
      // corner holes.
      for (int y = 1; y < 3; ++y)
        for (int x = 1; x < 3; ++x)
          if (sample(x - 1, y - 1) && sample(x, y - 1) && sample(x - 1, y) &&
              sample(x, y))
            for (int yy = y * 16 + 15; yy <= y * 16 + 16; ++yy)
              for (int xx = x * 16 + 15; xx <= x * 16 + 16; ++xx)
                assert(painted[yy * Size + xx]);
    }
  // The L join fills its empty quadrant instead of cutting the opposite block.
  assert(starsoil_terrain::fill_corners(3, 8) == 0);
  assert(starsoil_terrain::fill_corners(6, 8) == 8);
  std::puts("PASS 2048 composite neighborhoods: mixed terrain and three "
            "foliage palettes; no isolated transparent holes, floating fills "
            "or shared-vertex gaps");
}
