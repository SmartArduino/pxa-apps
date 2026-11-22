// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace carrot;
int main() {
  Game g;
  for (int columns : {15, 17, 25, 31}) {
    g.setColumns(columns);
    for (int level = 0; level < Levels; ++level) {
      g.reset(level);
      assert(g.visibleColumns == columns);
      g.coins = 100000;
      int visible = 0;
      for (int s = 0; s < Slots; ++s) {
        Point center = g.slotPoint(s);
        if (!g.visible(s)) {
          assert(g.cell(center) == -1 && !g.land(s));
          continue;
        }
        ++visible;
        assert(g.cell(center) == s);
        // A cell owns its entire interior, including all four corners.
        for (float dx : {-15.99f, -15.f, 0.f, 15.f, 15.99f})
          for (float dy : {-15.99f, -15.f, 0.f, 15.f, 15.99f}) {
            Point p{center.x + dx, center.y + dy};
            assert(g.cell(p) == s);
            assert(g.pick(p) == (g.land(s) || g.towers[s].obstacle ? s : -1));
          }
        bool canBuild = g.land(s) && !g.towers[s].obstacle;
        assert(g.build(s, 1) == canBuild);
        // Filling an adjacent cell must never steal this tile's input.
        if (canBuild)
          assert(g.pick({center.x - 15.9f, center.y + 15.9f}) == s);
      }
      assert(visible == columns * GridRows);
      // Every cell traversed by an axis-aligned route is reserved.
      const auto &map = g.map();
      for (int i = 1; i < map.count; ++i) {
        Point a = map.path[i - 1], b = map.path[i];
        int n = int(distance(a, b) / CellSize);
        for (int k = 0; k <= n; ++k) {
          Point p{a.x + (b.x - a.x) * k / n, a.y + (b.y - a.y) * k / n};
          int s = g.cell(p);
          assert(s >= 0 && !g.land(s));
          assert(std::fmod(p.x - CellSize / 2, CellSize) == 0);
          assert(std::fmod(p.y - GridOriginY - CellSize / 2, CellSize) == 0);
        }
      }
    }
  }
  g.setColumns(15);
  g.reset(0);
  int neighbour = g.cell({176, 122}), empty = g.cell({144, 122});
  assert(g.land(neighbour) && g.land(empty));
  g.towers[empty] = Tower{};
  assert(g.build(neighbour, 1));
  assert(g.pick({159, 122}) ==
         empty); // Old 23px radial hit test selected the neighbour.
  int original = neighbour;
  g.setColumns(25);
  assert(g.cell({176, 122}) == original && g.towers[original].type == 1);
  assert(g.land(g.cell({-112, 122})) && g.land(g.cell({592, 122})));
  assert(g.build(g.cell({-112, 122}), 1) && g.build(g.cell({592, 122}), 1));
  g.setColumns(15);
  g.setColumns(25);
  assert(g.towers[g.cell({-112, 122})].type ==
         1); // Stable slots across resize.
  std::puts("grid: all maps, tile corners, exact roads, adjacent taps, wide "
            "grass and resize passed");
}
