// SPDX-License-Identifier: MIT
#pragma once
#include "items.hpp"
#include <cstddef>
namespace starsoil {
// Fixed allocation/stride; each world's actual bounds are stored in State.
constexpr int WorldW = 512, WorldH = 144, TileSize = 16,
              Cells = WorldW * WorldH;
constexpr int DefaultWorldW = 384, DefaultWorldH = 112;
enum class WorldSize : uint8_t { Small, Medium, Large };
struct Dimensions {
  uint16_t width, height;
};
constexpr Dimensions dimensions(WorldSize size) {
  return size == WorldSize::Small   ? Dimensions{256, 96}
         : size == WorldSize::Large ? Dimensions{512, 144}
                                    : Dimensions{384, 112};
}
constexpr bool valid_dimensions(int width, int height) {
  return (width == 256 && height == 96) || (width == 384 && height == 112) ||
         (width == 512 && height == 144);
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
constexpr size_t SaveBytes = 160000;
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
};
struct State {
  uint16_t width = DefaultWorldW, height = DefaultWorldH;
  uint8_t tiles[Cells]{}, walls[Cells]{};
  uint8_t heights[WorldW]{};
  Slot bag[BagSlots]{}, chest[12]{};
  uint8_t equip[EquipSlots]{};
  Actor player{}, mobs[MaxMobs]{}, npcs[4]{};
  Drop drops[MaxDrops]{};
  Shot shots[16]{};
  uint32_t seed = 0, random = 0, clock = 0, day = 1, played = 0, kills = 0,
           mined = 0, crafted = 0, quests = 0;
  uint16_t hp = 100, mana = 60, invulnerable = 0, cooldown = 0, regen = 0,
           spawn_timer = 0;
  int16_t home_x = 0, home_y = 0, target_x = 0, target_y = 0, mine_x = -1,
          mine_y = -1;
  uint16_t mining = 0;
  uint8_t selected = 0, jumps = 0, boss_defeated = 0;
  bool jump_down = false, dirty = false;
  char event[96]{};
  uint32_t event_serial = 0;
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
bool equip(State &s, int bag_slot);
bool unequip(State &s, int equipment_slot);
bool store(State &s, int bag_slot);
bool withdraw(State &s, int chest_slot);
bool nearby_storage(const State &s);
int defense(const State &s);
int station(const State &s);
bool near_station(const State &s, int station);
bool can_craft(const State &s, int recipe);
bool craft(State &s, int recipe);
bool activate(State &s, int bag_slot);
int nearby_npc(const State &s);
bool interact(State &s);
bool trade(State &s, int npc);
void respawn(State &s);
size_t encode(const State &s, uint8_t *out, size_t capacity);
bool decode(State &s, const uint8_t *data, size_t size);
bool inspect_save(const uint8_t *data, size_t size, SaveInfo &info);
} // namespace starsoil
