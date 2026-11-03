// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <algorithm>
#include <cstdint>

namespace starsoil_scene {
// Rebuilt from current tiles each frame, including edits, moving players and
// day/night transitions. Emitters visit their small diamond once rather than
// every tile searching the same 11 x 11 neighborhood independently.
class Lighting {
  static constexpr int Capacity = 42;
  uint8_t cells[Capacity * Capacity]{};
  int left = 0, top = 0, width = 0, height = 0;
  static int ambient(const starsoil::State &s, int x, int y) {
    using namespace starsoil;
    int surface = s.heights[std::clamp(x, 0, s.width - 1)];
    bool night = s.clock > 240000 || s.clock < 25000;
    int light = y <= surface ? (night ? 112 : 255)
                             : std::max(28, 190 - (y - surface) * 23);
    int radius = s.equip[7] == GlowCharm                       ? 8
                 : items[s.bag[s.selected].item].tile == Torch ? 7
                                                               : 4;
    return std::clamp(
        std::max(light, 230 - (std::abs(x - int(s.player.x / 16)) +
                               std::abs(y - int(s.player.y / 16))) *
                                  255 / radius),
        28, 255);
  }
  static bool emissive(int t) {
    return t == starsoil::Torch || t == starsoil::Portal || t == starsoil::Lava;
  }

public:
  void prepare(const starsoil::State &s, int x, int y, int w, int h) {
    left = x;
    top = y;
    width = w > 0 && w <= Capacity ? w : 0;
    height = h > 0 && h <= Capacity ? h : 0;
    if (!width || !height)
      return;
    for (int yy = 0; yy < height; ++yy)
      for (int xx = 0; xx < width; ++xx)
        cells[yy * width + xx] = uint8_t(ambient(s, left + xx, top + yy));
    for (int yy = std::max(0, top - 5);
         yy <= std::min(s.height - 1, top + height + 4); ++yy)
      for (int xx = std::max(0, left - 5);
           xx <= std::min(s.width - 1, left + width + 4); ++xx) {
        if (!emissive(s.tiles[yy * starsoil::WorldW + xx]))
          continue;
        for (int dy = -5; dy <= 5; ++dy) {
          int row = yy + dy - top, reach = 5 - std::abs(dy);
          if (unsigned(row) >= unsigned(height))
            continue;
          for (int col = std::max(0, xx - reach - left);
               col <= std::min(width - 1, xx + reach - left); ++col) {
            int light = 255 - (std::abs(left + col - xx) + std::abs(dy)) * 38;
            auto &cell = cells[row * width + col];
            cell = uint8_t(std::max(int(cell), light));
          }
        }
      }
  }
  int at(const starsoil::State &s, int x, int y) const {
    if (unsigned(x - left) < unsigned(width) &&
        unsigned(y - top) < unsigned(height))
      return cells[(y - top) * width + x - left];
    int light = ambient(s, x, y);
    for (int yy = std::max(0, y - 5); yy <= std::min(s.height - 1, y + 5); ++yy)
      for (int xx = std::max(0, x - 5); xx <= std::min(s.width - 1, x + 5);
           ++xx)
        if (emissive(s.tiles[yy * starsoil::WorldW + xx]))
          light =
              std::max(light, 255 - (std::abs(x - xx) + std::abs(y - yy)) * 38);
    return std::clamp(light, 28, 255);
  }
};
} // namespace starsoil_scene
