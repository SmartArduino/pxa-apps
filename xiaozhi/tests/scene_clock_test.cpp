// SPDX-License-Identifier: Apache-2.0
#include "../scene_clock.hpp"
#include <cassert>
#include <cstdio>
int main() {
  constexpr unsigned scenes[] = {3, 3, 3, 3, 3, 0, 0, 0, 0, 1, 1, 1,
                                 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3};
  for (unsigned h = 0; h < 24; ++h) {
    assert(xiaozhi::scene_for_local_seconds(h * 3600) == scenes[h]);
    assert(xiaozhi::scene_for_local_seconds(h * 3600 + 3599) == scenes[h]);
    assert(xiaozhi::scene_for_local_seconds(h * 3600 + 86400) == scenes[h]);
  }
  assert(xiaozhi::scene_for_local_seconds(22 * 3600 + 8 * 3600) == 0);
  assert(xiaozhi::scene_for_local_seconds(2 * 3600 - 7 * 3600) == 2);
  assert(xiaozhi::scene_for_local_seconds(-1) == 3);
  puts("Scene clock: all hourly boundaries, day rollover and local offsets "
       "passed");
}
