// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace starsoil_pixels {
// Store equal pixels in aligned pairs. Peeling the odd first pixel retains
// unaligned-safe behavior without accessing uint16_t storage through a word
// pointer. Fixed-size copies become checked WASM word stores.
inline void fill(uint16_t *out, size_t count, uint16_t color) {
  if (!count) return;
  if (reinterpret_cast<uintptr_t>(out) & 3u) { *out++ = color; --count; }
  auto *aligned = static_cast<uint16_t *>(__builtin_assume_aligned(out, 4));
  const uint32_t pair = uint32_t(color) | uint32_t(color) << 16;
  while (count >= 8) {
    __builtin_memcpy(aligned, &pair, 4); __builtin_memcpy(aligned + 2, &pair, 4);
    __builtin_memcpy(aligned + 4, &pair, 4); __builtin_memcpy(aligned + 6, &pair, 4);
    aligned += 8; count -= 8;
  }
  while (count >= 2) { __builtin_memcpy(aligned, &pair, 4); aligned += 2; count -= 2; }
  if (count) *aligned = color;
}
// Bounds are already clipped. Rows remain independent when the region is
// narrower than the surface stride; no padding or neighboring pixel changes.
inline void rectangle(uint16_t *out, int stride, int left, int top,
                      int right, int bottom, uint16_t color) {
  if (!out || right <= left || bottom <= top) return;
  size_t width = size_t(right - left);
  uint16_t *first = out + top * stride + left;
  if (left == 0 && right == stride) {
    fill(first, width * size_t(bottom - top), color);
    return;
  }
  if (width < 32) {
    for (int row = top; row < bottom; ++row)
      fill(out + row * stride + left, width, color);
    return;
  }
  fill(first, width, color);
  for (int row = top + 1; row < bottom; ++row)
    __builtin_memcpy(out + row * stride + left, first, width * sizeof(*out));
}
} // namespace starsoil_pixels
