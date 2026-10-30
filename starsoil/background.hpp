// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <algorithm>
#include <cstdint>
namespace starsoil_scene {
constexpr uint16_t c(uint32_t v) {
  return uint16_t(((v >> 8) & 0xf800) | ((v >> 5) & 0x07e0) | ((v >> 3) & 31));
}
inline uint16_t blend(uint16_t a, uint16_t b, int k) {
  return uint16_t(
      (((((a >> 11) & 31) * (255 - k) + ((b >> 11) & 31) * k) / 255) << 11) |
      (((((a >> 5) & 63) * (255 - k) + ((b >> 5) & 63) * k) / 255) << 5) |
      (((a & 31) * (255 - k) + (b & 31) * k) / 255));
}
inline uint32_t hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352d;
  x ^= x >> 15;
  x *= 0x846ca68b;
  return x ^ (x >> 16);
}
inline int floor_div(int x, int d) {
  return x >= 0 ? x / d : -1 - (-x - 1) / d;
}
inline int ridge(int x, int period, int seed) {
  int n = floor_div(x, period), t = x - n * period;
  int a = int(hash(uint32_t(n + seed)) % 49),
      b = int(hash(uint32_t(n + seed + 1)) % 49);
  return (a * (period - t) + b * t) / period;
}
template <class Box, class Line>
void broadleaf(int x, int ground, int h, uint16_t trunk, uint16_t foliage,
               uint16_t light, Box box, Line line) {
  int top = ground - h;
  box(x, top + 7, 3, h - 7, trunk);
  line(x + 1, top + 26, x - 9, top + 12, trunk);
  line(x + 1, top + 23, x + 11, top + 10, trunk);
  auto crown = [&](int cx, int cy, int rx, int ry, uint16_t color) {
    for (int yy = -ry; yy <= ry; ++yy) {
      int span = rx - yy * yy * rx / (ry * ry + 2);
      box(cx - span, cy + yy, span * 2 + 1, 1, color);
    }
  };
  // Four joined lobes give the crown volume and an irregular stepped outline.
  crown(x - 8, top + 12, 10, 7, foliage);
  crown(x + 9, top + 11, 10, 8, foliage);
  crown(x, top + 6, 11, 8, foliage);
  crown(x + 1, top + 17, 12, 5, foliage);
  box(x - 6, top + 1, 10, 2, light);
  box(x - 10, top + 3, 7, 2, light);
  box(x + 9, top + 6, 7, 2, light);
}
// All backgrounds are original pixel geometry. Far layers use different scroll
// speeds; no giant bitmap, smoothing filter or extra surface buffer is needed.
template <class Rect, class Line, class Pixel>
void draw(int w, int bottom, int camera_x, int camera_y, int surface_y,
          int depth, int biome, uint32_t clock, uint32_t animation, Rect rect,
          Line line, Pixel pixel) {
  using namespace starsoil;
  const bool night = clock < 25000 || clock > 240000;
  auto box = [&](int x, int y, int ww, int hh, uint16_t color) {
    int end = std::min(bottom, y + hh);
    y = std::max(25, y);
    if (end > y)
      rect(x, y, ww, end - y, color);
  };
  auto dot = [&](int x, int y, uint16_t color) {
    if (y >= 25 && y < bottom)
      pixel(x, y, color);
  };
  auto diamond = [&](int x, int y, int half, int h, uint16_t fill,
                     uint16_t shine) {
    for (int yy = 0; yy < h; ++yy) {
      int v = yy < h / 2 ? yy : h - 1 - yy;
      int span = std::max(0, v * half / std::max(1, h / 2));
      box(x - span, y + yy, span * 2 + 1, 1, fill);
    }
    line(x, y + 2, x, y + h / 2, shine);
  };
  if (depth >= 4) {
    const uint16_t top = c(0x141d30),
                   low = biome == CrystalDepths ? c(0x284452) : c(0x303646);
    for (int y = 25; y < bottom; ++y)
      box(0, y, w, 1,
          blend(top, low, (y - 25) * 255 / std::max(1, bottom - 25)));
    for (int layer = 0; layer < 2; ++layer)
      for (int x = 0; x < w; ++x) {
        int world = x + camera_x / (layer ? 3 : 6);
        int ceiling = 34 + ridge(world, layer ? 53 : 79, 31 + layer * 9) / 2;
        int floor = bottom - 8 - ridge(world, layer ? 47 : 81, 117 + layer * 3);
        box(x, 25, 1, ceiling - 25, c(layer ? 0x182536 : 0x1c2b3c));
        box(x, floor, 1, bottom - floor, c(layer ? 0x182735 : 0x223543));
      }
    int scroll = camera_x / 4;
    for (int i = floor_div(scroll, 57) - 1; i < floor_div(scroll + w, 57) + 2;
         ++i) {
      uint32_t n = hash(i + 722);
      int x = i * 57 - scroll, y = bottom - 25 - int(n % 48),
          h = 12 + int(n % 17);
      if (biome == CrystalDepths || n % 3 == 0) {
        diamond(x, y, 4, h, c(0x38636a), c(0x79b1a4));
        diamond(x + 7, y + 7, 2, 13, c(0x2c4e5c), c(0x527d88));
        if ((animation / 180 + n) % 17 < 3)
          dot(x, y + 3, c(0xb2e3c0));
      } else {
        for (int yy = 0; yy < 22; ++yy)
          box(x - yy / 6, 35 + int(n % 12) + yy, yy / 3 + 1, 1, c(0x233342));
      }
    }
    return;
  }
  const uint32_t sky_top[] = {0x80afc4, 0x659fc0, 0x9ab7c8,
                              0x789fbd, 0x638eaa, 0x659fc0};
  const uint32_t sky_low[] = {0xeee0b5, 0xc6ddbd, 0xf0ce98,
                              0xe4ece3, 0xb1c8aa, 0xc6ddbd};
  biome = std::clamp(biome, 0, 5);
  uint16_t top = night ? c(0x192940) : c(sky_top[biome]);
  uint16_t low = night ? c(0x53657e) : c(sky_low[biome]);
  if (clock > 180000 && clock < 240000) {
    top = blend(top, c(0x917e9e), 105);
    low = blend(low, c(0xf0b68b), 145);
  }
  for (int y = 25; y < bottom; ++y)
    box(0, y, w, 1, blend(top, low, (y - 25) * 255 / std::max(1, bottom - 25)));
  if (night)
    for (int i = 0; i < 32; ++i) {
      uint32_t n = hash(i + 92);
      int x = int(n % w), y = 30 + int((n >> 9) % 65);
      dot(x, y, c((animation / 190 + n) % 9 ? 0x91a5b6 : 0xe8e5c7));
    }
  int sun_x = w - 31 - int((clock % 240000) * (w - 50) / 240000), sun_y = 54;
  for (int yy = -9; yy <= 9; ++yy) {
    int span = 9 - (yy * yy) / 12;
    box(sun_x - span, sun_y + yy, span * 2 + 1, 1,
        c(night ? 0xd2ddce : 0xffe7ab));
  }
  if (night)
    for (int yy = -8; yy <= 8; ++yy) {
      int span = 8 - (yy * yy) / 12;
      box(sun_x + 4 - span, sun_y - 2 + yy, span * 2 + 1, 1,
          blend(top, low, 40));
    }
  // Rounded pixel cloud clusters drift slowly above the ridges.
  for (int i = -1; i < 5; ++i) {
    uint32_t n = hash(i + 74);
    int x = i * 97 - int((animation / 330 + camera_x / 18) % 97),
        y = 37 + int(n % 38);
    uint16_t shade =
        blend(top, c(night ? 0x718397 : 0xf7ead2), night ? 90 : 170);
    box(x, y + 5, 49, 4, shade);
    box(x + 5, y + 2, 30, 5, shade);
    box(x + 15, y, 14, 3, shade);
    box(x + 34, y + 4, 9, 3, shade);
    box(x + 6, y + 8, 36, 2, blend(shade, low, 65));
  }
  int horizon = std::clamp(surface_y, 108, bottom + 32);
  for (int layer = 0; layer < 3; ++layer) {
    uint32_t hues[] = {0x8ca3af, 0x5d8492, 0x3d6d75};
    if (biome == Desert) {
      hues[0] = 0xc4ae98;
      hues[1] = 0xbc9a7c;
      hues[2] = 0xb69670;
    }
    if (biome == Jungle) {
      hues[0] = 0x87a9a8;
      hues[1] = 0x668f90;
      hues[2] = 0x4c7976;
    }
    if (biome == Tundra) {
      hues[0] = 0xb5cbd0;
      hues[1] = 0x8da9bc;
      hues[2] = 0x6e91a9;
    }
    uint16_t fill =
        night ? blend(c(hues[layer]), c(0x263b54), 175) : c(hues[layer]);
    for (int x = 0; x < w; ++x) {
      int world = x + camera_x / (12 - layer * 3),
          r = ridge(world, layer ? 61 : 93, layer * 51 + 11);
      int h = horizon - 43 + layer * 18 - r * (biome == Desert ? 1 : 2) / 2;
      box(x, h, 1, bottom - h, fill);
      if (layer == 0 && biome == Tundra)
        box(x, h, 1, 3 + (r % 5), night ? c(0xa1b9c4) : c(0xeaf0e1));
      else if (layer == 0 && x % 11 < 3)
        dot(x, h + 2, blend(fill, low, 65));
    }
  }
  // Biome-specific middle-distance vegetation, never generic pines in dunes.
  int scroll = camera_x / 3;
  for (int i = floor_div(scroll, 35) - 1; i < floor_div(scroll + w, 35) + 2;
       ++i) {
    uint32_t n = hash(i + 881);
    int x = i * 35 - scroll, y = horizon + 8 - int(n % 13),
        h = 27 + int(n % 37);
    uint16_t trunk =
        night ? c(0x2b4655) : c(biome == Desert ? 0x9a8b61 : 0x426965);
    uint16_t foliage = night ? c(0x345366)
                             : c(biome == Tundra   ? 0x5c8599
                                 : biome == Jungle ? 0x457b70
                                                   : 0x4c7d70);
    if (biome == Desert) {
      box(x, y - 27, 3, 27, trunk);
      box(x - 8, y - 18, 2, 10, trunk);
      box(x - 8, y - 10, 8, 2, trunk);
      box(x + 7, y - 24, 2, 12, trunk);
      box(x + 2, y - 14, 7, 2, trunk);
    } else if (biome == Meadow || biome == Forest || biome == Jungle) {
      broadleaf(x, y, h, trunk, foliage, blend(foliage, low, 35), box, line);
      if (biome == Jungle)
        line(x + 10, y - h + 13, x + 8, y - 6, trunk);
    } else {
      box(x, y - h + 8, 2, h - 8, trunk);
      for (int tier = 0; tier < 4; ++tier)
        for (int yy = 0; yy < 9; ++yy) {
          int span = 2 + yy + tier;
          int py = y - h + tier * (h - 10) / 5 + yy;
          box(x - span, py, span * 2 + 2, 1,
              (biome == Tundra && yy < 2) ? blend(foliage, c(0xe4eee1), 150)
                                          : foliage);
        }
    }
  }
  if (night)
    for (int i = 0; i < 9; ++i) {
      uint32_t n = hash(i + 113);
      int x = int((n + animation / 120) % (w + 40)) - 20,
          y = horizon - 7 - int((n >> 8) % 51);
      if ((animation / 240 + n) % 9 < 4)
        dot(x, y, c(0xc2d7a2));
    }
  (void)camera_y;
}
} // namespace starsoil_scene
