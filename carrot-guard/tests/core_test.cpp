// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
using namespace carrot;
int nearestFree(const Game &g, Point p) {
  int slot = -1;
  float best = 1e9f;
  for (int s = 0; s < Slots; ++s) {
    float d = distance(p, g.slotPoint(s));
    if (g.land(s) && !g.towers[s].obstacle && !g.towers[s].type && d < best) {
      slot = s;
      best = d;
    }
  }
  return slot;
}
int main() {
  Game g;
  g.reset(0);
  int first = nearestFree(g, {60, 126});
  assert(!g.build(-1, 1) && !g.build(first, 4));
  int money = g.coins;
  assert(g.build(first, 1) && g.coins == money - 90);
  assert(!g.build(first, 2));
  assert(g.upgrade(first) && g.towers[first].level == 2);
  int invested = g.towers[first].invested, before = g.coins;
  assert(g.sell(first));
  assert(g.coins == before + invested * 3 / 4 && !g.sell(first));
  // Every unoccupied land cell is buildable, not just the old twelve anchors.
  for (int l = 0; l < Levels; ++l) {
    g.reset(l);
    g.coins = 100000;
    int built = 0, roads = 0;
    for (int s = 0; s < Slots; ++s) {
      assert(g.cell(g.slotPoint(s)) == (g.visible(s) ? s : -1));
      bool legal = g.land(s) && !g.towers[s].obstacle;
      assert(g.build(s, 1) == legal);
      built += legal;
      roads += !g.land(s);
    }
    assert(built > 20 && roads > 0);
    assert(g.cell({-1, 20}) == -1 && g.cell({480, 20}) == -1);
  }
  g.reset(0);
  g.spawn(0);
  g.step(.1f);
  assert(g.ice());
  float p = g.enemies[0].progress;
  g.step(.5f);
  assert(g.enemies[0].progress == p && !g.ice());
  g.reset(0);
  assert(g.nextWave() && !g.nextWave());
  for (int i = 0; i < 30000 && g.result == Result::None; ++i)
    g.step(.02f);
  assert(g.result == Result::Loss && g.hearts == 0);
  // Every horizontal segment controls the facing, for all species and maps.
  for(int level=0;level<Levels;++level)for(int kind=0;kind<EnemyTypes;++kind){
    g.reset(level);float progress=0;const auto& map=g.map();
    for(int segment=1;segment<map.count;++segment){
      Point a=map.path[segment-1],b=map.path[segment];float length=distance(a,b);
      g.enemies[0].alive=false;g.spawn(kind,progress+length/2);
      auto& e=g.enemies[0];
      if(a.x!=b.x){g.step(.02f);assert(e.facesRight==(b.x>a.x));}
      else {e.facesRight=false;g.step(.02f);assert(!e.facesRight);}
      progress+=length;
    }
  }
  // Full 18-stage legal-economy strategies live in campaign_test.cpp.
  // A generation-tagged projectile cannot damage a reused enemy slot.
  g.reset(0);
  first = nearestFree(g, {60, 126});
  assert(g.build(first, 1));
  g.spawn(0);
  g.enemies[0].progress = 100;
  float facing = g.towers[first].facing;
  int shot = -1;
  for (int j = 0; j < 60 && shot < 0; ++j) {
    g.step(.02f);
    for (int i = 0; i < MaxShots; ++i)
      if (g.shots[i].active && g.shots[i].target == 0) {
        shot = i;
        break;
      }
  }
  assert(shot >= 0 && g.towers[first].facing != facing);
  g.enemies[0].alive = false;
  g.spawn(0);
  int id = g.enemies[0].id;
  float hp = g.enemies[0].hp;
  g.towers[first].cooldown = 100;
  for (int j = 0; j < 80; ++j)
    g.step(.02f);
  assert(g.enemies[0].id == id && g.enemies[0].hp == hp);
  g.result = Result::Win;
  money = g.coins;
  assert(!g.build(nearestFree(g, {100, 120}), 1) && !g.upgrade(first) &&
         !g.sell(first) && !g.nextWave() && !g.ice() && g.coins == money);
  // Focused obstacles can be attacked, removed and replaced with a tower.
  g.reset(0);
  int chest = -1;
  for (int s = 0; s < Slots; ++s)
    if (g.towers[s].obstacle == 3)
      chest = s;
  assert(chest >= 0 && !g.build(chest, 1));
  int nearby = nearestFree(g, g.slotPoint(chest));
  assert(distance(g.slotPoint(nearby), g.slotPoint(chest)) < 100 &&
         g.build(nearby, 3));
  g.focus = chest;
  money = g.coins;
  g.spawn(0);
  for (int i = 0; i < 1500 && g.towers[chest].obstacle; ++i)
    g.step(.02f);
  assert(!g.towers[chest].obstacle && g.focus == -1 && g.coins >= money + 90 &&
         g.build(chest, 1));
  std::puts("All gameplay, free placement, turret tracking and obstacle checks "
            "passed.");
}
