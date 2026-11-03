// SPDX-License-Identifier: MIT
#include "../scene_blit.hpp"
#include "../scene_lighting.hpp"
#include "../trees.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace starsoil;
static State state;
static int reference_light(int x, int y) {
  const auto &s = state;
  int surface = s.heights[std::clamp(x, 0, s.width - 1)];
  bool night = s.clock > 240000 || s.clock < 25000;
  int light =
      y <= surface ? night ? 112 : 255 : std::max(28, 190 - (y - surface) * 23);
  int radius = s.equip[7] == GlowCharm                       ? 8
               : items[s.bag[s.selected].item].tile == Torch ? 7
                                                             : 4;
  light = std::max(light, 230 - (std::abs(x - int(s.player.x / 16)) +
                                 std::abs(y - int(s.player.y / 16))) *
                                    255 / radius);
  for (int yy = std::max(0, y - 5); yy <= std::min(s.height - 1, y + 5); ++yy)
    for (int xx = std::max(0, x - 5); xx <= std::min(s.width - 1, x + 5); ++xx)
      if (s.tiles[yy * WorldW + xx] == Torch ||
          s.tiles[yy * WorldW + xx] == Portal ||
          s.tiles[yy * WorldW + xx] == Lava)
        light =
            std::max(light, 255 - (std::abs(x - xx) + std::abs(y - yy)) * 38);
  return std::clamp(light, 28, 255);
}
static bool same(const starsoil_tree::Crown &a, const starsoil_tree::Crown &b) {
  return a.valid == b.valid &&
         (!a.valid || (a.x == b.x && a.top == b.top && a.bottom == b.bottom &&
                       a.leaf_top == b.leaf_top && a.rows == b.rows));
}
int main() {
  {
    uint16_t source[16 * 16];
    for (int i = 0; i < 256; ++i)
      source[i] = i % 7 ? uint16_t(i * 251) : 0;
    for (int x : {-20, -1, 0, 8, 31, 40})
      for (int y : {-20, -1, 0, 8, 31, 40})
        for (bool flip : {false, true})
          for (int light : {28, 112, 190, 255}) {
            std::vector<uint16_t> actual(32 * 32, 0x1357), expected = actual;
            for (int yy = 0; yy < 16; ++yy)
              for (int xx = 0; xx < 16; ++xx) {
                auto c = source[yy * 16 + (flip ? 15 - xx : xx)];
                if (c && x + xx >= 0 && x + xx < 32 && y + yy >= 5 &&
                    y + yy < 27)
                  expected[(y + yy) * 32 + x + xx] =
                      starsoil_terrain::dim(c, light);
              }
            starsoil_scene::blit(actual.data(), 32, 32, x, y, source, 16, 16,
                                 flip, light, 5, 27);
            assert(actual == expected);
          }
    puts("PASS contiguous sprite rows equal per-pixel rendering: all edges, "
         "transparency, flip, light and scene clipping");
  }
  new_game(state, 314159, WorldSize::Small);
  starsoil_scene::Lighting light;
  for (int pass = 0; pass < 12; ++pass) {
    state.clock = pass % 2 ? 10000 : 100000;
    state.player.x = float(16 * (5 + pass * 17));
    state.player.y = 16 * (19 + pass);
    state.equip[7] = pass % 3 == 0 ? GlowCharm : 0;
    state.bag[state.selected].item = pass % 3 == 1 ? Torches : CopperPick;
    for (int i = 0; i < 70; ++i)
      state.tiles[(i * 13 % state.height) * WorldW +
                  (i * 23 + pass * 11) % state.width] = i % 3 == 0   ? Torch
                                                        : i % 3 == 1 ? Lava
                                                                     : Portal;
    int left = pass * 19, top = pass * 6 - 4;
    light.prepare(state, left, top, 32, 32);
    for (int y = top - 6; y < top + 38; ++y)
      for (int x = left - 6; x < left + 38; ++x)
        assert(light.at(state, x, y) == reference_light(x, y));
    // Moving/removing every emitter is visible on the very next preparation.
    for (auto &t : state.tiles)
      if (t == Torch || t == Portal || t == Lava)
        t = Air;
    light.prepare(state, left, top, 32, 32);
    for (int y = top; y < top + 32; ++y)
      for (int x = left; x < left + 32; ++x)
        assert(light.at(state, x, y) == reference_light(x, y));
  }
  puts("PASS cached lighting equals original per-tile search: emitters, world "
       "edges, day/night, equipment, edits and offscreen fallback");
  starsoil_tree::CrownCache trees;
  auto sample = [](int x, int y) { return tile(state, x, y); };
  for (int pass = 0; pass < 10; ++pass) {
    new_game(state, 314159 + pass, WorldSize::Small);
    int left = pass * 22, top = pass % 3 ? 0 : 16;
    for (int edit = 0; edit < 2; ++edit) {
      if (edit)
        for (int y = 0; y < state.height; ++y)
          for (int x = 0; x < state.width; ++x)
            if (state.tiles[y * WorldW + x] == Log && (x + y) % 7 == 0)
              state.tiles[y * WorldW + x] = Air;
      trees.prepare(left, top, 32, 32, sample);
      for (int y = top - 1; y < top + 33; ++y)
        for (int x = left - 1; x < left + 33; ++x)
          assert(same(trees.find(x, y, sample),
                      starsoil_tree::find(x, y, sample)));
      for (int y = top; y < top + 32; ++y)
        for (int x = left; x < left + 32; ++x) {
          auto c = trees.find(x, y, sample);
          uint16_t expected[256]{}, actual[256]{};
          auto plot = [](uint16_t *p) {
            return [p](int xx, int yy, uint16_t color) {
              assert(xx >= 0 && xx < 16 && yy >= 0 && yy < 16);
              p[yy * 16 + xx] = color;
            };
          };
          assert(starsoil_tree::draw_natural(x, y, 0, 0, pass % 6, 112, sample,
                                             plot(expected)) ==
                 starsoil_tree::draw_natural(x, y, 0, 0, pass % 6, 112, sample,
                                             plot(actual), &c));
          assert(std::equal(std::begin(expected), std::end(expected),
                            std::begin(actual)));
        }
    }
  }
  // A dense artificial forest overflows the record cache; results stay exact.
  for (auto &t : state.tiles)
    t = Air;
  for (int x = 0; x < 40; ++x)
    for (int y = 8; y < 38; ++y)
      state.tiles[y * WorldW + x] = Log;
  for (int x = 0; x < 40; ++x)
    for (int y = 5; y < 8; ++y)
      state.tiles[y * WorldW + x] = Leaves;
  trees.prepare(4, 8, 32, 30, sample);
  for (int y = 8; y < 38; ++y)
    for (int x = 4; x < 36; ++x)
      assert(same(trees.find(x, y, sample), starsoil_tree::find(x, y, sample)));
  puts("PASS tree cache preserves exact crown selection and pixels, both "
       "camera edges, mined trunks and bounded-cache overflow");
  // Golden plot stream from v0.9.4, before hoisting crown and trunk math.
  // Include empty cells, all tree palettes, shallow/deep lighting and seeds
  // with different crown and trunk heights; painter order is significant.
  uint64_t fingerprint = 1469598103934665603ull;
  for (int seed : {1, 314159, 1234567}) {
    new_game(state, seed, WorldSize::Small);
    // The tree-only v0.9.4 golden excludes later starter-house furniture fixes.
    set_tile(state, 53, 29, Door); set_tile(state, 53, 30, Air);
    for (int biome : {int(Forest), int(Tundra), int(Jungle)})
      for (int brightness : {28, 112, 255})
        for (int y = 0; y < 48; ++y)
          for (int x = 0; x < state.width; ++x) {
            bool drawn = starsoil_tree::draw_natural(
                x, y, x * 16, y * 16, biome, brightness, sample,
                [&](int px, int py, uint16_t color) {
                  fingerprint = (fingerprint ^ unsigned(px)) * 1099511628211ull;
                  fingerprint = (fingerprint ^ unsigned(py)) * 1099511628211ull;
                  fingerprint = (fingerprint ^ color) * 1099511628211ull;
                });
            fingerprint = (fingerprint ^ drawn) * 1099511628211ull;
          }
  }
  assert(fingerprint == 0xe60adbf2f1210f45ull);
  puts("PASS natural tree plot streams equal v0.9.4 across palettes, lighting and seeds");
}
