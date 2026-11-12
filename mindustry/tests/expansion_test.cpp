// SPDX-License-Identifier: GPL-3.0-only
#include "../core.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
using namespace md;
int checks = 0;
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
  State water;
  water.generate(154, 0, true);
  CHECK(!water.canBuild(Pump, 22, 30));
  CHECK(water.canBuild(Pump, 16, 34));
  CHECK(!water.canBuild(Wall, 16, 34));
  int pump = water.build(Pump, 16, 34, 0);
  CHECK(pump >= 0);
  CHECK(water.buildLine(Conduit, 17, 34, 18, 34) == 2);
  int drill = water.build(Pneumatic, 19, 33, 0);
  CHECK(drill >= 0);
  ticks(water, 100);
  CHECK(water.buildings[drill].inv[Copper] > 4);
  CHECK(water.buildings[drill].water > 0);
  auto filled = water.buildings[drill].inv[Copper];
  CHECK(water.remove(water.at(18, 34)));
  ticks(water, 80);
  CHECK(water.buildings[drill].inv[Copper] >= filled);
  CHECK(water.buildings[drill].water == 0 ||
        water.buildings[drill].inv[Copper] == 10);
  State dry, wet;
  dry.generate(154, 0, true);
  dry.remove(dry.at(21, 35));
  wet = dry;
  int id = dry.at(19, 35);
  wet.buildings[id].water = 50;
  // Refill the test reservoir without manufacturing water in the production
  // path.
  int dc = 0, wc = 0;
  for (int i = 0; i < 400; ++i) {
    wet.buildings[id].water = 50;
    dry.step();
    wet.step();
    dc += dry.buildings[id].inv[Copper];
    wc += wet.buildings[id].inv[Copper];
    dry.buildings[id].inv[Copper] = wet.buildings[id].inv[Copper] = 0;
  }
  CHECK(wc > dc * 2);
  CHECK(wc < dc * 3 + 3);
  State conservation;
  conservation.generate(154, 0, true);
  CHECK(conservation.buildLine(Conduit, 21, 30, 25, 30) == 5);
  conservation.buildings[conservation.at(21, 30)].water = 200;
  for (int i = 0; i < 100; ++i) {
    conservation.liquidStep();
    int sum = 0;
    for (auto &b : conservation.buildings) {
      sum += b.water;
      CHECK(b.water <= liquidCapacity(Block(b.type)));
    }
    CHECK(sum == 200);
  }
  State lines;
  lines.generate(154);
  auto coins = lines.stock[Copper];
  CHECK(lines.buildLine(Conveyor, 21, 30, 25, 30) == 5);
  CHECK(lines.stock[Copper] == coins - 5);
  coins = lines.stock[Copper];
  CHECK(lines.buildLine(Conveyor, 20, 30, 24, 30) == -1);
  CHECK(lines.stock[Copper] == coins);
  CHECK(lines.at(20, 30) == -1);
  CHECK(lines.buildLine(Conveyor, 20, 29, 24, 30) == -1);
  State ti;
  ti.generate(154, 0, true);
  CHECK(!ti.canBuild(Drill, 17, 22));
  CHECK(ti.canBuild(Pneumatic, 17, 22));
  int miner = ti.build(Pneumatic, 17, 22, 0);
  ticks(ti, 250);
  CHECK(ti.buildings[miner].inv[Titanium] > 0);
  State kiln;
  kiln.generate(154, 0, true);
  int k = kiln.build(Kiln, 27, 31, 0);
  CHECK(k >= 0);
  kiln.buildings[k].inv[Lead] = 3;
  kiln.buildings[k].inv[Sand] = 3;
  ticks(kiln, 30);
  CHECK(kiln.buildings[k].inv[Metaglass] == 0);
  int gen = kiln.build(Generator, 26, 31, 0);
  CHECK(gen >= 0);
  kiln.buildings[gen].inv[Coal] = 10;
  ticks(kiln, 30);
  CHECK(kiln.buildings[k].inv[Metaglass] == 3);
  State army;
  army.generate(154, 0, true);
  int factory = army.build(GroundFactory, 27, 32, 0);
  CHECK(factory >= 0);
  army.buildings[factory].inv[Lead] = 10;
  army.buildings[factory].inv[Silicon] = 10;
  ticks(army, 20);
  CHECK(army.buildings[factory].progress == 0);
  int g1 = army.build(Generator, 26, 32, 0),
      g2 = army.build(Generator, 26, 33, 0);
  CHECK(g1 >= 0 && g2 >= 0);
  army.buildings[g1].inv[Coal] = 30;
  army.buildings[g2].inv[Coal] = 30;
  ticks(army, 299);
  CHECK(army.unitCount() == 0);
  CHECK(army.buildings[factory].progress == 299 * WorkStep);
  army.step();
  CHECK(army.unitCount() == 1);
  CHECK(army.buildings[factory].inv[Lead] == 0 &&
        army.buildings[factory].inv[Silicon] == 0);
  CHECK(army.buildings[factory].progress == 0);
  // Production pauses without power and resumes from saved progress, without
  // paying twice.
  army.buildings[factory].inv[Lead] = 10;
  army.buildings[factory].inv[Silicon] = 10;
  ticks(army, 80);
  auto progress = army.buildings[factory].progress;
  army.buildings[g1].inv[Coal] = army.buildings[g2].inv[Coal] = 0;
  army.buildings[g1].timer = army.buildings[g2].timer = 0;
  ticks(army, 20);
  CHECK(army.buildings[factory].progress == progress);
  std::array<uint8_t, 3900> bytes{}, copy{};
  auto n = army.save(bytes.data(), bytes.size());
  State restored;
  CHECK(restored.load(bytes.data(), n));
  CHECK(restored.unitCount() == 1);
  CHECK(restored.buildings[factory].progress == progress);
  CHECK(restored.save(copy.data(), copy.size()) == n);
  CHECK(std::memcmp(bytes.data(), copy.data(), n) == 0);
  army.buildings[g1].inv[Coal] = army.buildings[g2].inv[Coal] = 20;
  ticks(army, (FactoryTime - progress + WorkStep - 1) / WorkStep);
  CHECK(army.unitCount() == 2);
  CHECK(army.buildings[factory].inv[Silicon] == 0);
  // A full unit roster holds a completed batch without consuming its inputs.
  for (auto &u : army.units)
    u = {24, 31, 150, 0, 0};
  army.buildings[factory].inv[Lead] = 10;
  army.buildings[factory].inv[Silicon] = 10;
  army.buildings[factory].progress = FactoryTime;
  army.step();
  CHECK(army.buildings[factory].progress == FactoryTime);
  CHECK(army.buildings[factory].inv[Lead] == 10);
  army.units[0].hp = 0;
  army.step();
  CHECK(army.unitCount() == MaxUnits);
  CHECK(army.buildings[factory].inv[Lead] == 0);
  State fight;
  fight.generate(154, 0, true);
  fight.units[0] = {24, 30, 150, 0, 0};
  fight.enemies[0] = {24, 21, 18, 0, 0};
  ticks(fight, 6);
  CHECK(!fight.enemies[0].hp);
  CHECK(fight.kills == 1);
  State airCombat;
  airCombat.generate(154, 0, true);
  airCombat.units[0] = {24, 30, 70, 1, 0};
  airCombat.enemies[0] = {24, 21, 150, 0, 0};
  airCombat.step();
  CHECK(airCombat.enemies[0].hp == 134);
  CHECK(airCombat.units[0].burst == 2);
  n = airCombat.save(bytes.data(), bytes.size());
  CHECK(restored.load(bytes.data(), n));
  CHECK(restored.units[0].burst == 2);
  ticks(restored, 2);
  CHECK(restored.enemies[0].hp == 102);
  CHECK(restored.units[0].burst == 0);
  // Factory output cannot pass through solid blocks. Flying units ignore this
  // obstruction.
  State blocked;
  blocked.generate(154, 0, true);
  int f = blocked.build(GroundFactory, 27, 31, 0);
  CHECK(f >= 0);
  for (int y = 30; y <= 34; ++y)
    for (int x = 26; x <= 30; ++x)
      if (x == 26 || x == 30 || y == 30 || y == 34)
        blocked.build(Wall, x, y, 0);
  CHECK(!blocked.spawnUnit(f));
  blocked.buildings[f].type = AirFactory;
  CHECK(blocked.spawnUnit(f));
  CHECK(blocked.units[0].kind == 1);
  // Ground troops navigate to a distant rally point around a solid wall.
  State nav;
  nav.generate(154, 0, true);
  nav.command = Command::Rally;
  nav.rallyX = 25.5f;
  nav.rallyY = 26.5f;
  nav.units[0] = {23.5f, 26.5f, 150, 0, 0};
  nav.buildLine(Wall, 24, 24, 24, 28);
  ticks(nav, 220);
  CHECK(nav.units[0].x > 25);
  CHECK(nav.walkable(int(nav.units[0].x), int(nav.units[0].y)));
  State tech;
  tech.generate(154);
  for (auto &v : tech.stock)
    v = 4000;
  CHECK(!tech.research(Pump));
  CHECK(tech.research(Press));
  CHECK(tech.research(Kiln));
  CHECK(tech.research(Pump));
  CHECK(tech.researched & (1u << Pump));
  CHECK(tech.research(Smelter));
  CHECK(tech.research(GroundFactory));
  // Upper-bound saves retain all buildings, enemies, units, water and
  // inventories.
  State dense;
  dense.generate(154, 0, true);
  for (int y = 3; y < 43; ++y)
    for (int x = 3; x < 43; ++x)
      dense.build(Duo, x, y, 0);
  for (auto &b : dense.buildings)
    if (b.type == Duo)
      b.inv[Copper] = b.inv[Graphite] = b.inv[Silicon] = 10;
  dense.startWave();
  for (int i = 0; i < MaxEnemies; ++i)
    dense.enemies[i] = {24, 5, 150, 0, 0};
  for (auto &u : dense.units)
    u = {24, 31, 150, 0, 0};
  dense.muted = true;
  dense.captured = 3;
  n = dense.save(bytes.data(), bytes.size());
  CHECK(n > 0 && n <= 3900);
  CHECK(restored.load(bytes.data(), n));
  CHECK(restored.muted && restored.captured == 3);
  CHECK(restored.unitCount() == MaxUnits);
  std::ifstream old("local/pxa-apps/mindustry/tests/fixtures/v0.1.mnd",
                    std::ios::binary);
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(old)), {});
  CHECK(!data.empty());
  CHECK(restored.load(data.data(), data.size()));
  CHECK(restored.wave == 1);
  CHECK(restored.stock[Copper] > 0);
  CHECK(restored.terrainVersion == 1);
  CHECK(restored.units[0].hp == 0);
  n = restored.save(bytes.data(), bytes.size());
  CHECK(n > 0);
  CHECK(bytes[3] == '8');
  CHECK(restored.load(bytes.data(), n));
  std::printf("%d expansion checks passed: liquids, boost, titanium, kiln, "
              "factories, troops, routing and 0.1 migration\n",
              checks);
}
