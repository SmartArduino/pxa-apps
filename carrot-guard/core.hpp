// SPDX-License-Identifier: MIT
#pragma once
#include "content.hpp"
#include "map_data.hpp"
#include <cmath>
#include <cstdint>
namespace carrot {
constexpr int Slots = GridColumns * GridRows, MaxEnemies = 48, MaxShots = 80,
              MaxEffects = 48;
inline float distance(Point a, Point b) {
  float x = a.x - b.x, y = a.y - b.y;
  return std::sqrt(x * x + y * y);
}
inline float distanceSquared(Point a, Point b) {
  float x = a.x - b.x, y = a.y - b.y;
  return x * x + y * y;
}
inline bool inRange(Point a, Point b, float radius) {
  return distanceSquared(a, b) < radius * radius;
}
struct Tower {
  int type = 0, level = 0, invested = 0, obstacle = 0;
  float hp = 0, maxhp = 0, cooldown = 0, pulse = 0;
  float facing = -1.5707963f;
};
struct Enemy {
  bool alive = false;
  int type = 0, id = 0;
  float progress = 0, hp = 0, maxhp = 0, slow = 0, hit = 0;
  Point p{};
  float ability = 0, shield = 0, burn = 0;
  bool facesRight = true;
};
struct Shot {
  bool active = false;
  int type = 0, target = -1, id = 0, obstacle = -1;
  float damage = 0, age = 0, duration = 0;
  Point start{}, end{}, p{};
  int level = 1;
};
struct Effect {
  float time = 0, total = 0;
  int kind = 0, value = 0;
  Point p{};
  Point end{};
};
enum class Result { None, Win, Loss };
struct Game {
  int visibleColumns = BaseGridColumns, availableTowers = 3;
  Tower towers[Slots]{};
  Enemy enemies[MaxEnemies]{};
  Shot shots[MaxShots]{};
  Effect effects[MaxEffects]{};
  int level = 0, coins = 420, hearts = 10, wave = 0, totalWaves = 8,
      remaining = 0, spawned = 0, waveSize = 0, kills = 0, serial = 1, focus = -1,
      maxHearts = 10, treasureLeft = 0;
  float spawnClock = 0, intermission = 0, freeze = 0, skillCooldown = 0,
        elapsed = 0, banner = 0, waveProgress = 0;
  bool active = false;
  Result result = Result::None;
  const LevelDef &definition() const { return levelDefs[level]; }
  const Map &map() const { return maps[definition().map]; }
  void unlock(int completed) {
    availableTowers = 3;
    for (int i = 4; i <= TowerTypes; ++i)
      if (completed >= towerDefs[i].unlock)
        availableTowers = i;
  }
  Point slotPoint(int s) const {
    return {float(GridOriginX + CellSize / 2 + s % GridColumns * CellSize),
            float(GridOriginY + CellSize / 2 + s / GridColumns * CellSize)};
  }
  Point goal() const { return map().path[map().count - 1]; }
  bool visible(int s) const;
  int cell(Point p) const;
  int pick(Point p) const;
  void setColumns(int columns);
  bool land(int s) const;
  void reset(int l);
  Point position(float p) const;
  float pathLength() const;
  int countEnemies() const;
  bool build(int slot, int type);
  int upgradeCost(int slot) const;
  bool upgrade(int slot);
  bool sell(int slot);
  bool nextWave();
  bool ice();
  void step(float dt);
  void spawn(int type, float progress = 0);
  void hurt(int e, float damage, int type, int level = 1);
  void effect(Point p, int kind, int value = 0, Point end = {});
  int stars() const { return hearts == maxHearts ? 3 : hearts * 10 >= maxHearts * 6 ? 2 : 1; }
  float range(int s) const {
    return towerDefs[towers[s].type].range + towers[s].level * 8;
  }
};
} // namespace carrot
