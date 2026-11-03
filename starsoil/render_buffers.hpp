// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace starsoil_buffers {
// The largest output is square (480 x 480), where the logical scene is
// 288 x 288. Taller/wider logical scenes have smaller output buffers.
constexpr unsigned Pixels = 2u * 480 * 480 + 288u * 288;
inline bool fits(unsigned surface_pixels, unsigned scene_pixels) {
  return 2u * surface_pixels + scene_pixels <= Pixels;
}
} // namespace starsoil_buffers
