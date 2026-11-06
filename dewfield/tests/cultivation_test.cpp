// SPDX-License-Identifier: MIT
#include "../terrain.hpp"
#include "../scene.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <queue>
constexpr int W = 160, H = 128;
using Pixels = std::array<uint16_t, W * H>;
static Pixels field, crop;
static void soil_texture() {
  farm::State s;
  farm::fresh(s);
  for (auto &p : s.plots)
    p = {1, 0, 0, 0};
  art::Raster r{field.data(), W, H, farm::PlotX * 16, farm::PlotY * 16};
  for (bool wet : {false, true}) {
    for (auto &p : s.plots)
      p.wet = wet;
    field.fill(0);
    terrain::plots(r, s);
    int accents = 0;
    const auto &colors = soil::colors(wet);
    for (int y = 2; y < H - 2; ++y) {
      int run = 0;
      for (int x = 2; x < W - 2; ++x) {
        uint16_t c = field[y * W + x];
        assert(c == colors.tones[soil::tone(r.ox + x, r.oy + y)]);
        accents += c != colors.tones[0];
        // Dark furrows must break into clods, including across tile seams.
        run = c == colors.tones[1] ? run + 1 : 0;
        assert(run <= 6);
      }
    }
    assert(accents > W * H / 10 && accents < W * H / 2);
  }
  // A wet inside corner inherits the adjacent watered soil, even if the
  // corner's own tile has never been hoed.
  for (auto &p : s.plots)
    p = {};
  for (int i : {11, 12, 21})
    s.plots[i] = {1, 0, 0, 1};
  field.fill(0);
  terrain::plots(r, s);
  assert(field[32 * W + 32] == soil::Wet.tones[soil::tone(r.ox + 32, r.oy + 32)]);
}
static void planted_crops() {
  farm::State s;
  farm::fresh(s);
  std::memset(s.cleared, 1, sizeof(s.cleared));
  for (auto &p : s.plots)
    p = {};
  constexpr int plot = 34;
  const int x = soil::root_x(plot), y = soil::root_y(plot);
  assert(x % 16 == 8 && y % 16 == 10);
  art::Raster r{field.data(), W, H, farm::PlotX * 16, farm::PlotY * 16};
  s.plots[plot] = {1, 0, 0, 0};
  field.fill(0);
  terrain::plots(r, s);
  const Pixels soil_only = field;
  for (bool wet : {false, true})
    for (int type = 0; type < 4; ++type)
      for (int stage = 0; stage < 4; ++stage) {
        s.plots[plot] = {1, uint8_t(type + 1), 0, uint8_t(wet)};
        field.fill(0);
        terrain::plots(r, s);
        int at = (y - r.oy) * W + x - r.ox;
        assert(field[at] == soil::colors(wet).hole);
        crop.fill(0);
        r.p = crop.data();
        art::crop(r, x, y, type, stage, stage == 3, wet);
        r.p = field.data();
        bool contact = false;
        for (int xx = 0; xx < W; ++xx) {
          if (crop[(y - r.oy - 1) * W + xx]) {
            // The whole actual base lies within this plot's cultivated soil.
            assert(soil_only[(y - r.oy) * W + xx]);
            assert(xx >= x - r.ox - 6 && xx <= x - r.ox + 6);
            contact = true;
          }
          for (int yy = y - r.oy; yy < H; ++yy)
            assert(!crop[yy * W + xx]);
        }
        assert(contact);
        // Leaves, fruit, stems and the stake form one connected plant.
        bool visited[W * H]{};
        std::queue<int> q;
        for (int i = 0; i < W * H; ++i)
          if (crop[i]) { q.push(i); visited[i] = true; break; }
        while (!q.empty()) {
          int a = q.front(); q.pop();
          for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
              int n = a + dy * W + dx;
              if (n >= 0 && n < W * H && crop[n] && !visited[n] &&
                  std::abs(n % W - a % W) <= 1) {
                visited[n] = true; q.push(n);
              }
            }
        }
        for (int i = 0; i < W * H; ++i)
          assert(!crop[i] || visited[i]);
      }
  farm_scene::Queue q;
  q.build(s, {}, {0, 0, 512, 384});
  for (unsigned i = 0; i < q.count; ++i)
    if (q.entries[i].kind == farm_scene::Kind::Crop) {
      assert(q.entries[i].x == x && q.entries[i].y == y);
      assert(q.entries[i].depth == y);
    }
}
int main() {
  soil_texture();
  planted_crops();
  puts("PASS: broken soil texture, seamless cultivated ground, wet concave "
       "corners, buried roots, connected plants and shared depth anchors");
}
