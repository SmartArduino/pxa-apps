// SPDX-License-Identifier: MIT
#pragma once
#include "items.hpp"
#include <cstddef>
namespace starsoil {
// Fixed allocation/stride; each world's actual bounds are stored in State.
constexpr int WorldW = 1024, WorldH = 256, TileSize = 16,
              Cells = WorldW * WorldH;
constexpr int DefaultWorldW = 384, DefaultWorldH = 112;
enum class WorldSize : uint8_t { Small, Medium, Large, Huge };
struct Dimensions {
  uint16_t width, height;
};
constexpr Dimensions dimensions(WorldSize size) {
  return size == WorldSize::Huge    ? Dimensions{1024, 256}
         : size == WorldSize::Small ? Dimensions{256, 96}
         : size == WorldSize::Large ? Dimensions{512, 144}
                                    : Dimensions{384, 112};
}
constexpr bool valid_dimensions(int width, int height) {
  return (width == 256 && height == 96) || (width == 384 && height == 112) ||
         (width == 512 && height == 144) || (width == 1024 && height == 256);
}
constexpr size_t save_size(int width, int height) {
  // 228-byte inventory/header + 48 x 11-byte drops + 4-byte checksum.
  return 760u + size_t(width) + size_t(width) * height * 2u;
}
struct SaveInfo {
  uint16_t width = 0, height = 0;
  uint32_t seed = 0, day = 0, played = 0;
};
constexpr int BagSlots = 40, HotSlots = 8, EquipSlots = 8, MaxMobs = 24,
              MaxDrops = 48;
constexpr size_t SaveBytes = save_size(WorldW, WorldH);
enum Biome { Meadow, Forest, Desert, Tundra, Jungle, CrystalDepths };
struct Slot {
  uint8_t item = 0;
  uint16_t count = 0;
};
struct Actor {
  float x = 0, y = 0, vx = 0, vy = 0;
  int16_t hp = 0;
  uint8_t type = 0, face = 1;
  uint16_t timer = 0, anim = 0;
  bool grounded = false;
};
struct Drop {
  float x = 0, y = 0, vy = 0;
  uint8_t item = 0;
  uint16_t count = 0;
};
struct Shot {
  float x = 0, y = 0, vx = 0, vy = 0;
  uint16_t life = 0;
  uint8_t damage = 0, magic = 0;
};
struct Input {
  int8_t move = 0, aim_x = 0, aim_y = 0;
  bool jump = false, use = false, interact = false, down = false;
  int16_t target_x = -1, target_y = -1;
  bool smart_cursor = false;
};
struct StateData {
  uint16_t width = DefaultWorldW, height = DefaultWorldH;
  Slot bag[BagSlots]{}, chest[12]{};
  uint8_t equip[EquipSlots]{};
  Actor player{}, mobs[MaxMobs]{}, npcs[4]{};
  Drop drops[MaxDrops]{};
  Shot shots[16]{};
  uint32_t seed = 0, random = 0, clock = 0, day = 1, played = 0, kills = 0,
           mined = 0, crafted = 0, quests = 0;
  uint16_t hp = 100, mana = 60, invulnerable = 0, cooldown = 0, regen = 0,
           spawn_timer = 0;
  // Transient swing recovery; mining completion must not reset combat timing.
  uint16_t tool_cooldown = 0;
  // Input forgiveness is transient and never changes the save format.
  uint16_t jump_buffer = 0, ground_grace = 0;
  int16_t home_x = 0, home_y = 0, target_x = 0, target_y = 0, mine_x = -1,
          mine_y = -1;
  uint16_t mining = 0;
  uint8_t selected = 0, jumps = 0, boss_defeated = 0;
  bool jump_down = false, dirty = false;
  // Selection feedback is transient; the world save layout is unchanged.
  bool smart_cursor = false, target_ready = true;
  int16_t last_place_x = -1, last_place_y = -1;
  uint8_t last_place_item = 0;
  char event[96]{};
  uint32_t event_serial = 0;
};
struct State : StateData {
  uint8_t tiles[Cells]{}, walls[Cells]{};
  uint8_t heights[WorldW]{};
};
uint32_t hash(uint32_t n);
uint32_t rng(State &s);
uint8_t tile(const State &s, int x, int y);
bool set_tile(State &s, int x, int y, uint8_t value);
Biome biome_at(int x, int y = 0, int width = DefaultWorldW);
const char *biome_name(Biome b, bool zh);
void generate(State &s, uint32_t seed);
void new_game(State &s, uint32_t seed, WorldSize size = WorldSize::Medium);
void tick(State &s, const Input &input, uint32_t ms);
int count(const State &s, int item);
bool add(State &s, int item, int amount);
bool remove(State &s, int item, int amount);
// Bag slots 0..39, shared chest slots 40..51. Merge matching stacks or swap.
bool move_stack(State &s, int from, int to);
bool equip(State &s, int bag_slot);
bool unequip(State &s, int equipment_slot);
bool store(State &s, int bag_slot);
bool withdraw(State &s, int chest_slot);
bool nearby_storage(const State &s);
// Preserve hotbar and the held slot while consolidating the remaining bag.
bool sort_inventory(State &s, bool chest = false);
// Deposit only kinds already in storage; keep hotbar and the held stack.
int quick_stack(State &s);
int take_all(State &s);
int defense(const State &s);
int station(const State &s);
bool near_station(const State &s, int station);
bool can_craft(const State &s, int recipe);
enum class CraftStatus { Ready, Invalid, Station, Ingredients, Space };
CraftStatus craft_status(const State &s, int recipe);
bool craft(State &s, int recipe);
// Complete up to the requested number of recipes, stopping safely at missing
// materials, a missing station or a full bag. Returns completed recipes.
int craft_batch(State &s, int recipe, int limit);
bool activate(State &s, int bag_slot);
int nearby_npc(const State &s);
bool interact(State &s);
// Exact target interaction: never falls back to another nearby piece of furniture.
bool interaction_range(const State &s, int x, int y);
bool interact_at(State &s, int x, int y);
struct SpawnRules { uint16_t interval; uint8_t limit; };
SpawnRules spawn_rules(const State &s);
// Enemy feet are at (x * 16 + 8, y * 16). Checks the entire body and shelter.
bool can_spawn_enemy(const State &s, int x, int y, int type);
bool trade(State &s, int npc);
void respawn(State &s);
size_t encode(const State &s, uint8_t *out, size_t capacity, bool seal = true);
bool decode(State &s, const uint8_t *data, size_t size);
bool inspect_save(const uint8_t *data, size_t size, SaveInfo &info);
} // namespace starsoil
