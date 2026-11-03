// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace starsoil;
static State s, loaded;
static Input use;
static void reset(int item) {
  new_game(s, 314159, WorldSize::Small);
  std::fill(std::begin(s.tiles), std::end(s.tiles), uint8_t(Air));
  for (int x = 1; x < s.width - 1; ++x) set_tile(s, x, 60, Stone);
  s.player = {100 * 16, 60 * 16, 0, 0, 100, 0, 1};
  s.invulnerable = 60000;
  for (auto &m : s.mobs) m = {};
  s.selected = 0; s.bag[0] = {uint8_t(item), 1};
  use = {}; use.use = true; use.target_x = 102; use.target_y = 59;
}
static void enemy(int slot = 0, int hp = 100, float dx = 26, float dy = 0) {
  s.mobs[slot] = {s.player.x + dx, s.player.y + dy, 0, 0, int16_t(hp), 1, 1};
}
static void step(const Input &input, int ms = 20) {
  s.spawn_timer = 0; tick(s, input, ms);
}
static void tools() {
  int previous = 0;
  for (int item : {WoodPick, CopperPick, IronPick, CrystalPick, WoodAxe, IronAxe}) {
    reset(item); enemy();
    step(use);
    const int damage = 100 - s.mobs[0].hp;
    assert(damage > 0 && damage < items[item == WoodAxe ? WoodSword :
      item == IronAxe ? IronSword : WoodSword + item - WoodPick].stat);
    if (item <= CrystalPick) { assert(damage > previous); previous = damage; }
    assert(s.bag[0].item == item && s.bag[0].count == 1);
    // Empty/released input cannot damage an enemy; repeated use has recovery.
    for (int i = 0; i < 23; ++i) { enemy(); step(use); assert(s.mobs[0].hp == 100); }
    enemy(); step(use); assert(s.mobs[0].hp == 100 - damage);
    // Switching tools cannot bypass the recovery of the previous tool.
    s.bag[0].item = item == IronAxe ? WoodPick : IronAxe;
    enemy(); step(use); assert(s.mobs[0].hp == 100);
    enemy(); step({}, 40); assert(s.mobs[0].hp == 100);
    // Facing and range apply independently of where the tile cursor is aimed.
    s.tool_cooldown = 0; s.player.face = 0;
    use.target_x = s.width + 50; use.target_y = 59;
    enemy(0, 100, -26); enemy(1, 100, 26); enemy(2, 100, -65); enemy(3, 100, -26, -50);
    step(use);
    assert(s.mobs[0].hp < 100 && s.mobs[1].hp == 100 && s.mobs[2].hp == 100 && s.mobs[3].hp == 100);
    s.tool_cooldown = 0; s.player.face = 1;
    enemy(0, 1); step(use);
    assert(s.mobs[0].hp == 0 && s.kills == 1);
    assert(count(s, Gel) > 0 || std::any_of(std::begin(s.drops), std::end(s.drops),
      [](const Drop &d) { return d.item == Gel && d.count > 0; }));
  }
  puts("PASS every pickaxe/axe: tiered damage, air swings, held recovery, tool swaps, facing, reach and loot");
}
static void gathering() {
  reset(WoodPick); enemy(); set_tile(s, 102, 59, Dirt);
  step(use); assert(s.mobs[0].hp < 100 && s.mining > 0);
  for (int i = 0; i < 10; ++i) step(use);
  assert(tile(s, 102, 59) == Air && s.mined == 1);
  enemy(); step(use); assert(s.mobs[0].hp == 100); // breaking dirt did not reset melee recovery
  reset(WoodAxe); enemy();
  for (int y = 56; y < 60; ++y) set_tile(s, 102, y, Log);
  step(use); assert(s.mobs[0].hp < 100 && s.mining > 0);
  for (int i = 0; i < 5; ++i) step(use);
  assert(tile(s, 102, 56) == Air && tile(s, 102, 59) == Air && s.mined == 1);
  reset(WoodPick); enemy(); set_tile(s, 102, 59, CrystalOre);
  step(use);
  assert(s.mobs[0].hp < 100 && tile(s, 102, 59) == CrystalOre && s.event_serial > 0);
  for (int item : {Boards, HealPotion, Wood}) {
    reset(item); enemy(); step(use); assert(s.mobs[0].hp == 100);
  }
  std::vector<uint8_t> wire(SaveBytes);
  reset(IronPick); enemy(); step(use);
  const auto n = encode(s, wire.data(), wire.size());
  assert(n && decode(loaded, wire.data(), n) && loaded.tool_cooldown == 0);
  puts("PASS simultaneous mining/felling and melee, tier notices, non-tools and unchanged save format");
}
static void weapons() {
  reset(IronSword); enemy(0, 12); step(use); assert(s.kills == 1 && s.cooldown > 0);
  reset(HunterBow); add(s, Arrows, 3);
  int arrows = count(s, Arrows); step(use);
  assert(count(s, Arrows) == arrows - 1);
  assert(std::any_of(std::begin(s.shots), std::end(s.shots), [](auto b) { return b.life && !b.magic; }));
  step(use); assert(count(s, Arrows) == arrows - 1);
  reset(HunterBow); remove(s, Arrows, count(s, Arrows)); step(use);
  assert(std::none_of(std::begin(s.shots), std::end(s.shots), [](auto b) { return b.life; }));
  reset(StarWand); int mana = s.mana; step(use); assert(s.mana == mana - 6);
  assert(std::any_of(std::begin(s.shots), std::end(s.shots), [](auto b) { return b.life && b.magic; }));
  reset(StarWand); s.mana = 5; step(use); assert(s.mana == 5);
  puts("PASS sword, bow ammunition/cooldown and wand mana/projectile regressions");
}
int main() { tools(); gathering(); weapons(); }
