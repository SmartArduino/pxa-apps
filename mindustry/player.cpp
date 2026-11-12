// SPDX-License-Identifier: GPL-3.0-only
// Fixed 60 Hz mining substeps model v154 MinerComp/ItemsComp for Alpha.
#include "core.hpp"
#include <algorithm>
#include <cmath>
namespace md {
bool State::canMine(int x, int y) const {
  if (x < 0 || y < 0 || x >= W || y >= H)
    return false;
  int id = at(x, y);
  if (id >= 0 && buildings[id].buildState != 1)
    return false;
  const auto &t = tiles[y * W + x];
  return t.ground != Rock && t.ground != Water &&
         (t.ore == Copper || t.ore == Lead || t.ore == Sand);
}
void State::stopMining() {
  mineX = mineY = -1;
  mineProgress = 0;
  mining = false;
}
void State::movePlayer(float x, float y) {
  stopMining();
  followingView = false;
  targetX = std::clamp(x, 0.f, float(W) - .01f);
  targetY = std::clamp(y, 0.f, float(H) - .01f);
}
void State::followView(float x, float y) {
  followingView = true;
  targetX = std::clamp(x, 0.f, float(W) - .01f);
  targetY = std::clamp(y, 0.f, float(H) - .01f);
}
bool State::mine(int x, int y) {
  if (!canMine(x, y))
    return false;
  if (mineX != x || mineY != y) {
    mineProgress = 0;
    mineX = int8_t(x);
    mineY = int8_t(y);
  }
  // Touch maps are freely panned: approach a distant selected ore first.
  if (std::hypot(playerX - x - .5f, playerY - y - .5f) > MineRange) {
    targetX = x + .5f;
    targetY = y + .5f;
  } else {
    targetX = playerX;
    targetY = playerY;
  }
  return true;
}
void State::miningStep() {
  mining = false;
  if (mineX < 0)
    return;
  if (!canMine(mineX, mineY)) {
    stopMining();
    return;
  }
  if (std::hypot(playerX - mineX - .5f, playerY - mineY - .5f) > MineRange)
    return;
  if (builder >= 0 && buildings[builder].buildState == 2)
    return;
  Item item = Item(tiles[mineY * W + mineX].ore);
  int coreId = core();
  bool near = false;
  if (coreId >= 0) {
    auto &b = buildings[coreId];
    float half = defs[b.type].size * .5f;
    near = std::hypot(playerX - b.x - half, playerY - b.y - half) <=
           ItemTransferRange;
  }
  mining = true;
  // Alpha mineSpeed=6.5; mineTier=1. Sand's hardness is zero, Cu/Pb is one.
  unsigned threshold = item == Sand ? 100 : 130;
  for (int frame = 0; frame < 3; ++frame) {
    mineProgress += 13;
    if (mineProgress < threshold)
      continue;
    mineProgress = 0;
    if (near && stock[item] < 4000) {
      unsigned extra = carriedItem == item ? carriedCount : 0;
      unsigned accepted = std::min(4000u - unsigned(stock[item]), extra + 1);
      stock[item] += uint16_t(accepted);
      if (extra && accepted > 1) {
        carriedCount -= uint8_t(accepted - 1);
        if (!carriedCount)
          carriedItem = None;
      }
    } else if ((!carriedCount || carriedItem == item) &&
               carriedCount < CarryCapacity) {
      carriedItem = item;
      ++carriedCount;
    } else {
      stopMining();
      break;
    }
  }
}
} // namespace md
