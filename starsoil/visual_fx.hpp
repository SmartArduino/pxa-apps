// SPDX-License-Identifier: MIT
// PixelFX-inspired small pixel effects, adapted for Starsoil's RGB565 renderer.
// Firefly pulse adapts patch-gg/pixelfx weather/fireflies.js (Patch / Hugo Duprez).
// See art/PIXELFX-LICENSE.txt. No JS runtime, textures or full-screen light buffer.
#pragma once
#include "core.hpp"
#include "color_math.hpp"
#include <algorithm>

namespace starsoil_fx {
constexpr uint16_t rgb(uint32_t v) {
  return uint16_t(((v >> 8) & 0xf800) | ((v >> 5) & 0x7e0) | ((v >> 3) & 31));
}
inline int triangle(uint32_t t, unsigned period, int amplitude) {
  unsigned p = t % period;
  return int((p < period / 2 ? p : period - p) * unsigned(amplitude) * 2 / period);
}
// Quick ignition, brief hold, long decay, then darkness; each fly has its own phase.
inline int pulse(unsigned t) {
  t %= 2400;
  return t < 144 ? int(t * 255 / 144) : t < 336 ? 255
       : t < 960 ? int((960 - t) * 255 / 624) : 0;
}
template<class Dot, class Glow>
void star(int x, int y, uint16_t color, int strength, Dot dot, Glow glow) {
  if (strength < 20) return;
  glow(x - 2, y - 1, 5, 3, color, unsigned(strength / 5));
  glow(x - 1, y - 2, 3, 5, color, unsigned(strength / 5));
  dot(x, y, starsoil_color::mix(color, rgb(0xfff5d4), unsigned(strength)));
  if (strength > 180) {
    dot(x - 1, y, color); dot(x + 1, y, color);
    dot(x, y - 1, color); dot(x, y + 1, color);
  }
}
template<class Dot, class Glow>
void torch(int x, int y, uint32_t time, uint32_t seed, Dot dot, Glow glow) {
  const unsigned t = time + seed % 1600;
  const int sway = triangle(t, 480, 2) - 1;
  const int h = 5 + triangle(t, 320, 3);
  glow(x - 7, y - 6, 15, 13, rgb(0xffa04b), 14 + triangle(t, 400, 9));
  glow(x - 3, y - 4, 7, 7, rgb(0xffcc75), 35);
  for (int row = 0; row < h; ++row) {
    int half = row < 2 ? 2 : row < h - 2 ? 1 : 0;
    int lean = row > 3 ? sway : 0;
    for (int xx = -half; xx <= half; ++xx)
      dot(x + xx + lean, y - row,
          rgb(row < 3 && std::abs(xx) < 1 ? 0xfff2b6 : row < 5 ? 0xffbc56 : 0xe87940));
  }
  for (int i = 0; i < 2; ++i) {
    unsigned age = (t + i * 630) % 1300;
    if (age < 900)
      dot(x + triangle(t + i * 237, 840, 4) - 2, y - 6 - int(age / 90),
          rgb(age < 450 ? 0xffdc83 : 0xa16c4a));
  }
}
template<class Dot>
void liquid(int x, int y, int world_x, bool lava, int light, uint32_t time, Dot dot) {
  const auto crest = starsoil_color::dim(rgb(lava ? 0xffd584 : 0xa7d5d0), unsigned(lava ? 255 : light));
  const auto shade = starsoil_color::dim(rgb(lava ? 0xe9924b : 0x598fa5), unsigned(lava ? 255 : light));
  // World coordinates join the wave across neighboring tiles; only exposed surfaces draw it.
  for (int xx = 0; xx < 16; ++xx) {
    unsigned phase = unsigned(world_x + xx + int(time / (lava ? 150 : 90))) & 31;
    dot(x + xx, y + (phase < 16 ? 1 : 2), phase < 7 ? crest : shade);
  }
  unsigned phase = (time / 80 + unsigned(world_x * 3)) % 32;
  if (phase < 16) {
    const int radius = int(phase / 4);
    dot(x + 8 - radius, y + 4, shade); dot(x + 8 + radius, y + 4, shade);
    if (radius > 1) { dot(x + 7, y + 3, crest); dot(x + 9, y + 5, shade); }
  }
}
template<class Dot, class Glow>
void ambient(const starsoil::State &s, int camera_x, int camera_y, int width,
             int bottom, uint32_t time, Dot dot, Glow glow) {
  using namespace starsoil;
  const bool night = s.clock < 25000 || s.clock > 240000;
  // Stable world cells prevent particles jumping when the camera moves.
  for (int cell = camera_x / 64 - 1; cell <= (camera_x + width) / 64 + 1; ++cell) {
    if (cell < 0 || cell * 4 >= s.width) continue;
    uint32_t seed = hash(uint32_t(cell) ^ s.seed);
    const int region = biome_at(cell * 4, 0, s.width);
    for (int i = 0; i < 3; ++i) {
      unsigned phase = seed + unsigned(i * 977);
      int wx = cell * 64 + int((phase >> 8) % 64) + triangle(time + phase, 4300, 12) - 6;
      wx = std::clamp(wx, 0, s.width * 16 - 1);
      int ground = s.heights[wx / 16] * 16;
      int wy = ground - 12 - int(phase % 78) + triangle(time + phase, 3100, 12);
      const bool snow = region == Tundra;
      if (snow) wy = ground - 144 + int((time / 55 + phase) % 160);
      int x = wx - camera_x, y = wy - camera_y;
      if (x < -3 || x >= width + 3 || y < 0 || y >= bottom || tile(s, wx / 16, wy / 16) != Air ||
          built_wall(s.walls[std::max(0, wy / 16) * WorldW + wx / 16])) continue;
      if (snow) {
        dot(x, y, rgb(0xd8e6e3)); if (i == 0) dot(x + 1, y, rgb(0x8fabbc));
      } else if (night && (region == Forest || region == Meadow || region == Jungle)) {
        star(x, y, rgb(0xb9d97a), pulse((time + phase) % 2400), dot, glow);
      } else if (!night && (region == Forest || region == Jungle) && i == 0) {
        dot(x, y, rgb(0xbacb88)); dot(x + 1, y + 1, rgb(0x799b71));
      }
    }
  }
}
template<class Dot>
void ring(int x, int y, unsigned age, uint16_t color, bool flat, Dot dot) {
  const int r = 2 + int(age / 45), h = flat ? std::max(1, r / 3) : r;
  const int d = std::max(1, r * 3 / 4), dy = flat ? 1 : d;
  for (int sign : {-1, 1}) {
    dot(x + sign * r, y, color); dot(x, y + sign * h, color);
    dot(x + sign * d, y - dy, color); dot(x + sign * d, y + dy, color);
  }
}
} // namespace starsoil_fx
