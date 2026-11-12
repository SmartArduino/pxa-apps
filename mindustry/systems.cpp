// SPDX-License-Identifier: GPL-3.0-only
#include "core.hpp"
#include <algorithm>
#include <cmath>
namespace md {
namespace {
constexpr int vx[4] = {1, 0, -1, 0}, vy[4] = {0, -1, 0, 1};
bool itemOutput(Block b) {
  return b == Conveyor || b == Router || b == Junction || b == Drill ||
         b == Pneumatic || b == Press || b == Smelter || b == Kiln;
}
bool itemInput(Block b) {
  return b == Core || b == Conveyor || b == Router || b == Junction ||
         b == Duo || b == Press || b == Smelter || b == Kiln ||
         b == Generator || b == GroundFactory || b == AirFactory;
}
}
ConveyorTiling State::conveyorTiling(int x, int y, int direction,
                                    bool includePlans) const {
  ConveyorTiling result;
  direction &= 3;
  for (int relative = 0; relative < 4; ++relative) {
    int side = (direction - relative + 4) & 3;
    int id = at(x + vx[side], y + vy[side]);
    if (id < 0) continue;
    const auto &other = buildings[id];
    if (other.buildState && (!includePlans || other.buildState == 3)) continue;
    auto type = Block(other.type);
    bool facesUs = type != Conveyor ||
                   other.dir == ((side + 2) & 3);
    bool facesOther = side == direction;
    if ((itemOutput(type) || (facesOther && itemInput(type))) &&
        (facesOther || facesUs))
      result.connections |= uint8_t(1u << relative);
  }
  // Same five variants and local-Y mirror cases as v154 Autotiler.
  bool back = result.connections & 4, side1 = result.connections & 2,
       side3 = result.connections & 8;
  if (back && side1 && side3) result.shape = 3;
  else if (side1 && side3) result.shape = 4;
  else if (back && (side1 || side3)) {
    result.shape = 2; result.flip = side3;
  } else if (side1 || side3) {
    result.shape = 1; result.flip = side1;
  }
  return result;
}
int State::buildLine(Block type, int x0, int y0, int x1, int y1,
                     int direction) {
  if (type != Conveyor && type != Conduit && type != Wall)
    return -1;
  if ((x0 != x1 && y0 != y1) || !unlocked(type))
    return -1;
  int n = std::abs(x1 - x0) + std::abs(y1 - y0) + 1;
  if (n > W)
    return -1;
  int dx = (x1 > x0) - (x1 < x0), dy = (y1 > y0) - (y1 < y0), slots = 0;
  for (auto &b : buildings)
    slots += b.type == Empty;
  if (slots < n)
    return -1;
  for (int k = 0; k < n; ++k)
    if (!canBuild(type, x0 + k * dx, y0 + k * dy))
      return -1;
  if (!sandbox)
    for (int j = 0; j < Items; ++j)
      if (stock[j] < defs[type].cost[j] * n)
        return -1;
  if (n > 1)
    direction = dx > 0 ? 0 : dy < 0 ? 1 : dx < 0 ? 2 : 3;
  for (int k = 0; k < n; ++k)
    build(type, x0 + k * dx, y0 + k * dy, direction);
  return n;
}
void State::liquidStep() {
  int delta[MaxBuildings]{};
  for (auto &b : buildings)
    if (b.type == Pump && !b.buildState) {
      int amount = (ticks & 1) ? 3 : 4;
      b.water = std::min<int>(liquidCapacity(Pump), b.water + amount);
    }
  for (int i = 0; i < MaxBuildings; ++i) {
    auto &b = buildings[i];
    if (b.type != Pump && b.type != Conduit && b.type != LiquidRouter &&
        b.type != Tank)
      continue;
    if (b.buildState)
      continue;
    int budget = b.water, sourceCap = liquidCapacity(Block(b.type)),
        size = defs[b.type].size;
    for (int side = 0; side < 4 && budget; ++side) {
      int d = b.type == Conduit ? b.dir : (b.cursor + side) % 4;
      if (b.type == Conduit && side)
        break;
      for (int offset = 0; offset < size && budget; ++offset) {
        int x = b.x + (d == 0   ? size
                       : d == 2 ? -1
                                : offset),
            y = b.y + (d == 3   ? size
                       : d == 1 ? -1
                                : offset),
            target = at(x, y);
        if (target < 0 || target == i)
          continue;
        auto &to = buildings[target];
        int cap = liquidCapacity(Block(to.type));
        if (!cap || to.buildState ||
            (to.type == Conduit && ((d + 2) & 3) == to.dir))
          continue;
        int source = b.water + delta[i], destination = to.water + delta[target];
        if (source * cap <= destination * sourceCap)
          continue;
        int amount = std::min({budget, 8, cap - destination, source});
        if (amount <= 0)
          continue;
        delta[i] -= amount;
        delta[target] += amount;
        budget -= amount;
      }
    }
    if (b.type != Conduit)
      b.cursor = (b.cursor + 1) & 3;
  }
  for (int i = 0; i < MaxBuildings; ++i)
    buildings[i].water = uint16_t(int(buildings[i].water) + delta[i]);
}
int State::unitCount() const {
  int n = 0;
  for (auto &u : units)
    n += u.hp != 0;
  return n;
}
bool State::walkable(int x, int y) const {
  if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2 ||
      tiles[y * W + x].ground == Water || tiles[y * W + x].ground == Rock)
    return false;
  int id = at(x, y);
  if (id < 0)
    return true;
  if (buildings[id].buildState == 1)
    return true;
  int t = buildings[id].type;
  return t == Conveyor || t == Conduit || t == Router || t == Junction ||
         t == Node || t == LiquidRouter;
}
bool State::spawnUnit(int factory) {
  if (factory < 0 || factory >= MaxBuildings)
    return false;
  auto &b = buildings[factory];
  if (b.buildState || (b.type != GroundFactory && b.type != AirFactory))
    return false;
  int slot = -1;
  for (int i = 0; i < MaxUnits; ++i)
    if (!units[i].hp) {
      slot = i;
      break;
    }
  if (slot < 0)
    return false;
  int size = defs[b.type].size;
  bool air = b.type == AirFactory;
  for (int dd = 0; dd < 4; ++dd) {
    int d = (b.dir + dd) & 3;
    for (int off = 0; off < size; ++off) {
      int x = b.x + (d == 0   ? size
                     : d == 2 ? -1
                              : off),
          y = b.y + (d == 3   ? size
                     : d == 1 ? -1
                              : off);
      if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2 ||
          (!air && !walkable(x, y)))
        continue;
      bool occupied = false;
      for (auto &u : units)
        if (u.hp && std::hypot(u.x - x - .5f, u.y - y - .5f) < .6f)
          occupied = true;
      if (occupied)
        continue;
      units[slot] = {x + .5f, y + .5f, uint16_t(air ? 70 : 150), uint8_t(air),
                     0};
      return true;
    }
  }
  return false;
}
void State::unitStep() {
  if (!unitCount())
    return;
  float goalX = rallyX, goalY = rallyY;
  if (command != Command::Rally) {
    float best = command == Command::Attack ? 1000.f : 15.f;
    for (auto &e : enemies)
      if (e.hp) {
        float d = std::hypot(e.x - rallyX, e.y - rallyY);
        if (d < best) {
          best = d;
          goalX = e.x;
          goalY = e.y;
        }
      }
  }
  int gx = std::clamp(int(goalX), 2, W - 3),
      gy = std::clamp(int(goalY), 2, H - 3);
  bool groundUnits = false;
  for (const auto &u : units)
    groundUnits |= u.hp && !u.kind;
  int requested = gy * W + gx;
  if (groundUnits && (unitNavDirty || unitRequestedGoal != requested)) {
    ++unitNavRebuilds;
    unitRequestedGoal = int16_t(requested);
    unitNavDirty = false;
    // A shared reverse flood fill guides ground units around terrain and
    // buildings. A blocked rally point resolves to its nearest walkable cell.
    if (!walkable(gx, gy)) {
      bool found = false;
      for (int r = 1; r < W && !found; ++r)
        for (int y = std::max(2, gy - r);
             y <= std::min(H - 3, gy + r) && !found; ++y)
          for (int x = std::max(2, gx - r); x <= std::min(W - 3, gx + r); ++x)
            if (walkable(x, y)) {
              gx = x;
              gy = y;
              found = true;
              break;
            }
    }
    std::fill(unitFlow, unitFlow + W * H, uint16_t(65535));
    int queue[W * H], head = 0, tail = 0;
    if (walkable(gx, gy)) {
      unitFlow[gy * W + gx] = 0;
      queue[tail++] = gy * W + gx;
    }
    while (head < tail) {
      int p = queue[head++], x = p % W, y = p / W;
      for (int d = 0; d < 4; ++d) {
        int xx = x + vx[d], yy = y + vy[d];
        if (!walkable(xx, yy))
          continue;
        int q = yy * W + xx;
        if (unitFlow[q] != 65535)
          continue;
        unitFlow[q] = unitFlow[p] + 1;
        queue[tail++] = q;
      }
    }
    unitPathGoal = int16_t(gy * W + gx);
  }
  if (groundUnits) {
    gx = unitPathGoal % W;
    gy = unitPathGoal / W;
  }
  for (auto &u : units)
    if (u.hp) {
      if (u.cooldown)
        --u.cooldown;
      int target = -1;
      float closest = u.kind ? 17.5f : 13.75f;
      for (int i = 0; i < MaxEnemies; ++i)
        if (enemies[i].hp) {
          float d = std::hypot(enemies[i].x - u.x, enemies[i].y - u.y);
          if (d < closest) {
            closest = d;
            target = i;
          }
        }
      if (u.burstTimer)
        --u.burstTimer;
      if (u.kind && target >= 0 && !u.cooldown && !u.burst) {
        u.burst = 3;
        u.burstTimer = 0;
        u.cooldown = 27;
      }
      bool firing =
          target >= 0 && (u.kind ? (u.burst && !u.burstTimer) : !u.cooldown);
      if (firing) {
        auto &e = enemies[target];
        int damage = u.kind ? 16 : 9;
        e.hp = e.hp > damage ? e.hp - damage : 0;
        if (!e.hp)
          ++kills;
        if (u.kind) {
          --u.burst;
          u.burstTimer = 1;
        } else
          u.cooldown = 5;
        for (auto &shot : shots)
          if (!shot.life) {
            shot = {u.x, u.y, e.x, e.y, 3, false};
            break;
          }
        emit(Sound::UnitShot, u.x, u.y);
      }
      // Ground units stop to fire; rally movement remains under player control.
      if (target >= 0 && command != Command::Rally)
        continue;
      float tx = goalX, ty = goalY;
      if (!u.kind) {
        int x = std::clamp(int(u.x), 2, W - 3),
            y = std::clamp(int(u.y), 2, H - 3), best = unitFlow[y * W + x];
        tx = gx + .5f;
        ty = gy + .5f;
        if (best == 65535)
          continue;
        if (best)
          for (int d = 0; d < 4; ++d) {
            int xx = x + vx[d], yy = y + vy[d];
            if (xx < 0 || yy < 0 || xx >= W || yy >= H)
              continue;
            if (unitFlow[yy * W + xx] < best) {
              best = unitFlow[yy * W + xx];
              tx = xx + .5f;
              ty = yy + .5f;
            }
          }
      }
      float dx = tx - u.x, dy = ty - u.y, len = std::hypot(dx, dy);
      if (len > .08f) {
        float speed = std::min(len, u.kind ? .14f : .09f);
        float nx = u.x + dx / len * speed, ny = u.y + dy / len * speed;
        if (u.kind || walkable(int(nx), int(ny))) {
          u.x = std::clamp(nx, 2.05f, 45.95f);
          u.y = std::clamp(ny, 2.05f, 45.95f);
        }
      }
    }
}
} // namespace md
