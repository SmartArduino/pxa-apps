// SPDX-License-Identifier: MIT
#include "../background_cache.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

static starsoil::State state;
static starsoil_scene::BackgroundCache cache;

static void draw(uint16_t *frame, int width, int height, int first,
                 int camera_x, int camera_y, uint32_t animation, bool sparkles = true) {
  auto pixel = [&](int x, int y, uint16_t color) {
    if (x >= 0 && x < width && y >= first && y < height)
      frame[y * width + x] = color;
  };
  auto rectangle = [&](int x, int y, int w, int h, uint16_t color) {
    for (int yy = std::max(first, y); yy < std::min(height, y + h); ++yy)
      for (int xx = std::max(0, x); xx < std::min(width, x + w); ++xx)
        pixel(xx, yy, color);
  };
  auto line = [&](int x, int y, int end_x, int end_y, uint16_t color) {
    int dx = std::abs(end_x - x), step_x = x < end_x ? 1 : -1;
    int dy = -std::abs(end_y - y), step_y = y < end_y ? 1 : -1;
    int error = dx + dy;
    for (;;) {
      pixel(x, y, color);
      if (x == end_x && y == end_y) break;
      const int twice = error * 2;
      if (twice >= dy) { error += dy; x += step_x; }
      if (twice <= dx) { error += dx; y += step_y; }
    }
  };
  starsoil_scene::draw_world(state, width, height, camera_x, camera_y,
    animation, rectangle, line, pixel,
    [&](int x, int y) { return frame[y * width + x]; }, first, sparkles);
}

static unsigned frames, hits;
static bool check(int camera_x, int camera_y, uint32_t animation,
                  int width = 288, int height = 288, int first = 25) {
  const uint16_t sentinel = uint16_t(0x1357 ^ ++frames);
  std::vector<uint16_t> actual(width * height + 2, sentinel), expected = actual;
  const auto key = starsoil_scene::BackgroundKey::make(
    state, width, height, camera_x, camera_y, animation, first);
  const bool hit = cache.restore(key, actual.data() + 1);
  if (!hit) {
    draw(actual.data() + 1, width, height, first, camera_x, camera_y, animation, false);
    cache.store(key, actual.data() + 1);
  } else ++hits;
  auto *pixels = actual.data() + 1;
  starsoil_scene::draw_cave_sparkles(width, height, camera_x, camera_y, key.ground_y(), animation,
    [&](int x, int y, uint16_t color) { pixels[y * width + x] = color; },
    [&](int x, int y) { return pixels[y * width + x]; }, first);
  draw(expected.data() + 1, width, height, first, camera_x, camera_y, animation);
  assert(actual == expected);
  assert(actual.front() == sentinel && actual.back() == sentinel);
  return hit;
}

int main() {
  starsoil::new_game(state, 1234567);
  std::fill(std::begin(state.heights), std::end(state.heights), uint8_t(32));
  state.clock = 100000;
  assert(!check(500, 300, 1000));
  assert(!check(500, 300, 1000));
  assert(check(500, 300, 1000));

  // Every actual output pixel is compared with the original renderer. Cross
  // sunlight, night/dusk, clouds, twinkles and uint32_t animation boundaries.
  for (int depth : {300, 420, 1200}) {
    for (uint32_t clock : {24999u, 25000u, 180000u, 180001u, 239999u, 240000u, 240001u, UINT32_MAX}) {
      state.clock = clock;
      for (uint32_t animation : {119u, 120u, 179u, 180u, 189u, 190u, 239u, 240u, 329u, 330u, UINT32_MAX}) {
        check(500, depth, animation);
        check(500, depth, animation);
        check(500, depth, animation);
      }
    }
  }
  // Continuous horizontal/vertical movement, biome boundaries and cliff edits.
  state.clock = 100000;
  for (int tile : {30, 120, 180, 250, 330})
    for (int step = 0; step < 30; ++step)
      check(tile * 16 + step, 320 + step / 7, 1000 + step * 40);
  state.heights[61] = 90;
  check(61 * 16, 500, 1000);
  check(61 * 16, 500, 1000);
  check(61 * 16, 500, 1000);
  state.heights[61] = 32;
  check(61 * 16, 500, 1000);

  // Resize, cutout layouts, bounded-cache overflow and switching worlds.
  for (auto dimensions : {std::pair{320,240}, std::pair{373,224},
                         std::pair{288,341}, std::pair{480,224}, std::pair{288,480}}) {
    for (int first : {0, 25}) {
      for (int repeat = 0; repeat < 3; ++repeat) {
        const bool hit = check(500, 300, 1000, dimensions.first, dimensions.second, first);
        if (dimensions.first * (dimensions.second - first) > 96 * 1024) assert(!hit);
      }
    }
  }
  for (auto size : {starsoil::WorldSize::Small, starsoil::WorldSize::Huge}) {
    starsoil::new_game(state, 87654321, size);
    for (int repeat = 0; repeat < 3; ++repeat) check(500, 300, 1000);
  }
  // Continuous camera movement must skip snapshot writes. The first stable
  // frame populates the cache and the following frame can reuse it.
  static starsoil_scene::BackgroundCache moving_cache;
  std::vector<uint16_t> snapshot(288 * 288);
  state.clock = 100000;
  starsoil_scene::BackgroundKey stopped;
  for (int step = 0; step < 8; ++step) {
    stopped = starsoil_scene::BackgroundKey::make(state, 288, 288, 800 + step * 24, 300, 1000, 25);
    assert(!moving_cache.restore(stopped, snapshot.data()));
    moving_cache.store(stopped, snapshot.data());
  }
  assert(!moving_cache.restore(stopped, snapshot.data()));
  moving_cache.store(stopped, snapshot.data());
  assert(moving_cache.restore(stopped, snapshot.data()));
  state.clock = 10000;
  const auto night = starsoil_scene::BackgroundKey::make(state, 288, 288, 800, 0, 1000, 25);
  for (int repeat = 0; repeat < 3; ++repeat) {
    assert(!moving_cache.restore(night, snapshot.data()));
    moving_cache.store(night, snapshot.data());
  }
  assert(hits > 100);
  std::printf("PASS background cache: %u exact frames, %u hits, animations, edits, camera, resize and capacity fallback\n", frames, hits);
}
