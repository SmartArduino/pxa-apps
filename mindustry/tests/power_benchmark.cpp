// SPDX-License-Identifier: GPL-3.0-only
// Compile with -I <app-source> against either the current or previous sources.
#include "core.hpp"
#include <chrono>
#include <cstdio>
using namespace md;
int main() {
  State s;
  s.generate(154, 0, true);
  for (int i = 0; i < MaxBuildings; ++i)
    if (s.buildings[i].type > Core)
      s.remove(i);
  for (auto &t : s.tiles)
    t = {Stone, None};
  s.rebuild();
  for (int y = 3; y < 43; ++y)
    for (int x = 3; x < 43; ++x)
      s.build((x + y) % 2 ? Node : Generator, x, y, 0, true);
  constexpr int count = 10000;
  for (auto &b : s.buildings)
    if (b.type == Generator)
      b.inv[Coal] = 40;
  s.step();
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < count; ++i) {
    if (!(i % 1000))
      for (auto &b : s.buildings)
        if (b.type == Generator)
          b.inv[Coal] = 40;
    s.step();
  }
  auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
  std::printf("{\"steps\":%d,\"microseconds\":%lld,\"us_per_step\":%.3f", count,
              static_cast<long long>(micros), double(micros) / count);
#ifdef MINDUSTRY_CACHE_TELEMETRY
  std::printf(",\"topology_rebuilds\":%u", s.powerRebuilds);
#endif
  std::printf("}\n");
}
