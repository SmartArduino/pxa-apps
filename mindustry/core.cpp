// SPDX-License-Identifier: GPL-3.0-only
// Independent bounded C++ simulation; no Java runtime or platform
// modifications.
#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace md {
const Def defs[Blocks] = {
    {"Cancel", "取消", 1, 0, 0, {}},
    {"Core: Shard", "核心：碎片", 3, 0, 1100, {}},
    {"Mechanical drill", "机械钻头", 2, 0, 100, {12}},
    {"Conveyor", "传送带", 1, 1, 45, {1}},
    {"Router", "路由器", 1, 1, 70, {3}},
    {"Junction", "交叉器", 1, 1, 30, {2}},
    {"Duo", "双管炮", 1, 2, 250, {35}},
    {"Copper wall", "铜墙", 1, 2, 320, {6}},
    {"Graphite press", "石墨压缩机", 2, 3, 160, {75, 30}},
    {"Silicon smelter", "硅冶炼厂", 2, 3, 100, {30, 25, 0, 0, 0}},
    {"Combustion gen.", "火力发电机", 1, 4, 100, {25, 15}},
    {"Power node", "电力节点", 1, 4, 60, {1, 3}},
    {"Battery", "电池", 1, 4, 60, {5, 20}},
    {"Mender", "修复器", 1, 4, 80, {25, 30}},
    {"Arc", "电弧", 1, 2, 260, {50, 50}},
    {"Pneumatic drill", "气动钻头", 2, 0, 160, {18, 0, 0, 0, 10}},
    {"Kiln", "窑炉", 2, 3, 180, {60, 30, 0, 0, 30}},
    {"Mechanical pump", "机械泵", 1, 5, 40, {15, 0, 0, 0, 0, 0, 10}},
    {"Conduit", "导管", 1, 5, 45, {0, 0, 0, 0, 0, 0, 1}},
    {"Liquid router", "液体路由器", 1, 5, 50, {0, 0, 0, 0, 4, 0, 2}},
    {"Liquid tank", "储液罐", 3, 5, 500, {0, 0, 0, 0, 0, 0, 40, 30}},
    {"Ground factory", "陆军工厂", 3, 6, 360, {50, 120, 0, 0, 0, 80}},
    {"Air factory", "空军工厂", 3, 6, 360, {60, 70}}};
