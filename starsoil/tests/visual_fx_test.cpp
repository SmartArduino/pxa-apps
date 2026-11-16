// SPDX-License-Identifier: MIT
#include "../visual_fx.hpp"
#include "../effects.hpp"
#include "../tile_cache.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
static starsoil::State state;
static starsoil_scene::TileCache cache;
int main() {
  // Cache collision/eviction, clipping, flip and opaque colors dimming to black
  // must all be pixel-identical to the uncached blitter.
  std::vector<uint16_t> art(96 * 256);
  for (unsigned i = 0; i < art.size(); ++i)
    art[i] = (i / 256) % 2 || i % 7 ? uint16_t((i * 193 + 1) | 1) : 0;
  art[1] = 1;
  for (int pass = 0; pass < 3; ++pass)
    for (int n = 0; n < 96; ++n)
      for (int light : {0, 1, 28, 112, 190, 255})
        for (bool flip : {false, true}) {
          int x = n % 37 - 12, y = n % 33 - 10;
          std::vector<uint16_t> actual(32 * 32 + 2, 0x1357), expected = actual;
          auto *source = art.data() + n * 256;
          cache.draw(actual.data() + 1, 32, 32, x, y, source, flip, light, 4, 28);
          starsoil_scene::blit(expected.data() + 1, 32, 32, x, y, source, 16, 16, flip, light, 4, 28);
          assert(actual == expected);
        }
  assert(sizeof(cache) < 18000);
  // Warm an entirely opaque tile, including colors becoming black, then hit
  // its row-copy path with partial clipping and both orientations.
  for (int light : {0, 1, 28, 112, 190}) {
    std::vector<uint16_t> warm(32 * 32);
    for (int reuse = 0; reuse < 5; ++reuse)
      cache.draw(warm.data(), 32, 32, 8, 8, art.data() + 256, false, light, 0, 32);
    for (int x : {-2, 0, 17, 24}) for (bool flip : {false, true}) {
      std::vector<uint16_t> actual(32 * 32 + 2, 0x1357), expected = actual;
      cache.draw(actual.data() + 1, 32, 32, x, 7, art.data() + 256, flip, light, 4, 21);
      starsoil_scene::blit(expected.data() + 1, 32, 32, x, 7, art.data() + 256, 16, 16, flip, light, 4, 21);
      assert(actual == expected);
    }
  }
  puts("PASS bounded lit tile cache: pixel equivalence, eviction, clip, flip and opaque black");
  {
    starsoil_scene::TileCache adaptive;
    std::vector<uint16_t> frame(32 * 32);
    adaptive.begin_frame(false); assert(!adaptive.active());
    adaptive.begin_frame(true); assert(adaptive.active());
    for (int n = 0; n < 96; ++n)
      adaptive.draw(frame.data(), 32, 32, 8, 8, art.data() + n * 256, false, 112, 0, 32);
    adaptive.begin_frame(true); assert(!adaptive.active());
    for (int i = 0; i < 61; ++i) adaptive.begin_frame(true);
    assert(adaptive.active());
    puts("PASS movement bypass and automatic cache-thrashing cooldown/retry");
  }

  starsoil::new_game(state, 314159, starsoil::WorldSize::Small);
  auto initial = state;
  int dots = 0, glows = 0;
  auto dot = [&](int, int, uint16_t) { ++dots; };
  auto glow = [&](int, int, int w, int h, uint16_t, unsigned alpha) {
    assert(w > 0 && h > 0 && w <= 15 && h <= 13 && alpha <= 255); ++glows;
  };
  for (uint32_t t : {0u, 144u, 336u, 960u, 2400u, 0xfffffff0u}) {
    assert(starsoil_fx::pulse(t) >= 0 && starsoil_fx::pulse(t) <= 255);
    starsoil_fx::torch(10, 20, t, 17, dot, glow);
    starsoil_fx::liquid(0, 25, 160, false, 112, t, dot);
    starsoil_fx::liquid(0, 25, 160, true, 28, t, dot);
    for (int camera : {0, 500, 1600, 3100}) {
      dots = 0;
      starsoil_fx::ambient(state, camera, 0, 480, 480, t, dot, glow);
      assert(dots < 300);
    }
  }
  assert(glows > 0 && state.random == initial.random && state.clock == initial.clock);
  // Cosmetics must not consume game randomness, mutate terrain or change saves.
  std::vector<uint8_t> before(starsoil::SaveBytes), after(before.size());
  size_t a = starsoil::encode(initial, before.data(), before.size());
  size_t b = starsoil::encode(state, after.data(), after.size());
  assert(a == b && before == after);
  puts("PASS FX bounded draw counts, timer wrap, day/night and world RNG/save isolation");

  starsoil::Effects effects;
  effects.sync(state);
  state.player.grounded = false; effects.sync(state);
  state.player.grounded = true; effects.step(state, 20, true);
  unsigned live = 0; for (auto p : effects.particles) live += p.life > 0;
  assert(live >= 5);
  state.mined++; effects.step(state, 20, true);
  assert(effects.pulses[0].life > 0);
  for (int i = 0; i < 50; ++i) effects.step(state, 20, false);
  for (auto p : effects.particles) assert(!p.life);
  for (auto p : effects.pulses) assert(!p.life);
  puts("PASS landing/mining feedback expires while gameplay is inactive");
}
