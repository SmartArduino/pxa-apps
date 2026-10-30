// SPDX-License-Identifier: MIT
// Topology of the actual foreground tree sprites and background geometry.
#include "../background.hpp"
#include "../trees.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace starsoil;
namespace {
constexpr int W = 160, H = 256;
std::array<uint16_t, W * H> pixels;
void plot(int x, int y, uint16_t c) {
  assert(x >= 0 && x < W && y >= 0 && y < H);
  pixels[y * W + x] = c;
}
void line(int x, int y, int xx, int yy, uint16_t c) {
  int dx = std::abs(xx - x), sx = x < xx ? 1 : -1, dy = -std::abs(yy - y),
      sy = y < yy ? 1 : -1, e = dx + dy;
  for (;;) {
    plot(x, y, c);
    if (x == xx && y == yy)
      break;
    int ee = 2 * e;
    if (ee >= dy) {
      e += dy;
      x += sx;
    }
    if (ee <= dx) {
      e += dx;
      y += sy;
    }
  }
}
void connected(int x, int y) {
  std::array<bool, W * H> seen{};
  std::vector<int> q{y * W + x};
  assert(pixels[q[0]]);
  seen[q[0]] = true;
  for (size_t i = 0; i < q.size(); ++i)
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int xx = q[i] % W + dx, yy = q[i] / W + dy;
        if (xx < 0 || xx >= W || yy < 0 || yy >= H)
          continue;
        int n = yy * W + xx;
        if (pixels[n] && !seen[n]) {
          seen[n] = true;
          q.push_back(n);
        }
      }
  for (int i = 0; i < W * H; ++i)
    assert(!pixels[i] || seen[i]);
}
} // namespace
int main() {
  for (int mask = 0; mask < 8; ++mask) {
    pixels.fill(0);
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x)
        if (art::logs[mask][y * 16 + x])
          plot(x, y, art::logs[mask][y * 16 + x]);
    connected(8, 15);
    if (mask & 1)
      assert(pixels[5 * W]);
    if (mask & 2)
      assert(pixels[4 * W + 15]);
    if (mask & 4)
      assert(pixels[15 * W + 2] && pixels[15 * W + 14]);
  }
  for (int h = 27; h <= 63; ++h) {
    pixels.fill(0);
    starsoil_scene::broadleaf(
        64, 144, h, 1, 2, 3,
        [](int x, int y, int w, int h, uint16_t c) {
          for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
              plot(xx, yy, c);
        },
        line);
    for (int y = 144 - h; y < 144; ++y)
      assert(pixels[y * W + 65]);
    connected(65, 143);
  }
  for (int biome : {int(Meadow), int(Tundra), int(Jungle)})
    for (int height : {5, 7, 9}) {
      std::array<int, 8 * 10> tiles{};
      auto at = [&](int x, int y) {
        return x < 0 || x >= 8 || y < 0 || y >= 10 ? int(Air)
                                                   : tiles[y * 8 + x];
      };
      for (int y = 9 - height; y < 9; ++y)
        tiles[y * 8 + 4] = Log;
      for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -3; dx <= 3; ++dx) {
          int x = 4 + dx, y = 10 - height + dy;
          if (x >= 0 && x < 8 && y >= 0 && y < 9 && dx * dx + dy * dy < 12 &&
              !at(x, y))
            tiles[y * 8 + x] = Leaves;
        }
      pixels.fill(0);
      for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 8; ++x) {
          auto art = starsoil_tree::art_for(at(x, y), x, y, biome, at);
          if (!art)
            continue;
          for (int yy = 0; yy < 16; ++yy)
            for (int xx = 0; xx < 16; ++xx)
              if (art[yy * 16 + xx])
                plot(x * 16 + xx, y * 16 + yy, art[yy * 16 + xx]);
        }
      connected(4 * 16 + 8, 8 * 16 + 8);
      // Both forks must cross the transparent gutter beside the narrow trunk.
      for (int y = 0; y < 9; ++y)
        if (at(4, y) == Log) {
          if (at(3, y) == Leaves)
            assert(pixels[(y * 16 + 5) * W + 4 * 16]);
          if (at(5, y) == Leaves)
            assert(pixels[(y * 16 + 4) * W + 4 * 16 + 15]);
        }
    }
  for (int biome : {int(Forest), int(Tundra), int(Jungle)})
    for (int height : {5, 7, 9}) {
      std::array<int, 10 * 16> grid{};
      for (int y = 4; y < 4 + height; ++y)
        grid[y * 10 + 5] = Log;
      for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -3; dx <= 3; ++dx)
          if (dx * dx + dy * dy < 12 && !grid[(4 + dy) * 10 + 5 + dx])
            grid[(4 + dy) * 10 + 5 + dx] = Leaves;
      auto sample = [&](int x, int y) {
        return x < 0 || x >= 10 || y < 0 || y >= 16 ? int(Air)
                                                    : grid[y * 10 + x];
      };
      auto render = [&] {
        pixels.fill(0);
        for (int y = 0; y < 16; ++y)
          for (int x = 0; x < 10; ++x)
            starsoil_tree::draw_natural(x, y, x * 16, y * 16, biome, 255,
                                        sample, plot);
      };
      render();
      connected(5 * 16 + 8, (3 + height) * 16 + 8);
      auto before = pixels;
      grid[3 * 10 + 3] = Air;
      render();
      int removed = 0;
      for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
          if (x >= 3 * 16 && x < 4 * 16 && y >= 3 * 16 && y < 4 * 16) {
            removed += before[y * W + x] != 0;
            assert(!pixels[y * W + x]);
          } else
            assert(pixels[y * W + x] == before[y * W + x]);
        }
      assert(removed > 16);
      grid.fill(Air);
      render();
      for (auto c : pixels)
        assert(!c);
    }
  std::puts("PASS 9 authored natural trees: connected crowns and stems; mining "
            "removes only that leaf cell; felling removes all canopy pixels");
  std::puts(
      "PASS 8 fork/root variants, 37 background heights and 9 foreground "
      "trees: continuous trunks and joined crowns across all foliage palettes");
}
