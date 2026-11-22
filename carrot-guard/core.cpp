// SPDX-License-Identifier: MIT
#include "core.hpp"
#include <algorithm>
namespace carrot {
void Game::reset(int l) {
  int columns = visibleColumns;
  int unlocked = availableTowers;
  *this = Game{};
  visibleColumns = columns;
  availableTowers = unlocked;
  level = std::clamp(l, 0, Levels - 1);
  totalWaves = definition().waves;
  coins = definition().coins;
  // Scatter existing scenery onto the nearest legal land cell. These are
  // obstacles, not a whitelist of permitted tower locations.
  for (int i = 5; i < 12; ++i) {
    if ((i + level) % 3 == 0)
      continue;
    int slot = -1;
    float nearest = 1e9f;
    for (int s = 0; s < Slots; ++s) {
      float d = distance(slotPoint(s), map().anchors[i]);
      if (land(s) && !towers[s].obstacle && d < nearest) {
        nearest = d;
        slot = s;
      }
    }
    if (slot < 0)
      continue;
    auto &t = towers[slot];
    t.obstacle = i == 8 ? 3 : (i % 2 ? 1 : 2);
    t.hp = t.maxhp = t.obstacle == 3 ? 180 : t.obstacle == 2 ? 130 : 80;
  }
  for (auto &t : towers)
    treasureLeft += t.obstacle == 3;
  for (int i = 0; treasureLeft < definition().chests && i < 12; ++i) {
    int s = cell(map().anchors[i]);
    if (s < 0 || !land(s) || towers[s].obstacle == 3)
      continue;
    towers[s].obstacle = 3;
    towers[s].hp = towers[s].maxhp = 150;
    ++treasureLeft;
  }
}
int Game::cell(Point p) const {
  int x = int(std::floor((p.x - GridOriginX) / CellSize));
  int y = int(std::floor((p.y - GridOriginY) / CellSize));
  if (x < 0 || x >= GridColumns || y < 0 || y >= GridRows)
    return -1;
  int s = y * GridColumns + x;
  return visible(s) ? s : -1;
}
void Game::setColumns(int columns) {
  visibleColumns = std::clamp(columns, BaseGridColumns, GridColumns);
  if (!(visibleColumns & 1))
    --visibleColumns;
}
bool Game::visible(int s) const {
  if (s < 0 || s >= Slots)
    return false;
  int left = (GridColumns - visibleColumns) / 2;
  return s % GridColumns >= left && s % GridColumns < left + visibleColumns;
}
bool Game::land(int s) const {
  return visible(s) &&
         !(map().blocked[s / GridColumns] & (1u << (s % GridColumns)));
}
int Game::pick(Point p) const {
  int s = cell(p);
  return s >= 0 && (land(s) || towers[s].type || towers[s].obstacle) ? s : -1;
}

float Game::pathLength() const {
  float n = 0;
  const auto &m = map();
  for (int i = 1; i < m.count; ++i)
    n += std::abs(m.path[i].x - m.path[i - 1].x) +
         std::abs(m.path[i].y - m.path[i - 1].y);
  return n;
}
Point Game::position(float p) const {
  const auto &m = map();
  for (int i = 1; i < m.count; ++i) {
    Point a = m.path[i - 1], b = m.path[i];
    float d = std::abs(a.x - b.x) + std::abs(a.y - b.y);
    if (p <= d)
      return {a.x + (b.x - a.x) * p / d, a.y + (b.y - a.y) * p / d};
    p -= d;
  }
  return m.path[m.count - 1];
}
int Game::countEnemies() const {
  int n = 0;
  for (auto &e : enemies)
    n += e.alive;
  return n;
}
bool Game::build(int s, int type) {
  if (!land(s) || type < 1 || type > availableTowers || type > TowerTypes || towers[s].type ||
      towers[s].obstacle || coins < costs[type] || result != Result::None)
    return false;
  coins -= costs[type];
  towers[s].type = type;
  towers[s].level = 1;
  towers[s].invested = costs[type];
  effect(slotPoint(s), 1);
  return true;
}
int Game::upgradeCost(int s) const {
  return s >= 0 && s < Slots && towers[s].type && towers[s].level < 3
             ? upgrades[towers[s].type] + (towers[s].level - 1) * 65
             : 0;
}
bool Game::upgrade(int s) {
  int c = upgradeCost(s);
  if (!c || coins < c || result != Result::None)
    return false;
  coins -= c;
  towers[s].invested += c;
  ++towers[s].level;
  effect(slotPoint(s), 1);
  return true;
}
bool Game::sell(int s) {
  if (s < 0 || s >= Slots || !towers[s].type || result != Result::None)
    return false;
  coins += towers[s].invested * 3 / 4;
  towers[s] = Tower{};
  effect(slotPoint(s), 0);
  return true;
}
bool Game::nextWave() {
  if (active || wave >= totalWaves || result != Result::None)
    return false;
  if (intermission > 0)
    coins += int(intermission) * 2;
  intermission = 0;
  ++wave;
  remaining = 5 + wave + level / 6;
  if (definition().mission == Mission::Swarm)
    remaining += 5;
  waveSize = remaining;
  waveProgress = 0;
  spawned = 0;
  active = true;
  spawnClock = .25f;
  banner = 2;
  return true;
}
bool Game::ice() {
  if (skillCooldown > 0 || !countEnemies() || result != Result::None)
    return false;
  freeze = 3;
  skillCooldown = 32;
  for (auto &e : enemies)
    if (e.alive)
      effect(e.p, 2);
  return true;
}
void Game::effect(Point p, int kind, int value, Point end) {
  for (auto &e : effects)
    if (e.time <= 0) {
      e = {kind == 3 ? 1.1f : .5f, kind == 3 ? 1.1f : .5f, kind, value, p};
      if (kind == 6)
        e.time = e.total = .18f;
      e.end = end;
      return;
    }
}
void Game::spawn(int type, float progress) {
  if (type < 0 || type >= EnemyTypes)
    return;
  for (auto &e : enemies)
    if (!e.alive) {
      float hp = (42 + wave * 11 + level * 4) * enemyDefs[type].hp;
      if (definition().mission == Mission::Swarm && !enemyDefs[type].boss)
        hp *= .72f;
      e = {true, type, serial++, progress, hp, hp, 0, 0, position(progress)};
      float dx=position(progress+1).x-e.p.x;
      if(std::abs(dx)>.0001f)e.facesRight=dx>0;
      e.ability = 7;
      return;
    }
}
void Game::hurt(int i, float damage, int type, int level) {
  if (i < 0 || i >= MaxEnemies || !enemies[i].alive)
    return;
  auto &e = enemies[i];
  if (type != 4 && type != 5)
    damage *= 1 - enemyDefs[e.type].armor;
  if (e.shield > 0)
    damage *= .4f;
  e.hp -= damage;
  e.hit = .12f;
  if (type == 2 && e.type != 13)
    e.slow = e.type == 8 ? .7f : 1.9f + (level - 1) * .4f;
  if (type == 6)
    e.burn = 3 + level - 1;
  if (e.hp <= 0) {
    int reward = enemyDefs[e.type].reward;
    coins += reward;
    ++kills;
    effect(e.p, 3, reward);
    int type = e.type;
    float progress = e.progress;
    e.alive = false;
    if (type == 6)
      for (int child = 0; child < 2; ++child)
        spawn(0, std::max(0.f, progress - child * 12));
  }
}
void Game::step(float dt) {
  if (result != Result::None)
    return;
  if (wave)
    elapsed += dt;
  if (definition().timeLimit && elapsed >= definition().timeLimit) {
    result = Result::Loss;
    return;
  }
  banner = std::max(0.f, banner - dt);
  freeze = std::max(0.f, freeze - dt);
  skillCooldown = std::max(0.f, skillCooldown - dt);
  for (auto &e : effects)
    e.time = std::max(0.f, e.time - dt);
  if (!active && wave && wave < totalWaves) {
    intermission -= dt;
    if (intermission <= 0)
      nextWave();
  }
  if (active && remaining) {
    spawnClock -= dt;
    if (spawnClock <= 0 && countEnemies() < MaxEnemies - 1) {
      const auto &d = definition();
      int variety = std::min(d.poolCount, 1 + wave / 2 + level / 3);
      int type = d.pool[(spawned + wave * 2) % variety];
      if (d.mission == Mission::Armor && spawned % 3 == 0)
        type = level >= 3 ? 5 : 2;
      if (wave == totalWaves && spawned == 0 && d.boss >= 0)
        type = d.boss;
      spawn(type);
      --remaining;
      ++spawned;
      spawnClock = std::max(.48f, 1.15f - wave * .055f);
    }
  }
  float end = pathLength() - 18;
  for (auto &e : enemies)
    if (e.alive) {
      e.slow = std::max(0.f, e.slow - dt);
      e.hit = std::max(0.f, e.hit - dt);
      e.shield = std::max(0.f, e.shield - dt);
      if (e.burn > 0) {
        e.burn -= dt;
        hurt(int(&e - enemies), 12 * dt, 0);
        if (!e.alive)
          continue;
      }
      if (e.type == 9)
        e.hp = std::min(e.maxhp, e.hp + e.maxhp * .018f * dt);
      if (enemyDefs[e.type].boss) {
        e.ability -= dt;
        if (e.ability <= 0) {
          e.ability = 8;
          if (e.type == 11 || e.type == 14)
            e.shield = 2.5f;
          if (e.type == 10)
            for (auto &other : enemies)
              if (other.alive && !enemyDefs[other.type].boss)
                other.hp = std::min(other.maxhp, other.hp + other.maxhp * .2f);
          if (e.type == 3 || e.type == 12 || e.type == 14)
            for (int child = 0; child < 2; ++child)
              spawn(e.type == 12 ? 7 : 0, std::max(0.f, e.progress - 30 - child * 15));
          effect(e.p, 2);
        }
      }
      float speed = enemyDefs[e.type].speed;
      if (definition().mission == Mission::Rush)
        speed *= 1.25f;
      if ((e.type == 13 || e.type == 14) && e.hp < e.maxhp * .5f)
        speed *= 1.4f;
      if (freeze <= 0)
        e.progress += speed * (e.slow > 0 ? .46f : 1.f) * dt;
      Point next=position(e.progress);
      if(std::abs(next.x-e.p.x)>.0001f)e.facesRight=next.x>e.p.x;
      e.p = next;
      if (e.progress >= end) {
        e.alive = false;
        hearts -= enemyDefs[e.type].boss ? 3 : 1;
        effect(e.p, 4);
        if (hearts <= 0) {
          hearts = 0;
          result = Result::Loss;
          return;
        }
      }
    }
  for (auto &s : shots)
    if (s.active) {
      s.age += dt;
      if (s.target >= 0 && enemies[s.target].alive &&
          enemies[s.target].id == s.id)
        s.end = enemies[s.target].p;
      float t = std::min(1.f, s.age / s.duration);
      s.p = {s.start.x + (s.end.x - s.start.x) * t,
             s.start.y + (s.end.y - s.start.y) * t};
      if (t >= 1) {
        s.active = false;
        effect(s.end, s.type == 3 || s.type == 6 ? 0 : s.type == 2 || s.type == 5 ? 2 : 5);
        if (s.obstacle >= 0) {
          auto &o = towers[s.obstacle];
          if (o.obstacle) {
            o.hp -= s.damage;
            if (o.hp <= 0) {
              int r = o.obstacle == 3 ? 90 : o.obstacle == 2 ? 35 : 25;
              if (o.obstacle == 3)
                --treasureLeft;
              o = Tower{};
              coins += r;
              effect(slotPoint(s.obstacle), 3, r);
              if (focus == s.obstacle)
                focus = -1;
            }
          }
        } else if (s.type == 3 || s.type == 6 || s.type == 4) {
          float radius = (s.type == 6 ? 62.f : s.type == 4 ? 25.f : 46.f) + (s.level - 1) * 6;
          for (int i = 0; i < MaxEnemies; ++i)
            if (enemies[i].alive && inRange(enemies[i].p, s.end, radius))
              hurt(i, s.damage, s.type, s.level);
        } else if (s.type == 5 && s.target >= 0 && enemies[s.target].id == s.id) {
          Point p = s.end;
          bool visited[MaxEnemies]{};
          for (int bounce = 0; bounce < 2 + s.level; ++bounce) {
            int target = -1;
            float nearest = bounce ? 72.f * 72 : 36.f * 36;
            for (int i = 0; i < MaxEnemies; ++i)
              if (!visited[i] && enemies[i].alive && distanceSquared(p, enemies[i].p) < nearest) {
                target = i;
                nearest = distanceSquared(p, enemies[i].p);
              }
            if (target < 0)
              break;
            visited[target] = true;
            Point previous = p;
            p = enemies[target].p;
            hurt(target, s.damage * (bounce ? .7f : 1.f), 5, s.level);
            effect(previous,6,0,p);
          }
        } else if (s.target >= 0 && enemies[s.target].id == s.id)
          hurt(s.target, s.damage, s.type, s.level);
      }
    }
  for (int i = 0; i < Slots; ++i) {
    auto &t = towers[i];
    t.pulse = std::max(0.f, t.pulse - dt);
    if (!t.type)
      continue;
    t.cooldown -= dt;
    float best = -1;
    int target = -1, ob = -1;
    float r = range(i);
    Point p = slotPoint(i);
    // An explicit obstacle focus takes precedence, including during a wave.
    if (focus >= 0 && focus < Slots && towers[focus].obstacle &&
        inRange(p, slotPoint(focus), r))
      ob = focus;
    if (ob < 0) {
      for (int j = 0; j < MaxEnemies; ++j)
        if (enemies[j].alive && enemies[j].progress > best &&
            inRange(p, enemies[j].p, r)) {
          best = enemies[j].progress;
          target = j;
        }
      if (target < 0)
        for (int j = 0; j < Slots; ++j)
          if (towers[j].obstacle && inRange(p, slotPoint(j), r)) {
            ob = j;
            break;
          }
    }
    if (target < 0 && ob < 0)
      continue;
    Point aim = target >= 0 ? enemies[target].p : slotPoint(ob);
    float desired = std::atan2(aim.y - p.y, aim.x - p.x);
    float delta = desired - t.facing;
    constexpr float Pi = 3.14159265f, Tau = Pi * 2;
    if (delta > Pi)
      delta -= Tau;
    else if (delta < -Pi)
      delta += Tau;
    t.facing += std::clamp(delta, -8.f * dt, 8.f * dt);
    if (t.facing > Pi)
      t.facing -= Tau;
    else if (t.facing < -Pi)
      t.facing += Tau;
    // Turn toward the target before firing; keep tracking while reloading.
    if (t.cooldown > 0 || std::abs(delta) > .35f)
      continue;
    for (auto &s : shots)
      if (!s.active) {
        Point dest = target >= 0 ? enemies[target].p : slotPoint(ob);
        float dmg = towerDefs[t.type].damage *
                    (1 + (t.level - 1) * .8f);
        Point muzzle{p.x + std::cos(t.facing) * 18,
                     p.y + std::sin(t.facing) * 18};
        s = {true,
             t.type,
             target,
             target >= 0 ? enemies[target].id : 0,
             ob,
             dmg,
             0,
             std::max(.12f, distance(p, dest) / (t.type == 3 ? 180 : 300)),
             muzzle,
             dest,
             muzzle};
        s.level = t.level;
        t.cooldown = towerDefs[t.type].cooldown /
                     (1 + (t.level - 1) * .15f);
        t.pulse = .16f;
        break;
      }
  }
  if (active)
    waveProgress = std::max(waveProgress, std::clamp(
        float(waveSize - remaining - countEnemies()) / std::max(1, waveSize), 0.f, .96f));
  if (active && !remaining && !countEnemies()) {
    active = false;
    if (wave == totalWaves)
      result = definition().mission == Mission::Treasure && treasureLeft
                   ? Result::Loss : Result::Win;
    else {
      coins += 30 + wave * 4;
      intermission = 9;
      banner = 1.5f;
    }
  }
}
} // namespace carrot
