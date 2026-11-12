// SPDX-License-Identifier: GPL-3.0-only
// Fixed-step counterpart of v154 GenericCrafter and UnitFactory.
#include "core.hpp"
#include <algorithm>
namespace md {
const Recipe *recipe(Block type) {
  static constexpr Recipe press{Coal, None, Graphite, 2, 0, 90 * 16};
  static constexpr Recipe smelter{Coal, Sand, Silicon, 1, 2, 40 * 16};
  static constexpr Recipe kiln{Lead, Sand, Metaglass, 1, 1, 30 * 16};
  return type == Press     ? &press
         : type == Smelter ? &smelter
         : type == Kiln    ? &kiln
                           : nullptr;
}
uint16_t processTime(Block type) {
  if (type == GroundFactory || type == AirFactory)
    return FactoryTime;
  const auto *r = recipe(type);
  return r ? r->duration : 0;
}
bool canProcess(const Building &b) {
  if (!b.type || b.buildState)
    return false;
  if (const auto *r = recipe(Block(b.type)))
    return b.inv[r->first] >= r->firstCount &&
           (r->second == None || b.inv[r->second] >= r->secondCount) &&
           b.inv[r->output] < 10;
  if (b.type == GroundFactory || b.type == AirFactory)
    return b.inv[Silicon] >= (b.type == GroundFactory ? 10 : 15) &&
           (b.type == AirFactory || b.inv[Lead] >= 10);
  return false;
}
uint16_t powerDemand(const Building &b) {
  if (b.buildState)
    return 0;
  switch (b.type) {
  case Smelter:
    return canProcess(b) ? 30 : 0;
  case Kiln:
    return canProcess(b) ? 36 : 0;
  case GroundFactory:
  case AirFactory:
    return canProcess(b) && b.progress < FactoryTime ? 72 : 0;
  case Arc:
    return 20;
  case Mender:
    return 18;
  default:
    return 0;
  }
}
void State::productionStep(int id) {
  auto &b = buildings[id];
  auto type = Block(b.type);
  if (!canProcess(b))
    return;
  auto duration = processTime(type);
  unsigned efficiency = type == Press ? FullPower : b.powerEfficiency;
  if (b.progress < duration && efficiency) {
    // Retain the fractional work in timer, including across saves. Smelting
    // takes 40 original frames, not a rounded 14 fixed simulation steps.
    unsigned work = WorkStep * efficiency + b.timer;
    unsigned next = b.progress + work / FullPower;
    b.progress = uint16_t((type == GroundFactory || type == AirFactory)
                              ? std::min<unsigned>(duration, next)
                              : next);
    b.timer = uint16_t(work % FullPower);
  }
  if (b.progress < duration)
    return;
  if (type == GroundFactory || type == AirFactory) {
    if (!spawnUnit(id))
      return;
    b.inv[Silicon] -= type == GroundFactory ? 10 : 15;
    if (type == GroundFactory)
      b.inv[Lead] -= 10;
    b.progress = b.timer = 0;
    emit(Sound::UnitCreate, b.x + 1.5f, b.y + 1.5f);
  } else if (const auto *r = recipe(type)) {
    b.inv[r->first] -= r->firstCount;
    if (r->second != None)
      b.inv[r->second] -= r->secondCount;
    ++b.inv[r->output];
    b.progress -= duration;
  }
}
} // namespace md
