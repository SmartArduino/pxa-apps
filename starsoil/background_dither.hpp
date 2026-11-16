// SPDX-License-Identifier: MIT
#pragma once
#include "color_math.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace starsoil_dither {
using Row = std::array<uint16_t, 4>;
// Screen-anchored Bayer ranks. No frame number or random state: a stationary
// gradient stays still and can be restored from the existing scene cache.
inline constexpr uint8_t Bayer[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
constexpr uint32_t mix_rgb(uint32_t a, uint32_t b, unsigned k) {
  uint32_t result = 0;
  for (unsigned shift : {0u, 8u, 16u})
    result |= starsoil_color::div255(((a >> shift) & 255) * (255-k) +
                                   ((b >> shift) & 255) * k) << shift;
  return result;
}
// Interpolate RGB888 first, retaining 1/16 of an RGB565 channel. All divides
// and quantization happen per row, never once per output pixel.
inline Row gradient_row(uint32_t a, uint32_t b, unsigned k, int y) {
  unsigned q[3];
  for (unsigned channel = 0; channel < 3; ++channel) {
    const unsigned shift = channel * 8, levels = channel == 1 ? 63 : 31;
    const unsigned weighted = ((a >> shift) & 255) * (256-k) + ((b >> shift) & 255) * k;
    q[channel] = (weighted * levels * 16 + 32640) / 65280;
  }
  Row result{};
  for (int x = 0; x < 4; ++x) {
    const unsigned rank = Bayer[y & 3][x];
    auto quantize = [&](unsigned v) { return (v >> 4) + ((v & 15) > rank); };
    result[x] = uint16_t((quantize(q[2]) << 11) | (quantize(q[1]) << 5) | quantize(q[0]));
  }
  return result;
}
inline uint16_t mix565(uint16_t a, uint16_t b, unsigned k, int x, int y) {
  const unsigned threshold = (2 * Bayer[y & 3][x & 3] + 1) * 255;
  auto channel = [&](unsigned av, unsigned bv) {
    const unsigned n = av * (255-k) + bv * k;
    const unsigned base = starsoil_color::div255(n);
    return base + ((n - base * 255) * 32 > threshold);
  };
  return uint16_t((channel(a >> 11, b >> 11) << 11) |
                  (channel((a >> 5) & 63, (b >> 5) & 63) << 5) |
                  channel(a & 31, b & 31));
}
// Four precomputed pixels repeat across a contiguous span. Aligned word
// copies keep the write count identical to a flat fill; no extra surface.
inline void fill(uint16_t *out, size_t count, const uint16_t *row, unsigned phase) {
  if (!count) return;
  phase &= 3;
  if (reinterpret_cast<uintptr_t>(out) & 3u) {
    *out++ = row[phase]; phase = (phase + 1) & 3; --count;
  }
  auto *aligned = static_cast<uint16_t *>(__builtin_assume_aligned(out, 4));
  uint32_t a = uint32_t(row[phase]) | (uint32_t(row[(phase+1)&3]) << 16);
  uint32_t b = uint32_t(row[(phase+2)&3]) | (uint32_t(row[(phase+3)&3]) << 16);
  while (count >= 8) {
    __builtin_memcpy(aligned, &a, 4); __builtin_memcpy(aligned+2, &b, 4);
    __builtin_memcpy(aligned+4, &a, 4); __builtin_memcpy(aligned+6, &b, 4);
    aligned += 8; count -= 8;
  }
  while (count >= 2) {
    __builtin_memcpy(aligned, &a, 4); aligned += 2; count -= 2;
    const auto old = a; a = b; b = old; phase = (phase+2)&3;
  }
  if (count) *aligned = row[phase];
}
struct Fallback {};
} // namespace starsoil_dither
