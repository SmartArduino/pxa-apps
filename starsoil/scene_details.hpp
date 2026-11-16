// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include "color_math.hpp"
#include "terrain.hpp"

namespace starsoil_details {
constexpr uint16_t rgb(uint32_t c) {
  return uint16_t(((c >> 8) & 0xf800) | ((c >> 5) & 0x7e0) | ((c >> 3) & 31));
}
template<class Rect>
void wall(int material, int mask, int x, int y, int light, Rect rect) {
  if (material != starsoil::Plank) return;
  auto box = [&](int xx, int yy, int w, int h, uint32_t c) {
    rect(x + xx, y + yy, w, h, starsoil_color::dim(rgb(c), unsigned(light)));
  };
  if (!(mask & 1)) { box(0,0,16,3,0x3b3030); box(1,3,14,1,0x926c4a); }
  if (!(mask & 8)) { box(0,0,3,16,0x473633); box(2,1,1,14,0xaa7951); }
  if (!(mask & 2)) { box(13,0,3,16,0x3b3030); box(13,2,1,12,0x7e5943); }
}
template<class Sample, class Rect>
void tile(int t, int tx, int ty, int x, int y, int light, Sample sample, Rect rect) {
  using namespace starsoil;
  auto box = [&](int xx, int yy, int w, int h, uint32_t c) {
    rect(x + xx, y + yy, w, h, starsoil_color::dim(rgb(c), unsigned(light)));
  };
  const int above = sample(tx, ty - 1);
  const uint32_t seed = hash(uint32_t(tx) * 131u + uint32_t(ty) * 977u);
  if (t == Plank) {
    if (!starsoil_terrain::terrain(above)) {
      box(0,0,16,1,0xe0b77d); box(0,1,16,2,0xa47951);
      box(0,3,16,1,0x694b3c); box(1,4,14,1,0xc29460);
      box(1,5,14,1,0x584135);
      if (seed % 3 == 0) box(11,1,1,1,0xf0d49c);
    }
    if (!starsoil_terrain::terrain(sample(tx - 1, ty))) box(0,2,1,14,0xc89b68);
    if (!starsoil_terrain::terrain(sample(tx + 1, ty))) box(15,3,1,13,0x584135);
  } else if (t == Grass && above == Air) {
    const int root = 2 + int(seed % 11);
    box(root,2,2,3,0x567c48); box(root,5,1,2,0x476640);
    if (seed % 9 == 0) {
      const int stem = 3 + int(seed % 9);
      box(stem,-3,1,4,0x6c9358);box(stem-1,-4,3,2,seed & 32 ? 0xe0bc78 : 0xcda7ba);
      box(stem,-4,1,1,0xf2dc9d);
    }
  } else if (t == Dirt && seed % 5 == 0) {
    box(3,10,5,1,0x73513d);box(7,11,4,1,0x5a4236);
    if (seed & 8) { box(11,5,3,2,0x8a7965);box(12,5,2,1,0xa18f73); }
  } else if (starsoil_terrain::terrain(t) && !starsoil_terrain::terrain(above)) {
    const bool cold = t == Snow || t == Ice;
    const uint32_t edge = cold ? 0xd7e7e1 : t == CrystalOre ? 0x87c2ba : 0x9babad;
    box(2,0,12,1,edge);box(3,1,8,1,cold ? 0xb1cccf : 0x75858d);
    if (seed % 3 == 0) { box(9,-1,3,1,cold ? 0xe1ece3 : 0x849399);box(10,-2,1,1,edge); }
  }
}
} // namespace starsoil_details
