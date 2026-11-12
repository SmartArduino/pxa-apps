// SPDX-License-Identifier: GPL-3.0-only
#include "../core.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace md;
static int checks = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)
void ticks(State &s, int n) {
  for (int i = 0; i < n; ++i)
    s.step();
}
int main() {
  State s;
  s.generate(154);
  CHECK(s.core() >= 0);
  CHECK(s.at(23, 35) == s.at(25, 37));
  CHECK(s.at(-1, 0) == -1);
  CHECK(!s.canBuild(Duo, 24, 36));
  CHECK(!s.canBuild(Drill, 23, 30));
  CHECK(!s.canBuild(Wall, 0, 0));
  auto copper = s.stock[Copper];
  ticks(s, 500);
  CHECK(s.stock[Copper] > copper + 4);
  CHECK(s.stock[Lead] == 80);
  int drill = s.at(19, 35);
  CHECK(drill >= 0);
  CHECK(s.remove(s.at(21, 35)));
  copper = s.stock[Copper];
  ticks(s, 1000);
  CHECK(s.stock[Copper] <= copper + 1);
  CHECK(s.buildings[drill].inv[Copper] == 10);
  int b = s.build(Conveyor, 21, 35, 0);
  CHECK(b >= 0);
  ticks(s, 200);
  CHECK(s.stock[Copper] > copper + 3);
  State route;
  route.generate(4, 0, true);
  int a = route.build(Conveyor, 23, 26, 0);
  int target = route.build(Conveyor, 24, 26, 0);
  CHECK(a >= 0 && target >= 0);
  route.buildings[a].inv[Copper] = 1;
  route.step();
  CHECK(route.buildings[target].inv[Copper] == 1);
  route.step();
  CHECK(route.buildings[target].inv[Copper] == 1);
  CHECK(!route.accept(a, Copper, 2));
  CHECK(route.accept(a, Copper, 0));
  State prod;
  prod.generate(4, 0, true);
  int press = prod.build(Press, 23, 27, 0);
  CHECK(press >= 0);
  CHECK(prod.accept(press, Coal, 0));
  CHECK(prod.accept(press, Coal, 0));
  prod.step();
  CHECK(prod.buildings[press].inv[Coal] == 2);
  CHECK(prod.buildings[press].inv[Graphite] == 0);
  ticks(prod, 29);
  CHECK(prod.buildings[press].inv[Coal] == 0);
  CHECK(prod.buildings[press].inv[Graphite] == 1);
  int smelter = prod.build(Smelter, 27, 31, 0);
  CHECK(smelter >= 0);
  prod.buildings[smelter].inv[Coal] = 1;
  prod.buildings[smelter].inv[Sand] = 2;
  ticks(prod, 50);
  CHECK(prod.buildings[smelter].inv[Silicon] == 0);
  int generator = prod.build(Generator, 26, 31, 0);
  CHECK(generator >= 0);
  prod.buildings[generator].inv[Coal] = 3;
  ticks(prod, 20);
  CHECK(prod.buildings[smelter].powered);
  CHECK(prod.buildings[smelter].inv[Silicon] > 0);
  // Independent networks cannot share generation without a physical link.
  int distant = prod.build(Smelter, 33, 31, 0);
  CHECK(distant >= 0);
  prod.buildings[distant].inv[Coal] = 1;
  prod.buildings[distant].inv[Sand] = 2;
  ticks(prod, 10);
  CHECK(!prod.buildings[distant].powered);
  CHECK(prod.buildings[distant].inv[Silicon] == 0);
  int node = prod.build(Node, 31, 32, 0);
  CHECK(node >= 0);
  ticks(prod, 20);
  CHECK(prod.buildings[distant].powered);
  CHECK(prod.buildings[distant].inv[Silicon] == 1);
  State batteries;
  batteries.generate(4, 0, true);
  int gen = batteries.build(Generator, 25, 31, 0),
      bat = batteries.build(Battery, 26, 31, 0),
      load = batteries.build(Mender, 27, 31, 0);
  CHECK(gen >= 0 && bat >= 0 && load >= 0);
  batteries.buildings[gen].inv[Coal] = 1;
  ticks(batteries, 35);
  CHECK(batteries.buildings[bat].timer > 0);
  ticks(batteries, 10);
  CHECK(batteries.buildings[gen].timer == 0);
  CHECK(batteries.buildings[load].powered);
  ticks(batteries, 200);
  CHECK(batteries.buildings[bat].timer == 0);
  CHECK(!batteries.buildings[load].powered);
  // A copper item provides two shots, including the final item in the hopper.
  State ammo;
  ammo.generate(2, 0, true);
  ammo.playerX = ammo.targetX = 4; ammo.playerY = ammo.targetY = 4;
  int gun = ammo.build(Duo, 23, 32, 0);
  CHECK(gun >= 0);
  ammo.buildings[gun].inv[Copper] = 1;
  ammo.enemies[0] = {24, 28, 150, 0, 0};
  ticks(ammo, 8);
  CHECK(ammo.enemies[0].hp == 132);
  CHECK(ammo.buildings[gun].inv[Copper] == 0);
  State fight;
  fight.generate(2, 0, true);
  fight.playerX = fight.targetX = 4; fight.playerY = fight.targetY = 4;
  int turret = fight.build(Duo, 23, 32, 0);
  CHECK(turret >= 0);
  fight.enemies[0] = {24, 25, 150, 0, 0};
  ticks(fight, 5);
  CHECK(fight.enemies[0].hp == 150);
  fight.buildings[turret].inv[Copper] = 20;
  ticks(fight, 180);
  CHECK(!fight.enemies[0].hp);
  CHECK(fight.kills > 0);
  State enemy;
  enemy.generate(154);
  CHECK(enemy.startWave());
  CHECK(!enemy.startWave());
  ticks(enemy, 15000);
  CHECK(enemy.lost);
  State research;
  research.generate(1);
  CHECK(!research.unlocked(Press));
  research.stock[Copper] = 500;
  research.stock[Lead] = 200;
  CHECK(research.research(Press));
  CHECK(!research.research(Press));
  CHECK(!research.research(Smelter));
  research.stock[Graphite] = 10;
  CHECK(research.research(Smelter));
  std::array<uint8_t, 3900> data{}, other{};
  size_t n = s.save(data.data(), data.size());
  CHECK(n > 0 && n <= 3900);
  State restored;
  CHECK(restored.load(data.data(), n));
  CHECK(restored.stock[Copper] == s.stock[Copper]);
  CHECK(restored.ticks == s.ticks);
  CHECK(restored.buildings[drill].inv[Copper] ==
        s.buildings[drill].inv[Copper]);
  CHECK(restored.save(other.data(), other.size()) == n);
  CHECK(std::memcmp(data.data(), other.data(), n) == 0);
  for (size_t len = 0; len < n; ++len)
    CHECK(!restored.load(data.data(), len));
  for (size_t i = 0; i < n; ++i) {
    data[i] ^= 0x80;
    CHECK(!restored.load(data.data(), n));
    data[i] ^= 0x80;
  }
  auto en = enemy.save(data.data(), data.size());
  CHECK(restored.load(data.data(), en));
  CHECK(restored.lost);
  State max;
  max.generate(4, 0, true);
  for (int y = 3; y < 43; ++y)
    for (int x = 3; x < 43; ++x)
      max.build(Wall, x, y, 0);
  CHECK(max.startWave());
  n = max.save(data.data(), data.size());
  CHECK(n > 0 && n <= 3900);
  CHECK(restored.load(data.data(), n));
  for (int seed = 0; seed < 30; ++seed) {
    State sim;
    sim.generate(seed);
    for (int y = 3; y < 42; ++y)
      for (int x = 3; x < 42; ++x)
        if (noise(seed, x, y) % 19 == 0)
          sim.build(Wall, x, y, 0);
    sim.startWave();
    ticks(sim, 1800);
    n = sim.save(data.data(), data.size());
    CHECK(restored.load(data.data(), n));
  }
  std::printf("%d checks passed; logistics, production, power, combat, save "
              "integrity and 30 seeded worlds\n",
              checks);
}
