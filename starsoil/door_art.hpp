// SPDX-License-Identifier: MIT
#pragma once
#include "color_math.hpp"
#include "items.hpp"
namespace starsoil_door {
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
// One native 16x32 door, split only when painting the two world cells. The
// middle has no extra frame or duplicated handle. Closed doors show the edge
// of the leaf; open doors turn their broad panel toward the camera.
constexpr uint16_t pixel(bool open, int x, int y) {
  constexpr auto ink = rgb(38, 51, 66), wood = rgb(158, 111, 75),
                 edge = rgb(210, 161, 111), shade = rgb(105, 73, 57),
                 gold = rgb(240, 205, 130);
  if (!open) {
    if ((x == 9 || x == 10) && y == 17) return gold;
    if (x < 6 || x > 9) return 0;
    if (y == 0 || y == 31 || x == 6) return ink;
    if ((y == 6 || y == 25) && x < 8) return rgb(105,118,120);
    if (x == 7) return edge;
    if (x == 8) return wood;
    if (x == 9) return shade;
    return 0;
  }
  if (x == 0 || x == 15 || y == 0 || y == 31) return ink;
  if (x == 1 || y == 1) return edge;
  if (x == 14 || y == 30) return shade;
  if ((x == 11 || x == 12) && y == 17) return gold;
  if (x >= 4 && x <= 11) {
    if (y == 4 || y == 19) return shade;
    if (y == 13 || y == 27) return edge;
    if (((y > 4 && y < 13) || (y > 19 && y < 27)) && x == 4) return shade;
    if (((y > 4 && y < 13) || (y > 19 && y < 27)) && x == 11) return edge;
  }
  return wood;
}
template <class Plot>
void draw(int tile, int sx, int sy, int light, Plot plot) {
  bool open = tile == starsoil::OpenDoor || tile == starsoil::OpenDoorTop;
  int top = tile == starsoil::DoorTop || tile == starsoil::OpenDoorTop ? 0 : 16;
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) {
      uint16_t c = pixel(open, x, top + y);
      if (c) plot(sx + x, sy + y, starsoil_color::dim(c, unsigned(light)));
    }
}
} // namespace starsoil_door
