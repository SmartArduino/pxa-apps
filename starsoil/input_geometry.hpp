// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace starsoil {
// UI pointer payloads are density-independent canvas units; Surface viewports
// use display pixels. Convert exactly once, before viewport mapping/hit tests.
inline int display_coordinate(int32_t value, uint32_t density_q16) {
  int64_t n = int64_t(value) * (density_q16 ? density_q16 : 65536u);
  return int(n >= 0 ? (n + 32768) / 65536 : -((-n + 32768) / 65536));
}
} // namespace starsoil
