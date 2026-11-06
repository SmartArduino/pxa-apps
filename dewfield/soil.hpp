// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include "palette.hpp"
#include <array>
#include <cstdint>
namespace soil {
constexpr int RootX = 8, RootY = 10;
constexpr int root_x(int plot) {
  return (farm::PlotX + plot % farm::PlotW) * farm::Tile + RootX;
}
constexpr int root_y(int plot) {
  return (farm::PlotY + plot / farm::PlotW) * farm::Tile + RootY;
}
struct Colors {
  uint16_t tones[4], edge, lip, hole, clod;
};
inline constexpr Colors Dry{
    {farm_palette::DrySoil, farm_palette::rgb(118, 77, 46),
     farm_palette::rgb(160, 112, 68), farm_palette::rgb(132, 86, 50)},
    farm_palette::rgb(121, 84, 51), farm_palette::rgb(163, 117, 70),
    farm_palette::rgb(101, 66, 41), farm_palette::rgb(154, 103, 59)};
inline constexpr Colors Wet{
    {farm_palette::WetSoil, farm_palette::rgb(76, 52, 38),
     farm_palette::rgb(110, 77, 52), farm_palette::rgb(86, 59, 41)},
    farm_palette::rgb(77, 56, 41), farm_palette::rgb(117, 83, 53),
    farm_palette::rgb(65, 45, 34), farm_palette::rgb(102, 69, 44)};
constexpr const Colors &colors(bool wet) { return wet ? Wet : Dry; }
constexpr uint32_t hash(unsigned x, unsigned y) {
  uint32_t h = x * 374761393u + y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
// Precomputed native pixel clusters: short broken grooves, crumbly clods and
// sparse highlights. The 36x28 period crosses 16px tile boundaries.
constexpr auto make_texture() {
  std::array<uint8_t, 36 * 28> result{};
  for (int y = 0; y < 28; ++y)
    for (int x = 0; x < 36; ++x) {
      auto h = hash(x / 9, y / 7);
      int dx = x % 9 - (2 + int(h % 4));
      int dy = y % 7 - (1 + int((h >> 4) % 3));
      uint8_t tone = 0;
      if (dx >= 0 && dx <= 2 && dy >= 0 && dy <= 1)
        tone = dy == 0 ? 2 : 1;
      if ((h & 8) && dy == 2 && dx >= -2 && dx <= 0)
        tone = 3;
      if ((h & 16) && dx >= -2 && dx <= 1 &&
          dy == -1 + int(dx > 0 && (h & 32)))
        tone = 1;
      result[y * 36 + x] = tone;
    }
  return result;
}
inline constexpr auto Texture = make_texture();
inline uint8_t tone(int x, int y) { return Texture[(y % 28) * 36 + x % 36]; }
} // namespace soil
