// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../door_art.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace starsoil;
static State s, loaded;
static void reset() {
  new_game(s, 314159);
  std::fill(std::begin(s.tiles), std::end(s.tiles), uint8_t(Air));
  std::fill(std::begin(s.walls), std::end(s.walls), uint8_t(0));
  std::fill(std::begin(s.heights), std::end(s.heights), uint8_t(60));
  for (int x = 0; x < s.width; ++x) set_tile(s, x, 60, Stone);
  s.player = {106 * 16.0f, 60 * 16.0f, 0, 0, 100, 0, 1};
  s.invulnerable = 60000;
  for (auto &m : s.mobs) m = {};
  for (auto &n : s.npcs) { n = {}; n.x = 40 * 16; n.y = 60 * 16; }
  s.spawn_timer = 0; s.cooldown = 0;
}
static void step(const Input &in = {}, int frames = 1) {
  for (int i = 0; i < frames; ++i) { s.spawn_timer = 0; tick(s, in, 40); }
}
static void barrier() {
  for (int y = 1; y < 60; ++y) set_tile(s, 115, y, Plank);
  set_tile(s, 115, 58, DoorTop); set_tile(s, 115, 59, Door);
}
static void spawning() {
  reset();
  for (int type = 1; type <= 4; ++type) {
    assert(can_spawn_enemy(s, 120, 59, type));
    s.walls[58 * WorldW + 120] = Plank;
    assert(!can_spawn_enemy(s, 120, 59, type));
    s.walls[58 * WorldW + 120] = Glass;
    assert(!can_spawn_enemy(s, 120, 59, type));
    s.walls[58 * WorldW + 120] = Stone;
    assert(can_spawn_enemy(s, 120, 59, type));
    s.walls[58 * WorldW + 120] = 0;
    assert(!can_spawn_enemy(s, 120, 0, type));
  }
  // Empty backing is safe in a bounded, completely enclosed wooden room.
  for (int x = 110; x <= 130; ++x) set_tile(s, x, 53, Plank);
  for (int y = 54; y < 60; ++y) {
    set_tile(s, 110, y, Plank); set_tile(s, 130, y, Plank);
  }
  assert(!can_spawn_enemy(s, 120, 59, 2));
  set_tile(s, 110, 57, Air);
  assert(can_spawn_enemy(s, 120, 59, 2));
  set_tile(s, 110, 57, Plank);
  for (int x = 110; x <= 130; ++x) set_tile(s, x, 53, Stone);
  for (int y = 54; y < 60; ++y) {
    set_tile(s, 110, y, Stone); set_tile(s, 130, y, Stone);
  }
  assert(can_spawn_enemy(s, 120, 59, 2)); // Natural caves stay dangerous.
  reset(); s.player.x = 100 * 16;
  for (int y = 1; y < 60; ++y)
    for (int x = 70; x < 132; ++x) s.walls[y * WorldW + x] = Plank;
  for (int i = 0; i < 100; ++i) { s.spawn_timer = spawn_rules(s).interval; tick(s, {}, 40); }
  assert(std::none_of(std::begin(s.mobs), std::end(s.mobs), [](auto m) { return m.hp > 0; }));
  std::fill(std::begin(s.walls), std::end(s.walls), uint8_t(0));
  for (int i = 0; i < 20; ++i) { s.spawn_timer = spawn_rules(s).interval; tick(s, {}, 40); }
  assert(std::any_of(std::begin(s.mobs), std::end(s.mobs), [](auto m) { return m.hp > 0; }));
  puts("PASS indoor spawn protection: backing walls, glass, enclosed buildings, natural caves and real spawn loop");
}
static void flights_and_doors() {
  for (int type : {1, 2, 4}) {
    reset(); barrier(); s.player.x = 120 * 16;
    s.mobs[0] = {110 * 16, 60 * 16, 0, 0, 55, uint8_t(type), 1};
    if (type == 2) s.mobs[0].y -= 8;
    for (int i = 0; i < 220; ++i) {
      step();
      int half = type == 2 ? 12 : type == 1 ? 7 : 5;
      assert(s.mobs[0].x + half <= 115 * 16 + .01f);
      assert(tile(s, 115, 58) == DoorTop && tile(s, 115, 59) == Door);
    }
  }
  reset();
  for (int x = 100; x < 126; ++x) set_tile(s, x, 45, Stone);
  s.player.y = 43 * 16; s.player.x = 113 * 16;
  s.mobs[0] = {110 * 16, 50 * 16, 0, 0, 24, 2, 1};
  for (int i = 0; i < 300; ++i) { step(); assert(s.mobs[0].y - 24 >= 46 * 16 - .01f); }
  reset(); barrier(); s.player.x = 120 * 16;
  s.mobs[0] = {115 * 16 - 6, 60 * 16, 0, 0, 40, 3, 1, 900};
  s.mobs[0].grounded = true;
  step(); assert(tile(s, 115, 58) == OpenDoorTop && tile(s, 115, 59) == OpenDoor);
  step({}, 90); assert(s.mobs[0].x > 116 * 16);
  // A door cannot close on either a player, NPC or an enemy.
  s.player.x = 112 * 16; s.target_x = 115; s.target_y = 58;
  s.mobs[0].x = 115 * 16 + 8; s.mobs[0].y = 60 * 16;
  assert(!interact(s) && tile(s, 115, 59) == OpenDoor);
  s.mobs[0].x = 120 * 16;
  s.npcs[0].x = 115 * 16 + 8;
  assert(!interact(s)); s.npcs[0].x = 40 * 16;
  s.player.x = 115 * 16 + 8; assert(!interact(s)); s.player.x = 112 * 16;
  s.walls[58 * WorldW + 115] = Glass; s.walls[59 * WorldW + 115] = Plank;
  assert(interact(s));
  assert(tile(s, 115, 58) == DoorTop && tile(s, 115, 59) == Door);
  assert(interact(s));
  assert(s.walls[58 * WorldW + 115] == Glass && s.walls[59 * WorldW + 115] == Plank);
  puts("PASS bats collide with walls/ceilings; ordinary enemies blocked; humanoid night enemy opens both door halves; safe closing");
}
static void building_and_saves() {
  reset(); s.bag[0] = {Doors, 2}; s.bag[1] = {WoodPick, 1}; s.selected = 0;
  Input use; use.use = true; use.target_x = 110; use.target_y = 59;
  step(use); assert(tile(s, 110, 58) == DoorTop && tile(s, 110, 59) == Door);
  assert(count(s, Doors) == 1);
  s.target_x = 110; s.target_y = 58; assert(interact(s));
  assert(tile(s, 110, 58) == OpenDoorTop && tile(s, 110, 59) == OpenDoor);
  std::vector<uint8_t> bytes(SaveBytes);
  auto n = encode(s, bytes.data(), bytes.size()); assert(decode(loaded, bytes.data(), n));
  assert(tile(loaded, 110, 58) == OpenDoorTop && tile(loaded, 110, 59) == OpenDoor);
  assert(std::memcmp(s.tiles, loaded.tiles, Cells) == 0);
  s.selected = 1; s.cooldown = 0; use.target_y = 58;
  step(use, 30); assert(tile(s, 110, 58) == Air && tile(s, 110, 59) == Air);
  assert(count(s, Doors) == 2); // Both halves give ONE door, including while open.
  s.selected = 0; s.cooldown = 0; use.target_y = 58; step(use);
  assert(tile(s, 110, 58) == DoorTop && tile(s, 110, 59) == Door);
  s.selected = 1; s.cooldown = 0; use.target_y = 59; step(use, 30);
  assert(count(s, Doors) == 2 && tile(s, 110, 58) == Air);
  s.selected = 0; s.cooldown = 0; set_tile(s, 110, 58, Brick); step(use);
  assert(count(s, Doors) == 2 && tile(s, 110, 58) == Brick);
  set_tile(s, 110, 58, Air); s.cooldown = 0;
  s.mobs[0] = {110 * 16 + 8, 60 * 16, 0, 0, 24, 1, 1}; step(use);
  assert(count(s, Doors) == 2 && tile(s, 110, 59) == Air);
  // Load old single-cell closed/open doors without overwriting solid edits.
  reset(); set_tile(s, 110, 58, Door);
  n = encode(s, bytes.data(), bytes.size()); assert(decode(loaded, bytes.data(), n));
  assert(tile(loaded, 110, 58) == DoorTop && tile(loaded, 110, 59) == Door);
  reset(); s.walls[58 * WorldW + 110] = Door;
  n = encode(s, bytes.data(), bytes.size()); assert(decode(loaded, bytes.data(), n));
  assert(tile(loaded, 110, 58) == OpenDoorTop && tile(loaded, 110, 59) == OpenDoor);
  reset(); set_tile(s, 110, 59, Door); set_tile(s, 110, 58, Brick);
  n = encode(s, bytes.data(), bytes.size()); assert(decode(loaded, bytes.data(), n));
  assert(tile(loaded, 110, 58) == Brick && tile(loaded, 110, 59) == Door);
  reset(); set_tile(s, 110, 58, Door); set_tile(s, 110, 59, Door);
  int doors_before = count(s, Doors);
  n = encode(s, bytes.data(), bytes.size()); assert(decode(loaded, bytes.data(), n));
  assert(tile(loaded, 110, 58) == DoorTop && tile(loaded, 110, 59) == Door);
  assert(count(loaded, Doors) == doors_before + 1);
  n = encode(loaded, bytes.data(), bytes.size()); assert(decode(s, bytes.data(), n));
  assert(count(s, Doors) == doors_before + 1); // No repeated upgrade refund.
  reset(); set_tile(s, 110, 59, Door);
  s.player.x = 110 * 16 + 8; s.player.y = 59 * 16;
  n = encode(s, bytes.data(), bytes.size()); assert(decode(loaded, bytes.data(), n));
  assert(tile(loaded, 110, 58) == Air && tile(loaded, 110, 59) == Door);
  assert(loaded.player.x == s.player.x && loaded.player.y == s.player.y);
  reset(); set_tile(s, 110, 58, Door); set_tile(s, 110, 57, Brick);
  set_tile(s, 110, 60, Air); // Old hanging door above a player-dug floor gap.
  n = encode(s, bytes.data(), bytes.size()); assert(decode(loaded, bytes.data(), n));
  assert(tile(loaded, 110, 58) == DoorTop && tile(loaded, 110, 59) == Door);
  assert(tile(loaded, 110, 60) == Air); // Never silently refill edited ground.
  puts("PASS two-cell placement, upper/lower mining without duplication, open-state saves and safe legacy migration");
}
static void projectiles_and_art() {
  reset(); barrier();
  s.shots[0] = {114 * 16.0f, 59 * 16.0f, 220, 0, 1000, 5, 1};
  step({}, 10); assert(!s.shots[0].life);
  set_tile(s, 115, 58, OpenDoorTop); set_tile(s, 115, 59, OpenDoor);
  s.shots[0] = {114 * 16.0f, 59 * 16.0f, 220, 0, 1000, 5, 1};
  step({}, 10); assert(s.shots[0].life && s.shots[0].x > 116 * 16);
  uint16_t painted[16 * 32]{};
  auto plot = [&](int x, int y, uint16_t c) {
    assert(x >= 0 && x < 16 && y >= 0 && y < 32); painted[y * 16 + x] = c;
  };
  starsoil_door::draw(DoorTop, 0, 0, 255, plot);
  starsoil_door::draw(Door, 0, 16, 255, plot);
  int handles = 0;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 16; ++x) {
      assert(painted[y * 16 + x] == starsoil_door::pixel(false, x, y));
      handles += painted[y * 16 + x] == starsoil_door::rgb(240, 205, 130);
    }
  assert(handles == 2 && painted[15 * 16 + 8] == painted[16 * 16 + 8]);
  int closed_pixels=0,open_pixels=0;
  for(int y=0;y<32;++y)for(int x=0;x<16;++x) {
    closed_pixels+=starsoil_door::pixel(false,x,y)!=0;
    open_pixels+=starsoil_door::pixel(true,x,y)!=0;
    if(y!=17 && (x<6 || x>9))assert(!starsoil_door::pixel(false,x,y));
  }
  assert(open_pixels>closed_pixels*3);
  puts("PASS closed doors block projectiles; open doors pass them; continuous 16x32 art has one handle");
}
int main() { spawning(); flights_and_doors(); building_and_saves(); projectiles_and_art(); }
