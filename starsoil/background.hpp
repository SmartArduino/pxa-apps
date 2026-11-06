// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include "color_math.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
namespace starsoil_scene {
constexpr uint16_t c(uint32_t v) {
  return uint16_t(((v >> 8) & 0xf800) | ((v >> 5) & 0x07e0) | ((v >> 3) & 31));
}
inline uint16_t blend(uint16_t a, uint16_t b, int k) {
  return starsoil_color::mix(a, b, unsigned(k));
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
          Line line, Pixel pixel, int first_y = 25, bool cave_sparkles = true) {
  using namespace starsoil;
  const bool night = clock < 25000 || clock > 240000;
  auto box = [&](int x, int y, int ww, int hh, uint16_t color) {
    int end = std::min(bottom, y + hh);
    y = std::max(first_y, y);
    if (end > y)
      rect(x, y, ww, end - y, color);
  };
  auto dot = [&](int x, int y, uint16_t color) {
    if (y >= first_y && y < bottom)
      pixel(x, y, color);
  };
  // Horizontal spans keep framebuffer writes contiguous. Tall one-pixel
  // columns otherwise revisit a different PSRAM cache line for every pixel.
  auto columns = [&](uint16_t color, const int *edge, bool below) {
    int low = bottom, high = first_y;
    for (int x = 0; x < w; ++x) { low = std::min(low, edge[x]); high = std::max(high, edge[x]); }
    low = std::clamp(low, first_y, bottom);
    high = std::clamp(high, first_y, bottom);
    // Empty/full rows do not need a per-pixel coverage test. Keep horizontal
    // writes contiguous while scanning only the narrow irregular boundary.
    if (below) box(0, high, w, bottom - high, color);
    else box(0, first_y, w, low - first_y, color);
    for (int y = low; y < high; ++y) {
      auto covered = [&](int x) { return below ? y >= edge[x] : y < edge[x]; };
      int x = 0;
      while (x < w) {
        while (x < w && !covered(x)) ++x;
        int start = x;
        while (x < w && covered(x)) ++x;
        if (x > start) box(start, y, x - start, 1, color);
      }
    }
  };
  int tops[480], floors[480];
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
    // Crystal hues emerge over eight world tiles, independent of the actor.
    int crystal = std::clamp((camera_y + (first_y + bottom) / 2 - 64 * 16) * 255 / (8 * 16), 0, 255);
    const uint16_t top = c(0x141d30),
                   low = blend(c(0x303646), c(0x284452), crystal);
    for (int y = first_y; y < bottom; ++y)
      box(0, y, w, 1,
          blend(top, low, (y - first_y) * 255 / std::max(1, bottom - first_y)));
    for (int layer = 0; layer < 2; ++layer) {
      for (int x = 0; x < w; ++x) {
        int world = x + camera_x / (layer ? 3 : 6);
        tops[x] = 34 + ridge(world, layer ? 53 : 79, 31 + layer * 9) / 2;
        floors[x] = bottom - 8 - ridge(world, layer ? 47 : 81, 117 + layer * 3);
      }
      columns(c(layer ? 0x182536 : 0x1c2b3c), tops, false);
      columns(c(layer ? 0x182735 : 0x223543), floors, true);
    }
    int scroll = camera_x / 4;
    for (int i = floor_div(scroll, 57) - 1; i < floor_div(scroll + w, 57) + 2;
         ++i) {
      uint32_t n = hash(i + 722);
      int x = i * 57 - scroll, y = bottom - 25 - int(n % 48),
          h = 12 + int(n % 17);
      if (n % 3 == 0) {
        diamond(x, y, 4, h, c(0x38636a), c(0x79b1a4));
        diamond(x + 7, y + 7, 2, 13, c(0x2c4e5c), c(0x527d88));
        if (cave_sparkles && (animation / 180 + n) % 17 < 3)
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
  for (int y = first_y; y < bottom; ++y)
    box(0, y, w, 1, blend(top, low, (y - first_y) * 255 / std::max(1, bottom - first_y)));
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
      tops[x] = h;
      floors[x] = r;
    }
    columns(fill, tops, true);
    for (int x = 0; x < w; ++x) {
      int h = tops[x], r = floors[x];
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
// Average a wide terrain span and interpolate in pixels. A one-tile cliff
// under the player must not teleport the distant mountains or tree line.
inline int surface_level(const starsoil::State &s, int world_x) {
  int tile_x = floor_div(world_x, 16), fraction = world_x - tile_x * 16;
  int sum = 0;
  for (int dx = -16; dx <= 16; ++dx)
    sum += s.heights[std::clamp(tile_x + dx, 0, s.width - 1)];
  int next = sum - s.heights[std::clamp(tile_x - 16, 0, s.width - 1)] +
             s.heights[std::clamp(tile_x + 17, 0, s.width - 1)];
  return (sum * (16 - fraction) + next * fraction) / 33;
}
inline int cave_opacity(int world_y, int surface) {
  return std::clamp((world_y - surface - 32) * 255 / 96, 0, 255);
}
template <class Rect, class Line, class Pixel, class Read>
void draw_world(const starsoil::State &s, int w, int bottom, int camera_x,
                int camera_y, uint32_t animation, Rect rect, Line line,
                Pixel pixel, Read read, int first_y = 25, bool cave_sparkles = true) {
  int ground = surface_level(s, camera_x + w / 2);
  int base = bottom - 47;
  int horizon = base + floor_div(ground - camera_y - base, 6);
  // Surface scenery is chosen by longitude, never the player's jump height.
  int biome = starsoil::biome_at((camera_x + w / 2) / 16, 0, s.width);
  if (cave_opacity(camera_y + first_y, ground) < 255)
    draw(w, bottom, camera_x, camera_y, horizon, 0, biome, s.clock,
         animation, rect, line, pixel, first_y);
  auto dot = [&](int x, int y, uint16_t color) {
    if (unsigned(x) >= unsigned(w) || y < first_y || y >= bottom) return;
    int alpha = cave_opacity(camera_y + y, ground);
    if (alpha) pixel(x, y, alpha == 255 ? color : blend(read(x, y), color, alpha));
  };
  auto box = [&](int x, int y, int ww, int hh, uint16_t color) {
    int end_x = std::min(w, x + ww), end_y = std::min(bottom, y + hh);
    x = std::max(0, x);
    for (int row = std::max(first_y, y); row < end_y; ++row) {
      int alpha = cave_opacity(camera_y + row, ground);
      if (alpha == 255) rect(x, row, std::max(0, end_x - x), 1, color);
      else if (alpha) {
        // A distant layer contains long spans of equal pixels. Blend each
        // distinct run once, retaining the exact RGB565 floor rounding.
        int col = x;
        while (col < end_x) {
          const uint16_t old = read(col, row);
          int end = col + 1;
          while (end < end_x && read(end, row) == old) ++end;
          rect(col, row, end - col, 1, blend(old, color, alpha));
          col = end;
        }
      }
    }
  };
  auto stroke = [&](int x, int y, int ex, int ey, uint16_t color) {
    int dx = std::abs(ex - x), sx = x < ex ? 1 : -1;
    int dy = -std::abs(ey - y), sy = y < ey ? 1 : -1, error = dx + dy;
    for (;;) {
      dot(x, y, color);
      if (x == ex && y == ey) break;
      int e = error * 2;
      if (e >= dy) { error += dy; x += sx; }
      if (e <= dx) { error += dx; y += sy; }
    }
  };
  if (cave_opacity(camera_y + bottom - 1, ground))
    draw(w, bottom, camera_x, camera_y, horizon, 8, biome, s.clock,
         animation, box, stroke, dot, std::max(first_y, ground + 32 - camera_y), cave_sparkles);
}

// Sparkles lie inside separate crystal clusters (57 pixels apart). They can
// be applied after the static cave geometry, retaining the original blend
// against the already painted crystal at surface/cave transitions.
template <class Pixel, class Read>
void draw_cave_sparkles(int width, int bottom, int camera_x, int camera_y,
                       int ground, uint32_t animation, Pixel pixel, Read read,
                       int first_y = 25) {
  if (!cave_opacity(camera_y + bottom - 1, ground)) return;
  const int scroll = camera_x / 4;
  for (int index = floor_div(scroll, 57) - 1;
       index < floor_div(scroll + width, 57) + 2; ++index) {
    const uint32_t value = hash(index + 722);
    if (value % 3 || (animation / 180 + value) % 17 >= 3) continue;
    const int x = index * 57 - scroll;
    const int y = bottom - 22 - int(value % 48);
    if (x < 0 || x >= width || y < first_y || y >= bottom) continue;
    const int alpha = cave_opacity(camera_y + y, ground);
    if (alpha)
      pixel(x, y, alpha == 255 ? c(0xb2e3c0) : blend(read(x, y), c(0xb2e3c0), alpha));
  }
}
} // namespace starsoil_scene
