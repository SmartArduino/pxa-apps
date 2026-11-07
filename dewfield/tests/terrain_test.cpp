// SPDX-License-Identifier: MIT
#include "../terrain.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <queue>
#include <set>
#include <vector>
using namespace terrain;
static unsigned rotate(unsigned bits) {
  unsigned result = 0;
  for (int i = 0; i < 4; ++i)
    if (bits & (1u << i))
      result |= 1u << ((i + 1) % 4);
  for (int i = 4; i < 8; ++i)
    if (bits & (1u << i))
      result |= 1u << (4 + (i - 3) % 4);
  return result;
}
static void joined(const sprites::Sprite &s) {
  bool mask[64 * 64]{};
  int total = 0;
  for (int i = 0; i < s.count; ++i) {
    auto &r = s.spans[i];
    assert(r.x + r.count <= s.w && r.y < s.h);
    for (int x = r.x; x < r.x + r.count; ++x) {
      mask[r.y * 64 + x] = true;
      ++total;
    }
  }
  int largest = 0;
  for (int y = 0; y < s.h; ++y)
    for (int x = 0; x < s.w; ++x) {
      if (!mask[y * 64 + x])
        continue;
      std::queue<int> q;
      q.push(y * 64 + x);
      mask[y * 64 + x] = false;
      int size = 0;
      while (!q.empty()) {
        int at = q.front();
        q.pop();
        ++size;
        for (int d : {-64, 1, 64, -1}) {
          int n = at + d;
          if (n < 0 || n >= 64 * 64 ||
              std::abs(n % 64 - at % 64) + std::abs(n / 64 - at / 64) != 1 ||
              !mask[n])
            continue;
          mask[n] = false;
          q.push(n);
        }
      }
      largest = std::max(size, largest);
    }
  assert(total > 200 && largest * 100 >= total * 96);
}
static void clustered_materials() {
  static Map map;
  constexpr int Side = 64;
  uint16_t patch[Side * Side];
  for (Kind kind : {Grass, Dirt, Path}) {
    std::fill(std::begin(map.kinds), std::end(map.kinds), uint8_t(kind));
    map.rasterize();
    const auto base = kind == Grass  ? farm_palette::Grass
                      : kind == Dirt ? farm_palette::Dirt
                                     : farm_palette::Path;
    std::set<uint16_t> palette;
    int textured = 0, crossings = 0;
    for (int y = 0; y < Side; ++y)
      for (int x = 0; x < Side; ++x) {
        auto c = map.pixel(48 + x, 48 + y, 0);
        patch[y * Side + x] = c;
        palette.insert(c);
        textured += c != base;
      }
    assert(palette.size() >= 3 && palette.size() <= 5);
    assert(textured > Side * Side / 25 && textured < Side * Side / 3);
    for (int y = 1; y < Side - 1; ++y)
      for (int x = 1; x < Side - 1; ++x) {
        int at = y * Side + x;
        if (patch[at] == base)
          continue;
        // Material accents form connected pixel clusters, not isolated noise.
        bool joined = false;
        for (int d : {-Side, Side, -1, 1})
          joined |= patch[at + d] != base;
        assert(joined);
        if ((48 + x) % 16 == 15 && patch[at] == patch[at + 1])
          ++crossings;
      }
    assert(crossings > 2); // Clusters continue over actual tile boundaries.
  }
}
int main() {
  clustered_materials();
  // The full neighborhood has no holes. All 256 masks rotate symmetrically.
  for (int radius : {4, 5, 8})
    for (unsigned n = 0; n < 256; ++n)
      for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
          assert(rounded(n, x, y, radius) ==
                 rounded(rotate(n), 15 - y, x, radius));
          assert(infill(n, x, y, radius) ==
                 infill(rotate(n), 15 - y, x, radius));
        }
  assert(!rounded(0, 0, 0) && rounded(0, 8, 8));
  assert(rounded(N | W, 0, 0));
  assert(!infill(N | W, 0, 0) && infill(N | W | NW, 0, 0));
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x)
      assert(rounded(255, x, y));
  farm::State s;
  farm::fresh(s);
  static Map a, b;
  a.build(s);
  for (int y = 10; y < 16; ++y)
    for (int x = 10; x < 20; ++x)
      for (int yy = 1; yy < 15; ++yy) {
        assert(a.earth(x * 16, y * 16 + yy));
        assert(a.earth(x * 16 + 15, y * 16 + yy));
      }
  // Different earth materials join without a grass line.
  static Map joined_earth;
  for(int y=9;y<17;++y)for(int x=9;x<20;++x)
    joined_earth.kinds[y*farm::MapW+x]=x==9?Path:Dirt;
  joined_earth.rasterize();
  for (int y = 9; y < 17; ++y)
    for (int yy = 4; yy < 12; ++yy) {
      assert(joined_earth.material(159, y * 16 + yy) == Path);
      assert(joined_earth.material(160, y * 16 + yy) == Dirt);
    }
  for (int y = 16; y < 20; ++y)
    for (int x = 25; x < 29; ++x)
      for (int xx = 0; xx < 16; ++xx)
        assert(a.material(x * 16 + xx, y * 16) == Pond);
  // Cleared objects never change the cached terrain material underneath.
  std::memset(s.cleared, 1, sizeof(s.cleared));
  b.build(s);
  assert(!std::memcmp(a.kinds, b.kinds, sizeof(a.kinds)));
  assert(!std::memcmp(a.materials, b.materials, sizeof(a.materials)));
  // The missing quadrant of an L junction receives a fillet; the occupied
  // quadrant stays filled, so shores cannot develop inward V-shaped holes.
  static Map corner;
  corner.kinds[10 * farm::MapW + 11] = Pond;
  corner.kinds[11 * farm::MapW + 10] = Pond;
  corner.kinds[11 * farm::MapW + 11] = Pond;
  corner.rasterize();
  assert(corner.material(175, 175) == Pond);
  assert(corner.material(176, 176) == Pond);
  assert(corner.material(164, 164) == Grass);
  const sprites::Sprite *actors[] = {
      &sprites::south,  &sprites::south0, &sprites::south1, &sprites::south2,
      &sprites::south3, &sprites::east,   &sprites::east0,  &sprites::east1,
      &sprites::east2,  &sprites::east3,  &sprites::north,  &sprites::north0,
      &sprites::north1, &sprites::north2, &sprites::north3, &sprites::west,
      &sprites::west0,  &sprites::west1,  &sprites::west2,  &sprites::west3,
  };
  for (auto actor : actors)
    joined(*actor);
  // Exercise the actual clipped blitter, shadows and poses with guarded
  // buffers.
  for (int width : {1, 7, 24, 67})
    for (int height : {1, 17, 49})
      for (int x : {-50, -1, 0, 35}) {
        std::vector<uint16_t> buffer(width * height + 32, 0xabcd);
        art::Raster r{buffer.data() + 16, width, height};
        for (int dir = 0; dir < 4; ++dir)
          for (int pose = 0; pose < 9; ++pose)
            for (int tool = 0; tool < 7; ++tool)
              for (int time : {360, 170, 70, 0})
                art::person(r, x, 13, dir, pose, false, time, tool);
        art::tree(r, x, 30, 0, 2, 4);
        art::house(r, x, -8);
        terrain::plots(r, s);
        for (int i = 0; i < 16; ++i) {
          assert(buffer[i] == 0xabcd);
          assert(buffer[buffer.size() - 1 - i] == 0xabcd);
        }
      }
  puts("PASS: 256 autotile rotations, convex/concave corners, joined "
       "materials, shores, terrain cache stability, connected character poses "
       "and clipped sprite rendering");
}
