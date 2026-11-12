// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../turn_log.hpp"
#include "../visible_tiles.hpp"
#include <algorithm>
#include <assert.h>
#include <chrono>
#include <stdio.h>
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
void oracle_reach(const State &s, int i, int16_t out[N]) {
  std::fill(out, out + N, int16_t(-1));
  bool done[N]{};
  const auto &u = s.units[i];
  out[u.tile] = 0;
  for (;;) {
    int best = -1;
    for (int t = 0; t < N; ++t)
      if (!done[t] && out[t] >= 0 && (best < 0 || out[t] < out[best]))
        best = t;
    if (best < 0)
      break;
    done[best] = true;
    if (out[best] == u.moves)
      continue;
    for (int d = 0; d < 6; ++d) {
      int t = neighbor(best, d);
      if (!land(s, t) || !can_enter(s, u.owner, t) ||
          !(s.tiles[t].seen & (1 << u.owner)))
        continue;
      int v = unit_at(s, t), c = city_at(s, t);
      if ((v >= 0 && v != i) || (c >= 0 && s.cities[c].owner != u.owner))
        continue;
      bool controlled = false;
      for (const auto &enemy : s.units)
        controlled |= enemy.tile >= 0 && military(enemy) &&
                      war(s, u.owner, enemy.owner) &&
                      (s.tiles[enemy.tile].visible & (1 << u.owner)) &&
                      distance(best, enemy.tile) == 1 &&
                      distance(t, enemy.tile) == 1;
      for (const auto &city : s.cities)
        controlled |= city.tile >= 0 && war(s, u.owner, city.owner) &&
                      (s.tiles[city.tile].seen & (1 << u.owner)) &&
                      distance(best, city.tile) == 1 &&
                      distance(t, city.tile) == 1;
      int step = u.kind != Scout && (s.tiles[t].terrain == Hill ||
                                     s.tiles[t].terrain == Forest)
                     ? 2
                     : 1;
      int cost =
          controlled ? u.moves : std::min(int(u.moves), out[best] + step);
      if (cost <= u.moves && (out[t] < 0 || cost < out[t]))
        out[t] = cost;
    }
  }
}
void optimized_rules() {
  double fast = 0, reference = 0;
  for (int seed = 1; seed <= 120; ++seed) {
    State s;
    start(s, seed, seed % 3);
    if (seed % 3 == 0) {
      int settler = unit_at(s, (s.height / 2) * W + 3);
      assert(settler >= 0 && found(s, settler));
    }
    auto before = s;
    vision(s);
    for (int t = 0; t < N; ++t) {
      unsigned mask = 0;
      if (inside(s, t)) {
        for (const auto &u : before.units)
          if (u.tile >= 0 && distance(t, u.tile) <= (u.kind == Scout ? 3 : 2))
            mask |= 1 << u.owner;
        for (const auto &c : before.cities)
          if (c.tile >= 0 && distance(t, c.tile) <= 2)
            mask |= 1 << c.owner;
      }
      assert(s.tiles[t].visible == mask);
    }
    for (int i = 0; i < UNITS; ++i)
      if (s.units[i].tile >= 0) {
        int16_t a[N], b[N];
        auto now = std::chrono::steady_clock::now();
        reachable(s, i, a);
        auto mid = std::chrono::steady_clock::now();
        oracle_reach(s, i, b);
        auto end = std::chrono::steady_clock::now();
        fast += std::chrono::duration<double, std::micro>(mid - now).count();
        reference +=
            std::chrono::duration<double, std::micro>(end - mid).count();
        assert(std::equal(a, a + N, b));
      }
  }
  printf("Reachability oracle: 120 worlds, bucket %.1f us, scan %.1f us "
         "(native instrumented CPU, not device FPS)\n",
         fast, reference);
}
void focus_and_work() {
  auto s = flat();
  int t = 10 * W + 10, c = city(s, 0, t);
  int farm = neighbor(t, 0), mine = neighbor(t, 1), coast = neighbor(t, 2);
  s.tiles[mine].terrain = Hill;
  s.tiles[coast].terrain = Coast;
  int16_t worked[18];
  assert(city_focus(s, c, FoodFocus) && worked_tiles(s, c, worked) == 1);
  assert(tile_yield(s, worked[0]).food == 2);
  assert(city_focus(s, c, ProductionFocus) && worked_tiles(s, c, worked) == 1 &&
         worked[0] == mine);
  assert(city_focus(s, c, GoldFocus) && worked_tiles(s, c, worked) == 1 &&
         worked[0] == coast);
  int c2 = city(s, 0, t + 4);
  s.cities[c].pop = s.cities[c2].pop = 20;
  int16_t other[18];
  int n = worked_tiles(s, c, worked), m = worked_tiles(s, c2, other);
  for (int a = 0; a < n; ++a)
    for (int b = 0; b < m; ++b)
      assert(worked[a] != other[b]);
  s.cities[c].pop = s.cities[c2].pop = 1;
  int u = add_unit(s, 0, Worker, farm);
  assert(improve(s, u));
  assert(!s.tiles[farm].improvement && s.units[u].work_left == 3);
  assert(!fortify(s, u) && !improve(s, u));
  auto before = s;
  end_turn(s);
  assert(s.units[u].work_left == 2 && !s.tiles[farm].improvement);
  uint8_t bytes[2048];
  State restored;
  size_t size = serialize(s, bytes, sizeof(bytes));
  assert(size && restore(restored, bytes, size));
  assert(restored.units[u].work_left == 2 &&
         restored.cities[c].focus == GoldFocus);
  end_turn(restored);
  assert(restored.units[u].work_left == 1);
  end_turn(restored);
  assert(!restored.units[u].work_left && restored.tiles[farm].improvement == 1);
  TurnLog log;
  log.collect(before, restored);
  bool found_improvement = false;
  for (int i = 0; i < log.count; ++i)
    found_improvement |= log.entries[i].type == TurnLog::Improved;
  assert(found_improvement);
  assert(move(restored, u, mine));
  assert(!improve(restored, u)); // Mine needs Mining and movement.
  restored.civ[0].tech |= 1 << Mining;
  restored.units[u].moves = 2;
  assert(improve(restored, u) && restored.units[u].work_left == 4);
  assert(cancel_work(restored, u) && !restored.units[u].moves &&
         !restored.units[u].project);
  restored.units[u].moves = 2;
  assert(improve(restored, u));
  restored.units[u].moves = 2;
  assert(move(restored, u, farm));
  assert(!restored.units[u].work_left && !restored.units[u].project);
  restored.tiles[mine].terrain = Forest;
  restored.units[u].moves = 2;
  assert(move(restored, u, mine));
  restored.units[u].moves = 2;
  assert(improve(restored, u));
  for (int k = 0; k < 4; ++k)
    end_turn(restored);
  assert(restored.tiles[mine].terrain == Grass &&
         restored.tiles[mine].improvement == 1);
  auto starving = flat();
  int sc = city(starving, 0, t);
  for (auto &tile : starving.tiles)
    tile.terrain = Desert;
  starving.cities[sc].pop = 5;
  end_turn(starving);
  assert(starving.cities[sc].pop == 4 && starving.cities[sc].food == 0);
  assert(!city_focus(s, c, FOCUSES));
}
void forecasts() {
  for (int kind : {Warrior, Archer, Swordsman})
    for (int hp : {15, 57, 100})
      for (bool is_city : {false, true}) {
        auto s = flat();
        int t = 10 * W + 10, target = neighbor(t, 0);
        int a = add_unit(s, 0, kind, t);
        s.units[a].hp = hp;
        int v = is_city ? city(s, 1, target) : add_unit(s, 1, Warrior, target);
        if (is_city)
          s.cities[v].hp = 20;
        else {
          s.units[v].hp = 35;
          s.units[v].fortified = 1;
          s.tiles[target].terrain = Hill;
        }
        s.wars = 1;
        vision(s);
        auto preview = battle_preview(s, a, target);
        assert(preview.possible);
        auto before = s;
        auto again = battle_preview(s, a, target);
        assert(before.rng == s.rng &&
               again.attacker_after == preview.attacker_after);
        assert(attack(s, a, target) && s.units[a].hp == preview.attacker_after);
        if (preview.capture)
          assert(s.cities[v].owner == 0 && s.cities[v].hp == 55);
        else
          assert((is_city ? s.cities[v].hp : s.units[v].hp) ==
                 preview.defender_after);
        if (kind == Archer)
          assert(s.units[a].hp == hp && !preview.capture);
        assert(valid(s));
      }
}
void ai_routes() {
  auto s = flat();
  int t = 8 * W + 8, c = city(s, 1, t + 4);
  int u = add_unit(s, 1, Warrior, t);
  city(s, 0, t + 10);
  diplomacy(s, 1, 0, true);
  // U-shaped obstruction: a direct geometric step cannot make progress.
  for (int y = 5; y <= 11; ++y)
    s.tiles[y * W + 9].terrain = Mountain;
  vision(s);
  for (auto &tile : s.tiles)
    tile.seen = 7;
  int original = s.units[u].tile;
  ai(s, 1);
  assert(s.units[u].tile != original); // Route begins away from target.
  auto wounded = flat();
  int home = city(wounded, 1, t);
  int w = add_unit(wounded, 1, Warrior, t + 1);
  wounded.units[w].hp = 30;
  ai(wounded, 1);
  assert(wounded.units[w].fortified);
  end_turn(wounded);
  assert(wounded.units[w].hp == 50);
  auto retreat = flat();
  city(retreat, 1, t);
  city(retreat, 0, t + 8);
  int civilian = add_unit(retreat, 1, Worker, t + 1);
  add_unit(retreat, 0, Warrior, t + 2);
  retreat.wars = 1;
  vision(retreat);
  assert(improve(retreat, civilian));
  retreat.units[civilian].moves = 2;
  ai(retreat, 1);
  assert(retreat.units[civilian].tile == t &&
         !retreat.units[civilian].work_left);
  (void)c;
  (void)home;
}
void legacy_v2(const char *path) {
  FILE *file = fopen(path, "rb");
  assert(file);
  uint8_t data[2048];
  size_t n = fread(data, 1, sizeof(data), file);
  fclose(file);
  State s;
  assert(n == 1910 && restore(s, data, n));
  assert(s.width == 36 && s.height == 20);
  for (const auto &u : s.units)
    assert(!u.work_left && !u.project);
  for (const auto &c : s.cities)
    assert(c.focus == Balanced);
  assert(serialize(s, data, sizeof(data)) == 1752 && data[3] == 7);
}
void citizens() {
  auto s = flat();
  int t = 10 * W + 10, c = city(s, 0, t);
  int food = neighbor(t, 0), hill = neighbor(t, 1);
  s.tiles[hill].terrain = Hill;
  assert(city_focus(s, c, FoodFocus));
  assert(lock_tile(s, c, hill, true) && tile_locked(s, c, hill));
  int16_t worked[18];
  assert(worked_tiles(s, c, worked) == 1 && worked[0] == hill);
  assert(!lock_tile(s, c, food, true)); // Every citizen is already locked.
  assert(!lock_tile(s, c, t, true) && !lock_tile(s, c, -1, true));
  assert(city_focus(s, c, ProductionFocus) && tile_locked(s, c, hill));
  uint8_t bytes[2048];
  State copy;
  size_t n = serialize(s, bytes, sizeof(bytes));
  assert(n == 1752 && restore(copy, bytes, n) && tile_locked(copy, c, hill));
  assert(auto_assign(s, c) && !tile_locked(s, c, hill));
  int other = city(s, 0, t + 4);
  assert(!lock_tile(s, other, food, true));
  for (auto &tile : s.tiles)
    tile.terrain = Desert;
  s.cities[c].pop = 3;
  int16_t tiles[18];
  city_tiles(s, c, tiles);
  assert(lock_tile(s, c, tiles[0], true) && lock_tile(s, c, tiles[1], true) &&
         lock_tile(s, c, tiles[2], true));
  end_turn(s);
  assert(s.cities[c].pop == 2);
  int locked = 0;
  for (int tile : tiles)
    locked += tile_locked(s, c, tile);
  assert(locked == 2 && valid(s));
  s.cities[c].locked = 1u << 18;
  assert(!valid(s));
}
void viewport_order() {
  State s;
  uint32_t seed = 83291;
  for (int n = 0; n < 2400; ++n) {
    auto next = [&] {
      seed = seed * 1664525u + 1013904223u;
      return seed;
    };
    s.width = 20 + next() % 17;
    s.height = 14 + next() % 7;
    int tw = 22 + next() % 22 * 2, width = 224 + next() % 400,
        bottom = 144 + next() % 440;
    int ox = int(next() % 3200) - 2000, oy = int(next() % 1800) - 1000;
    int expected[N], actual[N], count = 0, th = tw * 7 / 8;
    for (int row = 0; row < s.height; ++row)
      for (int parity = 0; parity < 2; ++parity)
        for (int col = parity; col < s.width; col += 2) {
          int x = col * (tw * 3 / 4) + ox,
              y = row * th + (col & 1) * (tw * 7 / 16) + oy;
          if (x >= -tw && x <= width + tw && y >= 40 - th && y <= bottom + th)
            expected[count++] = row * W + col;
        }
    int got = visible_tiles(s, ox, oy, tw, width, 40, bottom, actual);
    assert(got == count && std::equal(actual, actual + got, expected));
  }
  puts("Viewport culling matches full-board painter order in 2400 camera/zoom "
       "cases");
}
void legacy_v3(const char *path) {
  FILE *f = fopen(path, "rb");
  assert(f);
  uint8_t data[2048];
  size_t n = fread(data, 1, sizeof(data), f);
  fclose(f);
  State s;
  assert(n == 1962 && restore(s, data, n));
  assert(s.cities[0].focus == ProductionFocus && !s.cities[0].locked);
  bool work = false;
  for (const auto &u : s.units)
    if (u.tile >= 0 && u.kind == Worker)
      work |= u.work_left == 4 && u.project == 1;
  assert(work && serialize(s, data, sizeof(data)) == 1752);
}
int main(int argc, char **argv) {
  assert(argc == 3);
  legacy_v2(argv[1]);
  legacy_v3(argv[2]);
  citizens();
  viewport_order();
  optimized_rules();
  focus_and_work();
  forecasts();
  ai_routes();
  puts("Focus, construction, forecasts, AI routes, notifications and legacy-v2 "
       "migration passed");
}
