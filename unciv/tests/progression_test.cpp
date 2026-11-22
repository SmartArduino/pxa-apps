// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../turn_log.hpp"
#include <assert.h>
#include <initializer_list>
#include <stdio.h>
#include <string.h>
using namespace unciv;
State world() {
  State s;
  for (auto &t : s.tiles)
    t = {Grass, 0, 0, 7, 7};
  for (int p = 0; p < 2; ++p) {
    int u = add_unit(s, p, Settler, 10 * W + 10 + p * 10);
    assert(found(s, u));
  }
  return s;
}
void experience() {
  auto s = world();
  int at = 10 * W + 11, target = neighbor(at, 0);
  int a = add_unit(s, 0, Warrior, at), b = add_unit(s, 1, Warrior, target);
  s.wars = 1;
  s.units[a].xp = 5;
  vision(s);
  auto p = battle_preview(s, a, target);
  assert(attack(s, a, target));
  assert(s.units[a].hp == p.attacker_after &&
         s.units[b].hp == p.defender_after);
  assert(s.units[a].xp == 10 && s.units[b].xp == 4 &&
         earned_promotions(s.units[a]) == 1);
  assert(promote(s, a, OpenCombat) && !promote(s, a, Medic));
  s.units[a].moves = 2;
  auto improved = battle_preview(s, a, target);
  auto copy = s;
  copy.units[a].promotions = 0;
  auto plain = battle_preview(copy, a, target);
  assert(improved.attacker_strength > plain.attacker_strength);
  s.tiles[target].terrain = Forest;
  auto rough = battle_preview(s, a, target);
  auto ordinary = battle_preview(copy, a, target);
  assert(rough.attacker_strength == ordinary.attacker_strength);
  s.units[a].xp = 30;
  assert(promote(s, a, RoughCombat));
  assert(battle_preview(s, a, target).attacker_strength >
         ordinary.attacker_strength);
  s.units[a].xp = 60;
  assert(promote(s, a, Medic));
  s.units[a].xp = 100;
  s.units[a].hp = 40;
  assert(promote(s, a, Recover) && s.units[a].hp == 90 && !s.units[a].moves);
  s.units[a].xp = 127;
  assert(!promote(s, a, Recover) && !promote(s, a, OpenCombat));
  uint8_t bytes[2048], other[2048];
  size_t n = serialize(s, bytes, sizeof(bytes));
  State restored;
  assert(n == 1752 && bytes[3] == 7 && restore(restored, bytes, n));
  assert(restored.units[a].xp == 127 &&
         restored.units[a].promotions == s.units[a].promotions);
  assert(serialize(restored, other, sizeof(other)) == n &&
         !memcmp(bytes, other, n));
  auto civilian = world();
  a = add_unit(civilian, 0, Warrior, at);
  b = add_unit(civilian, 1, Worker, target);
  civilian.wars = 1;
  vision(civilian);
  assert(attack(civilian, a, target) && civilian.units[a].xp == 0 &&
         civilian.units[b].xp == 0);
  assert(!promote(civilian, b, OpenCombat));
}
void upgrades() {
  auto s = world();
  int u = add_unit(s, 0, Warrior, 10 * W + 11);
  s.civ[0].gold = 100;
  assert(upgrade_kind(s, u) == Swordsman && upgrade_cost(s, u) == 52 &&
         !upgrade(s, u));
  s.civ[0].tech |= 1 << Iron;
  s.units[u].xp = 30;
  s.units[u].promotions = 1;
  s.units[u].hp = 57;
  s.units[u].moves = 1;
  assert(!upgrade(s, u));
  s.units[u].moves = 2;
  s.units[u].tile = 10 * W + 15;
  assert(!upgrade(s, u));
  s.units[u].tile = 10 * W + 11;
  s.civ[0].gold = 51;
  assert(!upgrade(s, u) && s.civ[0].gold == 51);
  s.civ[0].gold = 100;
  assert(upgrade(s, u) && s.civ[0].gold == 48 && s.units[u].kind == Swordsman &&
         !s.units[u].moves);
  assert(s.units[u].hp == 57 && s.units[u].xp == 30 &&
         s.units[u].promotions == 1);
  assert(!upgrade(s, u) && upgrade_kind(s, u) == -1 && valid(s));
}
void orders() {
  auto s = world();
  int u = add_unit(s, 0, Warrior, 10 * W + 11);
  s.units[u].hp = 40;
  assert(heal(s, u) && s.units[u].order == Healing && !s.units[u].moves &&
         s.units[u].fortified);
  assert(cancel_order(s, u) && !s.units[u].moves && !s.units[u].fortified);
  end_turn(s);
  assert(s.units[u].hp == 40);
  assert(heal(s, u));
  end_turn(s);
  assert(s.units[u].hp == 60 && s.units[u].order == Healing);
  uint8_t bytes[2048];
  State r;
  size_t n = serialize(s, bytes, sizeof(bytes));
  assert(restore(r, bytes, n) && r.units[u].order == Healing);
  r.units[u].hp = 88;
  end_turn(r);
  assert(r.units[u].hp == 100 && r.units[u].order == Manual &&
         !r.units[u].fortified && r.units[u].moves == 2);
  assert(!heal(r, u));
  r.units[u].hp = 50;
  assert(unciv::wait(r, u));
  end_turn(r);
  assert(r.units[u].order == Manual && r.units[u].hp == 50 &&
         r.units[u].moves == 2);
  r.units[u].xp = 10;
  assert(promote(r, u, Medic) && heal(r, u));
  end_turn(r);
  assert(r.units[u].hp == 78);
  int worker = add_unit(r, 0, Worker, neighbor(r.cities[0].tile, 1));
  assert(improve(r, worker));
  r.units[worker].moves = 2;
  assert(!heal(r, worker) && !unciv::wait(r, worker));
  assert(valid(r));
  auto moved = world();
  int m = add_unit(moved, 0, Warrior, 10 * W + 11);
  moved.units[m].hp = 40;
  assert(move(moved, m, neighbor(moved.units[m].tile, 1)) && heal(moved, m));
  n = serialize(moved, bytes, sizeof(bytes));
  State loaded;
  assert(n && restore(loaded, bytes, n) && loaded.units[m].acted);
  end_turn(loaded);
  assert(loaded.units[m].hp == 40 && loaded.units[m].order == Healing &&
         !loaded.units[m].acted);
  end_turn(loaded);
  assert(loaded.units[m].hp == 60);
}
void economy() {
  auto s = world();
  int c = 0;
  assert(!produce(s, c, Settler));
  s.cities[c].pop = 2;
  s.cities[c].food = 10;
  assert(produce(s, c, Settler));
  int raw = yields(s, c).production, total = production_yield(s, c);
  assert(total > raw && growth_surplus(s, c) == 0);
  end_turn(s);
  assert(s.cities[c].food == 10 && s.cities[c].pop == 2 &&
         s.cities[c].production == total);
  assert(produce(s, c, Worker));
  end_turn(s);
  assert(s.cities[c].food > 10);
  s.cities[c].buildings |= 8;
  s.civ[0].gold = 1000;
  assert(produce(s, c, Warrior));
  auto before = s;
  assert(purchase(s, c));
  int soldier = unit_at(s, s.cities[c].tile);
  assert(soldier >= 0 && s.units[soldier].xp == 15);
  TurnLog log;
  log.collect(before, s);
  bool ready = false;
  for (int i = 0; i < log.count; ++i)
    ready |= log.entries[i].type == TurnLog::PromotionReady;
  assert(ready);
  assert(produce(s, c, Worker) && purchase(s, c));
  bool worker = false;
  for (const auto &u : s.units)
    if (u.tile >= 0 && u.owner == 0 && u.kind == Worker) {
      assert(u.xp == 0);
      worker = true;
    }
  assert(worker && valid(s));
}
void elimination() {
  for (bool settler : {false, true}) {
    auto s = world();
    int city = s.cities[1].tile;
    int a = add_unit(s, 0, Swordsman, neighbor(city, 3));
    int remote = add_unit(s, 1, Warrior, city + 3);
    if (settler)
      add_unit(s, 1, Settler, city + 4);
    s.cities[1].hp = 1;
    s.wars = 1;
    vision(s);
    assert(attack(s, a, city));
    assert(s.cities[1].owner == 0 && ((s.units[remote].tile >= 0) == settler));
    assert(valid(s));
  }
}
void legacy(const char *path) {
  uint8_t data[2048];
  FILE *f = fopen(path, "rb");
  assert(f);
  size_t n = fread(data, 1, sizeof(data), f);
  fclose(f);
  State s;
  assert(n == 2010 && data[3] == 4 && restore(s, data, n));
  assert(s.cities[0].locked && s.cities[0].focus == ProductionFocus);
  bool fortified = false, work = false;
  for (const auto &u : s.units) {
    assert(!u.xp && !u.promotions && u.order == Manual);
    fortified |= u.tile >= 0 && u.fortified;
    work |= u.tile >= 0 && u.work_left == 4;
  }
  assert(fortified && work && serialize(s, data, sizeof(data)) == 1752 &&
         data[3] == 7);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  legacy(argv[1]);
  experience();
  upgrades();
  orders();
  economy();
  elimination();
  puts("XP, terrain promotions, upgrades, orders, settler food, barracks, "
       "elimination and v4 migration passed");
}
