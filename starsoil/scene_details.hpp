// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include "color_math.hpp"
#include "terrain.hpp"

namespace starsoil_details {
constexpr uint16_t rgb(uint32_t c) {
  return uint16_t(((c >> 8) & 0xf800) | ((c >> 5) & 0x7e0) | ((c >> 3) & 31));
}
template<class Sample, class Rect>
void tile(int t, int tx, int ty, int x, int y, int light, Sample sample, Rect rect, bool sandstone=false) {
  using namespace starsoil;
  auto box = [&](int xx, int yy, int w, int h, uint32_t c) {
    rect(x + xx, y + yy, w, h, starsoil_color::dim(rgb(c), unsigned(light)));
  };
  const int above = sample(tx, ty - 1);
  const uint32_t seed = hash(uint32_t(tx) * 131u + uint32_t(ty) * 977u);
  if (t == Grass && above == Air) {
    const int root = 2 + int(seed % 11);
    box(root,2,2,3,0x567c48); box(root,5,1,2,0x476640);
    if (seed % 9 == 0) {
      const int stem = 3 + int(seed % 9);
      box(stem,-3,1,4,0x6c9358);box(stem-1,-4,3,2,seed & 32 ? 0xe0bc78 : 0xcda7ba);
      box(stem,-4,1,1,0xf2dc9d);
    }
  } else if (t == Dirt) {
    // All dirt takes this branch, including cells without extra grain.
    // Surface colour and contour belong to the authored material sprite.
    if (seed % 5 == 0) {
      box(3,10,5,1,0x73513d);box(7,11,4,1,0x5a4236);
      if (seed & 8) { box(11,5,3,2,0x8a7965);box(12,5,2,1,0xa18f73); }
    }
  } else if (t == Sand || sandstone) {
    // Sand has warm grains, never the blue-grey rock rim below. Keep every
    // detail inside the authored contour, including an isolated falling tile.
    if (above == Air || above == Cactus) {
      box(5,0,6,1,0xf0d297);box(6,1,4,1,0xe2bd7b);
      const int drift = 4 + int(seed % 5);
      box(drift,4,4,1,0xe6c68b);box(drift+1,5,3,1,0xc8a365);
    }
  }
  // Other materials already have their own exposed-edge highlights. Never
  // stamp a shared grey rim or floating pebble over timber, ore, snow or moss.
}
} // namespace starsoil_details
