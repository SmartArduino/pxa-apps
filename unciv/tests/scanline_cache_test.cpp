// SPDX-License-Identifier: MIT
#include "../scanline_cache.hpp"
#include <array>
#include <assert.h>
#include <stdio.h>
int main() {
  scanline_cache::Cache<7000, 3> cache;
  std::array<uint16_t, 19 * 17 + 2> actual, expected;
  uint16_t rgb[3][63];
  uint8_t alpha[3][63];
  for (int n = 0; n < 3; ++n)
    for (int k = 0; k < 63; ++k) {
      rgb[n][k] = uint16_t((k * 1349 + n * 139) & 0xffff);
      constexpr uint8_t levels[] = {0, 59, 60, 128, 255};
      alpha[n][k] = levels[(k + n) % 5];
    }
  for (int pass = 0; pass < 150; ++pass) {
    actual.fill(0x45ab);
    expected = actual;
    int n = pass % 3, w = 5 + pass % 31, h = 5 + pass % 29, x = pass % 30 - 12,
        y = pass % 24 - 10, dark = pass % 2 ? 130 : 0;
    assert(cache.blit({actual.data() + 1, 19, 17, 2, 15}, x, y, w, h, rgb[n],
                      alpha[n], 9, 7, dark));
    assert(cache.blit({actual.data() + 1, 19, 17, 2, 15}, x, y, w, h, rgb[n],
                      alpha[n], 9, 7, dark));
    for (int yy = 2; yy < 15; ++yy)
      for (int xx = 0; xx < 19; ++xx) {
        if (xx < x || xx >= x + w || yy < y || yy >= y + h)
          continue;
        int k = (yy - y) * 7 / h * 9 + (xx - x) * 9 / w;
        if (alpha[n][k] < 60)
          continue;
        uint16_t c =
            dark ? overlay_raster::blend(rgb[n][k], 0x08c5, dark) : rgb[n][k];
        expected[1 + yy * 19 + xx] = c ? c : 1;
      }
    assert(actual == expected);
    assert(cache.bytes() <= 14000);
  }
  assert(cache.evictions > 0);
  assert(cache.hits >= 150);
  auto before = actual;
  assert(!cache.blit({actual.data() + 1, 19, 17, 0, 17}, 0, 0, 129, 5, rgb[0],
                     alpha[0], 9, 7, 0));
  assert(actual == before);
  scanline_cache::Cache<4> tiny;
  assert(!tiny.blit({actual.data() + 1, 19, 17, 0, 17}, 0, 0, 4, 4, rgb[0],
                    alpha[0], 9, 7, 0));
  assert(actual == before);
  puts("Native sprite cache: pixel equivalence, transparent holes, clipping, "
       "eviction and fallback passed");
}
