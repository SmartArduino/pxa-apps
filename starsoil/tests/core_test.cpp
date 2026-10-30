// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../input_geometry.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static State s, t;
static void advance(Input in, int duration) {
  for (int n = 0; n < duration; n += 20)
    tick(s, in, 20);
}
static int find(int id) {
  for (int i = 0; i < BagSlots; ++i)
    if (s.bag[i].item == id)
      return i;
  return -1;
}
static void clear_room() {
  for (int y = 20; y < 33; ++y)
    for (int x = 35; x < 85; ++x)
      set_tile(s, x, y, y == 32 ? Stone : Air);
  s.player = {48 * 16.0f, 32 * 16.0f, 0, 0, 100, 0, 1};
  for (auto &m : s.mobs)
    m.hp = 0;
}
static int recipe_for(int id) {
  for (int i = 0; i < RecipeCount; ++i)
    if (recipes[i].out == id)
      return i;
  return -1;
}
static void world_generation() {
  for (unsigned seed : {1u, 54321u, 0x9a7bcu, 0xffffffffu}) {
    new_game(s, seed);
    new_game(t, seed);
    assert(std::memcmp(s.tiles, t.tiles, Cells) == 0);
    assert(std::memcmp(s.walls, t.walls, Cells) == 0);
    unsigned types[TileCount]{};
    for (auto v : s.tiles) {
      assert(v < TileCount);
      ++types[v];
    }
    for (int ore = CopperOre; ore <= RubyOre; ++ore)
      assert(types[ore] > 5);
    assert(types[Sand] > 100 && types[Snow] > 100 && types[Chest] > 0 &&
           types[Portal] > 0);
    assert(tile(s, -1, 5) == Deepstone && tile(s, 4, -1) == Air);
    assert(!set_tile(s, 0, 0, Air));
    assert(tile(s, 48, 31) == Grass);
  }
  puts("PASS deterministic biomes, caves, ore seams and boundaries");
}
static void inventory() {
  new_game(s, 44);
  int before = count(s, Wood);
  assert(!remove(s, Wood, before + 1) && count(s, Wood) == before);
  assert(add(s, Wood, 1500));
  assert(count(s, Wood) == before + 1500);
  assert(remove(s, Wood, 1500));
  assert(add(s, CopperHelmet, 1));
  int slot = find(CopperHelmet);
  assert(equip(s, slot));
  assert(s.equip[0] == CopperHelmet && count(s, StrawHat) == 1 &&
         defense(s) == 2);
  assert(add(s, IronMail, 1));
  assert(equip(s, find(IronMail)));
  assert(defense(s) == 7);
  assert(add(s, GlowCharm, 1));
  assert(equip(s, find(GlowCharm)));
  assert(s.equip[7] == GlowCharm);
  assert(unequip(s, 7) && count(s, GlowCharm) == 1);
  int wood = find(Wood), stored = s.bag[wood].count;
  assert(store(s, wood) && count(s, Wood) == 0);
  assert(withdraw(s, 0) && count(s, Wood) == stored);
  for (int i = 0; i < BagSlots; ++i)
    s.bag[i] = {Rock, 999};
  s.bag[1] = {IronHelmet, 2};
  auto old = s;
  assert(!equip(s, 1));
  assert(std::memcmp(s.bag, old.bag, sizeof(s.bag)) == 0 &&
         std::memcmp(s.equip, old.equip, sizeof(s.equip)) == 0);
  assert(!add(s, Wood, 1) && !unequip(s, 0));
  puts("PASS stacking, full bags, armor swaps and charm slots");
}
static void crafting() {
  new_game(s, 33);
  clear_room();
  int r = recipe_for(Boards), before = count(s, Wood);
  assert(craft(s, r) && count(s, Wood) == before - 1);
  assert(!craft(s, recipe_for(IronPick)));
  assert(add(s, IronBar, 20));
  assert(add(s, Coal, 10));
  assert(!near_station(s, Forge));
  assert(!can_craft(s, recipe_for(IronPick)));
  set_tile(s, 50, 31, Anvil);
  assert(near_station(s, Forge));
  assert(craft(s, recipe_for(IronPick)));
  assert(count(s, IronPick) == 1 && count(s, IronBar) == 13);
  for (int n = 0; n < RecipeCount; ++n) {
    auto &p = recipes[n];
    assert(p.out > 0 && p.out < ItemCount && p.count > 0 && p.station <= Altar);
    for (auto a : p.in)
      assert(a.item < ItemCount);
  }
  puts("PASS recipe costs, required stations and all catalogue definitions");
}
static void movement_and_mining() {
  new_game(s, 77);
  clear_room();
  advance({}, 100);
  assert(s.player.grounded);
  float start = s.player.x;
  Input move;
  move.move = 1;
  advance(move, 400);
  assert(s.player.x > start + 25);
  Input jump;
  jump.jump = true;
  advance(jump, 100);
  assert(s.player.y < 32 * 16 - 10);
  s.equip[5] = CloudBoots;
  advance({}, 60);
  float vy = s.player.vy;
  advance(jump, 20);
  assert(s.player.vy < vy && s.jumps == 2);
  clear_room();
  set_tile(s, 50, 31, Stone);
  s.selected = 0;
  Input mine;
  mine.use = true;
  mine.target_x = 50;
  mine.target_y = 31;
  advance(mine, 1100);
  assert(tile(s, 50, 31) == Air && s.mined >= 1);
  set_tile(s, 50, 31, CrystalOre);
  advance(mine, 1100);
  assert(tile(s, 50, 31) == CrystalOre);
  s.bag[0] = {IronPick, 1};
  advance(mine, 1000);
  assert(tile(s, 50, 31) == Air);
  clear_room();
  s.bag[0] = {WoodPick, 1};
  mine.target_x = -1;
  mine.aim_y = 1;
  advance(mine, 600);
  assert(tile(s, 48, 32) == Air);
  clear_room();
  s.bag[0] = {WoodAxe, 1};
  for (int y = 26; y < 32; ++y)
    set_tile(s, 50, y, Log);
  set_tile(s, 50, 25, Leaves);
  mine.target_x = 50;
  mine.target_y = 30;
  mine.aim_y = 0;
  advance(mine, 300);
  assert(tile(s, 50, 26) == Air && tile(s, 50, 31) == Air);
  clear_room();
  s.bag[0] = {Boards, 10};
  s.selected = 0;
  s.cooldown = 0;
  int boards_before = count(s, Boards);
  Input build;
  build.use = true;
  build.target_x = 47;
  build.target_y = 31;
  advance(build, 40);
  assert(tile(s, 47, 31) == Air && count(s, Boards) == boards_before);
  build.down = true;
  advance(build, 40);
  assert(s.walls[31 * WorldW + 47] == Plank &&
         count(s, Boards) == boards_before - 1);
  s.cooldown = 0;
  build.down = false;
  build.target_y = 29;
  advance(build, 40);
  assert(tile(s, 47, 29) == Plank);
  s.player.y = 26;
  s.player.vy = -215;
  advance({}, 40);
  assert(s.player.y >= 24);
  puts("PASS walking, jumps, double jump, mining tiers, downward aim and tree "
       "felling");
}
static void combat_and_trade() {
  new_game(s, 81);
  clear_room();
  s.bag[2] = {IronSword, 1};
  s.selected = 2;
  s.mobs[0] = {s.player.x + 25, s.player.y, 0, 0, 12, 1, 0};
  Input use;
  use.use = true;
  advance(use, 20);
  assert(s.mobs[0].hp == 0 && s.kills == 1);
  s.cooldown = 0;
  s.bag[2] = {CrystalSword, 1};
  s.mobs[0] = {s.player.x + 30, s.player.y, 0, 0, 20, 5, 0};
  advance(use, 20);
  assert(s.boss_defeated && count(s, EmberCape) == 1 &&
         count(s, StarCore) == 1);
  assert(trade(s, 0) && s.quests == 1);
  assert(!trade(s, 0));
  s.mined = 20;
  assert(trade(s, 0) && (s.quests & 2));
  s.crafted = 10;
  assert(trade(s, 0) && (s.quests & 4));
  add(s, GoldBar, 2);
  assert(trade(s, 1) && count(s, Silk) >= 8);
  s.hp = 25;
  add(s, HealPotion, 1);
  assert(activate(s, find(HealPotion)) && s.hp == 70);
  s.player.x = 100;
  respawn(s);
  assert(s.player.x == s.home_x * 16.0f && s.hp == 100);
  clear_room();
  s.bag[0] = {Glass, 3};
  s.selected = 0;
  s.cooldown = 0;
  set_tile(s, 50, 31, Water);
  Input fill;
  fill.use = true;
  fill.target_x = 50;
  fill.target_y = 31;
  advance(fill, 20);
  assert(count(s, WaterBucket) == 1 && count(s, Glass) == 2 &&
         tile(s, 50, 31) == Air);
  puts("PASS sword combat, loot, NPC quest stages, trading, healing and "
       "respawn");
}
static void stable_ground_contact() {
  new_game(s, 77);
  clear_room();
  // The production loop can end with a 1 ms remainder after its 20 ms steps.
  // A prior unsnapped landing alternated grounded/jump at these short steps.
  s.player.y -= .2f;
  advance({}, 100);
  for (int i = 0; i < 500; ++i) {
    tick(s, {}, uint32_t(i % 20 + 1));
    assert(s.player.grounded && s.player.vy == 0 && s.player.y == 32 * 16);
  }
  for (int x = 35; x < 85; ++x)
    set_tile(s, x, 30, Platform);
  s.player.y = 30 * 16 - .2f;
  advance({}, 100);
  for (int i = 0; i < 200; ++i) {
    tick(s, {}, uint32_t(i % 20 + 1));
    assert(s.player.grounded && s.player.y == 30 * 16);
  }
  Input drop;
  drop.down = true;
  tick(s, drop, 20);
  assert(!s.player.grounded && s.player.y > 30 * 16);
  advance(drop, 600);
  assert(s.player.grounded && s.player.y == 32 * 16);
  Input jump;
  jump.jump = true;
  tick(s, jump, 1);
  assert(!s.player.grounded && s.player.vy < 0);
  puts("PASS stable ground/platform contact at 1..20 ms; drop-through and jump remain airborne");
}
static void persistence() {
  new_game(s, 98765);
  set_tile(s, 100, 60, Brick);
  set_tile(s, 120, 60, Air);
  s.walls[60 * WorldW + 100] = Plank;
  add(s, CrystalMail, 1);
  equip(s, find(CrystalMail));
  s.drops[0] = {100, 200, 0, Ruby, 5};
  std::vector<uint8_t> data(SaveBytes);
  size_t n = encode(s, data.data(), data.size());
  assert(n > 2048);
  assert(decode(t, data.data(), n));
  assert(std::memcmp(s.tiles, t.tiles, Cells) == 0 &&
         std::memcmp(s.walls, t.walls, Cells) == 0);
  for (int i = 0; i < BagSlots; ++i)
    assert(s.bag[i].item == t.bag[i].item && s.bag[i].count == t.bag[i].count);
  assert(s.equip[1] == t.equip[1] && t.drops[0].count == 5 && t.seed == s.seed);
  auto original = t;
  data[200] ^= 1;
  assert(!decode(t, data.data(), n) && t.seed == original.seed);
  data[200] ^= 1;
  assert(!decode(t, data.data(), n - 1));
  assert(!decode(t, data.data(), 0));
  assert(!encode(s, data.data(), 2048));
  // Random corrupt files and truncations must never replace live state.
  for (size_t p = 0; p < n; p += 317) {
    data[p] ^= 0x80;
    assert(!decode(t, data.data(), n));
    data[p] ^= 0x80;
  }
  s.player.x = -10;
  s.player.y = -20;
  n = encode(s, data.data(), data.size());
  assert(decode(t, data.data(), n) && t.player.x == 20 && t.player.y == 24);
  printf("PASS %zu-byte whole-world save roundtrip, equipment, loot, "
         "corruption and truncation\n",
         n);
}
static void variable_worlds() {
  std::vector<uint8_t> wire(SaveBytes);
  for (auto size : {WorldSize::Small, WorldSize::Medium, WorldSize::Large}) {
    auto d = dimensions(size);
    for (uint32_t seed : {0u, 1234567u, 0xffffffffu}) {
      new_game(s, seed, size);
      new_game(t, seed, size);
      assert(s.width == d.width && s.height == d.height);
      assert(std::memcmp(s.tiles, t.tiles, Cells) == 0);
      bool biomes[6]{};
      int liquids[2]{}, portal = 0, ores = 0;
      for (int x = 0; x < s.width; ++x) biomes[biome_at(x, 0, s.width)] = true;
      biomes[biome_at(10, 68, s.width)] = true;
      for (bool found : biomes) assert(found);
      for (int y = 0; y < s.height; ++y)
        for (int x = 0; x < s.width; ++x) {
          int v = tile(s, x, y);
          liquids[0] += v == Water; liquids[1] += v == Lava;
          portal += v == Portal;
          ores += v >= CopperOre && v <= RubyOre;
        }
      assert(liquids[0] && liquids[1] && portal && ores);
      assert(tile(s, s.width, 30) == Deepstone);
      assert(!set_tile(s, s.width, 30, Air));
      set_tile(s, s.width - 3, s.height - 3, Brick);
      size_t n = encode(s, wire.data(), wire.size());
      assert(n == save_size(s.width, s.height));
      SaveInfo info;
      assert(inspect_save(wire.data(), n, info));
      assert(info.width == d.width && info.height == d.height && info.seed == seed);
      assert(decode(t, wire.data(), n));
      assert(t.width == s.width && t.height == s.height);
      assert(std::memcmp(t.tiles, s.tiles, Cells) == 0);
      assert(std::memcmp(t.walls, s.walls, Cells) == 0);
      wire[n / 2] ^= 1;
      auto old_seed = t.seed;
      assert(!decode(t, wire.data(), n) && t.seed == old_seed);
    }
  }
  puts("PASS small/medium/large worlds: actual dimensions, all biomes and liquids, numeric seed extremes, reproducible terrain, bounded edges and independent variable-length saves");
}
int main() {
  for (uint32_t density : {51200u, 65536u, 124928u})
    for (int px = -20; px < 480; ++px) {
      int logical = int(int64_t(px) * 65536 / density);
      assert(std::abs(display_coordinate(logical, density) - px) <= 2);
    }
  assert(display_coordinate(302, 51200) == 236);
  world_generation();
  inventory();
  crafting();
  movement_and_mining();
  stable_ground_contact();
  new_game(s, 1234567);
  clear_room();
  uint16_t idle_phase = s.player.anim;
  advance(Input{}, 400);
  assert(s.player.anim == idle_phase);
  Input walk;
  walk.move = 1;
  advance(walk, 400);
  assert(s.player.anim != idle_phase && s.player.face == 1);
  puts("PASS gait advances with travel distance and stops at idle");
  new_game(s, 1234567);
  clear_room();
  s.npcs[0].x = 55 * 16;
  s.npcs[0].y = 32 * 16;
  s.npcs[0].timer = 4500;
  tick(s, Input{}, 20);
  assert(s.npcs[0].vx > 0 && s.npcs[0].face == 1);
  s.npcs[0].x = 78 * 16;
  s.npcs[0].timer = 1000;
  tick(s, Input{}, 20);
  assert(s.npcs[0].vx < 0 && s.npcs[0].face == 0);
  puts("PASS NPC boundary turns match movement direction");
  combat_and_trade();
  persistence();
  variable_worlds();
  printf("%d tiles, %d items, %d recipes. All core checks passed.\n", TileCount,
         ItemCount, RecipeCount);
}
