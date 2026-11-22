// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace unciv {
constexpr int W = 36, H = 20, N = W * H, PLAYERS = 3, UNITS = 40, CITIES = 12,
              TECHS = 12, QUEUE = 9;
enum Terrain : uint8_t {
  Grass,
  Plains,
  Desert,
  Hill,
  Mountain,
  Forest,
  Coast,
  Ocean
};
enum Kind : uint8_t {
  Settler,
  Warrior,
  Scout,
  Worker,
  Archer,
  Swordsman,
  KINDS
};
enum Building : uint8_t {
  Monument = 6,
  Granary,
  Library,
  Barracks,
  JOBS,
  NoJob = JOBS
};
enum Tech : uint8_t {
  Agriculture,
  Pottery,
  Mining,
  Archery,
  Writing,
  Bronze,
  Wheel,
  Mathematics,
  Iron,
  Education,
  Engineering,
  Astronomy
};
enum Outcome : uint8_t { Playing, Domination, Science, Score, Defeat };
enum Focus : uint8_t {
  Balanced,
  FoodFocus,
  ProductionFocus,
  GoldFocus,
  FOCUSES
};
enum Order : uint8_t { Manual, Waiting, Healing };
enum Promotion : uint8_t {
  OpenCombat,
  RoughCombat,
  Medic,
  Recover,
  PROMOTIONS
};
struct Tile {
  uint8_t terrain, improvement, resource, seen, visible;
};
struct Unit {
  int16_t tile = -1;
  uint8_t owner = 0, kind = 0, hp = 100, moves = 2, fortified = 0;
  uint8_t work_left = 0, project = 0, fortify_turns = 0;
  uint8_t xp = 0, promotions = 0, order = Manual, acted = 0;
};
struct City {
  int16_t tile = -1;
  uint8_t owner = 0, pop = 1, hp = 100, buildings = 0, job = NoJob;
  uint16_t food = 0, production = 0;
  uint8_t focus = Balanced;
  uint32_t locked = 0;
  uint16_t invested[JOBS]{};
  uint8_t queue[QUEUE]{}, queued = 0, repeat = 0;
};
struct Civ {
  uint16_t tech = 1, science = 0, gold = 30, culture = 0;
  uint8_t research = Pottery, policies = 0;
};
struct State {
  uint8_t width = W, height = H;
  Tile tiles[N]{};
  Unit units[UNITS]{};
  City cities[CITIES]{};
  Civ civ[PLAYERS]{};
  uint32_t rng = 1;
  uint16_t turn = 1;
  int16_t capitals[PLAYERS]{-1, -1, -1};
  uint8_t wars = 0, truce[3]{}, result = Playing, winner = 255;
};
struct Yield {
  int food = 0, production = 0, gold = 0, science = 0, culture = 0;
};
struct TechInfo {
  uint16_t prerequisites, cost;
};
struct BattlePreview {
  bool possible = false, ranged = false, city = false, capture = false;
  int attacker_strength = 0, defender_strength = 0;
  int attacker_hp = 0, defender_hp = 0, attacker_after = 0, defender_after = 0;
};
extern const TechInfo tech_info[TECHS];
extern const uint16_t costs[JOBS + 1];
uint32_t random(State &);
void start(State &, uint32_t seed, int map_size = 1);
bool inside(const State &, int tile);
int neighbor(int tile, int direction);
int distance(int a, int b);
int unit_at(const State &, int tile);
int city_at(const State &, int tile);
bool land(const State &, int tile);
void territory(const State &, int8_t out[N]);
bool can_enter(const State &, int owner, int tile);
bool war(const State &, int a, int b);
bool diplomacy(State &, int a, int b, bool declare_war);
bool alive(const State &, int owner);
int add_unit(State &, int owner, int kind, int tile);
bool found(State &, int unit);
void vision(State &);
// Optional predecessors reconstruct the same legal, cheapest route used by
// move().
void reachable(const State &, int unit, int16_t out[N],
               int16_t *previous = nullptr);
bool move(State &, int unit, int tile);
bool zone_of_control(const State &, int owner, int from, int to);
int healing_rate(const State &, int unit, int tile = -1);
int fortification_bonus(const Unit &);
bool can_pillage(const State &, int unit);
bool pillage(State &, int unit);
bool attack(State &, int unit, int tile);
bool can_attack(const State &, int unit, int tile);
BattlePreview battle_preview(const State &, int unit, int tile);
bool improve(State &, int unit);
bool cancel_work(State &, int unit);
int improvement_turns(const State &, int tile);
bool fortify(State &, int unit);
bool wake(State &, int unit);
bool military(const Unit &);
int promotion_count(const Unit &);
int earned_promotions(const Unit &);
int next_promotion_xp(const Unit &);
bool can_promote(const State &, int unit, int promotion);
bool promote(State &, int unit, int promotion);
int upgrade_kind(const State &, int unit);
int upgrade_cost(const State &, int unit);
bool can_upgrade(const State &, int unit);
bool upgrade(State &, int unit);
bool wait(State &, int unit);
bool heal(State &, int unit);
bool cancel_order(State &, int unit);
int job_prerequisite(int job);
bool available(const State &, int owner, int job);
bool can_produce(const State &, int city, int job);
bool produce(State &, int city, int job);
int construction_progress(const State &, int city, int job);
bool enqueue(State &, int city, int job);
bool remove_queue(State &, int city, int position);
bool reorder_queue(State &, int city, int position, int direction);
bool repeat_production(State &, int city);
int food_consumption(const State &, int city);
int growth_threshold(int population);
int purchase_cost(const State &, int city);
bool purchase(State &, int city);
bool research(State &, int owner, int tech);
bool policy(State &, int owner, int index);
int policy_cost(const State &, int owner);
Yield yields(const State &, int city);
int production_yield(const State &, int city, int job = -1);
int growth_surplus(const State &, int city);
Yield tile_yield(const State &, int tile);
int working_city(const State &, int tile);
int worked_tiles(const State &, int city, int16_t out[18]);
bool city_focus(State &, int city, int focus);
int city_tiles(const State &, int city, int16_t out[18]);
bool tile_locked(const State &, int city, int tile);
bool lock_tile(State &, int city, int tile, bool locked);
bool auto_assign(State &, int city);
int score(const State &, int owner);
void end_turn(State &);
void ai(State &, int owner);
void evaluate(State &);
size_t serialize(const State &, uint8_t *, size_t);
bool restore(State &, const uint8_t *, size_t);
bool valid(const State &);
} // namespace unciv
