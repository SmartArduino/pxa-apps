// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../turn_log.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace unciv;
State flat() {
  State s;
  for (auto &t : s.tiles)
    t = {Grass, 0, 0, 7, 7};
  return s;
}
int city(State &s, int owner, int tile) {
  int u = add_unit(s, owner, Settler, tile);
  assert(u >= 0 && found(s, u));
  return city_at(s, tile);
}
void movement_rules() {
  auto s = flat();
  int t = 10 * W + 10, u = add_unit(s, 0, Warrior, t), next = neighbor(t, 0);
  s.units[u].moves = 1;
  s.tiles[next].terrain = Hill;
  int16_t reach[N], prev[N];
  reachable(s, u, reach, prev);
  assert(reach[next] == 1 && prev[next] == t);
  assert(move(s, u, next) && !s.units[u].moves);
  reachable(s, u, reach);
  for (int n = 0; n < N; ++n)
    assert(n == next ? reach[n] == 0 : reach[n] < 0);
  s = flat();
  u = add_unit(s, 0, Scout, t);
  int enemy = neighbor(t, 0), destination = neighbor(t, 1);
  int e = add_unit(s, 1, Archer, enemy);
  for (int d = 0; d < 6; ++d) {
    int n = neighbor(t, d);
    if (n != enemy && n != destination)
      s.tiles[n].terrain = Mountain;
  }
  s.wars = 1;
  vision(s);
  assert(distance(enemy, destination) == 1 &&
         zone_of_control(s, 0, t, destination));
  reachable(s, u, reach, prev);
  assert(reach[destination] == 3 && prev[destination] == t);
  assert(move(s, u, destination) && !s.units[u].moves);
  // Leaving a control zone, civilians, peace and unseen units do not stop it.
  s.units[u].tile = t;
  s.units[u].moves = 3;
  assert(!zone_of_control(s, 0, t, neighbor(t, 3)));
  s.units[e].kind = Worker;
  assert(!zone_of_control(s, 0, t, destination));
  s.units[e].kind = Archer;
  s.wars = 0;
  assert(!zone_of_control(s, 0, t, destination));
  s.wars = 1;
  s.tiles[enemy].visible = 0;
  assert(!zone_of_control(s, 0, t, destination));
  s.units[e].tile = -1;
  city(s, 1, enemy);
  s.tiles[enemy].seen = 3;
  assert(zone_of_control(s, 0, t, destination));
  reachable(s, u, reach);
  assert(reach[destination] == 3);
}
void rest_and_fortification() {
  auto s = flat();
  int t = 10 * W + 10;
  city(s, 0, t);
  city(s, 1, t + 10);
  int u = add_unit(s, 0, Warrior, t + 1);
  s.units[u].hp = 20;
  assert(healing_rate(s, u) == 20 && healing_rate(s, u, t) == 25 &&
         healing_rate(s, u, t + 4) == 10);
  s.wars = 1;
  assert(healing_rate(s, u, t + 9) == 10);
  s.wars = 0;
  assert(fortify(s, u) && fortification_bonus(s.units[u]) == 0);
  end_turn(s);
  assert(s.units[u].hp == 40 && s.units[u].fortify_turns == 1 &&
         fortification_bonus(s.units[u]) == 20);
  end_turn(s);
  assert(s.units[u].hp == 60 && fortification_bonus(s.units[u]) == 40);
  end_turn(s);
  assert(s.units[u].hp == 80 && s.units[u].fortify_turns == 2);
  auto saved = s;
  uint8_t data[2048];
  size_t n = serialize(s, data, sizeof(data));
  State r;
  assert(n == 1752 && data[3] == 7 && restore(r, data, n) &&
         r.units[u].fortify_turns == 2);
  assert(wake(r, u) && !r.units[u].fortify_turns);
  int a = add_unit(saved, 1, Warrior, neighbor(saved.units[u].tile, 0));
  saved.wars = 1;
  vision(saved);
  auto bonus = battle_preview(saved, a, saved.units[u].tile);
  saved.units[u].fortify_turns = 0;
  assert(bonus.defender_strength >
         battle_preview(saved, a, saved.units[u].tile).defender_strength);
  assert(move(s, u, neighbor(s.units[u].tile, 1)) && !s.units[u].fortify_turns);
  s.units[u].hp = 20;
  assert(heal(s, u));
  end_turn(s);
  assert(s.units[u].hp == 20 && !s.units[u].fortify_turns);
  end_turn(s);
  assert(s.units[u].hp == 40 && s.units[u].fortify_turns == 1);
}
void pillage_and_repair() {
  auto s = flat();
  int home = 10 * W + 10, c = city(s, 0, home);
  city(s, 1, home + 10);
  int t = neighbor(home, 0);
  s.tiles[t].improvement = 1;
  int a = add_unit(s, 1, Warrior, t);
  s.units[a].hp = 40;
  assert(!pillage(s, a));
  s.wars = 1;
  auto before = s;
  int food = tile_yield(s, t).food;
  assert(pillage(s, a) && s.tiles[t].improvement == 3 && s.units[a].hp == 65 &&
         s.units[a].moves == 1);
  assert(s.civ[1].gold == 55 && tile_yield(s, t).food == food - 2 &&
         !pillage(s, a));
  TurnLog log;
  log.collect(before, s);
  bool notified = false;
  for (int i = 0; i < log.count; i++)
    notified |= log.entries[i].type == TurnLog::Pillaged;
  assert(notified);
  uint8_t data[2048];
  State r;
  auto n = serialize(s, data, sizeof(data));
  assert(n && restore(r, data, n) && r.tiles[t].improvement == 3);
  r.units[a].tile = -1;
  r.wars = 0;
  int worker = add_unit(r, 0, Worker, t);
  assert(improvement_turns(r, t) == 2 && improve(r, worker) &&
         r.units[worker].work_left == 2);
  end_turn(r);
  assert(r.tiles[t].improvement == 3 && r.units[worker].work_left == 1);
  end_turn(r);
  assert(r.tiles[t].improvement == 1 && !r.units[worker].work_left);
  // Captured mines can be repaired without possessing the original build tech.
  r.units[worker].moves = 2;
  r.tiles[t].terrain = Hill;
  r.tiles[t].improvement = 3;
  assert(improve(r, worker) && r.units[worker].project == 2);
  assert(valid(r) && r.cities[c].owner == 0);
}
void ai_raid_and_repair() {
  auto s = flat();
  int t = 10 * W + 10;
  city(s, 0, t);
  int c = city(s, 1, t + 10);
  int tile = neighbor(t, 0);
  s.tiles[tile].improvement = 1;
  s.wars = 1;
  int raider = add_unit(s, 1, Scout, tile);
  s.units[raider].hp = 30;
  vision(s);
  ai(s, 1);
  assert(s.tiles[tile].improvement == 3 && s.civ[1].gold == 55);
  // A worker should choose repair over an equally distant unmodified site.
  s = flat();
  t = 10 * W + 10;
  c = city(s, 1, t);
  city(s, 0, t + 10);
  tile = neighbor(t, 0);
  s.tiles[tile].improvement = 3;
  int worker = add_unit(s, 1, Worker, t);
  ai(s, 1);
  assert(s.units[worker].tile == tile && s.units[worker].work_left == 2 &&
         s.units[worker].project == 1);
  assert(valid(s) && s.cities[c].owner == 1);
}
void legacy(const char *path) {
  uint8_t data[2048];
  FILE *f = fopen(path, "rb");
  assert(f);
  size_t n = fread(data, 1, sizeof(data), f);
  fclose(f);
  assert(n == 1752 && data[3] == 6);
  State s;
  assert(restore(s, data, n));
  int u = unit_at(s, neighbor(10 * W + 3, 0));
  assert(u >= 0 && s.units[u].fortified && s.units[u].fortify_turns == 1 &&
         s.units[u].xp == 30);
  assert(s.cities[0].production == 11 && s.cities[0].queued == 1 &&
         s.cities[0].queue[0] == Worker);
  assert(serialize(s, data, sizeof(data)) == 1752 && data[3] == 7);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  movement_rules();
  rest_and_fortification();
  pillage_and_repair();
  ai_raid_and_repair();
  legacy(argv[1]);
  puts("Tactics: last movement, shared control zones, territorial healing, "
       "fortification, pillage/repair, AI and v6 migration passed");
}
