// SPDX-License-Identifier: GPL-3.0-only
#include "core.hpp"
#include <algorithm>
namespace md {
namespace {
struct Codec {
  uint8_t *p;
  size_t n, pos = 0;
  bool ok = true;
  uint32_t read(int bytes) {
    uint32_t v = 0;
    for (int j = 0; j < bytes; ++j) {
      if (pos >= n) {
        ok = false;
        return 0;
      }
      v |= uint32_t(p[pos++]) << (j * 8);
    }
    return v;
  }
  void write(uint32_t v, int bytes) {
    for (int j = 0; j < bytes; ++j) {
      if (pos >= n) {
        ok = false;
        return;
      }
      p[pos++] = uint8_t(v >> (j * 8));
    }
  }
};
uint32_t hash(const uint8_t *p, size_t n) {
  uint32_t v = 2166136261u;
  for (size_t i = 0; i < n; ++i)
    v = (v ^ p[i]) * 16777619u;
  return v;
}
bool inventoryValid(const Building &b) {
  int total = 0;
  for (auto n : b.inv) {
    if (n > 40)
      return false;
    total += n;
  }
  if (b.type == Conveyor || b.type == Router || b.type == Junction)
    return total <= 1;
  for (int j = 0; j < Items; ++j)
    if (b.inv[j]) {
      bool ok = false;
      switch (b.type) {
      case Drill:
        ok = j != Titanium;
        break;
      case Pneumatic:
        ok = true;
        break;
      case Duo:
        ok = j == Copper || j == Graphite || j == Silicon;
        break;
      case Press:
        ok = j == Coal || j == Graphite;
        break;
      case Smelter:
        ok = j == Coal || j == Sand || j == Silicon;
        break;
      case Kiln:
        ok = j == Lead || j == Sand || j == Metaglass;
        break;
      case Generator:
        ok = j == Coal;
        break;
      case GroundFactory:
        ok = j == Lead || j == Silicon;
        break;
      case AirFactory:
        ok = j == Silicon;
        break;
      default:
        break;
      }
      if (!ok)
        return false;
    }
  return total <= 40;
}
} // namespace
size_t State::save(uint8_t *data, size_t cap) const {
  Codec o{data, cap};
  o.write(0x38444e4d, 4);
  o.write(seed, 4);
  o.write(ticks, 4);
  o.write(wave, 2);
  o.write(countdown, 2);
  o.write(researched, 4);
  o.write(sector, 1);
  o.write(sandbox | won << 1 | lost << 2 | muted << 3 | musicMode << 4, 1);
  o.write(kills, 2);
  for (auto v : stock)
    o.write(v, 2);
  o.write(uint16_t(playerX * 256), 2);
  o.write(uint16_t(playerY * 256), 2);
  o.write(terrainVersion, 1);
  o.write(captured, 1);
  o.write(uint8_t(command), 1);
  o.write(uint16_t(rallyX * 256), 2);
  o.write(uint16_t(rallyY * 256), 2);
  int count = 0;
  for (auto &b : buildings)
    count += b.type != Empty;
  o.write(count, 1);
  int16_t index[MaxBuildings];
  int serial = 0;
  for (int i = 0; i < MaxBuildings; ++i)
    index[i] = buildings[i].type ? int16_t(serial++) : -1;
  for (auto &b : buildings)
    if (b.type) {
      o.write(b.type, 1);
      o.write(b.x, 1);
      o.write(b.y, 1);
      o.write(b.dir | b.buildState << 2, 1);
      o.write(b.hp, 2);
      unsigned mask = 0;
      for (int j = 0; j < Items; ++j)
        if (b.inv[j])
          mask |= 1u << j;
      o.write(mask, 1);
      for (int j = 0; j < Items; ++j)
        if (mask & (1u << j))
          o.write(b.inv[j], 1);
      o.write(b.timer, 2);
      o.write(b.cursor, 1);
      o.write(b.ammoType, 1);
      o.write(b.ammoLeft, 1);
      o.write(b.water, 2);
      o.write(b.progress, 2);
    }
  o.write(alive(), 1);
  for (auto &e : enemies)
    if (e.hp) {
      o.write(uint16_t(e.x * 256), 2);
      o.write(uint16_t(e.y * 256), 2);
      o.write(e.hp, 2);
      o.write(e.kind, 1);
    }
  o.write(unitCount(), 1);
  for (auto &u : units)
    if (u.hp) {
      o.write(uint16_t(u.x * 256), 2);
      o.write(uint16_t(u.y * 256), 2);
      o.write(u.hp, 2);
      o.write(u.kind, 1);
      o.write(u.cooldown, 1);
      o.write(u.burst, 1);
      o.write(u.burstTimer, 1);
    }
  int plans = 0;
  for (int i = 0; i < buildCount; ++i)
    plans +=
        buildings[buildQueue[i]].type && buildings[buildQueue[i]].buildState;
  o.write(plans, 1);
  for (int i = 0; i < buildCount; ++i) {
    int id = buildQueue[i];
    if (buildings[id].type && buildings[id].buildState)
      o.write(uint8_t(index[id]), 1);
  }
  o.write(uint8_t(carriedItem), 1);
  o.write(carriedCount, 1);
  o.write(uint8_t(mineX), 1);
  o.write(uint8_t(mineY), 1);
  o.write(mineProgress, 2);
  o.write(uint16_t(targetX * 256), 2);
  o.write(uint16_t(targetY * 256), 2);
  if (!o.ok || o.pos + 4 > cap)
    return 0;
  auto h = hash(data, o.pos);
  o.write(h, 4);
  return o.pos;
}
bool State::load(const uint8_t *data, size_t size) {
  if (size < 44 || size > 3900)
    return false;
  Codec in{const_cast<uint8_t *>(data), size};
  auto magic = in.read(4);
  bool legacy = magic == 0x32444e4d;
  bool audio = magic == 0x38444e4d;
  bool player = magic == 0x37444e4d || audio;
  bool processes = magic == 0x36444e4d || player;
  bool construction = magic == 0x35444e4d || processes;
  bool oldDrills = magic != 0x34444e4d && !construction;
  if (magic != 0x32444e4d && magic != 0x33444e4d && magic != 0x34444e4d &&
      !construction)
    return false;
  State next;
  next.seed = in.read(4);
  next.ticks = in.read(4);
  next.wave = in.read(2);
  next.countdown = in.read(2);
  next.researched = in.read(legacy ? 2 : 4);
  next.sector = in.read(1);
  unsigned flags = in.read(1);
  next.sandbox = flags & 1;
  next.won = flags & 2;
  next.muted = flags & 8;
  next.musicMode = audio ? uint8_t(flags >> 4) : 0;
  next.kills = in.read(2);
  if (next.sector > 2 || next.wave > 999 || next.countdown > 1200 ||
      flags > (audio ? 47 : legacy ? 7 : 15) || next.musicMode > 2 || (next.researched >> Blocks))
    return false;
  for (int i = 0; i < (legacy ? 6 : Items); ++i) {
    next.stock[i] = in.read(2);
    if (next.stock[i] > 4000)
      return false;
  }
  next.playerX = in.read(2) / 256.f;
  next.playerY = in.read(2) / 256.f;
  if (next.playerX >= W || next.playerY >= H)
    return false;
  next.targetX = next.playerX;
  next.targetY = next.playerY;
  if (legacy) {
    next.terrainVersion = 1;
    next.captured = next.won ? uint8_t(1u << next.sector) : 0;
  } else {
    next.terrainVersion = in.read(1);
    next.captured = in.read(1);
    next.command = Command(in.read(1));
    next.rallyX = in.read(2) / 256.f;
    next.rallyY = in.read(2) / 256.f;
    if (next.terrainVersion < 1 || next.terrainVersion > 2 ||
        next.captured > 7 || uint8_t(next.command) > 2 || next.rallyX >= W ||
        next.rallyY >= H)
      return false;
  }
  next.terrain();
  next.rebuild();
  int count = in.read(1), cores = 0;
  if (count > MaxBuildings)
    return false;
  for (int i = 0; i < count; ++i) {
    auto &b = next.buildings[i];
    b.type = in.read(1);
    b.x = in.read(1);
    b.y = in.read(1);
    b.dir = in.read(1);
    if (construction) {
      b.buildState = b.dir >> 2;
      b.dir &= 3;
    }
    b.hp = in.read(2);
    if (legacy) {
      for (int j = 0; j < 6; ++j)
        b.inv[j] = in.read(1);
    } else {
      unsigned mask = in.read(1);
      for (int j = 0; j < Items; ++j)
        if (mask & (1u << j))
          b.inv[j] = in.read(1);
    }
    b.timer = in.read(2);
    b.cursor = in.read(1);
    b.ammoType = in.read(1);
    b.ammoLeft = in.read(1);
    if (!legacy) {
      b.water = in.read(2);
      b.progress = in.read(2);
    }
    bool drill = b.type == Drill || b.type == Pneumatic;
    if (b.type <= Empty || b.type >= Blocks || (legacy && b.type > Arc) ||
        b.dir > 3 || b.cursor > 3 || !b.hp || b.hp > defs[b.type].hp ||
        b.ammoType >= Items || b.ammoLeft > 5 || b.buildState > 3 ||
        b.water > liquidCapacity(Block(b.type)))
      return false;
    if (b.buildState) {
      if (b.type == Core || b.hp > 10 || b.water || b.ammoLeft ||
          b.progress > BuildComplete || b.timer > BuildComplete)
        return false;
      if (b.buildState != 3 && (b.timer || b.progress == BuildComplete))
        return false;
      if (b.buildState == 1 && b.progress)
        return false;
      if (b.buildState == 3 &&
          (b.timer < b.progress || !b.timer || !b.progress))
        return false;
      for (int j = 0; j < Items; ++j) {
        unsigned paid =
            b.buildState == 2
                ? (defs[b.type].cost[j] * b.progress + BuildComplete - 1) /
                      BuildComplete
                : 0;
        if (b.inv[j] != paid)
          return false;
      }
    } else if (processes && processTime(Block(b.type))) {
      unsigned limit = processTime(Block(b.type));
      bool factory = b.type == GroundFactory || b.type == AirFactory;
      if (b.timer >= FullPower || b.progress > limit ||
          (!factory && b.progress == limit))
        return false;
    } else if (b.timer > (b.type == Battery
                              ? (processes ? BatteryCapacity : 4000)
                          : drill && !oldDrills ? 1600
                                                : 200))
      return false;
    else if (drill && !oldDrills) {
      auto ore = next.drillOre(Block(b.type), b.x, b.y);
      if (!ore.count || b.progress >= ore.delay * 32u)
        return false;
    } else if (processes && (b.type == Arc || b.type == Mender)) {
      if (b.progress >= FullPower)
        return false;
    } else if (b.progress > 300 ||
               (b.progress && b.type != GroundFactory && b.type != AirFactory))
      return false;
    if (!b.buildState && !inventoryValid(b))
      return false;
    int type = b.type;
    b.type = Empty;
    bool valid = next.canBuild(Block(type), b.x, b.y);
    b.type = type;
    if (!valid)
      return false;
    if (drill && oldDrills && !b.buildState) {
      auto ore = next.drillOre(Block(type), b.x, b.y);
      // Preserve the old remaining cooldown as work already completed.
      unsigned remaining = b.timer * 3u * ore.count;
      b.progress = b.timer && remaining < ore.delay
                       ? uint16_t((ore.delay - remaining) * 32u)
                       : 0;
      b.timer = 1000;
    }
    if (!processes && !b.buildState) {
      if (b.type == Battery)
        b.timer *= 10;
      else if (b.type == GroundFactory || b.type == AirFactory) {
        b.progress *= WorkStep;
        b.timer = 0;
      } else if (recipe(Block(b.type))) {
        // Older versions already delivered the output before their cooldown.
        // Start a fresh craft; never manufacture an extra migrated item.
        b.progress = b.timer = 0;
      }
    }
    cores += type == Core;
    next.rebuild();
  }
  int n = in.read(1);
  if (n > MaxEnemies)
    return false;
  for (int i = 0; i < n; ++i) {
    auto &e = next.enemies[i];
    e.x = in.read(2) / 256.f;
    e.y = in.read(2) / 256.f;
    e.hp = in.read(2);
    e.kind = in.read(1);
    if (e.x >= W || e.y >= H || !e.hp || e.hp > 500 || e.kind > 1)
      return false;
  }
  if (!legacy) {
    n = in.read(1);
    if (n > MaxUnits)
      return false;
    for (int i = 0; i < n; ++i) {
      auto &u = next.units[i];
      u.x = in.read(2) / 256.f;
      u.y = in.read(2) / 256.f;
      u.hp = in.read(2);
      u.kind = in.read(1);
      u.cooldown = in.read(1);
      u.burst = in.read(1);
      u.burstTimer = in.read(1);
      if (u.x >= W || u.y >= H || u.kind > 1 || !u.hp ||
          u.hp > (u.kind ? 70 : 150) || u.cooldown > 27 || u.burst > 3 ||
          u.burstTimer > 1 || (!u.kind && (u.burst || u.burstTimer)))
        return false;
    }
  }
  if (construction) {
    unsigned plans = in.read(1);
    if (plans > MaxPlans)
      return false;
    bool seen[MaxBuildings]{};
    for (unsigned i = 0; i < plans; ++i) {
      unsigned id = in.read(1);
      if (id >= unsigned(count) || seen[id] || !next.buildings[id].buildState)
        return false;
      seen[id] = true;
      next.buildQueue[next.buildCount++] = uint8_t(id);
    }
    for (int i = 0; i < count; ++i)
      if (bool(next.buildings[i].buildState) != seen[i])
        return false;
  }
  if (player) {
    next.carriedItem = Item(in.read(1));
    next.carriedCount = uint8_t(in.read(1));
    unsigned mx = in.read(1), my = in.read(1);
    next.mineProgress = uint16_t(in.read(2));
    next.targetX = in.read(2) / 256.f;
    next.targetY = in.read(2) / 256.f;
    if (next.carriedCount > CarryCapacity ||
        (next.carriedCount ? next.carriedItem >= Items
                           : next.carriedItem != None) ||
        next.targetX >= W || next.targetY >= H ||
        !((mx == 255 && my == 255 && !next.mineProgress) ||
          (mx < W && my < H && next.mineProgress < 130)))
      return false;
    next.mineX = mx == 255 ? -1 : int8_t(mx);
    next.mineY = my == 255 ? -1 : int8_t(my);
  }
  size_t end = in.pos;
  auto h = in.read(4);
  if (!in.ok || in.pos != size || h != hash(data, end) ||
      cores != ((flags & 4) ? 0 : 1))
    return false;
  next.lost = flags & 4;
  *this = next;
  return true;
}
} // namespace md
