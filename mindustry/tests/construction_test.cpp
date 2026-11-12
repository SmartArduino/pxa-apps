// SPDX-License-Identifier: GPL-3.0-only
#include "../core.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace md;
int checks = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)
State world() {
  State s;
  s.generate(154, 0, false);
  for (int i = 0; i < MaxBuildings; ++i)
    if (s.buildings[i].type > Core)
      s.remove(i);
  s.researched = (1u << Blocks) - 1;
  for (auto &n : s.stock)
    n = 4000;
  return s;
}
void ticks(State &s, int n) {
  for (int i = 0; i < n; ++i)
    s.step();
}
int main() {
  auto s = world();
  auto copper = s.stock[Copper];
  int f = s.queueBuild(GroundFactory, 27, 32);
  CHECK(f >= 0 && s.buildCount == 1 && s.buildings[f].buildState == 1);
  CHECK(s.stock[Copper] == copper && s.buildings[f].progress == 0);
  CHECK(!s.accept(f, Silicon, 0));
  ticks(s, 20);
  CHECK(s.buildings[f].buildState == 2);
  CHECK(s.buildings[f].progress > 0 && s.buildings[f].progress < BuildComplete);
  CHECK(s.stock[Copper] < copper && s.stock[Copper] > copper - 50);
  CHECK(!s.buildings[f].powered && !s.unitCount());
  std::array<uint8_t, 3900> bytes{}, again{};
  auto n = s.save(bytes.data(), bytes.size());
  CHECK(n > 0 && bytes[3] == '8');
  State restored;
  CHECK(restored.load(bytes.data(), n));
  CHECK(restored.save(again.data(), again.size()) == n &&
        !std::memcmp(bytes.data(), again.data(), n));
  ticks(s, 95);
  CHECK(s.buildings[f].buildState == 2);
  ticks(s, 1);
  CHECK(!s.buildings[f].buildState && s.buildCount == 0);
  CHECK(s.stock[Copper] == copper - 50 && s.stock[Lead] == 3880 &&
        s.stock[Silicon] == 3920);
  ticks(restored, 96);
  CHECK(!restored.buildCount);
  for (auto count : restored.buildings[restored.at(27, 32)].inv)
    CHECK(count == 0);
  auto lack = world();
  for (auto &v : lack.stock)
    v = 0;
  int id = lack.queueBuild(GroundFactory, 27, 32);
  CHECK(id >= 0);
  ticks(lack, 20);
  CHECK(lack.buildings[id].buildState == 1);
  lack.stock[Copper] = 10;
  lack.stock[Lead] = 120;
  lack.stock[Silicon] = 80;
  ticks(lack, 50);
  CHECK(lack.buildings[id].buildState == 2 && lack.stock[Copper] == 0);
  auto progress = lack.buildings[id].progress;
  ticks(lack, 20);
  CHECK(lack.buildings[id].progress == progress);
  lack.stock[Copper] = 40;
  ticks(lack, 120);
  CHECK(!lack.buildings[id].buildState);
  // A plan beyond the original 220-world-unit reach remains a harmless ghost.
  auto range = world();
  for (int y = 3; y < 6; ++y)
    for (int x = 3; x < 6; ++x)
      range.tiles[y * W + x] = {Stone, None};
  id = range.queueBuild(Generator, 3, 3);
  CHECK(id >= 0);
  range.buildings[id].inv[Coal] = 0;
  ticks(range, 20);
  CHECK(range.buildings[id].buildState == 1 && range.buildings[id].timer == 0);
  CHECK(range.walkable(3, 3));
  range.playerX = range.targetX = 10;
  range.playerY = range.targetY = 10;
  ticks(range, 20);
  CHECK(!range.buildings[id].buildState);
  // Pending plans can be cancelled without spending or refunding anything.
  auto cancel = world();
  id = cancel.queueBuild(Press, 27, 31);
  auto funds = cancel.stock[Copper];
  CHECK(cancel.queueRemove(id));
  CHECK(cancel.at(27, 31) == -1 && !cancel.buildCount);
  CHECK(cancel.stock[Copper] == funds);
  // Started construction is dismantled by Alpha and refunds invested work.
  id = cancel.queueBuild(GroundFactory, 27, 32);
  ticks(cancel, 20);
  CHECK(cancel.queueRemove(id));
  CHECK(cancel.buildings[id].buildState == 3);
  n = cancel.save(bytes.data(), bytes.size());
  CHECK(n > 0 && restored.load(bytes.data(), n));
  ticks(cancel, 25);
  CHECK(cancel.at(27, 32) == -1 && !cancel.buildCount);
  // Ordinary demolition takes the original build time and rounds half costs.
  auto dismantle = world();
  id = dismantle.build(Duo, 27, 31, 0, true);
  auto stock = dismantle.stock[Copper];
  dismantle.buildings[id].ammoLeft = 2;
  CHECK(dismantle.queueRemove(id));
  n = dismantle.save(bytes.data(), bytes.size());
  CHECK(n > 0 && restored.load(bytes.data(), n));
  CHECK(dismantle.stock[Copper] == stock &&
        dismantle.buildings[id].buildState == 3);
  ticks(dismantle, 12);
  CHECK(dismantle.at(27, 31) == -1);
  CHECK(dismantle.stock[Copper] == 4000); // capped core
  dismantle.stock[Copper] = 100;
  id = dismantle.build(Duo, 27, 31, 0, true);
  CHECK(dismantle.queueRemove(id));
  ticks(dismantle, 12);
  CHECK(dismantle.stock[Copper] == 118); // round(35 * .5)
  // Whole lines reserve space and queue slots atomically, without upfront cost.
  auto line = world();
  funds = line.stock[Copper];
  CHECK(line.queueLine(Conveyor, 20, 30, 24, 30) == 5);
  CHECK(line.buildCount == 5 && line.stock[Copper] == funds);
  CHECK(line.queueLine(Conveyor, 19, 30, 23, 30) == -1 && line.buildCount == 5);
  ticks(line, 4);
  CHECK(!line.buildCount && line.stock[Copper] == funds - 5);
  // Compact save indices preserve execution order when low-numbered slots free.
  auto order = world();
  int lower = order.build(Wall, 20, 30, 0, true);
  int first = order.queueBuild(GroundFactory, 27, 32);
  order.remove(lower);
  int second = order.queueBuild(Press, 27, 28);
  CHECK(second < first);
  n = order.save(bytes.data(), bytes.size());
  CHECK(n > 0 && restored.load(bytes.data(), n));
  CHECK(restored.buildings[restored.buildQueue[0]].type == GroundFactory);
  CHECK(restored.buildings[restored.buildQueue[1]].type == Press);
  // Infinite-resource sandbox retains the original immediate completion rule.
  auto sandbox = world();
  sandbox.sandbox = true;
  id = sandbox.queueBuild(GroundFactory, 27, 32);
  funds = sandbox.stock[Copper];
  sandbox.step();
  CHECK(!sandbox.buildings[id].buildState && sandbox.stock[Copper] == funds);
  std::printf("%d construction checks passed: Alpha range/speed, incremental "
              "costs, no early production, queues, dismantling and saves\n",
              checks);
}
