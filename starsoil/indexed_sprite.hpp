// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include <array>
#include <cstddef>
#include <iterator>
#include <utility>

namespace starsoil_actor {
static_assert(art::HumanWidth <= 32 && art::HumanHeight <= 32,
              "Packed character coordinates use five bits per axis");
// Each command stores a nontransparent pixel: x[0:4], y[5:9], index[10:13].
// Keeping the original row order preserves all layer and hand attachments.
struct IndexedImage {
  const uint16_t *pixels = nullptr;
  uint16_t count = 0;
};
inline constexpr uint16_t empty_pixel = 0;
template <const auto &Images, size_t I> constexpr size_t count_pixels() {
  size_t count = 0;
  for (auto pixel : Images[I])
    if (pixel) ++count;
  return count;
}
template <const auto &Images, size_t I> constexpr auto encode_pixels() {
  std::array<uint16_t, count_pixels<Images, I>()> pixels{};
  size_t count = 0;
  for (int y = 0; y < art::HumanHeight; ++y)
    for (int x = 0; x < art::HumanWidth; ++x) {
      uint8_t value = Images[I][y * art::HumanWidth + x];
      if (value)
        pixels[count++] = uint16_t(x | (y << 5) | (value << 10));
    }
  return pixels;
}
// Compile each image separately to stay within the WASM constexpr step limit.
// The transparent bitmaps are no longer needed in the runtime image.
template <const auto &Images, size_t I>
inline constexpr auto encoded_pixels = encode_pixels<Images, I>();
template <const auto &Images, size_t... I>
constexpr auto image_table(std::index_sequence<I...>) {
  return std::array<IndexedImage, sizeof...(I)>{{
      {encoded_pixels<Images, I>.empty() ? &empty_pixel
                                        : encoded_pixels<Images, I>.data(),
       uint16_t(encoded_pixels<Images, I>.size())}...}};
}
template <const auto &Images>
inline constexpr auto images =
    image_table<Images>(std::make_index_sequence<std::size(Images)>{});
template <const auto &Images> IndexedImage image(size_t row) {
  return images<Images>[row];
}
} // namespace starsoil_actor
