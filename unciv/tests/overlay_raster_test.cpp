// SPDX-License-Identifier: MIT
#include "../art.hpp"
#include "../overlay_raster.hpp"
#include <array>
#include <assert.h>
#include <stdio.h>
int main() {
  // Every opaque ground pixel must survive under transparent trees and rocks.
  for (const auto *im : {&art::forest, &art::hill, &art::mountain}) {
    assert(im->w == art::grassland.w && im->h >= art::grassland.h);
    int offset = im->h - art::grassland.h;
    for (int y = 0; y < art::grassland.h; ++y)
      for (int x = 0; x < im->w; ++x)
        if (art::grassland.alpha[y * im->w + x] >= 60)
          assert(im->alpha[(y + offset) * im->w + x] >= 60);
  }
  constexpr uint16_t background = 0x1234, guard = 0x9876, ink = 0xffff;
  std::array<uint16_t, 12 * 12 + 2> storage;
  storage.fill(background);
  storage.front() = storage.back() = guard;
  overlay_raster::Surface view{storage.data() + 1, 12, 12, 2, 10};
  uint8_t opaque[4] = {255, 255, 255, 255};
  overlay_raster::mask(view, -3, -3, 20, 20, opaque, 2, 2, ink, 256);
  for (int y = 0; y < 12; ++y)
    for (int x = 0; x < 12; ++x)
      assert(view.pixels[y * 12 + x] == (y >= 2 && y < 10 ? ink : background));
  std::fill(view.pixels, view.pixels + 144, uint16_t(0));
  uint8_t coverage[] = {0, 128, 255};
  overlay_raster::mask(view, 1, 3, 6, 2, coverage, 3, 1, ink, 256);
  assert(view.pixels[3 * 12 + 1] == 0);
  assert(view.pixels[3 * 12 + 3] == 0x7bef);
  assert(view.pixels[3 * 12 + 5] == ink);
  std::fill(view.pixels, view.pixels + 144, background);
  for (int x : {-15, 0, 5, 15})
    for (int y : {-15, 0, 5, 15}) {
      overlay_raster::stroke(view, x, y, 11 - x, 11 - y, 3, ink);
      overlay_raster::mask(view, x, y, 8, 8, opaque, 2, 2, ink, 128);
    }
  for (int y : {0, 1, 10, 11})
    for (int x = 0; x < 12; ++x)
      assert(view.pixels[y * 12 + x] == background);
  assert(storage.front() == guard && storage.back() == guard);
  puts("Movement overlays: clipped edges, alpha coverage and framebuffer "
       "guards passed");
}
