// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
using namespace carrot;

// A legal, deterministic player: use earned coins, cover the actual route,
// diversify towers, clear treasure and upgrade. No health or economy overrides.
struct Player {
  Game game;
  std::vector<Point> route;
  int purchases = 0;
  explicit Player(int level) {
    game.unlock(level);
    game.reset(level);
    for (float p = 0; p < game.pathLength(); p += 18)
      route.push_back(game.position(p));
  }
  void decide() {
    int count = 0, ice = 0;
    for (auto &t : game.towers) {
      count += t.type != 0;
      ice += t.type == 2;
    }
    if (count >= 6 && purchases % 3 != 0) {
      for (int s = 0; s < Slots; ++s)
        if (game.towers[s].type && game.towers[s].level < 3 &&
            game.upgradeCost(s) <= game.coins && game.upgrade(s)) {
          ++purchases;
          return;
        }
    }
    int kind = count && !ice ? 2 : 1;
    if (count >= 2) {
      int order[] = {3,1,4,1,5,1,6,2};
      int preferred = order[purchases % 8];
      if (preferred <= game.availableTowers && costs[preferred] <= game.coins)
        kind = preferred;
    }
    if (costs[kind] > game.coins)
      return;
    float best = -1;
    int selected = -1;
    float radius = towerDefs[kind].range + 8;
    for (int slot = 0; slot < Slots; ++slot) {
      if (!game.land(slot) || game.towers[slot].type || game.towers[slot].obstacle)
        continue;
      Point p = game.slotPoint(slot);
      float score = 0;
      for (unsigned i = 0; i < route.size(); ++i)
        if (distance(p, route[i]) < radius) {
          float coverage = 1;
          for (int s = 0; s < Slots; ++s)
            if (game.towers[s].type && distance(game.slotPoint(s),route[i]) < game.range(s))
              coverage += game.towers[s].level * .45f;
          score += (i < route.size()/3 ? 1.3f : 1.f) / coverage;
        }
      if (game.definition().mission == Mission::Treasure)
        for (int s = 0; s < Slots; ++s)
          if (game.towers[s].obstacle == 3 && distance(p,game.slotPoint(s)) < radius)
            score += 20;
      if (score > best) {
        best = score;
        selected = slot;
      }
    }
    if (selected >= 0 && game.build(selected,kind))
      ++purchases;
  }
  void run() {
    for (int tick = 0; tick < 30000 && game.result == Result::None; ++tick) {
      if (tick % 25 == 0)
        decide();
      if (tick > 1000 && !game.active)
        game.nextWave();
      if (game.countEnemies() >= 7 && game.skillCooldown <= 0)
        game.ice();
      game.step(.04f);
      assert(game.coins >= 0 && game.countEnemies() <= MaxEnemies);
    }
  }
};

int main() {
  Game g;
  for (int completed = 0; completed < Levels; ++completed) {
    g.unlock(completed);
    g.reset(0);
    assert(g.availableTowers == (completed < 3 ? 3 : completed < 6 ? 4 : completed < 9 ? 5 : 6));
  }
  // Armor resists ordinary damage; piercing and electricity bypass it.
  g.reset(3); g.spawn(5); float hp = g.enemies[0].hp;
  g.hurt(0,20,1); assert(g.enemies[0].hp > hp-20);
  hp = g.enemies[0].hp; g.hurt(0,20,4); assert(g.enemies[0].hp == hp-20);
  // A split monster creates two correctly positioned, fresh-generation slimes.
  g.reset(6); g.spawn(6,120); g.hurt(0,10000,1);
  assert(g.countEnemies() == 2 && g.enemies[0].type == 0 && g.enemies[0].progress == 120);
  // Boss shield and summon abilities are bounded by the shared enemy capacity.
  g.reset(8);g.spawn(11);g.enemies[0].ability=0;g.step(.04f);
  assert(g.enemies[0].shield > 0);
  g.reset(11);g.spawn(12,100);g.enemies[0].ability=0;g.step(.04f);
  assert(g.countEnemies() == 3);
  // Snow boss resists slowing; lava burning still applies.
  g.reset(14);g.spawn(13);g.hurt(0,1,2);assert(g.enemies[0].slow == 0);
  g.hurt(0,1,6);assert(g.enemies[0].burn > 0);
  // Mission time starts at the first wave, not during preparation.
  g.reset(16);g.step(300);assert(g.result == Result::None && g.elapsed == 0);
  g.nextWave();g.step(221);assert(g.result == Result::Loss);
  // Treasure is an actual victory condition, with a visible remaining count.
  g.reset(1);assert(g.treasureLeft == 3);
  g.wave=g.totalWaves;g.active=true;g.remaining=0;g.step(.04f);
  assert(g.result == Result::Loss);
  int failed=0;
  for (int level=0;level<Levels;++level) {
    Player player(level);player.run();const auto &game=player.game;
    std::printf("level=%d theme=%d mission=%d result=%d hearts=%d stars=%d elapsed=%.1f treasure=%d purchases=%d\n",
      level+1,game.definition().theme,int(game.definition().mission),int(game.result),game.hearts,
      game.stars(),game.elapsed,game.treasureLeft,player.purchases);
    failed += game.result != Result::Win;
  }
  assert(!failed && "A campaign stage has no verified legal winning strategy");
  std::puts("All 18 stages, legal economy, unlocks, missions and boss mechanics passed.");
}
