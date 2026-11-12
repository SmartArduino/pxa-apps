// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace unciv;
State world() {
  State s;
  for (auto &t : s.tiles)
    t = {Grass, 0, 0, 7, 7};
  for (int p = 0; p < 2; ++p) {
    int u = add_unit(s, p, Settler, 10 * W + 8 + 12 * p);
    assert(found(s, u));
  }
  return s;
}
void queues() {
  auto s = world();
  int c = 0;
  auto &city = s.cities[c];
  assert(city.job == NoJob);
  assert(produce(s, c, Warrior));
  city.production = 7;
  assert(produce(s, c, Monument) && city.production == 0 &&
         city.invested[Warrior] == 7);
  city.production = 5;
  assert(produce(s, c, Warrior) && city.production == 7 &&
         city.invested[Monument] == 5);
  assert(enqueue(s, c, Monument) && !enqueue(s, c, Monument));
  assert(enqueue(s, c, Scout));
  assert(reorder_queue(s, c, 1, -1) && city.queue[0] == Scout &&
         city.queue[1] == Monument);
  assert(produce(s, c, Monument) && city.production == 5 && city.queued == 1 &&
         city.queue[0] == Scout && valid(s));
  assert(enqueue(s, c, Worker));
  assert(remove_queue(s, c, 0) && city.queued == 1 && city.queue[0] == Worker);
  city.production = costs[Monument] - 1;
  int output = production_yield(s, c);
  end_turn(s);
  assert(city.buildings & 1);
  assert(city.job == Worker && city.queued == 0 &&
         city.production == output - 1);
  assert(produce(s, c, Warrior));
  assert(repeat_production(s, c));
  city.production = costs[Warrior] - 1;
  end_turn(s);
  assert(city.job == Warrior && city.repeat && city.production == 1);
  assert(repeat_production(s, c));
  city.production = costs[Warrior] - 1;
  end_turn(s);
  assert(city.job == NoJob && !city.repeat && city.production == 1);
  int overflow = city.production;
  end_turn(s);
  assert(city.production == overflow);
  assert(enqueue(s, c, Scout) && city.job == Scout && !city.queued &&
         city.production >= overflow);
  for (int n = 0; n < QUEUE; ++n)
    assert(enqueue(s, c, Worker));
  assert(!enqueue(s, c, Warrior));
  assert(city.queued == 9);
  assert(!reorder_queue(s, c, 0, -1) && !reorder_queue(s, c, QUEUE - 1, 1) &&
         !remove_queue(s, c, QUEUE));
  uint8_t bytes[2048], again[2048];
  State r;
  size_t len = serialize(s, bytes, sizeof(bytes));
  assert(len == 1752 && restore(r, bytes, len));
  assert(r.cities[c].queued == 9 &&
         r.cities[c].invested[Warrior] == city.invested[Warrior]);
  assert(serialize(r, again, sizeof(again)) == len &&
         !memcmp(bytes, again, len));
  auto overflowed = world();
  assert(produce(overflowed, 0, Warrior));
  overflowed.cities[0].production = 1000;
  end_turn(overflowed);
  assert(overflowed.cities[0].job == NoJob &&
         overflowed.cities[0].production == costs[Warrior]);
  auto bought = world();
  bought.civ[0].gold = 1000;
  assert(produce(bought, 0, Warrior) && enqueue(bought, 0, Monument));
  assert(purchase(bought, 0) && bought.cities[0].job == Monument &&
         bought.cities[0].production == 0);
  assert(purchase(bought, 0) && bought.cities[0].job == NoJob);
}
void food() {
  assert(growth_threshold(1) == 15 && growth_threshold(2) == 24 &&
         growth_threshold(3) == 33 && growth_threshold(4) == 44);
  auto s = world();
  auto &c = s.cities[0];
  assert(food_consumption(s, 0) == 2 && growth_surplus(s, 0) == 2);
  c.food = 14;
  end_turn(s);
  assert(c.pop == 2 && c.food == 1);
  for (int i = 0; i < 4; ++i)
    s.tiles[neighbor(c.tile, i)].terrain = Hill;
  c.pop = 3;
  int16_t worked[18];
  worked_tiles(s, 0, worked);
  assert(yields(s, 0).food >= food_consumption(s, 0));
  assert(city_focus(s, 0, ProductionFocus));
  assert(growth_surplus(s, 0) < 0);
  end_turn(s);
  assert(c.pop == 2 && c.food == 0);
}
void borders() {
  auto s = world();
  int home = s.cities[0].tile, enemy = s.cities[1].tile;
  int edge = enemy - 2, unit = add_unit(s, 0, Scout, enemy - 3);
  assert(!can_enter(s, 0, edge));
  int16_t r[N];
  reachable(s, unit, r);
  assert(r[edge] < 0 && !move(s, unit, edge));
  assert(diplomacy(s, 0, 1, true) && can_enter(s, 0, edge) &&
         move(s, unit, edge));
  int other = add_unit(s, 1, Warrior, home + 1);
  assert(other >= 0);
  s.units[unit].order = Healing;
  s.units[unit].fortified = 1;
  assert(diplomacy(s, 0, 1, false) && !war(s, 0, 1) && s.truce[0] == 10);
  assert(s.units[unit].tile != edge && can_enter(s, 0, s.units[unit].tile) &&
         !s.units[unit].moves && s.units[unit].order == Manual);
  assert(can_enter(s, 1, s.units[other].tile) && !s.units[other].moves &&
         valid(s));
  int8_t claims[N];
  territory(s, claims);
  for (int t = 0; t < N; ++t)
    assert(claims[t] == working_city(s, t));
  // If the entire land is controlled by the other civilization, rejected peace
  // must preserve the old war state and unit positions.
  auto blocked = world();
  for (auto &t : blocked.tiles)
    t.terrain = Ocean;
  int tile = blocked.cities[1].tile, tile2 = neighbor(tile, 3);
  blocked.tiles[blocked.cities[0].tile].terrain = Grass;
  blocked.tiles[tile].terrain = blocked.tiles[tile2].terrain = Grass;
  assert(add_unit(blocked, 0, Worker, blocked.cities[0].tile) >= 0);
  int prisoner = add_unit(blocked, 0, Warrior, tile2);
  assert(prisoner >= 0);
  blocked.wars = 1;
  assert(valid(blocked));
  auto before = blocked;
  assert(!diplomacy(blocked, 0, 1, false) && blocked.wars == 1 &&
         blocked.units[prisoner].tile == before.units[prisoner].tile);
}
void saves(const char *path) {
  uint8_t bytes[2048];
  FILE *f = fopen(path, "rb");
  assert(f);
  size_t n = fread(bytes, 1, sizeof(bytes), f);
  fclose(f);
  State s;
  assert(n == 2010 && bytes[3] == 5 && restore(s, bytes, n));
  assert(s.cities[0].production == 9 && s.cities[0].locked &&
         !s.cities[0].queued && !s.cities[0].repeat);
  bool heal = false, work = false;
  for (const auto &u : s.units)
    if (u.tile >= 0) {
      heal |= u.order == Healing && u.xp == 30 && u.promotions == 1;
      work |= u.work_left == 4;
    }
  assert(heal && work && serialize(s, bytes, sizeof(bytes)) == 1752 &&
         bytes[3] == 7);
  State odd;
  odd.width = 17;
  odd.height = 13;
  for (int t = 0; t < N; ++t)
    odd.tiles[t] =
        inside(odd, t) ? Tile{Forest, 1, 1, 7, 0} : Tile{Ocean, 0, 0, 0, 0};
  n = serialize(odd, bytes, sizeof(bytes));
  State r;
  assert(n && restore(r, bytes, n));
  for (int t = 0; t < N; ++t)
    if (inside(odd, t))
      assert(r.tiles[t].terrain == Forest && r.tiles[t].improvement == 1 &&
             r.tiles[t].resource && r.tiles[t].seen == 7);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  queues();
  food();
  borders();
  saves(argv[1]);
  puts("Production banks, ten-entry queues, overflow/repeat, food growth, "
       "closed borders, atomic peace and v5 migration passed");
}
