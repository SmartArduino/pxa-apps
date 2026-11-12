// SPDX-License-Identifier: GPL-3.0-only
// Behavioral reference: v154 GenericCrafter, UnitFactory and PowerGraph.
#include "../core.hpp"
#include <array>
#include <cmath>
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
      std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)
State world() {
  State s;
  s.generate(154, 0, true);
  for (int i = 0; i < MaxBuildings; ++i)
    if (s.buildings[i].type > Core)
      s.remove(i);
  s.countdown = 0;
  return s;
}
void ticks(State &s, int n) {
  for (int i = 0; i < n; ++i)
    s.step();
}
int add(State &s, Block type, int x, int y) {
  int id = s.build(type, x, y, 0, true);
  CHECK(id >= 0);
  return id;
}
void factoryInputs(Building &b) {
  b.inv[Lead] = 20;
  b.inv[Silicon] = 20;
}
int main() {
  auto supplied = world();
  int sm = add(supplied, Smelter, 27, 31);
  supplied.stock[Coal] = supplied.stock[Sand] = 20;
  supplied.supply(sm);
  CHECK(supplied.buildings[sm].inv[Coal] == 10 &&
        supplied.buildings[sm].inv[Sand] == 10);
  CHECK(!supplied.accept(sm, Coal, 0));
  supplied.buildings[sm].inv[Sand] = 9;
  CHECK(supplied.accept(sm, Sand, 0));
  auto caps = world();
  int f = add(caps, GroundFactory, 27, 32);
  for (int i = 0; i < 20; ++i)
    CHECK(caps.accept(f, Lead, 0));
  CHECK(!caps.accept(f, Lead, 0));
  for (int i = 0; i < 20; ++i)
    CHECK(caps.accept(f, Silicon, 0));
  CHECK(!caps.accept(f, Silicon, 0));
  auto press = world();
  int p = add(press, Press, 27, 31);
  auto &pb = press.buildings[p];
  pb.inv[Coal] = 2;
  ticks(press, 29);
  CHECK(pb.inv[Graphite] == 0 && pb.inv[Coal] == 2);
  press.step();
  CHECK(pb.inv[Graphite] == 1 && pb.inv[Coal] == 0 && pb.progress == 0);
  pb.inv[Coal] = 2;
  ticks(press, 5);
  auto progress = pb.progress;
  pb.inv[Graphite] = 10;
  ticks(press, 50);
  CHECK(pb.progress == progress && pb.inv[Coal] == 2);
  pb.inv[Graphite] = 9;
  ticks(press, 25);
  CHECK(pb.inv[Graphite] == 10 && pb.inv[Coal] == 0);
  auto smelt = world();
  sm = add(smelt, Smelter, 27, 31);
  int gen = add(smelt, Generator, 26, 31);
  smelt.buildings[gen].inv[Coal] = 10;
  auto &sb = smelt.buildings[sm];
  sb.inv[Coal] = 3;
  sb.inv[Sand] = 6;
  ticks(smelt, 13);
  CHECK(sb.inv[Silicon] == 0 && sb.inv[Coal] == 3 && sb.inv[Sand] == 6);
  smelt.step();
  CHECK(sb.inv[Silicon] == 1 && sb.progress > 0);
  ticks(smelt, 26);
  CHECK(sb.inv[Silicon] == 3 && sb.inv[Coal] == 0 && sb.inv[Sand] == 0);
  smelt.step();
  CHECK(smelt.powerUsed == 0);
  // One 60-power generator supplies a 72-power factory at roughly 83% speed.
  auto brownout = world();
  f = add(brownout, GroundFactory, 27, 32);
  gen = add(brownout, Generator, 26, 32);
  factoryInputs(brownout.buildings[f]);
  brownout.buildings[gen].inv[Coal] = 10;
  ticks(brownout, 300);
  CHECK(brownout.unitCount() == 0 && brownout.buildings[f].progress > 0);
  CHECK(brownout.buildings[f].powerEfficiency >= 852 &&
        brownout.buildings[f].powerEfficiency <= 854);
  CHECK(brownout.buildings[f].inv[Silicon] == 20);
  ticks(brownout, 62);
  CHECK(brownout.unitCount() == 1 && brownout.buildings[f].inv[Silicon] == 10);
  auto battery = world();
  f = add(battery, GroundFactory, 27, 32);
  gen = add(battery, Generator, 26, 32);
  int bat = add(battery, Battery, 26, 33);
  factoryInputs(battery.buildings[f]);
  battery.buildings[gen].inv[Coal] = 10;
  battery.buildings[bat].timer = 6000;
  ticks(battery, 200);
  CHECK(battery.buildings[bat].timer == 4800);
  CHECK(battery.buildings[f].powerEfficiency == FullPower);
  // An idle, unsupplied factory must not empty the battery.
  auto idle = world();
  f = add(idle, GroundFactory, 27, 32);
  bat = add(idle, Battery, 26, 32);
  idle.buildings[bat].timer = 5000;
  ticks(idle, 100);
  CHECK(idle.powerUsed == 0 && idle.buildings[bat].timer == 5000);
  // Multiple batteries share draw in proportion to their stored charge.
  auto shared = world();
  f = add(shared, GroundFactory, 27, 32);
  int b1 = add(shared, Battery, 26, 32), b2 = add(shared, Battery, 26, 33);
  factoryInputs(shared.buildings[f]);
  shared.buildings[b1].timer = 7500;
  shared.buildings[b2].timer = 2500;
  ticks(shared, 100);
  CHECK(shared.buildings[b1].timer == 4800 &&
        shared.buildings[b2].timer == 1600);
  // Correct footprint adjacency: consumers cannot relay power to consumers.
  Building factory{}, generator{}, smelter{};
  factory.type = GroundFactory;
  factory.x = 27;
  factory.y = 31;
  generator.type = Generator;
  generator.x = 30;
  generator.y = 33;
  CHECK(powerConnects(factory, generator));
  generator.y = 34;
  CHECK(!powerConnects(factory, generator));
  smelter.type = Smelter;
  smelter.x = 25;
  smelter.y = 31;
  CHECK(!powerConnects(factory, smelter));
  auto cache = world();
  f = add(cache, GroundFactory, 27, 32);
  gen = add(cache, Generator, 26, 32);
  factoryInputs(cache.buildings[f]);
  cache.buildings[gen].inv[Coal] = 40;
  cache.step();
  auto rebuilds = cache.powerRebuilds;
  ticks(cache, 1000);
  CHECK(cache.powerRebuilds == rebuilds);
  CHECK(cache.queueRemove(gen));
  cache.step();
  CHECK(cache.powerRebuilds == rebuilds + 1);
  CHECK(!cache.buildings[f].powered);
  // Persist fractional production and battery energy exactly.
  std::array<uint8_t, 3900> bytes{}, again{};
  auto n = battery.save(bytes.data(), bytes.size());
  CHECK(n > 0 && bytes[3] == '8');
  State restored;
  CHECK(restored.load(bytes.data(), n));
  CHECK(restored.save(again.data(), again.size()) == n &&
        !std::memcmp(bytes.data(), again.data(), n));
  CHECK(restored.buildings[restored.at(26, 33)].timer == 4800);
  auto fractional = world();
  sm = add(fractional, Smelter, 27, 31);
  gen = add(fractional, Generator, 26, 31);
  int k = add(fractional, Kiln, 27, 28);
  int node = add(fractional, Node, 26, 30);
  (void)node;
  fractional.buildings[gen].inv[Coal] = 10;
  fractional.buildings[sm].inv[Coal] = 10;
  fractional.buildings[sm].inv[Sand] = 10;
  fractional.buildings[k].inv[Lead] = fractional.buildings[k].inv[Sand] = 10;
  ticks(fractional, 5);
  CHECK(fractional.buildings[sm].timer > 0);
  n = fractional.save(bytes.data(), bytes.size());
  CHECK(n > 0 && restored.load(bytes.data(), n));
  ticks(fractional, 30);
  ticks(restored, 30);
  CHECK(restored.buildings[restored.at(27, 31)].progress ==
        fractional.buildings[sm].progress);
  CHECK(restored.buildings[restored.at(27, 31)].inv[Silicon] ==
        fractional.buildings[sm].inv[Silicon]);
  // Actual 0.6 code emitted this fixture, including a queued construction site.
  std::ifstream old("local/pxa-apps/mindustry/tests/fixtures/v0.6.mnd",
                    std::ios::binary);
  std::vector<uint8_t> v5((std::istreambuf_iterator<char>(old)), {});
  CHECK(!v5.empty() && restored.load(v5.data(), v5.size()));
  CHECK(restored.buildings[restored.at(27, 32)].progress == 123 * WorkStep);
  CHECK(restored.buildings[restored.at(26, 34)].timer == 7770);
  CHECK(restored.buildings[restored.at(27, 28)].timer == 0 &&
        restored.buildings[restored.at(27, 28)].inv[Graphite] == 1);
  CHECK(restored.buildCount == 1 &&
        restored.buildings[restored.buildQueue[0]].buildState == 1);
  std::printf("%d production/power checks passed: mixed inputs, cycle timing, "
              "brownouts, batteries, footprint links, topology cache and MND5 "
              "migration\n",
              checks);
}
