// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace starsoil {
struct HeaderButton {int x,y,w,h;};
inline HeaderButton pause_button_bounds(int width) {return {width-33,8,22,18};}
inline HeaderButton back_button_bounds(int width) {return {width-61,8,22,18};}
inline bool header_button_hit(int x,int y,HeaderButton button) {
  // Keep the active area below the system's top notification gesture strip.
  return x>=button.x-2 && x<button.x+button.w+2 && y>=14 && y<28;
}
inline bool pause_button_hit(int x, int y, int width) {
  return header_button_hit(x,y,pause_button_bounds(width));
}
// UI pointer payloads are density-independent canvas units; Surface viewports
// use display pixels. Convert exactly once, before viewport mapping/hit tests.
inline int display_coordinate(int32_t value, uint32_t density_q16) {
  int64_t n = int64_t(value) * (density_q16 ? density_q16 : 65536u);
  return int(n >= 0 ? (n + 32768) / 65536 : -((-n + 32768) / 65536));
}
} // namespace starsoil
