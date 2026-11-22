// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <assert.h>
#include <stdio.h>
using namespace unciv;
State flat() {
  State s;
  for (auto &t : s.tiles)
    t = {Grass, 0, 0, 7, 7};
  return s;
}
int city(State &s, int o, int t) {
  int u = add_unit(s, o, Settler, t);
  assert(u >= 0 && found(s, u));
  return city_at(s, t);
}
void ranged_position() {
  auto s = flat();
  int t = 10 * W + 10;
  city(s, 1, t - 6);
  city(s, 0, t + 8);
  int a = add_unit(s, 1, Archer, t),
      enemy = add_unit(s, 0, Warrior, neighbor(t, 0));
  s.wars = 1;
  vision(s);
  ai(s, 1);
  assert(s.units[enemy].hp < 100 && s.units[a].hp == 100);
  assert(s.units[a].tile != t &&
         distance(s.units[a].tile, s.units[enemy].tile) == 2);
  assert(!s.units[a].moves && valid(s));
}
void advance_attack_and_visibility() {
  auto s = flat();
  int t = 10 * W + 10;
  city(s, 1, t - 6);
  city(s, 0, t + 8);
  int a = add_unit(s, 1, Warrior, t), enemy = add_unit(s, 0, Scout, t + 2);
  s.wars = 1;
  vision(s);
  auto hidden = s;
  hidden.tiles[t + 2].visible &= ~2;
  ai(hidden, 1);
  assert(hidden.units[enemy].hp == 100);
  ai(s, 1);
  assert(s.units[enemy].hp < 100 && s.units[a].tile != t && !s.units[a].moves);
  assert(valid(s));
}
void production_and_research() {
  auto s = flat();
  int t = 10 * W + 10, c = city(s, 1, t);
  city(s, 0, t + 8);
  s.cities[c].buildings = 15;
  s.civ[1].tech |=
      (1 << Pottery) | (1 << Mining) | (1 << Archery) | (1 << Writing);
  add_unit(s, 1, Worker, t + W);
  add_unit(s, 1, Warrior, t - 2);
  add_unit(s, 1, Warrior, t - 3);
  ai(s, 1);
  assert(s.cities[c].job == NoJob); // Peace army cap stops endless recruitment.
  int enemy = add_unit(s, 0, Warrior, t + 2);
  s.wars = 1;
  s.cities[c].buildings &= ~2;
  assert(produce(s, c, Granary));
  s.cities[c].production = 11;
  vision(s);
  ai(s, 1);
  assert(s.cities[c].job == Archer && s.cities[c].invested[Granary] == 11);
  assert(s.units[enemy].tile >= 0 && valid(s));
  auto research = flat();
  int rc = city(research, 1, t);
  city(research, 0, t + 8);
  add_unit(research, 0, Warrior, t + 2);
  research.wars = 1;
  vision(research);
  ai(research, 1);
  assert(research.civ[1].research == Archery &&
         research.cities[rc].job == Warrior);
}
void settlement() {
  auto s = flat();
  int t = 10 * W + 12;
  city(s, 1, t - 8);
  city(s, 0, t + 12);
  for (auto &tile : s.tiles)
    tile.terrain = Desert;
  int target = t + 4;
  for (int n = 0; n < N; ++n)
    if (distance(n, target) <= 2)
      s.tiles[n].terrain = Grass;
  int u = add_unit(s, 1, Settler, t);
  ai(s, 1);
  assert(city_at(s, t) < 0 && s.units[u].tile >= 0 && s.units[u].tile != t);
  for (int n = 0; n < 5 && s.units[u].tile >= 0; ++n) {
    s.units[u].moves = 2;
    ai(s, 1);
  }
  assert(s.units[u].tile < 0);
  bool good = false;
  for (const auto &c : s.cities)
    good |= c.tile >= 0 && c.owner == 1 && distance(c.tile, target) <= 2;
  assert(good && valid(s));
}
void city_fire() {
  auto s = flat();
  int t = 10 * W + 10;
  city(s, 0, t);
  city(s, 1, t + 10);
  int enemy = add_unit(s, 1, Warrior, t + 2);
  s.units[enemy].moves = 0;
  s.wars = 1;
  // Block both intermediate hexes to the range-two target.
  for (int d = 0; d < 6; ++d) {
    int n = neighbor(t, d);
    if (distance(n, t + 2) == 1)
      s.tiles[n].terrain = Mountain;
  }
  vision(s);
  end_turn(s);
  assert(s.units[enemy].hp == 100);
  for (auto &tile : s.tiles)
    if (tile.terrain == Mountain)
      tile.terrain = Grass;
  int worker = add_unit(s, 1, Worker, neighbor(t, 3));
  assert(worker >= 0);
  s.units[worker].hp = 10;
  s.units[worker].moves = 0;
  s.units[enemy].moves = 0;
  s.units[enemy].acted = 1;
  end_turn(s);
  assert(s.units[enemy].hp == 82 && s.units[worker].hp == 10);
}
int main() {
  ranged_position();
  advance_attack_and_visibility();
  production_and_research();
  settlement();
  city_fire();
  puts("AI: firing positions, move-and-attack, visibility, production caps, "
       "emergency research, settlement and city line of sight passed");
}
