// SPDX-License-Identifier: GPL-3.0-only
// Cached topology and fixed-point power coverage, including battery energy.
#include "core.hpp"
#include <algorithm>
namespace md {
namespace {
bool electrical(const Building &b) {
  if (b.buildState)
    return false;
  switch (b.type) {
  case Generator:
  case Node:
  case Battery:
  case Smelter:
  case Mender:
  case Arc:
  case Kiln:
  case GroundFactory:
  case AirFactory:
    return true;
  default:
    return false;
  }
}
bool conducts(const Building &b) {
  return b.type == Node || b.type == Generator || b.type == Battery;
}
bool adjacent(const Building &a, const Building &b) {
  int as = defs[a.type].size, bs = defs[b.type].size;
  bool horizontal = a.x < b.x + bs && b.x < a.x + as;
  bool vertical = a.y < b.y + bs && b.y < a.y + as;
  return (horizontal && (a.y + as == b.y || b.y + bs == a.y)) ||
         (vertical && (a.x + as == b.x || b.x + bs == a.x));
}
bool nodeLink(const Building &a, const Building &b) {
  if (a.type != Node)
    return false;
  int s = defs[b.type].size;
  float x = a.x + .5f, y = a.y + .5f;
  float dx = x - std::clamp(x, float(b.x), float(b.x + s));
  float dy = y - std::clamp(y, float(b.y), float(b.y + s));
  return dx * dx + dy * dy <= blockRange(Node) * blockRange(Node);
}
} // namespace
bool powerConnects(const Building &a, const Building &b) {
  return electrical(a) && electrical(b) &&
         (nodeLink(a, b) || nodeLink(b, a) ||
          ((conducts(a) || conducts(b)) && adjacent(a, b)));
}
void State::powerStep() {
  if (powerDirty) {
    ++powerRebuilds;
    powerCount = 0;
    for (int i = 0; i < MaxBuildings; ++i) {
      powerGroup[i] = int16_t(i);
      if (electrical(buildings[i]))
        powerIds[powerCount++] = uint8_t(i);
    }
    auto root = [&](int i) {
      while (powerGroup[i] != i) {
        powerGroup[i] = powerGroup[powerGroup[i]];
        i = powerGroup[i];
      }
      return i;
    };
    for (int a = 0; a < powerCount; ++a)
      for (int b = a + 1; b < powerCount; ++b) {
        int i = powerIds[a], j = powerIds[b];
        auto &x = buildings[i], &y = buildings[j];
        if (powerConnects(x, y))
          powerGroup[root(j)] = int16_t(root(i));
      }
    for (int i = 0; i < powerCount; ++i)
      powerGroup[powerIds[i]] = int16_t(root(powerIds[i]));
    powerDirty = false;
  }
  uint16_t produced[MaxBuildings]{}, needed[MaxBuildings]{};
  uint32_t stored[MaxBuildings]{}, room[MaxBuildings]{};
  uint16_t coverage[MaxBuildings]{};
  powerProduced = powerUsed = 0;
  for (int n = 0; n < powerCount; ++n) {
    auto &b = buildings[powerIds[n]];
    int group = powerGroup[powerIds[n]];
    if (b.type == Generator) {
      if (b.timer)
        --b.timer;
      if (!b.timer && b.inv[Coal]) {
        --b.inv[Coal];
        b.timer = 40;
      }
      if (b.timer)
        produced[group] += 60;
    }
    needed[group] += powerDemand(b);
    if (b.type == Battery) {
      stored[group] += b.timer;
      room[group] += BatteryCapacity - b.timer;
    }
  }
  for (int r = 0; r < MaxBuildings; ++r) {
    powerProduced += produced[r];
    powerUsed += needed[r];
    // All current power ratings are even, so 0.1 energy units at 20 Hz
    // exactly represent each update, including a 12-power/s shortfall.
    unsigned supply = produced[r] / 2, demand = needed[r] / 2;
    unsigned discharge =
        demand > supply ? std::min<unsigned>(demand - supply, stored[r]) : 0;
    unsigned charge =
        supply > demand ? std::min<unsigned>(supply - demand, room[r]) : 0;
    unsigned remaining = discharge ? discharge : charge;
    unsigned total = discharge ? stored[r] : room[r];
    if (remaining)
      for (int n = 0; n < powerCount; ++n) {
        auto &b = buildings[powerIds[n]];
        if (b.type != Battery || powerGroup[powerIds[n]] != r)
          continue;
        unsigned available = discharge ? b.timer : BatteryCapacity - b.timer;
        unsigned amount = total ? uint64_t(remaining) * available / total : 0;
        total -= available;
        remaining -= amount;
        b.timer = uint16_t(discharge ? b.timer - amount : b.timer + amount);
      }
    unsigned served = supply + discharge;
    coverage[r] = uint16_t(
        demand ? std::min<unsigned>(FullPower, served * FullPower / demand)
               : (supply || stored[r] ? FullPower : 0));
  }
  for (int n = 0; n < powerCount; ++n) {
    auto &b = buildings[powerIds[n]];
    b.powerEfficiency = coverage[powerGroup[powerIds[n]]];
    b.powered = b.powerEfficiency != 0;
  }
}
} // namespace md