uint16_t liquidCapacity(Block b) {
  switch (b) {
  case Drill:
  case Pneumatic:
    return 50;
  case Pump:
  case Conduit:
    return 200;
  case LiquidRouter:
    return 1200;
  case Tank:
    return 18000;
  default:
    return 0;
  }
}
Block prerequisite(Block b) {
  switch (b) {
  case Pneumatic:
  case Kiln:
    return Press;
  case Pump:
  case Conduit:
  case LiquidRouter:
    return Kiln;
  case Tank:
    return Pneumatic;
  case GroundFactory:
  case AirFactory:
    return Smelter;
  default:
    return Empty;
  }
}
uint16_t researchCost(Block b, Item item) {
  if (b < Press || b >= Blocks || item >= Items)
    return 0;
  if ((b == Smelter || b == Arc) && item == Graphite)
    return 10;
  return defs[b].cost[item] * 2;
}
namespace {
unsigned soundPriority(Sound kind) {
  return kind == Sound::Wave ? 3 : kind == Sound::Explosion ||
    kind == Sound::UnitCreate || kind == Sound::Unlock ? 2 :
    kind == Sound::AlphaShot || kind == Sound::DuoShot ||
    kind == Sound::ArcShot || kind == Sound::UnitShot ? 0 : 1;
}
}
void State::emit(Sound kind, float x, float y) {
  // Coalesce the same nearby sound within one update, while keeping spatially
  // distinct sources. A burst of shots cannot overwrite a wave or explosion.
  for (unsigned i = 0; i < soundCount; ++i)
    if (soundEvents[i].kind == kind &&
        std::abs(soundEvents[i].x-x) + std::abs(soundEvents[i].y-y) < 3) return;
  unsigned slot = soundCount;
  if (slot == MaxSoundEvents) {
    slot = 0;
    for (unsigned i = 1; i < MaxSoundEvents; ++i)
      if (soundPriority(soundEvents[i].kind) < soundPriority(soundEvents[slot].kind)) slot = i;
    if (soundPriority(kind) <= soundPriority(soundEvents[slot].kind)) return;
  } else ++soundCount;
  soundEvents[slot] = {kind, x, y};
}
uint32_t noise(uint32_t s, int x, int y) {
  uint32_t n = s ^ uint32_t(x) * 374761393u ^ uint32_t(y) * 668265263u;
  n = (n ^ (n >> 13)) * 1274126177u;
  return n ^ (n >> 16);
}
int State::at(int x, int y) const {
  return x >= 0 && y >= 0 && x < W && y < H ? occupancy[y * W + x] : -1;
}
int State::core() const {
  for (int i = 0; i < MaxBuildings; ++i)
    if (buildings[i].type == Core)
      return i;
  return -1;
}
void State::terrain() {
  flowDirty = unitNavDirty = true;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      auto n = noise(seed, x / 4, y / 4);
      auto &t = tiles[y * W + x];
      t = {Stone, None};
      if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2)
        t.ground = Rock;
      else if ((x - 8) * (x - 8) + (y - 17) * (y - 17) < 26)
        t.ground = Water;
      else if (n % 17 == 0 && y > 7 && y < 31 && std::abs(x - 24) > 3)
        t.ground = Rock;
      else if (n % 7 == 1)
        t.ground = Sandy;
      if (t.ground == Stone && n % 5 == 0)
        t.ore = uint8_t((n / 7) % 3);
      if (x >= 17 && x <= 21 && y >= 33 && y <= 37)
        t = {Stone, Copper};
      if (x >= 28 && x <= 31 && y >= 34 && y <= 38)
        t = {Stone, Lead};
      if (x >= 17 && x <= 20 && y >= 27 && y <= 30)
        t = {Stone, Coal};
      if (x >= 28 && x <= 32 && y >= 28 && y <= 31)
        t = {Sandy, Sand};
      if (t.ground == Sandy)
        t.ore = Sand;
      if (x >= 16 && x <= 20 && y >= 22 && y <= 24) {
        if (terrainVersion >= 2)
          t = {Stone, Titanium};
        else if (t.ground == Stone && t.ore == None)
          t.ore = Titanium;
      }
      if (terrainVersion >= 2 && x >= 14 && x <= 16 && y >= 33 && y <= 37)
        t = {Water, None};
      if (x >= 22 && x <= 26 && y >= 3 && y <= 38)
        t = {Stone, None};
    }
}
void State::rebuild() {
  powerDirty = true;
  unitNavDirty = true;
  std::fill(occupancy, occupancy + W * H, int16_t(-1));
  for (int i = 0; i < MaxBuildings; ++i) {
    auto &b = buildings[i];
    if (!b.type)
      continue;
    if (b.type == Drill || b.type == Pneumatic)
      b.ore = drillOre(Block(b.type), b.x, b.y);
    for (int y = b.y; y < b.y + defs[b.type].size; ++y)
      for (int x = b.x; x < b.x + defs[b.type].size; ++x)
        occupancy[y * W + x] = int16_t(i);
  }
}
void State::generate(uint32_t value, uint8_t map, bool free) {
  *this = State{};
  seed = value;
  sector = map;
  sandbox = free;
  stock[Copper] = 200;
  stock[Lead] = 80;
  countdown = 1200;
  terrain();
  rebuild();
  build(Core, 23, 35, 0, true);
  build(Drill, 19, 35, 0, true);
  build(Conveyor, 21, 35, 0, true);
  build(Conveyor, 22, 35, 0, true);
  if (sandbox)
    for (auto &v : stock)
      v = 3000;
}
bool State::unlocked(Block b) const {
  if (b >= Blocks)
    return false;
  return b < Press || sandbox || (researched & (1u << b));
}
bool State::research(Block b) {
  if (b < Press || b >= Blocks || unlocked(b) || !unlocked(prerequisite(b)))
    return false;
  for (int j = 0; j < Items; ++j)
    if (stock[j] < researchCost(b, Item(j)))
      return false;
  for (int j = 0; j < Items; ++j)
    stock[j] -= researchCost(b, Item(j));
  researched |= 1u << b;
  emit(Sound::Unlock, playerX, playerY);
  return true;
}
DrillOre State::drillOre(Block type, int x, int y) const {
  DrillOre result;
  if ((type != Drill && type != Pneumatic) || x < 0 || y < 0 || x + 2 > W ||
      y + 2 > H)
    return result;
  int counts[Items]{};
  for (int yy = y; yy < y + 2; ++yy)
    for (int xx = x; xx < x + 2; ++xx) {
      const auto &t = tiles[yy * W + xx];
      if (t.ground != Rock && t.ore < Items &&
          (type == Pneumatic || t.ore != Titanium))
        ++counts[t.ore];
    }
  // v154: sand is low priority; then count, then the original item ID.
  // Original IDs increase copper, lead, coal, titanium, sand. Process sand
  // first so any mineable ordinary ore takes priority even over more sand.
  for (Item item : {Sand, Copper, Lead, Coal, Titanium})
    if (counts[item] && (result.item == None || result.item == Sand ||
                         counts[item] >= result.count)) {
      result.item = item;
      result.count = uint8_t(counts[item]);
    }
  if (result.item != None) {
    int hardness = result.item == Sand       ? 0
                   : result.item == Coal     ? 2
                   : result.item == Titanium ? 3
                                             : 1;
    result.delay = uint16_t((type == Drill ? 600 : 400) + 50 * hardness);
  }
  return result;
}
bool State::canBuild(Block type, int x, int y) const {
  if (type <= Empty || type >= Blocks || lost)
    return false;
  int size = defs[type].size;
  if (x < 2 || y < 2 || x + size > W - 2 || y + size > H - 2)
    return false;
  for (int yy = y; yy < y + size; ++yy)
    for (int xx = x; xx < x + size; ++xx) {
      if (at(xx, yy) >= 0 || tiles[yy * W + xx].ground == Rock ||
          (tiles[yy * W + xx].ground == Water && type != Pump &&
           type != Conduit))
        return false;
    }
  if (type == Pump && tiles[y * W + x].ground != Water)
    return false;
  if (type == Drill || type == Pneumatic) {
    if (!drillOre(type, x, y).count)
      return false;
  }
  return true;
}
int State::build(Block type, int x, int y, int dir, bool free) {
  if (!canBuild(type, x, y) || (!free && !unlocked(type)))
    return -1;
  int slot = -1;
  for (int i = 0; i < MaxBuildings; ++i)
    if (!buildings[i].type) {
      slot = i;
      break;
    }
  if (slot < 0)
    return -1;
  if (!free && !sandbox)
    for (int i = 0; i < Items; ++i)
      if (stock[i] < defs[type].cost[i])
        return -1;
  if (!free && !sandbox)
    for (int i = 0; i < Items; ++i)
      stock[i] -= defs[type].cost[i];
  buildings[slot] = Building{};
  auto &b = buildings[slot];
  b.type = type;
  b.x = x;
  b.y = y;
  b.dir = dir & 3;
  b.hp = defs[type].hp;
  rebuild();
  return slot;
}
bool State::remove(int i) {
  if (i < 0 || i >= MaxBuildings || !buildings[i].type ||
      buildings[i].type == Core)
    return false;
  auto &b = buildings[i];
  for (int j = 0; j < Items; ++j)
    stock[j] = std::min(4000, int(stock[j]) + defs[b.type].cost[j] / 2);
  b = Building{};
  rebuild();
  return true;
}
int State::alive() const {
  int n = 0;
  for (auto &e : enemies)
    n += e.hp > 0;
  return n;
}
bool State::startWave() {
  if (lost || won || alive() || wave >= 999)
    return false;
  ++wave;
  countdown = 1200;
  int n = std::min(MaxEnemies, 2 + int(wave) * 2);
  for (int i = 0; i < n; ++i) {
    auto &e = enemies[i];
    e = Enemy{};
    e.x = 23.0f + (i % 5) * .65f;
    e.y = 3.0f + (i / 5) * .7f;
    e.kind = (wave >= 3 && i % 4 == 0) ? 1 : 0;
    e.hp = uint16_t((e.kind ? 60 : 150) + std::min(200, int(wave) * 5));
  }
  emit(Sound::Wave, playerX, playerY);
  return true;
}
bool State::accept(int i, Item item, int from) {
  if (i < 0 || i >= MaxBuildings || item >= Items)
    return false;
  auto &b = buildings[i];
  if (!b.type || b.buildState)
    return false;
  if (b.type == Core) {
    if (stock[item] >= 4000)
      return false;
    ++stock[item];
    return true;
  }
  int total = 0;
  for (auto v : b.inv)
    total += v;
  bool allowed = false;
  int cap = 10;
  bool perItem = false;
  switch (b.type) {
  case Conveyor:
    allowed = ((from + 2) & 3) != b.dir;
    cap = 1;
    break;
  case Router:
  case Junction:
    allowed = true;
    cap = 1;
    break;
  case Duo:
    allowed = item == Copper || item == Graphite || item == Silicon;
    cap = 30;
    break;
  case Press:
    allowed = item == Coal;
    perItem = true;
    break;
  case Smelter:
    allowed = item == Coal || item == Sand;
    perItem = true;
    break;
  case Kiln:
    allowed = item == Lead || item == Sand;
    perItem = true;
    break;
  case GroundFactory:
    allowed = item == Lead || item == Silicon;
    cap = 20;
    perItem = true;
    break;
  case AirFactory:
    allowed = item == Silicon;
    cap = 30;
    perItem = true;
    break;
  case Generator:
    allowed = item == Coal;
    break;
  default:
    break;
  }
  if (!allowed || (perItem ? int(b.inv[item]) : total) >= cap)
    return false;
  ++b.inv[item];
  if (b.type == Junction)
    b.cursor = from & 3;
  return true;
}
void State::supply(int id) {
  if (id < 0 || id >= MaxBuildings)
    return;
  auto &b = buildings[id];
  float half = defs[b.type].size * .5f;
  if (std::hypot(playerX - b.x - half, playerY - b.y - half) >
      ItemTransferRange)
    return;
  if (carriedCount) {
    while (carriedCount && accept(id, carriedItem, 0))
      --carriedCount;
    if (!carriedCount)
      carriedItem = None;
    return;
  }
  int source = core();
  if (source < 0 || b.type == Core)
    return;
  const auto &c = buildings[source];
  if (std::hypot(playerX - c.x - 1.5f, playerY - c.y - 1.5f) >
      ItemTransferRange)
    return;
  for (int j = 0; j < Items; ++j)
    for (int k = 0; k < 10 && stock[j]; ++k) {
      if (accept(id, Item(j), 0))
        --stock[j];
      else
        break;
    }
}
void State::step() {
  if (lost || won)
    return;
  ++ticks;
  if (!alive() && !sandbox && countdown && !--countdown)
    startWave();
  for (auto &shot : shots)
    if (shot.life)
      --shot.life;
  float dx = targetX - playerX, dy = targetY - playerY,
        dist = std::hypot(dx, dy);
  float remaining = dist - (followingView ? ViewAttractRange : .04f);
  if (remaining > 0) {
    float f = std::min(PlayerSpeed, remaining) / dist;
    playerX += dx * f;
    playerY += dy * f;
  }
  int coreId = core();
  if (coreId < 0) {
    lost = true;
    return;
  }
  constructionStep();
  miningStep();
  auto &c = buildings[coreId];
  bool received[MaxBuildings]{};
  powerStep();
  liquidStep();
  static constexpr int vx[4] = {1, 0, -1, 0}, vy[4] = {0, -1, 0, 1};
  for (int i = 0; i < MaxBuildings; ++i) {
    auto &b = buildings[i];
    if (!b.type)
      continue;
    if (b.buildState)
      continue;
    auto type = Block(b.type);
    int size = defs[type].size;
    if (type == Arc || type == Mender) {
      if (b.timer) {
        unsigned elapsed = b.progress + b.powerEfficiency;
        unsigned delta = elapsed / FullPower;
        b.progress = uint16_t(elapsed % FullPower);
        b.timer = b.timer > delta ? uint16_t(b.timer - delta) : 0;
      }
    } else if (type != Generator && type != Battery && type != Drill &&
               type != Pneumatic && !processTime(type) && b.timer)
      --b.timer;
    if (type == Drill || type == Pneumatic) {
      int total = 0;
      for (auto count : b.inv)
        total += count;
      bool working = b.ore.count && total < 10;
      int speed = 1000;
      if (working) {
        // Quantized tenths: 3 water/s mechanical, 3.5 water/s pneumatic.
        int usage =
            type == Drill ? ((ticks & 1) ? 1 : 2) : ((ticks % 4) ? 2 : 1);
        if (b.water >= usage) {
          b.water -= usage;
          speed = 1600;
        }
      }
      // Original approachDelta(0.015) with three 60 Hz frames per step.
      int target = working ? speed : 0;
      b.timer =
          uint16_t(b.timer < target ? std::min(target, int(b.timer) + 45)
                                    : std::max(target, int(b.timer) - 45));
      b.drillRotation += b.timer * .006f;
      if (b.drillRotation >= 90)
        b.drillRotation -= 90;
      b.drillRate = working ? b.ore.count * 60.f * speed * b.timer /
                                  (1000000.f * b.ore.delay)
                            : 0.f;
      if (working) {
        unsigned added =
            (uint32_t(96) * b.ore.count * speed * b.timer + 500000) / 1000000;
        b.progress += uint16_t(added);
        unsigned delay = b.ore.delay * 32u;
        if (b.progress >= delay) {
          ++b.inv[b.ore.item];
          b.progress -= uint16_t(delay);
        }
      }
    }
    if (processTime(type))
      productionStep(i);
    bool transport = type == Conveyor || type == Router || type == Junction;
    bool conveyorSent = false;
    if (transport && (b.timer || received[i]))
      continue;
    for (int item = 0; item < Items; ++item)
      if (b.inv[item] && !(type == Press && item != Graphite) &&
          !(type == Smelter && item != Silicon) &&
          !(type == Kiln && item != Metaglass) && type != GroundFactory &&
          type != AirFactory && type != Generator && type != Duo) {
        bool sent = false;
        for (int dd = 0; dd < 4 && !sent; ++dd) {
          int d = type == Junction              ? b.cursor
                  : transport && type != Router ? b.dir
                                                : (b.cursor + dd) % 4;
          if (transport && type != Router && dd)
            break;
          for (int offset = 0; offset < size && !sent; ++offset) {
            int x = b.x + (d == 0   ? size
                           : d == 2 ? -1
                                    : offset),
                y = b.y + (d == 3   ? size
                           : d == 1 ? -1
                                    : offset);
            int target = at(x, y);
            if (target != i && accept(target, Item(item), d)) {
              --b.inv[item];
              conveyorSent = true;
              if (transport)
                b.timer = type == Router ? 4 : 5;
              if (type != Junction)
                b.cursor = (d + 1) & 3;
              if (target >= 0)
                received[target] = true;
              sent = true;
            }
          }
        }
        if (sent)
          break;
      }
    if (type == Conveyor) {
      bool loaded = false;
      for (auto count : b.inv) loaded |= count != 0;
      b.conveyorClog = loaded && !conveyorSent
                           ? uint8_t(std::min(20, int(b.conveyorClog) + 1)) : 0;
    }
    if ((type == Duo || type == Arc) && !b.timer) {
      int ammo = b.ammoLeft        ? int(b.ammoType)
                 : b.inv[Silicon]  ? Silicon
                 : b.inv[Graphite] ? Graphite
                                   : Copper;
      if (type == Duo && !b.inv[ammo] && !b.ammoLeft)
        continue;
      if (type == Arc && !b.powered)
        continue;
      int target = -1;
      float best = blockRange(Block(type));
      for (int j = 0; j < MaxEnemies; ++j) {
        auto &e = enemies[j];
        if (!e.hp)
          continue;
        float dd = std::hypot(e.x - b.x - .5f, e.y - b.y - .5f);
        if (dd < best) {
          best = dd;
          target = j;
        }
      }
      if (target >= 0) {
        auto &e = enemies[target];
        int damage = type == Arc        ? 20
                     : ammo == Silicon  ? 12
                     : ammo == Graphite ? 18
                                        : 9;
        e.hp = e.hp > damage ? e.hp - damage : 0;
        if (!e.hp)
          ++kills;
        if (type == Duo) {
          if (!b.ammoLeft) {
            --b.inv[ammo];
            b.ammoType = uint8_t(ammo);
            b.ammoLeft = ammo == Silicon ? 5 : ammo == Graphite ? 4 : 2;
          }
          --b.ammoLeft;
        }
        b.timer = ammo == Silicon ? 5 : 7;
        for (auto &shot : shots)
          if (!shot.life) {
            shot = {b.x + .5f, b.y + .5f, e.x, e.y, 4, false};
            break;
          }
        emit(type == Arc ? Sound::ArcShot : Sound::DuoShot, b.x + .5f, b.y + .5f);
      }
    }
    if (type == Mender && b.powered && !b.timer) {
      bool healed = false;
      for (auto &a : buildings)
        if (a.type && !a.buildState &&
            std::hypot(float(a.x) - b.x, float(a.y) - b.y) <=
                blockRange(Mender) && a.hp < defs[a.type].hp) {
          a.hp = std::min<int>(defs[a.type].hp,
                               a.hp + std::max(1, defs[a.type].hp * 4 / 100));
          healed = true;
        }
      if (healed) emit(Sound::Heal, b.x + .5f, b.y + .5f);
      b.timer = 67;
    }
  }
  if (ticks % 10 == 0) {
    for (auto &e : enemies)
      if (e.hp) {
        int target = -1;
        float best = 5.5f;
        for (int i = 0; i < MaxBuildings; ++i) {
          auto &b = buildings[i];
          if (!b.type || b.buildState == 1)
            continue;
          float dd = std::hypot(e.x - b.x - defs[b.type].size * .5f,
                                e.y - b.y - defs[b.type].size * .5f);
          if (dd < best) {
            best = dd;
            target = i;
          }
        }
        int friendly = -1;
        for (int i = 0; i < MaxUnits; ++i)
          if (units[i].hp) {
            float d = std::hypot(e.x - units[i].x, e.y - units[i].y);
            if (d < best) {
              best = d;
              friendly = i;
            }
          }
        if (friendly >= 0) {
          emit(e.kind ? Sound::Explosion : Sound::UnitShot, e.x, e.y);
          auto &u = units[friendly];
          int damage = e.kind ? 35 : 9;
          u.hp = u.hp > damage ? u.hp - damage : 0;
          if (e.kind)
            e.hp = 0;
          continue;
        }
        if (target >= 0) {
          if (!e.kind) emit(Sound::UnitShot, e.x, e.y);
          auto &b = buildings[target];
          int damage = e.kind ? 35 : 9;
          b.hp = b.hp > damage ? b.hp - damage : 0;
          for (auto &shot : shots)
            if (!shot.life) {
              shot = {e.x, e.y, b.x + .5f, b.y + .5f, 4, true};
              break;
            }
          if (e.kind) {
            e.hp = 0;
            emit(Sound::Explosion, e.x, e.y);
          }
          if (!b.hp) {
            bool isCore = b.type == Core;
            b = Building{};
            rebuild();
            emit(Sound::Explosion, e.x, e.y);
            if (isCore) {
              lost = true;
              return;
            }
          }
        }
      }
  }
  // Reverse flood field over walkable terrain. Buildings remain attackable
  // obstacles.
  int flowTarget = (c.y + 1) * W + c.x + 1;
  if (alive() && (flowDirty || flowGoal != flowTarget)) {
    ++flowRebuilds;
    flowGoal = int16_t(flowTarget);
    flowDirty = false;
    std::fill(flow, flow + W * H, uint16_t(65535));
    int queue[W * H];
    int head = 0, tail = 0;
    int dest = (c.y + 1) * W + c.x + 1;
    flow[dest] = 0;
    queue[tail++] = dest;
    while (head < tail) {
      int p = queue[head++], x = p % W, y = p / W;
      for (int d = 0; d < 4; ++d) {
        int xx = x + vx[d], yy = y + vy[d];
        if (xx < 2 || yy < 2 || xx >= W - 2 || yy >= H - 2)
          continue;
        int pp = yy * W + xx;
        if (flow[pp] != 65535 || tiles[pp].ground == Rock ||
            tiles[pp].ground == Water)
          continue;
        flow[pp] = flow[p] + 1;
        queue[tail++] = pp;
      }
    }
  }
  for (auto &e : enemies)
    if (e.hp) {
      int x = std::clamp(int(e.x), 2, W - 3),
          y = std::clamp(int(e.y), 2, H - 3);
      float tx = c.x + 1.5f, ty = c.y + 1.5f;
      int best = flow[y * W + x];
      for (int d = 0; d < 4; ++d) {
        int xx = x + vx[d], yy = y + vy[d];
        if (xx < 2 || yy < 2 || xx >= W - 2 || yy >= H - 2)
          continue;
        if (flow[yy * W + xx] < best) {
          best = flow[yy * W + xx];
          tx = xx + .5f;
          ty = yy + .5f;
        }
      }
      float ax = tx - e.x, ay = ty - e.y, len = std::hypot(ax, ay);
      float speed = e.kind ? .085f : .045f;
      if (len > .05f) {
        float nx = e.x + ax / len * speed, ny = e.y + ay / len * speed;
        int b = at(int(nx), int(ny));
        if (b < 0 || b == coreId) {
          e.x = nx;
          e.y = ny;
        }
      }
    }
  // v154 doubles each mirrored mount's reload to 34 frames. Alternating
  // mounts therefore emit ONE 11-damage bolt every 17 original frames.
  // Quantizing original frames to 20 Hz retains the average reload rate.
  if (builder < 0 && !mining && (ticks * 3) / 17 != ((ticks - 1) * 3) / 17) {
    int target = -1;
    float nearest = 150.f / 8.f;
    for (int i = 0; i < MaxEnemies; ++i) if (enemies[i].hp) {
      float d = std::hypot(enemies[i].x - playerX, enemies[i].y - playerY);
      if (d < nearest) { nearest = d; target = i; }
    }
    if (target >= 0) {
      auto &e = enemies[target];
      e.hp = e.hp > 11 ? e.hp - 11 : 0;
      if (!e.hp) ++kills;
      for (auto &shot : shots) if (!shot.life) {
        shot = {playerX, playerY, e.x, e.y, 4, false}; break;
      }
      emit(Sound::AlphaShot, playerX, playerY);
    }
  }
  unitStep();
  if (!sandbox && wave >= uint16_t(10 + sector * 5) && !alive()) {
    won = true;
    captured |= uint8_t(1u << sector);
  }
}
} // namespace md
