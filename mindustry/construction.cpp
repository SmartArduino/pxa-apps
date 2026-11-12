// SPDX-License-Identifier: GPL-3.0-only
// Independent implementation of Mindustry's builder plans and ConstructBlock.
#include "core.hpp"
#include <algorithm>
#include <cmath>
namespace md {
float buildTime(Block type) {
  constexpr float cost[Items] = {.5f, .7f, 1.f, 1.f, 1.f, .8f, 1.5f, 1.f};
  float time = 0;
  for (int i = 0; i < Items; ++i)
    time += defs[type].cost[i] * cost[i];
  float multiplier = type == Conveyor   ? 2.f
                     : type == Router   ? 4.f
                     : type == Junction ? 6.f
                                        : 1.f;
  return std::max(1.f, time * multiplier);
}
int State::queueBuild(Block type, int x, int y, int direction) {
  // A destroyed site can leave a stale plan until the next builder update.
  // Remove it before reusing its building slot.
  for (int i = 0; i < buildCount;)
    if (!buildings[buildQueue[i]].type ||
        !buildings[buildQueue[i]].buildState) {
      for (int j = i + 1; j < buildCount; ++j)
        buildQueue[j - 1] = buildQueue[j];
      --buildCount;
    } else
      ++i;
  if (buildCount == MaxPlans || type <= Core || !unlocked(type))
    return -1;
  int id = build(type, x, y, direction, true);
  if (id < 0)
    return -1;
  auto &b = buildings[id];
  b.buildState = 1;
  b.hp = 10;
  buildQueue[buildCount++] = uint8_t(id);
  return id;
}
int State::queueLine(Block type, int x0, int y0, int x1, int y1,
                     int direction) {
  if ((type != Conveyor && type != Conduit && type != Wall) ||
      (x0 != x1 && y0 != y1) || !unlocked(type))
    return -1;
  int n = std::abs(x1 - x0) + std::abs(y1 - y0) + 1;
  if (n > MaxPlans - buildCount)
    return -1;
  int slots = 0;
  for (auto &b : buildings)
    slots += !b.type;
  if (slots < n)
    return -1;
  int dx = (x1 > x0) - (x1 < x0), dy = (y1 > y0) - (y1 < y0);
  for (int k = 0; k < n; ++k)
    if (!canBuild(type, x0 + k * dx, y0 + k * dy))
      return -1;
  if (n > 1)
    direction = dx > 0 ? 0 : dy < 0 ? 1 : dx < 0 ? 2 : 3;
  for (int k = 0; k < n; ++k)
    queueBuild(type, x0 + k * dx, y0 + k * dy, direction);
  return n;
}
bool State::queueRemove(int id) {
  if (id < 0 || id >= MaxBuildings)
    return false;
  auto &b = buildings[id];
  if (b.type <= Core || b.buildState == 3)
    return false;
  if (b.buildState == 1) {
    b = Building{};
    for (int i = 0; i < buildCount; ++i)
      if (buildQueue[i] == id) {
        for (int j = i + 1; j < buildCount; ++j)
          buildQueue[j - 1] = buildQueue[j];
        --buildCount;
        break;
      }
    if (builder == id)
      builder = -1;
    rebuild();
    return true;
  }
  if (!b.buildState) {
    if (buildCount == MaxPlans)
      return false;
    buildQueue[buildCount++] = uint8_t(id);
    b.progress = BuildComplete;
    b.hp = uint16_t(std::max(1, 10 * int(b.hp) / defs[b.type].hp));
  }
  b.timer = b.progress;
  b.buildState = 3;
  powerDirty = true;
  b.water = 0;
  b.powered = false;
  b.ammoLeft = b.ammoType = 0;
  std::fill(b.inv, b.inv + Items, uint8_t(0));
  return true;
}
void State::constructionStep() {
  builder = -1;
  float budget = 1.5f; // Alpha buildSpeed=.5, three original 60 Hz frames.
  int attempts = buildCount;
  while (buildCount && attempts-- && budget > .0001f) {
    int id = buildQueue[0];
    auto &b = buildings[id];
    auto pop = [&] {
      for (int i = 1; i < buildCount; ++i)
        buildQueue[i - 1] = buildQueue[i];
      --buildCount;
    };
    auto rotate = [&] {
      pop();
      buildQueue[buildCount++] = uint8_t(id);
    };
    if (!b.type || !b.buildState) {
      pop();
      continue;
    }
    int size = defs[b.type].size;
    float x = b.x + size * .5f, y = b.y + size * .5f;
    if (!sandbox && std::hypot(playerX - x, playerY - y) > BuildRange) {
      rotate();
      continue; // Mobile/player movement brings Alpha into range.
    }
    if (b.buildState == 1 && !sandbox) {
      bool ready = true;
      for (int j = 0; j < Items; ++j)
        if (defs[b.type].cost[j] && !stock[j])
          ready = false;
      if (!ready) {
        rotate();
        continue;
      }
    }
    if (b.buildState == 1) {
      b.buildState = 2;
      unitNavDirty = true;
    }
    unsigned old = b.progress;
    float time = buildTime(Block(b.type));
    unsigned delta =
        std::max(1u, unsigned(budget * BuildComplete / time + .001f));
    unsigned next = b.buildState == 3
                        ? (old > delta ? old - delta : 0)
                        : std::min<unsigned>(BuildComplete, old + delta);
    if (sandbox)
      next = b.buildState == 3 ? 0 : BuildComplete;
    if (b.buildState == 2 && !sandbox) {
      for (int j = 0; j < Items; ++j) {
        unsigned cost = defs[b.type].cost[j];
        if (cost)
          next = std::min(next, (unsigned(b.inv[j]) + stock[j]) *
                                    BuildComplete / cost);
      }
      if (next <= old) {
        rotate();
        continue;
      }
      for (int j = 0; j < Items; ++j) {
        unsigned paid =
            (defs[b.type].cost[j] * next + BuildComplete - 1) / BuildComplete;
        stock[j] -= uint16_t(paid - b.inv[j]);
        b.inv[j] = uint8_t(paid);
      }
    }
    if (b.buildState == 3 && !sandbox) {
      for (int j = 0; j < Items; ++j) {
        unsigned cost = defs[b.type].cost[j];
        unsigned before = (b.timer - old) * cost / (2u * BuildComplete);
        unsigned after = (b.timer - next) * cost / (2u * BuildComplete);
        // Original rounds the final half-cost refund, including odd costs.
        if (!next && b.timer == BuildComplete)
          after = (cost + 1) / 2;
        stock[j] =
            uint16_t(std::min(4000u, unsigned(stock[j]) + after - before));
      }
    }
    builder = int16_t(id);
    b.progress = uint16_t(next);
    budget -= time * std::abs(int(next) - int(old)) / BuildComplete;
    if (b.buildState == 2 && next == BuildComplete) {
      auto type = b.type, xx = b.x, yy = b.y, dir = b.dir;
      auto health = b.hp;
      b = Building{};
      b.type = type;
      b.x = xx;
      b.y = yy;
      b.dir = dir;
      b.hp = uint16_t(std::max(1, int(defs[type].hp) * health / 10));
      pop();
      rebuild();
      emit(Sound::Place, b.x + .5f, b.y + .5f);
    } else if (b.buildState == 3 && !next) {
      emit(Sound::Break, b.x + .5f, b.y + .5f);
      b = Building{};
      pop();
      rebuild();
      builder = -1;
    } else
      break;
  }
}
} // namespace md
