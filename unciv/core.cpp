// SPDX-License-Identifier: MIT
// Original bounded simulation. No Kotlin/Java implementation is included or
// translated.
#include "core.hpp"
#include <algorithm>
#include <stdlib.h>
#include <string.h>
namespace unciv {
const TechInfo tech_info[TECHS] = {
    {0, 0},
    {1 << Agriculture, 18},
    {1 << Agriculture, 20},
    {1 << Agriculture, 22},
    {1 << Pottery, 34},
    {1 << Mining, 32},
    {1 << Pottery, 30},
    {(1 << Writing) | (1 << Archery), 48},
    {1 << Bronze, 50},
    {(1 << Writing) | (1 << Mathematics), 85},
    {(1 << Wheel) | (1 << Iron), 85},
    {(1 << Education) | (1 << Engineering), 130}};
const uint16_t costs[JOBS + 1] = {65, 24, 18, 28, 32, 45, 24, 32, 45, 38, 0};
static int clamp(int n, int a, int b) { return std::max(a, std::min(b, n)); }
uint32_t random(State &s) {
  uint32_t x = s.rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return s.rng = x ? x : 1;
}
static bool tile_ok(int t) { return t >= 0 && t < N; }
namespace {
struct BoardIndex {
  int8_t units[N], cities[N], claims[N];
  explicit BoardIndex(const State &s) {
    std::fill(units, units + N, int8_t(-1));
    std::fill(cities, cities + N, int8_t(-1));
    territory(s, claims);
    for (int i = 0; i < UNITS; ++i)
      if (s.units[i].tile >= 0)
        units[s.units[i].tile] = i;
    for (int i = 0; i < CITIES; ++i)
      if (s.cities[i].tile >= 0)
        cities[s.cities[i].tile] = i;
  }
};
template <class F> void radius(const State &s, int tile, int r, F visit) {
  if (!inside(s, tile))
    return;
  int q = tile % W, row = tile / W;
  for (int y = std::max(0, row - r); y <= std::min(int(s.height) - 1, row + r);
       ++y)
    for (int x = std::max(0, q - r); x <= std::min(int(s.width) - 1, q + r);
         ++x) {
      int t = y * W + x;
      if (distance(tile, t) <= r)
        visit(t);
    }
}
int movement_cost(const State &s, const Unit &u, int tile) {
  return u.kind != Scout && (s.tiles[tile].terrain == Hill ||
                             s.tiles[tile].terrain == Forest)
             ? 2
             : 1;
}
} // namespace
int neighbor(int t, int d) {
  if (!tile_ok(t) || d < 0 || d >= 6)
    return -1;
  int x = t % W, y = t / W;
  static const int dx[6] = {1, 0, -1, -1, 0, 1};
  static const int dy[2][6] = {{0, 1, 0, -1, -1, -1}, {1, 1, 1, 0, -1, 0}};
  y += dy[x & 1][d];
  x += dx[d];
  return x >= 0 && x < W && y >= 0 && y < H ? y * W + x : -1;
}
int distance(int a, int b) {
  if (!tile_ok(a) || !tile_ok(b))
    return 999;
  int ax = a % W, bx = b % W, ay = a / W - (ax - (ax & 1)) / 2,
      by = b / W - (bx - (bx & 1)) / 2;
  return (abs(ax - bx) + abs(ay - by) + abs(ax + ay - bx - by)) / 2;
}
int unit_at(const State &s, int t) {
  for (int i = 0; i < UNITS; i++)
    if (s.units[i].tile == t && t >= 0)
      return i;
  return -1;
}
int city_at(const State &s, int t) {
  for (int i = 0; i < CITIES; i++)
    if (s.cities[i].tile == t && t >= 0)
      return i;
  return -1;
}
bool inside(const State &s, int t) {
  return tile_ok(t) && t % W < s.width && t / W < s.height;
}
bool land(const State &s, int t) {
  return inside(s, t) && s.tiles[t].terrain != Mountain &&
         s.tiles[t].terrain < Coast;
}
void territory(const State &s, int8_t out[N]) {
  std::fill(out, out + N, int8_t(-1));
  for (int i = 0; i < CITIES; ++i)
    if (s.cities[i].tile >= 0)
      radius(s, s.cities[i].tile, 2, [&](int t) {
        int other = out[t];
        if (other < 0 ||
            distance(t, s.cities[i].tile) < distance(t, s.cities[other].tile))
          out[t] = i;
      });
}
bool can_enter(const State &s, int owner, int t) {
  if (owner < 0 || owner >= PLAYERS || !inside(s, t))
    return false;
  int c = working_city(s, t);
  return c < 0 || s.cities[c].owner == owner ||
         war(s, owner, s.cities[c].owner);
}
static bool expel_units(State &s) {
  BoardIndex index(s);
  // Reserve existing legal positions first, then relocate displaced units in
  // stable slot order. Callers stage this on a copy so failure is atomic.
  bool displaced[UNITS]{};
  for (int i = 0; i < UNITS; ++i) {
    auto &u = s.units[i];
    if (u.tile < 0)
      continue;
    int c = index.claims[u.tile];
    if (c >= 0 && s.cities[c].owner != u.owner &&
        !war(s, u.owner, s.cities[c].owner)) {
      displaced[i] = true;
      index.units[u.tile] = -1;
    }
  }
  for (int i = 0; i < UNITS; ++i)
    if (displaced[i]) {
      auto &u = s.units[i];
      int best = -1;
      for (int t = 0; t < N; ++t) {
        if (!land(s, t) || index.units[t] >= 0)
          continue;
        int c = index.claims[t];
        if (c >= 0 && s.cities[c].owner != u.owner &&
            !war(s, u.owner, s.cities[c].owner))
          continue;
        int city = index.cities[t];
        if (city >= 0 && s.cities[city].owner != u.owner)
          continue;
        if (best < 0 || distance(u.tile, t) < distance(u.tile, best))
          best = t;
      }
      if (best < 0)
        return false;
      u.tile = best;
      u.moves = u.fortified = u.fortify_turns = u.work_left = u.project = 0;
      u.order = Manual;
      u.acted = 1;
      index.units[best] = i;
    }
  vision(s);
  return true;
}
static int pair(int a, int b) {
  if (a > b)
    std::swap(a, b);
  return a == 0 ? b - 1 : 2;
}
bool war(const State &s, int a, int b) {
  return a >= 0 && b >= 0 && a < PLAYERS && b < PLAYERS && a != b &&
         (s.wars & (1 << pair(a, b)));
}
bool alive(const State &s, int o) {
  for (const auto &c : s.cities)
    if (c.tile >= 0 && c.owner == o)
      return true;
  for (const auto &u : s.units)
    if (u.tile >= 0 && u.owner == o && u.kind == Settler)
      return true;
  return false;
}
bool diplomacy(State &s, int a, int b, bool declare) {
  if (a < 0 || b < 0 || a >= PLAYERS || b >= PLAYERS || a == b ||
      !alive(s, a) || !alive(s, b) || s.result != Playing)
    return false;
  int p = pair(a, b);
  if (declare == war(s, a, b))
    return false;
  if (declare) {
    if (s.truce[p])
      return false;
    s.wars |= 1 << p;
  } else {
    State next = s;
    next.wars &= ~(1 << p);
    next.truce[p] = 10;
    if (!expel_units(next))
      return false;
    s = next;
  }
  return true;
}
int add_unit(State &s, int o, int k, int t) {
  if (o < 0 || o >= PLAYERS || k < 0 || k >= KINDS || !land(s, t) ||
      unit_at(s, t) >= 0)
    return -1;
  int c = city_at(s, t);
  if (c >= 0 && s.cities[c].owner != o)
    return -1;
  for (int i = 0; i < UNITS; i++)
    if (s.units[i].tile < 0) {
      s.units[i] = {
          int16_t(t), uint8_t(o), uint8_t(k), 100, uint8_t(k == Scout ? 3 : 2),
          0};
      return i;
    }
  return -1;
}
void vision(State &s) {
  for (auto &t : s.tiles)
    t.visible = 0;
  for (int p = 0; p < PLAYERS; p++) {
    for (const auto &u : s.units)
      if (u.tile >= 0 && u.owner == p)
        radius(s, u.tile, u.kind == Scout ? 3 : 2,
               [&](int t) { s.tiles[t].visible |= 1 << p; });
    for (const auto &c : s.cities)
      if (c.tile >= 0 && c.owner == p)
        radius(s, c.tile, 2, [&](int t) { s.tiles[t].visible |= 1 << p; });
  }
  for (auto &t : s.tiles)
    t.seen |= t.visible;
}
static void normalize_locks(State &s);
bool found(State &s, int i) {
  if (i < 0 || i >= UNITS || s.result != Playing)
    return false;
  auto &u = s.units[i];
  if (u.tile < 0 || u.kind != Settler || !u.moves || !land(s, u.tile))
    return false;
  for (auto &c : s.cities)
    if (c.tile >= 0 && distance(u.tile, c.tile) < 4)
      return false;
  for (auto &c : s.cities)
    if (c.tile < 0) {
      c = City{};
      c.tile = u.tile;
      c.owner = u.owner;
      if (s.capitals[u.owner] < 0)
        s.capitals[u.owner] = u.tile;
      s.tiles[u.tile].improvement = 0;
      u.tile = -1;
      normalize_locks(s);
      vision(s);
      return true;
    }
  return false;
}
void start(State &s, uint32_t seed, int map_size) {
  s = State{};
  s.rng = seed ? seed : 1;
  const uint8_t widths[] = {20, 28, 36}, heights[] = {14, 18, 20};
  map_size = std::clamp(map_size, 0, 2);
  s.width = widths[map_size];
  s.height = heights[map_size];
  for (int t = 0; t < N; t++) {
    int x = t % W, y = t / W;
    if (!inside(s, t)) {
      s.tiles[t].terrain = Ocean;
      continue;
    }
    unsigned n = random(s) % 100;
    int tr = n < 44   ? Grass
             : n < 62 ? Plains
             : n < 72 ? Hill
             : n < 83 ? Forest
             : n < 91 ? Desert
                      : Mountain;
    if (x < 2 || x > s.width - 3 || y == 0 || y == s.height - 1)
      tr = x == 0 || x == s.width - 1 ? Ocean : Coast;
    s.tiles[t].terrain = tr;
    s.tiles[t].resource = (tr < Mountain && random(s) % 7 == 0) ? 1 : 0;
  }
  const int starts[3] = {int(s.height / 2) * W + 3, 3 * W + s.width - 5,
                         (s.height - 4) * W + s.width - 5};
  // A traversable belt connects the start regions; remove isolated spawn
  // pockets.
  for (int y = 2; y < s.height - 2; y++)
    for (int x = 3; x < s.width - 3; x++)
      if (x == s.width / 2 || y == s.height / 2)
        s.tiles[y * W + x].terrain = Grass;
  for (int o = 0; o < 3; o++) {
    int t = starts[o];
    for (int n = 0; n < N; n++)
      if (inside(s, n) && distance(t, n) <= 2) {
        s.tiles[n].terrain = (random(s) % 5 == 0) ? Hill : Grass;
        s.tiles[n].resource = 0;
      }
    s.tiles[neighbor(t, 0)].resource = 1;
    int settler = add_unit(s, o, Settler, t);
    add_unit(s, o, Warrior, neighbor(t, 0));
    add_unit(s, o, Scout, neighbor(t, 1));
    add_unit(s, o, Worker, neighbor(t, 3));
    if (o)
      found(s, settler);
  }
  vision(s);
}
namespace {
bool controlled_step(const State &s, int owner, int from, int to,
                     const BoardIndex &index) {
  for (int d = 0; d < 6; ++d) {
    int n = neighbor(from, d);
    if (!inside(s, n) || distance(n, to) != 1)
      continue;
    int c = index.cities[n], v = index.units[n];
    if (c >= 0 && war(s, owner, s.cities[c].owner) &&
        (s.tiles[n].seen & (1 << owner)))
      return true;
    if (v >= 0 && military(s.units[v]) && war(s, owner, s.units[v].owner) &&
        (s.tiles[n].visible & (1 << owner)))
      return true;
  }
  return false;
}
} // namespace
bool zone_of_control(const State &s, int owner, int from, int to) {
  if (owner < 0 || owner >= PLAYERS || !inside(s, from) || !inside(s, to) ||
      distance(from, to) != 1)
    return false;
  return controlled_step(s, owner, from, to, BoardIndex(s));
}
void reachable(const State &s, int i, int16_t out[N], int16_t *previous) {
  std::fill(out, out + N, int16_t(-1));
  if (previous)
    std::fill(previous, previous + N, int16_t(-1));
  if (i < 0 || i >= UNITS || s.units[i].tile < 0)
    return;
  const auto &u = s.units[i];
  BoardIndex index(s);
  // Movement budgets are at most 3. Each cost bucket contains a subset of the
  // 37 hexes in radius 3, avoiding a whole-world minimum scan for every step.
  int16_t buckets[4][64];
  uint8_t count[4]{};
  int budget = std::min(3, int(u.moves));
  out[u.tile] = 0;
  buckets[0][count[0]++] = u.tile;
  for (int cost = 0; cost <= budget; ++cost)
    for (int n = 0; n < count[cost]; ++n) {
      int best = buckets[cost][n];
      if (out[best] != cost || cost == budget)
        continue;
      for (int d = 0; d < 6; d++) {
        int t = neighbor(best, d);
        if (!land(s, t) || !(s.tiles[t].seen & (1 << u.owner)))
          continue;
        int other = index.units[t], c = index.cities[t],
            claim = index.claims[t];
        if (claim >= 0 && s.cities[claim].owner != u.owner &&
            !war(s, u.owner, s.cities[claim].owner))
          continue;
        if ((other >= 0 && other != i) ||
            (c >= 0 && s.cities[c].owner != u.owner))
          continue;
        int value = controlled_step(s, u.owner, best, t, index)
                        ? budget
                        : std::min(budget, cost + movement_cost(s, u, t));
        if (value <= budget && (out[t] < 0 || value < out[t])) {
          out[t] = value;
          buckets[value][count[value]++] = t;
          if (previous)
            previous[t] = best;
        }
      }
    }
}
bool move(State &s, int i, int t) {
  if (!tile_ok(t) || i < 0 || i >= UNITS || s.result != Playing)
    return false;
  auto &u = s.units[i];
  if (u.tile < 0 || u.tile == t)
    return false;
  int16_t reach[N];
  reachable(s, i, reach);
  if (reach[t] < 0)
    return false;
  u.moves -= reach[t];
  u.acted = 1;
  u.tile = t;
  u.fortified = u.fortify_turns = 0;
  u.work_left = u.project = 0;
  u.order = Manual;
  vision(s);
  return true;
}
bool military(const Unit &u) { return u.kind != Settler && u.kind != Worker; }
int promotion_count(const Unit &u) {
  return (u.promotions & 3) + ((u.promotions >> 2) & 3) +
         bool(u.promotions & 16) + bool(u.promotions & 32);
}
int earned_promotions(const Unit &u) {
  int n = 0;
  for (int xp : {10, 30, 60, 100})
    n += u.xp >= xp;
  return n;
}
int next_promotion_xp(const Unit &u) {
  static const int thresholds[] = {10, 30, 60, 100, 150};
  return thresholds[std::min(4, promotion_count(u))];
}
bool can_promote(const State &s, int i, int p) {
  if (i < 0 || i >= UNITS || p < 0 || p >= PROMOTIONS || s.result != Playing)
    return false;
  const auto &u = s.units[i];
  if (u.tile < 0 || !military(u) || earned_promotions(u) <= promotion_count(u))
    return false;
  return p == OpenCombat    ? (u.promotions & 3) < 3
         : p == RoughCombat ? ((u.promotions >> 2) & 3) < 3
         : p == Medic       ? !(u.promotions & 16)
                            : !(u.promotions & 32) && u.hp < 100;
}
bool promote(State &s, int i, int p) {
  if (!can_promote(s, i, p))
    return false;
  auto &u = s.units[i];
  if (p == OpenCombat)
    ++u.promotions;
  else if (p == RoughCombat)
    u.promotions += 4;
  else if (p == Medic)
    u.promotions |= 16;
  else {
    u.promotions |= 32;
    u.hp = std::min(100, int(u.hp) + 50);
    u.moves = 0;
    u.acted = 1;
  }
  return true;
}
int upgrade_kind(const State &s, int i) {
  return i >= 0 && i < UNITS && s.units[i].tile >= 0 &&
                 s.units[i].kind == Warrior
             ? Swordsman
             : -1;
}
int upgrade_cost(const State &s, int i) {
  int kind = upgrade_kind(s, i);
  return kind < 0 ? 0 : 10 + 2 * (int(costs[kind]) - costs[s.units[i].kind]);
}
bool can_upgrade(const State &s, int i) {
  int k = upgrade_kind(s, i);
  if (k < 0 || s.result != Playing)
    return false;
  const auto &u = s.units[i];
  int c = working_city(s, u.tile);
  return available(s, u.owner, k) && c >= 0 && s.cities[c].owner == u.owner &&
         u.moves == 2 && s.civ[u.owner].gold >= upgrade_cost(s, i);
}
bool upgrade(State &s, int i) {
  if (!can_upgrade(s, i))
    return false;
  auto &u = s.units[i];
  s.civ[u.owner].gold -= upgrade_cost(s, i);
  u.kind = upgrade_kind(s, i);
  u.moves = u.fortified = u.fortify_turns = 0;
  u.acted = 1;
  u.order = Manual;
  return true;
}
bool wait(State &s, int i) {
  if (i < 0 || i >= UNITS || s.result != Playing || s.units[i].tile < 0 ||
      !s.units[i].moves || s.units[i].work_left)
    return false;
  auto &u = s.units[i];
  u.order = Waiting;
  u.acted = 1;
  u.moves = u.fortified = u.fortify_turns = 0;
  return true;
}
bool heal(State &s, int i) {
  if (i < 0 || i >= UNITS || s.result != Playing || s.units[i].tile < 0 ||
      !s.units[i].moves || s.units[i].work_left || s.units[i].hp == 100)
    return false;
  auto &u = s.units[i];
  u.order = Healing;
  u.moves = 0;
  if (!u.fortified)
    u.fortify_turns = 0;
  u.fortified = military(u);
  return true;
}
bool cancel_order(State &s, int i) {
  if (i < 0 || i >= UNITS || s.result != Playing || s.units[i].tile < 0 ||
      s.units[i].order == Manual)
    return false;
  auto &u = s.units[i];
  if (u.order == Healing)
    u.fortified = u.fortify_turns = 0;
  u.order = Manual;
  return true;
}
static int promotion_bonus(const Unit &u, int terrain, bool attacking) {
  if (u.kind == Archer && !attacking)
    return 0;
  int levels = terrain == Hill || terrain == Forest ? (u.promotions >> 2) & 3
                                                    : u.promotions & 3;
  return levels * 15;
}
static void gain_xp(Unit &u, int amount) {
  if (military(u))
    u.xp = std::min(127, int(u.xp) + amount);
}
static int strength(const State &s, const Unit &u) {
  int base = u.kind == Swordsman ? 17
             : u.kind == Warrior ? 10
             : u.kind == Archer  ? 7
             : u.kind == Scout   ? 6
                                 : 3;
  return base + ((s.civ[u.owner].policies & 2) ? 2 : 0);
}
static bool clear_shot(const State &s, int from, int to) {
  if (distance(from, to) <= 1)
    return true;
  for (int d = 0; d < 6; ++d) {
    int n = neighbor(from, d);
    if (inside(s, n) && distance(n, to) == 1 &&
        s.tiles[n].terrain != Mountain && s.tiles[n].terrain != Forest)
      return true;
  }
  return false;
}
bool can_attack(const State &s, int i, int t) {
  if (i < 0 || i >= UNITS || !tile_ok(t) || s.result != Playing)
    return false;
  const auto &u = s.units[i];
  if (u.tile < 0 || !u.moves || u.kind == Settler || u.kind == Worker ||
      !(s.tiles[t].visible & (1 << u.owner)))
    return false;
  int range = u.kind == Archer ? 2 : 1, dist = distance(u.tile, t);
  if (dist < 1 || dist > range)
    return false;
  int c = city_at(s, t), v = unit_at(s, t);
  int owner = c >= 0 ? s.cities[c].owner : v >= 0 ? s.units[v].owner : u.owner;
  if (!war(s, u.owner, owner))
    return false;
  if (dist == 2 && !clear_shot(s, u.tile, t))
    return false;
  return true;
}
BattlePreview battle_preview(const State &s, int i, int t) {
  BattlePreview p;
  if (!can_attack(s, i, t))
    return p;
  const auto &u = s.units[i];
  int c = city_at(s, t), v = unit_at(s, t);
  p.possible = true;
  p.ranged = u.kind == Archer;
  p.city = c >= 0 && s.cities[c].owner != u.owner;
  // Wounds reduce strength to 50% at zero HP, using integer arithmetic shared
  // by the forecast and the actual battle.
  int base = p.ranged
                 ? 12 + ((s.civ[u.owner].tech & (1 << Mathematics)) ? 3 : 0)
                 : strength(s, u);
  base = base * (100 + promotion_bonus(u, s.tiles[t].terrain, true)) / 100;
  p.attacker_strength = std::max(1, base * (100 + u.hp) / 200);
  p.attacker_hp = p.attacker_after = u.hp;
  if (p.city) {
    const auto &city = s.cities[c];
    p.defender_strength = 11 + city.pop + ((city.buildings & 8) ? 4 : 0);
    p.defender_hp = city.hp;
    if (!p.ranged)
      p.attacker_after = std::max(
          0, int(u.hp) -
                 clamp(18 * p.defender_strength / p.attacker_strength, 8, 42));
    p.defender_after = std::max(
        p.ranged || !p.attacker_after ? 1 : 0,
        int(city.hp) -
            clamp(30 * p.attacker_strength / p.defender_strength, 10, 55));
    p.capture = !p.defender_after;
  } else {
    const auto &target = s.units[v];
    p.defender_strength =
        std::max(1,
                 strength(s, target) *
                     (100 + promotion_bonus(target, s.tiles[t].terrain, false) +
                      fortification_bonus(target)) *
                     (100 + target.hp) / 20000) +
        (s.tiles[t].terrain == Hill || s.tiles[t].terrain == Forest ? 3 : 0);
    p.defender_hp = target.hp;
    p.defender_after = std::max(
        0, int(target.hp) -
               clamp(35 * p.attacker_strength / p.defender_strength, 12, 80));
    if (!p.ranged && target.kind != Settler && target.kind != Worker)
      p.attacker_after = std::max(
          0, int(u.hp) -
                 clamp(24 * p.defender_strength / p.attacker_strength, 8, 60));
  }
  return p;
}
bool attack(State &s, int i, int t) {
  auto p = battle_preview(s, i, t);
  if (!p.possible)
    return false;
  auto &u = s.units[i];
  int c = city_at(s, t), v = unit_at(s, t);
  if (p.city || (v >= 0 && military(s.units[v])))
    gain_xp(u, p.city ? 3 : p.ranged ? 2 : 5);
  if (!p.city && v >= 0)
    gain_xp(s.units[v], p.ranged ? 2 : 4);
  u.hp = p.attacker_after;
  if (p.city) {
    auto &city = s.cities[c];
    city.hp = p.defender_after;
    if (p.capture) {
      int previous_owner = city.owner;
      city.owner = u.owner;
      city.hp = 55;
      city.production = 0;
      city.job = NoJob;
      city.locked = 0;
      std::fill(city.invested, city.invested + JOBS, uint16_t(0));
      std::fill(city.queue, city.queue + QUEUE, uint8_t(0));
      city.queued = city.repeat = 0;
      if (v >= 0)
        s.units[v].tile = -1;
      u.tile = t;
      if (!alive(s, previous_owner))
        for (auto &lost : s.units)
          if (lost.owner == previous_owner)
            lost.tile = -1;
    }
  } else {
    auto &target = s.units[v];
    target.hp = p.defender_after;
    if (!target.hp) {
      target.tile = -1;
      if (!p.ranged && u.hp)
        u.tile = t;
    }
  }
  u.moves = 0;
  u.fortified = u.fortify_turns = 0;
  u.acted = 1;
  u.order = Manual;
  if (!u.hp)
    u.tile = -1;
  normalize_locks(s);
  vision(s);
  evaluate(s);
  return true;
}
int fortification_bonus(const Unit &u) {
  return military(u) && u.fortified ? 20 * u.fortify_turns : 0;
}
int healing_rate(const State &s, int i, int tile) {
  if (i < 0 || i >= UNITS || s.units[i].tile < 0)
    return 0;
  const auto &u = s.units[i];
  if (tile < 0)
    tile = u.tile;
  if (!land(s, tile))
    return 0;
  int c = city_at(s, tile), claim = working_city(s, tile);
  int amount = c >= 0 && s.cities[c].owner == u.owner           ? 25
               : claim >= 0 && s.cities[claim].owner == u.owner ? 20
                                                                : 10;
  return amount + ((u.promotions & 16) ? 8 : 0);
}
bool can_pillage(const State &s, int i) {
  if (i < 0 || i >= UNITS || s.result != Playing)
    return false;
  const auto &u = s.units[i];
  if (u.tile < 0 || !military(u) || !u.moves || city_at(s, u.tile) >= 0 ||
      (s.tiles[u.tile].improvement != 1 && s.tiles[u.tile].improvement != 2))
    return false;
  int c = working_city(s, u.tile);
  return c < 0 || war(s, u.owner, s.cities[c].owner);
}
bool pillage(State &s, int i) {
  if (!can_pillage(s, i))
    return false;
  auto &u = s.units[i];
  s.tiles[u.tile].improvement = 3; // Damaged; Workers repair the existing site.
  s.civ[u.owner].gold = std::min(60000, int(s.civ[u.owner].gold) + 25);
  u.hp = std::min(100, int(u.hp) + 25);
  --u.moves;
  u.acted = 1;
  u.fortified = u.fortify_turns = 0;
  u.order = Manual;
  return true;
}
int improvement_turns(const State &s, int tile) {
  if (!land(s, tile))
    return 0;
  if (s.tiles[tile].improvement == 3)
    return 2;
  return s.tiles[tile].terrain == Hill || s.tiles[tile].terrain == Forest ? 4
                                                                          : 3;
}
bool improve(State &s, int i) {
  if (i < 0 || i >= UNITS || s.result != Playing)
    return false;
  auto &u = s.units[i];
  if (u.tile < 0 || u.kind != Worker || !u.moves || u.work_left ||
      (s.tiles[u.tile].improvement && s.tiles[u.tile].improvement != 3))
    return false;
  int c = working_city(s, u.tile);
  if (c < 0 || s.cities[c].owner != u.owner || city_at(s, u.tile) >= 0)
    return false;
  bool mine = s.tiles[u.tile].terrain == Hill;
  if (mine && s.tiles[u.tile].improvement != 3 &&
      !(s.civ[u.owner].tech & (1 << Mining)))
    return false;
  u.project = mine ? 2 : 1;
  u.work_left = improvement_turns(s, u.tile);
  u.acted = 1;
  u.order = Manual;
  u.moves = 0;
  return true;
}
bool cancel_work(State &s, int i) {
  if (i < 0 || i >= UNITS || s.result != Playing || s.units[i].tile < 0 ||
      !s.units[i].work_left)
    return false;
  s.units[i].work_left = s.units[i].project = 0;
  return true;
}
bool fortify(State &s, int i) {
  if (i < 0 || i >= UNITS || s.units[i].tile < 0 || !s.units[i].moves ||
      s.units[i].work_left || s.result != Playing)
    return false;
  s.units[i].order = Manual;
  if (!s.units[i].fortified)
    s.units[i].fortify_turns = 0;
  s.units[i].fortified = 1;
  s.units[i].moves = 0;
  return true;
}
bool wake(State &s, int i) {
  if (i < 0 || i >= UNITS || s.units[i].tile < 0 || !s.units[i].fortified ||
      s.result != Playing)
    return false;
  s.units[i].fortified = s.units[i].fortify_turns = 0;
  s.units[i].order = Manual;
  return true;
}
int job_prerequisite(int job) {
  return job == Archer      ? Archery
         : job == Swordsman ? Iron
         : job == Granary   ? Pottery
         : job == Library   ? Writing
         : job == Barracks  ? Bronze
                            : -1;
}
bool available(const State &s, int p, int job) {
  if (p < 0 || p >= PLAYERS || job < 0 || job >= JOBS)
    return false;
  int tech = job_prerequisite(job);
  return tech < 0 || (s.civ[p].tech & (1 << tech));
}
bool can_produce(const State &s, int c, int j) {
  if (c < 0 || c >= CITIES || s.cities[c].tile < 0 || s.result != Playing)
    return false;
  const auto &v = s.cities[c];
  return available(s, v.owner, j) &&
         !(j >= KINDS && (v.buildings & (1 << (j - KINDS)))) &&
         (j != Settler || v.pop >= 2);
}
int construction_progress(const State &s, int c, int j) {
  if (c < 0 || c >= CITIES || s.cities[c].tile < 0 || j < 0 || j >= JOBS)
    return 0;
  const auto &v = s.cities[c];
  return j == v.job ? v.production
                    : std::min(1000, int(v.invested[j]) +
                                         (v.job == NoJob ? v.production : 0));
}
bool produce(State &s, int c, int j) {
  if (!can_produce(s, c, j))
    return false;
  auto &v = s.cities[c];
  if (v.job == j)
    return true;
  if (j >= KINDS)
    for (int n = 0; n < v.queued;) {
      if (v.queue[n] == j)
        remove_queue(s, c, n);
      else
        ++n;
    }
  int carry = v.job == NoJob ? v.production : 0;
  if (v.job < JOBS)
    v.invested[v.job] = v.production;
  v.job = j;
  v.production = std::min(1000, int(v.invested[j]) + carry);
  v.invested[j] = 0;
  v.repeat = 0;
  return true;
}
bool remove_queue(State &s, int c, int p) {
  if (c < 0 || c >= CITIES || s.cities[c].tile < 0 || p < 0 ||
      p >= s.cities[c].queued || s.result != Playing)
    return false;
  auto &v = s.cities[c];
  for (int n = p; n + 1 < v.queued; ++n)
    v.queue[n] = v.queue[n + 1];
  v.queue[--v.queued] = 0;
  return true;
}
bool enqueue(State &s, int c, int j) {
  if (!can_produce(s, c, j))
    return false;
  auto &v = s.cities[c];
  if (j >= KINDS) {
    if (v.job == j)
      return false;
    for (int n = 0; n < v.queued; ++n)
      if (v.queue[n] == j)
        return false;
  }
  if (v.job == NoJob)
    return produce(s, c, j);
  if (v.queued >= QUEUE)
    return false;
  v.queue[v.queued++] = j;
  v.repeat = 0;
  return true;
}
bool reorder_queue(State &s, int c, int p, int d) {
  if (c < 0 || c >= CITIES || s.cities[c].tile < 0 || p < 0 ||
      p >= s.cities[c].queued || (d != -1 && d != 1) || p + d < 0 ||
      p + d >= s.cities[c].queued || s.result != Playing)
    return false;
  std::swap(s.cities[c].queue[p], s.cities[c].queue[p + d]);
  return true;
}
bool repeat_production(State &s, int c) {
  if (c < 0 || c >= CITIES || s.cities[c].tile < 0 ||
      s.cities[c].job >= KINDS || s.cities[c].queued || s.result != Playing)
    return false;
  s.cities[c].repeat ^= 1;
  return true;
}
static void next_construction(State &s, int c) {
  auto &v = s.cities[c];
  if (v.job != NoJob)
    return;
  while (v.queued) {
    int j = v.queue[0];
    if (j >= KINDS && (v.buildings & (1 << (j - KINDS)))) {
      remove_queue(s, c, 0);
      continue;
    }
    if (!can_produce(s, c, j))
      return;
    remove_queue(s, c, 0);
    produce(s, c, j);
    return;
  }
}
static void finish_construction(State &s, int c, bool bought) {
  auto &v = s.cities[c];
  int completed = v.job;
  int remaining =
      bought ? 0 : std::max(0, int(v.production) - int(costs[completed]));
  if (!bought)
    remaining = std::min(
        remaining, std::max(int(costs[completed]), production_yield(s, c)));
  bool repeat = v.repeat && completed < KINDS && !v.queued;
  v.invested[completed] = 0;
  v.job = NoJob;
  v.production = remaining;
  v.repeat = 0;
  if (repeat && can_produce(s, c, completed)) {
    produce(s, c, completed);
    v.repeat = 1;
  } else
    next_construction(s, c);
}
int food_consumption(const State &s, int c) {
  return c >= 0 && c < CITIES && s.cities[c].tile >= 0 ? 2 * s.cities[c].pop
                                                       : 0;
}
int growth_threshold(int population) {
  int n = clamp(population, 1, 20) - 1, square = 0;
  while ((square + 1) * (square + 1) <= n * n * n)
    ++square;
  return 15 + 8 * n + square;
}
bool research(State &s, int p, int t) {
  if (p < 0 || p >= PLAYERS || t < 0 || t >= TECHS || s.result != Playing)
    return false;
  auto &v = s.civ[p];
  if ((v.tech & (1 << t)) ||
      (v.tech & tech_info[t].prerequisites) != tech_info[t].prerequisites)
    return false;
  v.research = t;
  return true;
}
int policy_cost(const State &s, int p) {
  int n = s.civ[p].policies;
  return 20 + 15 * ((n & 1) + ((n >> 1) & 1) + ((n >> 2) & 1));
}
bool policy(State &s, int p, int i) {
  if (p < 0 || p >= PLAYERS || i < 0 || i >= 3 || s.result != Playing)
    return false;
  auto &v = s.civ[p];
  int cost = policy_cost(s, p);
  if ((v.policies & (1 << i)) || v.culture < cost)
    return false;
  v.culture -= cost;
  v.policies |= 1 << i;
  return true;
}
Yield tile_yield(const State &s, int t) {
  Yield v;
  if (!inside(s, t) || s.tiles[t].terrain == Mountain)
    return v;
  int tr = s.tiles[t].terrain;
  v.food = tr == Grass                                   ? 2
           : tr == Plains || tr >= Coast || tr == Forest ? 1
                                                         : 0;
  v.production = tr == Hill ? 2 : tr == Forest || tr == Plains ? 1 : 0;
  v.gold = tr >= Coast ? 1 : 0;
  v.food +=
      (s.tiles[t].improvement == 1 ? 2 : 0) + (s.tiles[t].resource ? 1 : 0);
  v.production += s.tiles[t].improvement == 2 ? 2 : 0;
  return v;
}
int working_city(const State &s, int t) {
  if (!inside(s, t))
    return -1;
  int best = -1, dist = 3;
  for (int i = 0; i < CITIES; ++i) {
    if (s.cities[i].tile < 0)
      continue;
    int d = distance(t, s.cities[i].tile);
    if (d < dist) {
      best = i;
      dist = d;
    }
  }
  return best;
}
int city_tiles(const State &s, int i, int16_t out[18]) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0)
    return 0;
  int count = 0;
  radius(s, s.cities[i].tile, 2, [&](int t) {
    if (t != s.cities[i].tile)
      out[count++] = t;
  });
  return count;
}
bool tile_locked(const State &s, int i, int t) {
  int16_t tiles[18];
  int count = city_tiles(s, i, tiles);
  for (int n = 0; n < count; ++n)
    if (tiles[n] == t)
      return (s.cities[i].locked & (1u << n)) && working_city(s, t) == i &&
             s.tiles[t].terrain != Mountain;
  return false;
}
int worked_tiles(const State &s, int i, int16_t out[18]) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0)
    return 0;
  const auto &c = s.cities[i];
  struct Candidate {
    int tile, value, food;
  } ranked[18];
  int count = 0;
  int16_t tiles[18];
  int total = city_tiles(s, i, tiles);
  for (int n = 0; n < total; ++n) {
    int t = tiles[n];
    if (s.tiles[t].terrain == Mountain || working_city(s, t) != i)
      continue;
    auto y = tile_yield(s, t);
    int value = (c.locked & (1u << n) ? 100000 : 0) +
                y.food * (c.focus == FoodFocus ? 8 : 3) +
                y.production * (c.focus == ProductionFocus ? 8 : 2) +
                y.gold * (c.focus == GoldFocus ? 12 : 1);
    ranked[count++] = {t, value, y.food};
  }
  std::sort(ranked, ranked + count, [](const Candidate &a, const Candidate &b) {
    return a.value != b.value ? a.value > b.value : a.tile < b.tile;
  });
  int candidates = count;
  count = std::min(count, int(c.pop));
  if (c.focus == Balanced) {
    int food = 2 + ((c.buildings & 2) ? 2 : 0) +
               ((s.civ[c.owner].policies & 1) ? 1 : 0);
    for (int n = 0; n < count; ++n)
      food += ranked[n].food;
    while (food < 2 * c.pop) {
      int take = -1, drop = -1, best = -1000000;
      for (int a = 0; a < count; ++a)
        if (ranked[a].value < 100000)
          for (int b = count; b < candidates; ++b) {
            int gain = ranked[b].food - ranked[a].food;
            if (gain <= 0)
              continue;
            int score = gain * 1000 + ranked[b].value - ranked[a].value;
            if (score > best) {
              best = score;
              take = b;
              drop = a;
            }
          }
      if (take < 0)
        break;
      food += ranked[take].food - ranked[drop].food;
      std::swap(ranked[take], ranked[drop]);
    }
  }
  for (int n = 0; n < count; ++n)
    out[n] = ranked[n].tile;
  return count;
}
bool lock_tile(State &s, int i, int t, bool locked) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0 || s.result != Playing ||
      working_city(s, t) != i || t == s.cities[i].tile ||
      s.tiles[t].terrain == Mountain)
    return false;
  int16_t tiles[18];
  int total = city_tiles(s, i, tiles), bit = -1, used = 0;
  for (int n = 0; n < total; ++n) {
    if (tiles[n] == t)
      bit = n;
    used += (s.cities[i].locked >> n) & 1;
  }
  if (bit < 0 || (locked && !tile_locked(s, i, t) && used >= s.cities[i].pop))
    return false;
  if (locked)
    s.cities[i].locked |= 1u << bit;
  else
    s.cities[i].locked &= ~(1u << bit);
  return true;
}
bool auto_assign(State &s, int i) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0 || s.result != Playing)
    return false;
  s.cities[i].locked = 0;
  return true;
}
static void normalize_locks(State &s) {
  for (int i = 0; i < CITIES; ++i)
    if (s.cities[i].tile >= 0 && s.cities[i].locked) {
      int16_t worked[18], tiles[18];
      int count = worked_tiles(s, i, worked), total = city_tiles(s, i, tiles);
      uint32_t mask = 0;
      for (int n = 0; n < total; ++n)
        for (int k = 0; k < count; ++k)
          if (tiles[n] == worked[k] && (s.cities[i].locked & (1u << n)))
            mask |= 1u << n;
      s.cities[i].locked = mask;
    }
}
bool city_focus(State &s, int i, int f) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0 || f < 0 || f >= FOCUSES ||
      s.result != Playing)
    return false;
  s.cities[i].focus = f;
  return true;
}
Yield yields(const State &s, int i) {
  Yield y;
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0)
    return y;
  const auto &c = s.cities[i];
  y = {2, 2, 2, 2 + c.pop, 1};
  int16_t worked[18];
  int count = worked_tiles(s, i, worked);
  for (int n = 0; n < count; ++n) {
    auto v = tile_yield(s, worked[n]);
    y.food += v.food;
    y.production += v.production;
    y.gold += v.gold;
  }
  if (c.buildings & 1)
    y.culture += 2;
  if (c.buildings & 2)
    y.food += 2;
  if (c.buildings & 4)
    y.science += 2 + c.pop / 2;
  if (s.civ[c.owner].tech & (1 << Education))
    y.science += 3;
  if (s.civ[c.owner].policies & 1)
    y.food++;
  if (s.civ[c.owner].policies & 2)
    y.production++;
  if (s.civ[c.owner].policies & 4)
    y.science += 2;
  return y;
}
int production_yield(const State &s, int i, int job) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0)
    return 0;
  const auto &c = s.cities[i];
  auto y = yields(s, i);
  if (job < 0)
    job = c.job;
  if (job == NoJob)
    return 0;
  return y.production + (job == Settler && c.pop >= 2
                             ? std::max(0, y.food - food_consumption(s, i))
                             : 0);
}
int growth_surplus(const State &s, int i) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0)
    return 0;
  const auto &c = s.cities[i];
  int net = yields(s, i).food - food_consumption(s, i);
  return c.job == Settler && c.pop >= 2 ? std::min(0, net) : net;
}
int score(const State &s, int o) {
  int v = 0;
  for (const auto &c : s.cities)
    if (c.tile >= 0 && c.owner == o)
      v += 25 + c.pop * 8;
  for (int t = 0; t < TECHS; t++)
    if (s.civ[o].tech & (1 << t))
      v += 12;
  for (auto &t : s.tiles)
    if (t.seen & (1 << o))
      v++;
  return v;
}
void evaluate(State &s) {
  if (s.result != Playing)
    return;
  for (int o = 0; o < PLAYERS; o++) {
    if (!alive(s, o))
      continue;
    bool all = true;
    for (int p = 0; p < PLAYERS; p++) {
      int c = city_at(s, s.capitals[p]);
      if (c < 0 || s.cities[c].owner != o)
        all = false;
    }
    if (all) {
      s.result = Domination;
      s.winner = o;
      return;
    }
    if (s.civ[o].tech == ((1 << TECHS) - 1)) {
      s.result = Science;
      s.winner = o;
      return;
    }
  }
  if (!alive(s, 0)) {
    s.result = Defeat;
    s.winner = score(s, 1) >= score(s, 2) ? 1 : 2;
  } else if (s.turn > 150) {
    s.result = Score;
    s.winner = 0;
    for (int o = 1; o < 3; o++)
      if (score(s, o) > score(s, s.winner))
        s.winner = o;
  }
}
static int spawn(State &s, int o, int k, int t) {
  int v = add_unit(s, o, k, t);
  for (int d = 0; d < 6 && v < 0; d++)
    v = add_unit(s, o, k, neighbor(t, d));
  return v;
}
int purchase_cost(const State &s, int i) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0)
    return 0;
  auto &c = s.cities[i];
  return c.job == NoJob
             ? 0
             : std::max(1, int(costs[c.job]) - int(c.production)) * 3;
}
static bool complete(State &s, City &c) {
  if (c.job < KINDS) {
    if (c.job == Settler && c.pop < 2)
      return false;
    int unit = spawn(s, c.owner, c.job, c.tile);
    if (unit < 0)
      return false;
    if ((c.buildings & 8) && military(s.units[unit]))
      s.units[unit].xp = 15;
  } else
    c.buildings |= 1 << (c.job - 6);
  return true;
}
bool purchase(State &s, int i) {
  if (i < 0 || i >= CITIES || s.cities[i].tile < 0 || s.result != Playing)
    return false;
  auto &c = s.cities[i];
  int cost = purchase_cost(s, i);
  if (s.civ[c.owner].gold < cost || !can_produce(s, i, c.job) ||
      (c.job >= 6 && (c.buildings & (1 << (c.job - 6)))))
    return false;
  if (!complete(s, c))
    return false;
  s.civ[c.owner].gold -= cost;
  finish_construction(s, i, true);
  vision(s);
  return true;
}
static void economy(State &s, int o) {
  auto &civ = s.civ[o];
  int sci = 0, culture = 0, gold = 0;
  for (auto &c : s.cities)
    if (c.tile >= 0 && c.owner == o) {
      int idx = int(&c - s.cities);
      auto y = yields(s, idx);
      sci += y.science;
      culture += y.culture;
      gold += y.gold;
      int net = y.food - food_consumption(s, idx),
          output = c.job == NoJob ? 0 : y.production;
      bool settler = c.job == Settler && c.pop >= 2;
      if (settler)
        output += std::max(0, net);
      int next_food = int(c.food) + (settler ? std::min(0, net) : net);
      if (next_food < 0 && c.pop > 1) {
        --c.pop;
        next_food = 0;
      }
      c.food = clamp(next_food, 0, 600);
      int needed = growth_threshold(c.pop);
      if (c.food >= needed && c.pop < 20) {
        c.food -= needed;
        c.pop++;
      }
      next_construction(s, idx);
      c.production = std::min(1000, int(c.production) + output);
      if (c.job < JOBS && c.production >= costs[c.job] && complete(s, c))
        finish_construction(s, idx, false);
      c.hp = std::min(100, int(c.hp) + 8);
      // The garrison attacks a single visible hostile per turn.
      int target = -1;
      for (int i = 0; i < UNITS; i++) {
        auto &u = s.units[i];
        if (u.tile >= 0 && war(s, o, u.owner) &&
            distance(c.tile, u.tile) <= 2 && clear_shot(s, c.tile, u.tile) &&
            (s.tiles[u.tile].visible & (1 << o)) &&
            (target < 0 || (military(u) && !military(s.units[target])) ||
             (military(u) == military(s.units[target]) &&
              u.hp < s.units[target].hp)))
          target = i;
      }
      if (target >= 0) {
        auto &u = s.units[target];
        gain_xp(u, 2);
        u.hp = std::max(0, int(u.hp) - 18);
        if (!u.hp)
          u.tile = -1;
      }
    }
  civ.gold = std::min(60000, int(civ.gold) + gold);
  civ.culture = std::min(60000, int(civ.culture) + culture);
  civ.science = std::min(60000, int(civ.science) + sci);
  int tech = civ.research;
  if (tech < TECHS && !(civ.tech & (1 << tech)) &&
      civ.science >= tech_info[tech].cost &&
      (civ.tech & tech_info[tech].prerequisites) ==
          tech_info[tech].prerequisites) {
    civ.science -= tech_info[tech].cost;
    civ.tech |= 1 << tech;
    for (int t = 1; t < TECHS; t++)
      if (research(s, o, t))
        break;
  }
  for (auto &u : s.units)
    if (u.tile >= 0 && u.owner == o) {
      if (!u.work_left && !u.acted &&
          (u.moves == (u.kind == Scout ? 3 : 2) || u.fortified ||
           u.order == Healing))
        u.hp = std::min(100, int(u.hp) + healing_rate(s, int(&u - s.units)));
      if (u.fortified && !u.acted && military(u))
        u.fortify_turns = std::min(2, int(u.fortify_turns) + 1);
      if (u.order == Waiting || (u.order == Healing && u.hp == 100)) {
        if (u.order == Healing)
          u.fortified = u.fortify_turns = 0;
        u.order = Manual;
      }
      if (u.work_left) {
        int c = working_city(s, u.tile);
        if (c < 0 || s.cities[c].owner != o ||
            (s.tiles[u.tile].improvement && s.tiles[u.tile].improvement != 3)) {
          u.work_left = u.project = 0;
        } else if (!--u.work_left) {
          s.tiles[u.tile].improvement = u.project;
          if (s.tiles[u.tile].terrain == Forest)
            s.tiles[u.tile].terrain = Grass;
          u.project = 0;
        }
      }
      u.moves = u.kind == Scout ? 3 : 2;
      u.acted = 0;
    }
}
namespace {
// Rebuild from visible opponents after each action. Expansion touches local
// hexes once instead of scanning every enemy for every candidate map tile.
struct ThreatMap {
  uint8_t attack[N]{}, melee[N]{}, approach[N]{};
  ThreatMap(const State &s, int owner) {
    for (const auto &v : s.units)
      if (v.tile >= 0 && military(v) && war(s, owner, v.owner) &&
          (s.tiles[v.tile].visible & (1 << owner))) {
        radius(s, v.tile, v.kind == Archer ? 4 : 3,
               [&](int t) { ++approach[t]; });
        radius(s, v.tile, v.kind == Archer ? 2 : 1, [&](int t) {
          ++attack[t];
          if (v.kind != Archer)
            ++melee[t];
        });
      }
    for (const auto &c : s.cities)
      if (c.tile >= 0 && war(s, owner, c.owner) &&
          (s.tiles[c.tile].visible & (1 << owner)))
        radius(s, c.tile, 2, [&](int t) { ++attack[t]; });
  }
};
int attack_value(const State &s, int i, int t, bool retreat) {
  auto p = battle_preview(s, i, t);
  if (!p.possible || !p.attacker_after ||
      (retreat && p.defender_after && !p.capture))
    return 0;
  // Ranged fire at a one-HP city cannot help capture it. Do not waste the turn.
  int damage = p.defender_hp - p.defender_after;
  if (!damage && !p.capture)
    return 0;
  int v = unit_at(s, t);
  return damage - (p.attacker_hp - p.attacker_after) +
         (!p.defender_after ? 65 : 0) + (p.capture ? 120 : 0) +
         (v >= 0 && military(s.units[v]) ? 12 : 0);
}
bool tactical_attack(State &s, int i, const ThreatMap &threat, bool retreat) {
  auto &u = s.units[i];
  if (!military(u))
    return false;
  // Most units have no local combat. Avoid constructing a reachability map or
  // trying firing positions until a visible opponent is within this turn's
  // movement plus attack envelope.
  int envelope = u.moves + (u.kind == Archer ? 2 : 1);
  bool contact = false;
  for (const auto &v : s.units)
    contact |= v.tile >= 0 && war(s, u.owner, v.owner) &&
               (s.tiles[v.tile].visible & (1 << u.owner)) &&
               distance(u.tile, v.tile) <= envelope;
  for (const auto &c : s.cities)
    contact |= c.tile >= 0 && war(s, u.owner, c.owner) &&
               (s.tiles[c.tile].visible & (1 << u.owner)) &&
               distance(u.tile, c.tile) <= envelope;
  if (!contact)
    return false;
  int16_t reach[N];
  reachable(s, i, reach);
  int origin = u.tile, moves = u.moves, target = -1, position = origin,
      best = 0;
  // Include current location, then legal destinations leaving an attack point.
  radius(s, origin, moves, [&](int tile) {
    if (reach[tile] < 0 || reach[tile] >= moves)
      return;
    u.tile = tile;
    u.moves = moves - reach[tile];
    radius(s, tile, u.kind == Archer ? 2 : 1, [&](int t) {
      int value = attack_value(s, i, t, retreat);
      if (!value)
        return;
      value -= reach[tile] * 2;
      // Prefer a firing tile away from melee, retaining the same target.
      if (u.kind == Archer)
        value -= threat.melee[tile] * 22;
      if (value > best) {
        best = value;
        target = t;
        position = tile;
      }
    });
  });
  u.tile = origin;
  u.moves = moves;
  if (target < 0)
    return false;
  if (position != origin && !move(s, i, position))
    return false;
  return attack(s, i, target);
}
int settlement_value(const State &s, int owner, int tile) {
  if (!land(s, tile) || !(s.tiles[tile].seen & (1 << owner)) ||
      !can_enter(s, owner, tile))
    return -10000;
  for (const auto &c : s.cities)
    if (c.tile >= 0 && distance(c.tile, tile) < 4)
      return -10000;
  int food = 0, output = 0, workable = 0;
  radius(s, tile, 2, [&](int t) {
    if (!(s.tiles[t].seen & (1 << owner)))
      return;
    auto y = tile_yield(s, t);
    food += y.food;
    output += y.production;
    workable += land(s, t);
  });
  return workable >= 7 && food >= 10 ? food * 3 + output * 2 + workable
                                     : -10000;
}
} // namespace
void ai(State &s, int o) {
  if (o < 0 || o >= 3 || !alive(s, o) || s.result != Playing)
    return;
  for (int p = 0; p < 3; p++)
    policy(s, o, p);
  if (s.turn >= 24 && s.turn % 18 == uint16_t(o * 3))
    for (int p = 0; p < PLAYERS; ++p) {
      if (p == o || war(s, o, p))
        continue;
      int own_power = 0, known_power = 0;
      bool known_city = false;
      for (const auto &c : s.cities)
        known_city |=
            c.tile >= 0 && c.owner == p && (s.tiles[c.tile].seen & (1 << o));
      for (const auto &u : s.units)
        if (u.tile >= 0 && military(u)) {
          if (u.owner == o)
            own_power += strength(s, u) * u.hp / 100;
          else if (u.owner == p && (s.tiles[u.tile].visible & (1 << o)))
            known_power += strength(s, u) * u.hp / 100;
        }
      if (known_city && own_power >= 25 && own_power > known_power * 3 / 2)
        diplomacy(s, o, p, true);
    }
  for (int i = 0; i < UNITS; i++) {
    auto &u = s.units[i];
    if (u.tile < 0 || u.owner != o || !u.moves)
      continue;
    ThreatMap threat(s, o);
    auto danger = [&](int t) { return threat.attack[t]; };
    if (u.order == Healing) {
      if (!danger(u.tile) && u.hp < 100)
        continue;
      cancel_order(s, i);
    }
    for (int n = 0; n < 4 && earned_promotions(u) > promotion_count(u); ++n) {
      int p =
          u.hp < 40                           ? Recover
          : u.hp < 80 && !(u.promotions & 16) ? Medic
          : s.tiles[u.tile].terrain == Hill || s.tiles[u.tile].terrain == Forest
              ? RoughCombat
              : OpenCombat;
      if (!promote(s, i, p)) {
        for (int k = 0; k < PROMOTIONS; ++k)
          if (promote(s, i, k))
            break;
      }
    }
    if (!u.moves)
      continue;
    if (can_upgrade(s, i)) {
      upgrade(s, i);
      continue;
    }
    if (can_pillage(s, i)) {
      pillage(s, i);
      if (!u.moves)
        continue;
    }
    bool civilian = u.kind == Settler || u.kind == Worker;
    bool retreat = u.hp < 50 || (civilian && danger(u.tile));
    if (!civilian && u.hp < 100 && !threat.approach[u.tile] && u.hp >= 50) {
      heal(s, i);
      continue;
    }
    if (!retreat) {
      if (u.work_left || (u.kind == Worker && improve(s, i)))
        continue;
    }
    if (tactical_attack(s, i, threat, retreat))
      continue;
    if (s.result != Playing)
      continue;
    int goal = -1;
    if (retreat) {
      for (const auto &c : s.cities)
        if (c.tile >= 0 && c.owner == o &&
            (goal < 0 || distance(u.tile, c.tile) < distance(u.tile, goal)))
          goal = c.tile;
      if (!danger(u.tile) && (goal < 0 || distance(u.tile, goal) <= 2)) {
        if (!civilian)
          heal(s, i);
        continue;
      }
    } else if (u.kind == Worker) {
      int work_value = -10000;
      for (int ci = 0; ci < CITIES; ++ci) {
        const auto &c = s.cities[ci];
        if (c.tile < 0 || c.owner != o)
          continue;
        radius(s, c.tile, 2, [&](int t) {
          if (land(s, t) && (s.tiles[t].seen & (1 << o)) &&
              (s.tiles[t].improvement == 0 || s.tiles[t].improvement == 3) &&
              !danger(t) && working_city(s, t) == ci && t != c.tile &&
              city_at(s, t) < 0 &&
              (s.tiles[t].terrain != Hill || (s.civ[o].tech & (1 << Mining))) &&
              (unit_at(s, t) < 0 || unit_at(s, t) == i)) {
            int value = (s.tiles[t].improvement == 3 ? 30 : 0) +
                        tile_yield(s, t).food * 3 +
                        tile_yield(s, t).production * 2 -
                        distance(u.tile, t) * 3;
            if (value > work_value) {
              work_value = value;
              goal = t;
            }
          }
        });
      }
    } else if (u.kind == Settler) {
      int best = -10000;
      for (int t = 0; t < N; t++) {
        int value = settlement_value(s, o, t) - distance(u.tile, t) * 3 -
                    danger(t) * 100;
        if (value > best) {
          best = value;
          goal = t;
        }
      }
    } else {
      int urgency = 0;
      for (const auto &c : s.cities)
        if (c.tile >= 0 && c.owner == o) {
          int value = threat.approach[c.tile] * 40 - distance(u.tile, c.tile);
          if (threat.approach[c.tile] && value > urgency) {
            urgency = value;
            goal = c.tile;
          }
        }
      if (goal < 0)
        for (const auto &c : s.cities)
          if (c.tile >= 0 && war(s, o, c.owner) &&
              (s.tiles[c.tile].seen & (1 << o)) &&
              (goal < 0 || distance(u.tile, c.tile) < distance(u.tile, goal)))
            goal = c.tile;
    }
    if (goal < 0 && u.kind != Worker) {
      int best = -999;
      for (int t = 0; t < N; t++)
        if (land(s, t) && can_enter(s, o, t) && (s.tiles[t].seen & (1 << o))) {
          int frontier = 0;
          for (int d = 0; d < 6; ++d) {
            int n = neighbor(t, d);
            if (inside(s, n) && !(s.tiles[n].seen & (1 << o)))
              ++frontier;
          }
          int value = frontier * 10 - distance(u.tile, t);
          if (frontier && value > best) {
            best = value;
            goal = t;
          }
        }
    }
    if (goal < 0) {
      if (!civilian && u.hp < 100)
        fortify(s, i);
      continue;
    }
    // A reverse flood gives a route around mountains, water and occupied
    // tiles, rather than greedily stepping toward a blocked destination.
    BoardIndex index(s);
    int16_t paths[N], queue[N];
    std::fill(paths, paths + N, int16_t(-1));
    int head = 0, tail = 0;
    queue[tail++] = goal;
    paths[goal] = 0;
    while (head < tail) {
      int at = queue[head++];
      for (int d = 0; d < 6; ++d) {
        int t = neighbor(at, d);
        if (!land(s, t) || !(s.tiles[t].seen & (1 << o)) || paths[t] >= 0)
          continue;
        int other = index.units[t], c = index.cities[t],
            claim = index.claims[t];
        if (claim >= 0 && s.cities[claim].owner != o &&
            !war(s, o, s.cities[claim].owner))
          continue;
        if ((other >= 0 && other != i) || (c >= 0 && s.cities[c].owner != o))
          continue;
        paths[t] = paths[at] + 1;
        queue[tail++] = t;
      }
    }
    int16_t reach[N];
    reachable(s, i, reach);
    int dest = -1, best = paths[u.tile] >= 0
                              ? 1000 - paths[u.tile] * 10 -
                                    danger(u.tile) * (civilian ? 180 : 40)
                              : -9999;
    for (int t = 0; t < N; ++t)
      if (reach[t] > 0 && paths[t] >= 0) {
        int value = 1000 - paths[t] * 10 - reach[t] -
                    danger(t) * (civilian  ? 180
                                 : retreat ? 80
                                           : 4) +
                    (retreat && !civilian ? healing_rate(s, i, t) : 0);
        if (value > best) {
          dest = t;
          best = value;
        }
      }
    if (dest >= 0)
      move(s, i, dest);
    if (!retreat && u.kind == Settler && u.tile == goal)
      found(s, i);
    if (!retreat && u.kind == Worker)
      improve(s, i);
  }
  int cities = 0, settlers = 0, workers = 0, army = 0, ranged = 0;
  bool at_war = false;
  for (int p = 0; p < PLAYERS; ++p)
    at_war |= war(s, o, p);
  for (const auto &c : s.cities)
    if (c.tile >= 0 && c.owner == o) {
      cities++;
      settlers += c.job == Settler;
      workers += c.job == Worker;
      army += c.job < KINDS && c.job != Settler && c.job != Worker;
      ranged += c.job == Archer;
    }
  for (const auto &u : s.units)
    if (u.tile >= 0 && u.owner == o) {
      settlers += u.kind == Settler;
      workers += u.kind == Worker;
      army += military(u);
      ranged += u.kind == Archer;
    }
  ThreatMap threat(s, o);
  // Choose an available research goal; progress is the civilization's shared
  // pool. Archery is urgent under attack, then economy/terrain unlocks.
  const int priorities[] = {Pottery,   Mining,      Writing,     Archery,
                            Bronze,    Wheel,       Mathematics, Iron,
                            Education, Engineering, Astronomy};
  bool threatened = false;
  for (const auto &c : s.cities)
    threatened |= c.tile >= 0 && c.owner == o && threat.approach[c.tile];
  if (!(threatened && research(s, o, Archery)))
    for (int tech : priorities)
      if (research(s, o, tech))
        break;
  for (int i = 0; i < CITIES; i++) {
    auto &c = s.cities[i];
    if (c.tile < 0 || c.owner != o)
      continue;
    bool emergency = threat.approach[c.tile] != 0;
    bool training_army = c.job < KINDS && c.job != Settler && c.job != Worker;
    if (c.job != NoJob && (!emergency || training_army))
      continue;
    int soldier = available(s, o, Archer) && ranged * 3 < army + 1 ? Archer
                  : available(s, o, Swordsman)                     ? Swordsman
                                                                   : Warrior;
    int j = NoJob;
    if (emergency || army < cities)
      j = soldier;
    else if (cities < 3 && !settlers && s.turn > 8 && c.pop >= 2 && !at_war) {
      // Do not pay for settlers when all known sites are unsuitable.
      for (int t = 0; t < N && j == NoJob; ++t)
        if (settlement_value(s, o, t) > 0 && !threat.attack[t])
          j = Settler;
    }
    if (j == NoJob) {
      if (workers < cities)
        j = Worker;
      else if (!(c.buildings & 1))
        j = Monument;
      else if (!(c.buildings & 2) && available(s, o, Granary))
        j = Granary;
      else if (!(c.buildings & 4) && available(s, o, Library))
        j = Library;
      else if (at_war && !(c.buildings & 8) && available(s, o, Barracks))
        j = Barracks;
      else if (army < cities * 2 + (at_war ? 2 : 0))
        j = soldier;
    }
    int previous = c.job;
    if (j == NoJob || !produce(s, i, j))
      continue;
    settlers -= previous == Settler;
    workers -= previous == Worker;
    settlers += j == Settler;
    workers += j == Worker;
    army += j < KINDS && j != Settler && j != Worker;
    ranged += j == Archer;
  }
}
void end_turn(State &s) {
  if (s.result != Playing)
    return;
  for (int o = 1; o < 3 && s.result == Playing; o++)
    ai(s, o);
  if (s.result != Playing)
    return;
  vision(s);
  for (int o = 0; o < 3; o++)
    if (alive(s, o))
      economy(s, o);
  normalize_locks(s);
  s.turn++;
  for (auto &t : s.truce)
    if (t)
      t--;
  vision(s);
  evaluate(s);
}
// Portable little-endian format, version and FNV checksum; no raw
// struct/padding writes.
size_t serialize(const State &s, uint8_t *out, size_t cap) {
  const size_t need = 4 + 2 + 4 + 2 + 6 + 6 + 3 * 10 +
                      (size_t(s.width) * s.height * 9 + 7) / 8 + UNITS * 8 +
                      CITIES * 47 + 4;
  static_assert(4 + 2 + 4 + 2 + 6 + 6 + 3 * 10 + (N * 9 + 7) / 8 + UNITS * 8 +
                    CITIES * 47 + 4 <=
                2048);
  if (!out || cap < need || !valid(s))
    return 0;
  size_t n = 0;
  auto b = [&](unsigned v) { out[n++] = uint8_t(v); };
  auto w = [&](unsigned v) {
    b(v);
    b(v >> 8);
  };
  auto d = [&](uint32_t v) {
    w(v);
    w(v >> 16);
  };
  b('U');
  b('P');
  b('X');
  b(7);
  b(s.width);
  b(s.height);
  d(s.rng);
  w(s.turn);
  for (int t : s.capitals)
    w(t);
  b(s.wars);
  for (int t : s.truce)
    b(t);
  b(s.result);
  b(s.winner);
  for (const auto &v : s.civ) {
    w(v.tech);
    w(v.science);
    w(v.gold);
    w(v.culture);
    b(v.research);
    b(v.policies);
  }
  uint32_t bits = 0;
  int used = 0;
  for (int y = 0; y < s.height; ++y)
    for (int x = 0; x < s.width; ++x) {
      const auto &t = s.tiles[y * W + x];
      unsigned value =
          t.terrain | (t.improvement << 3) | (t.resource << 5) | (t.seen << 6);
      bits |= value << used;
      used += 9;
      while (used >= 8) {
        b(bits);
        bits >>= 8;
        used -= 8;
      }
    }
  if (used)
    b(bits);
  for (const auto &u : s.units) {
    w(u.tile);
    b(u.owner | (u.fortify_turns << 2));
    b(u.kind);
    b(u.hp);
    b(u.moves | (u.promotions << 2));
    b(u.fortified | (u.xp << 1));
    b((u.project << 3) | u.work_left | (u.order << 5) | (u.acted << 7));
  }
  for (const auto &c : s.cities) {
    w(c.tile);
    b(c.owner);
    b(c.pop);
    b(c.hp);
    b(c.buildings);
    b(c.job);
    w(c.food);
    w(c.production);
    b(c.focus);
    d(c.locked);
    for (auto progress : c.invested)
      w(progress);
    b(c.queued);
    for (auto j : c.queue)
      b(j);
    b(c.repeat);
  }
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++)
    h = (h ^ out[i]) * 16777619u;
  d(h);
  return n;
}
bool valid(const State &s) {
  if (s.width < 16 || s.width > W || s.height < 12 || s.height > H)
    return false;
  if (!s.rng || s.turn < 1 || s.turn > 151 || s.wars > 7 || s.result > Defeat ||
      (s.result == Playing ? s.winner != 255 : s.winner >= 3))
    return false;
  for (auto t : s.truce)
    if (t > 10)
      return false;
  for (auto t : s.capitals)
    if (t < -1 || t >= N)
      return false;
  for (const auto &t : s.tiles)
    if (t.terrain > Ocean || t.improvement > 3 || t.resource > 1 || t.seen > 7)
      return false;
  for (const auto &c : s.civ)
    if (c.tech > 4095 || !(c.tech & 1) || c.research >= TECHS || c.policies > 7)
      return false;
  for (int i = 0; i < UNITS; i++) {
    auto &u = s.units[i];
    if (u.tile < -1 || u.tile >= N || u.kind >= KINDS || u.owner >= 3 ||
        u.moves > (u.kind == Scout ? 3 : 2) || u.fortified > 1 || u.hp > 100 ||
        u.fortify_turns > 2 ||
        (u.fortify_turns && (!u.fortified || !military(u))) ||
        u.work_left > 4 || u.project > 2 || ((!u.work_left) != (!u.project)) ||
        u.xp > 127 || u.promotions > 63 || u.order > Healing || u.acted > 1 ||
        promotion_count(u) > earned_promotions(u) ||
        (!military(u) && (u.xp || u.promotions)) ||
        (u.work_left && u.order != Manual) ||
        (u.work_left && (u.kind != Worker || u.fortified)) ||
        (u.tile >= 0 && (!u.hp || !land(s, u.tile))))
      return false;
    if (u.tile >= 0) {
      for (int j = 0; j < i; j++)
        if (s.units[j].tile == u.tile)
          return false;
      int c = city_at(s, u.tile);
      if (c >= 0 && s.cities[c].owner != u.owner)
        return false;
    }
  }
  for (int i = 0; i < CITIES; i++) {
    auto &c = s.cities[i];
    if (c.tile < -1 || c.tile >= N || c.owner >= 3 || !c.pop || c.pop > 20 ||
        !c.hp || c.hp > 100 || c.buildings > 15 || c.job > NoJob ||
        c.food > 600 || c.production > 1000 || c.focus >= FOCUSES ||
        c.locked > 0x3ffffu)
      return false;
    if (c.queued > QUEUE || c.repeat > 1 || (c.repeat && c.job >= KINDS))
      return false;
    for (int j = 0; j < JOBS; ++j)
      if (c.invested[j] > 1000 || (j == c.job && c.invested[j]))
        return false;
    for (int n = 0; n < QUEUE; ++n) {
      int j = c.queue[n];
      if (n >= c.queued) {
        if (j)
          return false;
        continue;
      }
      if (j >= JOBS || !available(s, c.owner, j))
        return false;
      if (j >= KINDS) {
        if (j == c.job || (c.buildings & (1 << (j - KINDS))))
          return false;
        for (int m = 0; m < n; ++m)
          if (c.queue[m] == j)
            return false;
      }
    }
    if (c.tile >= 0) {
      if (!land(s, c.tile) || (c.job < JOBS && !available(s, c.owner, c.job)))
        return false;
      for (int j = 0; j < i; j++)
        if (s.cities[j].tile == c.tile)
          return false;
    }
  }
  return true;
}
bool restore(State &s, const uint8_t *in, size_t len) {
  if (!in || len < 8 || (memcmp(in, "UPX", 3) || (in[3] < 1 || in[3] > 7)))
    return false;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len - 4; i++)
    h = (h ^ in[i]) * 16777619u;
  uint32_t expected = uint32_t(in[len - 4]) | (uint32_t(in[len - 3]) << 8) |
                      (uint32_t(in[len - 2]) << 16) |
                      (uint32_t(in[len - 1]) << 24);
  if (h != expected)
    return false;
  size_t n = 4;
  bool ok = true;
  auto b = [&]() -> unsigned {
    if (n >= len - 4) {
      ok = false;
      return 0;
    }
    return in[n++];
  };
  auto w = [&]() {
    unsigned lo = b();
    return lo | (b() << 8);
  };
  State v;
  v.width = in[3] == 1 ? 20 : b();
  v.height = in[3] == 1 ? 14 : b();
  if (v.width < 16 || v.width > W || v.height < 12 || v.height > H)
    return false;
  for (auto &t : v.tiles)
    t.terrain = Ocean;
  unsigned lo = w();
  v.rng = lo | (uint32_t(w()) << 16);
  v.turn = w();
  for (auto &t : v.capitals)
    t = int16_t(w());
  v.wars = b();
  for (auto &t : v.truce)
    t = b();
  v.result = b();
  v.winner = b();
  for (auto &c : v.civ) {
    c.tech = w();
    c.science = w();
    c.gold = w();
    c.culture = w();
    c.research = b();
    c.policies = b();
  }
  uint32_t bits = 0;
  int used = 0;
  for (int y = 0; y < v.height; ++y)
    for (int x = 0; x < v.width; ++x) {
      auto &t = v.tiles[y * W + x];
      unsigned value;
      if (in[3] >= 6) {
        while (used < 9) {
          bits |= b() << used;
          used += 8;
        }
        value = bits & 511;
        bits >>= 9;
        used -= 9;
      } else {
        value = b();
        if (value & ~63u)
          ok = false;
        unsigned seen = b();
        if (seen > 7)
          ok = false;
        value |= seen << 6;
      }
      t.terrain = value & 7;
      t.improvement = (value >> 3) & 3;
      if (in[3] < 7 && t.improvement == 3)
        ok = false;
      t.resource = (value >> 5) & 1;
      t.seen = (value >> 6) & 7;
    }
  if (in[3] >= 6 && bits)
    ok = false;
  for (auto &u : v.units) {
    u.tile = int16_t(w());
    unsigned owner = b();
    u.owner = in[3] >= 7 ? owner & 3 : owner;
    u.fortify_turns = in[3] >= 7 ? owner >> 2 : 0;
    u.kind = b();
    u.hp = b();
    unsigned moves = b(), fortified = b();
    u.moves = in[3] >= 5 ? moves & 3 : moves;
    u.promotions = in[3] >= 5 ? moves >> 2 : 0;
    u.fortified = in[3] >= 5 ? fortified & 1 : fortified;
    u.xp = in[3] >= 5 ? fortified >> 1 : 0;
    if (in[3] < 7 && u.fortified && military(u))
      u.fortify_turns = 1;
    if (in[3] >= 3) {
      unsigned packed = b();
      u.work_left = packed & 7;
      u.project = in[3] >= 5 ? (packed >> 3) & 3 : packed >> 3;
      u.order = in[3] >= 5 ? (packed >> 5) & 3 : 0;
      u.acted = in[3] >= 5 ? packed >> 7 : 0;
    }
  }
  for (auto &c : v.cities) {
    c.tile = int16_t(w());
    c.owner = b();
    c.pop = b();
    c.hp = b();
    c.buildings = b();
    c.job = b();
    c.food = w();
    c.production = w();
    if (in[3] >= 3)
      c.focus = b();
    if (in[3] >= 4) {
      unsigned low = w();
      c.locked = low | (uint32_t(w()) << 16);
    }
    if (in[3] >= 6) {
      for (auto &progress : c.invested)
        progress = w();
      c.queued = b();
      for (auto &j : c.queue)
        j = b();
      c.repeat = b();
    }
  }
  if (in[3] == 1) {
    auto convert = [](int t) { return t < 0 ? -1 : (t / 20) * W + t % 20; };
    for (auto &t : v.capitals)
      t = convert(t);
    for (auto &u : v.units)
      u.tile = convert(u.tile);
    for (auto &c : v.cities)
      c.tile = convert(c.tile);
  }
  if (!ok || n != len - 4 || !valid(v))
    return false;
  if (in[3] < 6) {
    State moved = v;
    if (expel_units(moved))
      v = moved;
  }
  vision(v);
  s = v;
  return true;
}
} // namespace unciv
