// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace starsoil_color {
// Exact floor(n / 255) for RGB565 channel products (0 <= n <= 63 * 255).
// Keep this in 32-bit shift/add arithmetic: S3 has no native integer divide.
constexpr unsigned div255(unsigned n) { return (n + 1 + (n >> 8)) >> 8; }
constexpr uint16_t dim(uint16_t c, unsigned light) {
  return uint16_t((div255(((c >> 11) & 31) * light) << 11) |
                  (div255(((c >> 5) & 63) * light) << 5) |
                  div255((c & 31) * light));
}
constexpr uint16_t mix(uint16_t a, uint16_t b, unsigned k) {
  return uint16_t((div255(((a >> 11) & 31) * (255 - k) + ((b >> 11) & 31) * k) << 11) |
                  (div255(((a >> 5) & 63) * (255 - k) + ((b >> 5) & 63) * k) << 5) |
                  div255((a & 31) * (255 - k) + (b & 31) * k));
}
} // namespace starsoil_color
