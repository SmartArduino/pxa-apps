// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <assert.h>
#include <chrono>
#include <stdio.h>
#include <string.h>
using namespace unciv;
State flat() {
  State s;
  for (auto &t : s.tiles) {
    t.terrain = Grass;
    t.seen = 7;
    t.visible = 7;
  }
  return s;
}
void geometry() {
  for (int a = 0; a < N; a++) {
    assert(distance(a, a) == 0);
    for (int d = 0; d < 6; d++) {
      int b = neighbor(a, d);
      if (b < 0)
        continue;
      assert(distance(a, b) == 1);
      assert(neighbor(b, (d + 3) % 6) == a);
    }
    for (int b = 0; b < N; b++)
      assert(distance(a, b) == distance(b, a));
  }
  assert(neighbor(-1, 0) == -1);
  assert(distance(-1, 0) == 999);
}
void movement() {
  auto s = flat();
  int a = 100, b = neighbor(a, 0), c = neighbor(b, 0);
  int u = add_unit(s, 0, Warrior, a);
  s.tiles[b].terrain = Hill;
  int16_t reach[N], previous[N];
  reachable(s, u, reach, previous);
  assert(reach[b] == 2);
  assert(previous[b] == a && previous[a] == -1);
  // Every displayed route must be a legal sequence with exactly the shown cost.
  for (int t = 0; t < N; ++t) {
    if (reach[t] <= 0)
      continue;
    auto copy = s;
    int path[N], count = 0, at = t;
    while (at != a) {
      assert(count < N && previous[at] >= 0);
      path[count++] = at;
      assert(distance(at, previous[at]) == 1 &&
             reach[previous[at]] < reach[at]);
      at = previous[at];
    }
    while (count)
      assert(move(copy, u, path[--count]));
    assert(copy.units[u].moves == s.units[u].moves - reach[t]);
  }
  assert(move(s, u, b));
  assert(s.units[u].moves == 0);
  reachable(s, u, reach, previous);
  for (int t = 0; t < N; ++t)
    assert(t == b ? reach[t] == 0 : reach[t] == -1);
  assert(!move(s, u, c));
  assert(!move(s, -1, c));
  s.units[u].moves = 2;
  s.tiles[c].terrain = Mountain;
  assert(!move(s, u, c));
  s.tiles[c].terrain = Grass;
  add_unit(s, 0, Scout, c);
  assert(!move(s, u, c));
  s.tiles[b].terrain = Grass;
  assert(valid(s));
  reachable(s, -1, reach, previous);
  for (int t = 0; t < N; ++t)
    assert(reach[t] == -1 && previous[t] == -1);
}
void founding() {
  auto s = flat();
  int u = add_unit(s, 0, Settler, 100);
  assert(found(s, u));
  assert(s.units[u].tile == -1 && s.capitals[0] == 100);
  assert(city_at(s, 100) >= 0);
  u = add_unit(s, 0, Settler, neighbor(100, 0));
  assert(!found(s, u));
  int worker = add_unit(s, 0, Worker, neighbor(100, 1));
  assert(improve(s, worker));
  assert(s.tiles[s.units[worker].tile].improvement == 0);
  assert(s.units[worker].work_left == 3);
  assert(!improve(s, worker));
  assert(valid(s));
}
void research_and_economy() {
  auto s = flat();
  int u = add_unit(s, 0, Settler, 100);
  assert(found(s, u));
  int city = city_at(s, 100);
  assert(!research(s, 0, Astronomy));
  assert(!produce(s, city, Swordsman));
  assert(produce(s, city, Monument));
  assert(research(s, 0, Pottery));
  for (int i = 0; i < 12; i++)
    end_turn(s);
  assert(s.civ[0].tech & (1 << Pottery));
  assert(s.cities[city].buildings & 1);
  assert(s.cities[city].pop > 1);
  assert(s.civ[0].gold > 30);
  s.civ[0].culture = 100;
  assert(policy(s, 0, 0));
  assert(!policy(s, 0, 0));
  assert(policy_cost(s, 0) == 35);
  assert(valid(s));
  assert(produce(s, city, Granary));
  s.civ[0].gold = 0;
  assert(!purchase(s, city));
  s.civ[0].gold = 1000;
  int cost = purchase_cost(s, city);
  assert(purchase(s, city));
  assert(s.civ[0].gold == 1000 - cost);
  assert(s.cities[city].buildings & 2);
  assert(valid(s));
}
void combat() {
  auto s = flat();
  int a = 105, b = neighbor(a, 0);
  int u = add_unit(s, 0, Warrior, a), v = add_unit(s, 1, Warrior, b);
  assert(!can_attack(s, u, b));
  s.wars = 1;
  assert(attack(s, u, b));
  assert(s.units[v].hp < 100 && s.units[u].hp < 100 && s.units[u].moves == 0);
  assert(!attack(s, u, b));
  assert(valid(s));
  s = flat();
  int cap = add_unit(s, 1, Settler, b);
  assert(found(s, cap));
  s.cities[0].hp = 1;
  add_unit(s, 0, Settler, 240);
  u = add_unit(s, 0, Archer, a);
  vision(s);
  s.wars = 1;
  assert(attack(s, u, b));
  assert(s.cities[0].owner == 1 && s.cities[0].hp == 1);
  s.units[u].kind = Swordsman;
  s.units[u].moves = 2;
  assert(attack(s, u, b));
  assert(s.cities[0].owner == 0 && s.units[u].tile == b);
  assert(valid(s));
  s = flat();
  a = 105;
  b = neighbor(neighbor(a, 0), 0);
  u = add_unit(s, 0, Archer, a);
  v = add_unit(s, 1, Warrior, b);
  s.wars = 1;
  assert(can_attack(s, u, b));
  s.tiles[neighbor(a, 0)].terrain = Mountain;
  assert(!can_attack(s, u, b));
}
void diplomacy_and_victory() {
  auto s = flat();
  for (int p = 0; p < 3; p++) {
    int u = add_unit(s, p, Settler, 3 * W + 2 + p * 6);
    assert(found(s, u));
  }
  assert(diplomacy(s, 0, 1, true));
  assert(war(s, 0, 1));
  assert(diplomacy(s, 0, 1, false));
  assert(!diplomacy(s, 0, 1, true));
  for (int i = 0; i < 10; i++)
    end_turn(s);
  assert(diplomacy(s, 0, 1, true));
  s.civ[0].tech = 4095;
  evaluate(s);
  assert(s.result == Science && s.winner == 0);
  s = flat();
  for (int p = 0; p < 3; p++) {
    int u = add_unit(s, p, Settler, 3 * W + 2 + p * 6);
    assert(found(s, u));
  }
  for (auto &c : s.cities)
    if (c.tile >= 0)
      c.owner = 0;
  evaluate(s);
  assert(s.result == Domination && s.winner == 0);
  s = flat();
  int u = add_unit(s, 0, Settler, 100);
  found(s, u);
  s.turn = 150;
  end_turn(s);
  assert(s.result == Score && s.turn == 151);
}
void saves() {
  State s;
  start(s, 12345);
  uint8_t bytes[2048], other[2048];
  size_t n = serialize(s, bytes, sizeof(bytes));
  assert(n && n <= 2048);
  State copy;
  assert(restore(copy, bytes, n));
  assert(serialize(copy, other, sizeof(other)) == n &&
         !memcmp(bytes, other, n));
  State preserved = copy;
  bytes[100] ^= 1;
  assert(!restore(copy, bytes, n));
  assert(copy.rng == preserved.rng);
  bytes[100] ^= 1;
  assert(!restore(copy, bytes, n - 1));
  assert(!restore(copy, nullptr, n));
  assert(!serialize(s, other, 10));
  for (size_t i = 0; i < n; i++) {
    bytes[i] ^= 0x80;
    assert(!restore(copy, bytes, n));
    bytes[i] ^= 0x80;
  }
  // A valid checksum must not bypass semantic range checks.
  memcpy(other, bytes, n);
  other[33] = 8; // unsupported civilization policy bit
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < n - 4; ++i)
    hash = (hash ^ other[i]) * 16777619u;
  for (int i = 0; i < 4; ++i)
    other[n - 4 + i] = uint8_t(hash >> (8 * i));
  assert(!restore(copy, other, n));
  assert(serialize(s, other, n) == n);
  printf("Save: %zu / 2048 bytes\n", n);
}
void simulations() {
  auto now = std::chrono::steady_clock::now();
  unsigned turns = 0;
  for (int seed = 1; seed <= 48; seed++) {
    State s;
    start(s, seed, (seed - 1) % 3);
    assert(valid(s));
    assert(s.width == ((seed - 1) % 3 == 0   ? 20
                       : (seed - 1) % 3 == 1 ? 28
                                             : 36));
    int settler = unit_at(s, int(s.height / 2) * W + 3);
    assert(found(s, settler));
    for (int t = 0; t < 151 && s.result == Playing; t++) {
      ai(s, 0);
      end_turn(s);
      assert(valid(s));
      State round;
      uint8_t bytes[2048];
      size_t n = serialize(s, bytes, sizeof(bytes));
      assert(n && restore(round, bytes, n));
      assert(score(round, 0) == score(s, 0));
      turns++;
    }
    assert(s.result != Playing);
  }
  double ms = std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - now)
                  .count();
  printf("Simulation: 48 campaigns, %u turns, %.1f ms\n", turns, ms);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  FILE *legacy = fopen(argv[1], "rb");
  assert(legacy);
  uint8_t old[2048];
  size_t old_size = fread(old, 1, sizeof(old), legacy);
  fclose(legacy);
  State old_state;
  assert(restore(old_state, old, old_size));
  assert(old_state.width == 20 && old_state.height == 14 &&
         city_at(old_state, 7 * W + 3) >= 0);
  geometry();
  movement();
  founding();
  research_and_economy();
  combat();
  diplomacy_and_victory();
  saves();
  simulations();
  puts("All rule, lifecycle and save checks passed");
}
