// SPDX-License-Identifier: MIT
#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
namespace starsoil {
namespace {
void notice(State &s, const char *message) {
  std::snprintf(s.event, sizeof(s.event), "%s", message);
  ++s.event_serial;
}
int ix(float x) { return int(std::floor(x / 16)); }
int index(int x, int y) { return y * WorldW + x; }
bool inside(const State &s, int x, int y) {
  return x > 0 && x < s.width - 1 && y > 0 && y < s.height - 1;
}
void put(State &s, int x, int y, int t) {
  if (inside(s, x, y))
    s.tiles[index(x, y)] = uint8_t(t);
}
bool pack(Slot *bag, int n, int item, int amount) {
  if (item <= None || item >= ItemCount || amount < 0)
    return false;
  int capacity = 0;
  for (int i = 0; i < n; ++i)
    if (!bag[i].item || bag[i].item == item)
      capacity += 999 - bag[i].count;
  if (capacity < amount)
    return false;
  for (int empty = 0; empty < 2; ++empty)
    for (int i = 0; i < n && amount; ++i)
      if (empty ? !bag[i].item : bag[i].item == item) {
        int step = std::min(amount, 999 - int(bag[i].count));
        bag[i].item = uint8_t(item);
        bag[i].count += step;
        amount -= step;
      }
  return true;
}
void scatter(State &s, float x, float y, int item, int amount) {
  for (auto &d : s.drops)
    if (!d.item) {
      d = {x, y, -20, uint8_t(item), uint16_t(amount)};
      return;
    }
  // Keep loot when the bounded pool fills by merging a matching stack.
  for (auto &d : s.drops)
    if (d.item == item && d.count + amount <= 999) {
      d.count += amount;
      return;
    }
}
void give(State &s, float x, float y, int item, int amount) {
  if (!add(s, item, amount))
    scatter(s, x, y, item, amount);
}
bool body_clear(const State &s, float x, float y, float w = 10, float h = 24) {
  for (int ty = ix(y - h); ty <= ix(y - .01f); ++ty)
    for (int tx = ix(x - w / 2); tx <= ix(x + w / 2 - .01f); ++tx)
      if (solid(tile(s, tx, ty)) || tile(s, tx, ty) == Door)
        return false;
  return true;
}
bool occupied(const State &s, int x, int y) {
  auto overlaps = [&](const Actor &a) {
    return x * 16 < a.x + 5 && (x + 1) * 16 > a.x - 5 && y * 16 < a.y &&
           (y + 1) * 16 > a.y - 24;
  };
  if (overlaps(s.player))
    return true;
  for (const auto &n : s.npcs)
    if (overlaps(n))
      return true;
  return false;
}
void physics(const State &s, Actor &a, float dt, float w = 10, float h = 24,
             bool drop = false) {
  float dx = a.vx * dt;
  int steps = std::max(1, int(std::ceil(std::fabs(dx) / 3)));
  for (int i = 0; i < steps; ++i) {
    float nx = a.x + dx / steps;
    if (body_clear(s, nx, a.y, w, h))
      a.x = nx;
    else {
      a.vx = 0;
      break;
    }
  }
  a.vy = std::min(260.0f, a.vy + 560 * dt);
  float dy = a.vy * dt;
  steps = std::max(1, int(std::ceil(std::fabs(dy) / 3)));
  a.grounded = false;
  for (int i = 0; i < steps; ++i) {
    float ny = a.y + dy / steps;
    bool platform = false;
    if (dy >= 0 && !drop)
      for (int tx = ix(a.x - w / 2); tx <= ix(a.x + w / 2 - .01f); ++tx)
        if (tile(s, tx, ix(ny)) == Platform && ix(a.y - .1f) < ix(ny))
          platform = true;
    if (!platform && body_clear(s, a.x, ny, w, h))
      a.y = ny;
    else {
      if (a.vy >= 0) {
        a.grounded = true;
        // Land on the surface instead of keeping a variable subpixel gap.
        a.y = float(ix(platform ? ny : ny - .01f) * 16);
      }
      a.vy = 0;
      break;
    }
  }
  // Short final simulation steps may move less than the collision epsilon.
  // Resolve support explicitly so a resting actor never flickers into jump.
  if (!a.grounded && a.vy >= 0) {
    int row = ix(a.y + .0625f);
    float top = float(row * 16);
    if (a.y >= top - .0625f && a.y <= top + .01f)
      for (int tx = ix(a.x - w / 2); tx <= ix(a.x + w / 2 - .01f); ++tx) {
        int t = tile(s, tx, row);
        if (solid(t) || t == Door || (!drop && t == Platform)) {
          a.y = top;
          a.vy = 0;
          a.grounded = true;
          break;
        }
      }
  }
  a.x = std::clamp(a.x, 20.0f, float((s.width - 1) * 16 - 8));
  if (a.y < h) {
    a.y = h;
    a.vy = std::max(0.0f, a.vy);
  }
  a.y = std::min(a.y, float((s.height - 1) * 16) - .0625f);
}
void hurt(State &s, int damage) {
  if (s.invulnerable)
    return;
  damage = std::max(1, damage - defense(s) / 2);
  s.hp = uint16_t(std::max(0, int(s.hp) - damage));
  s.invulnerable = 850;
  s.player.vy = -100;
  if (!s.hp) {
    respawn(s);
    notice(s, "你在床边醒来，背包物品已保留。");
  }
  s.dirty = true;
}
void kill(State &s, Actor &m) {
  ++s.kills;
  s.dirty = true;
  int type = m.type;
  give(s, m.x, m.y,
       type == 1   ? Gel
       : type == 2 ? Silk
       : type == 3 ? StarDust
       : type == 4 ? Crystal
                   : StarDust,
       type == 5 ? 15 : 1 + rng(s) % 3);
  if (type == 5) {
    s.boss_defeated = 1;
    give(s, m.x, m.y, EmberCape, 1);
    give(s, m.x, m.y, StarCore, 1);
    notice(s, "击败星核守卫！获得余烬披风与星核。");
  }
  m.hp = 0;
}
void attack(State &s, int id) {
  const auto &def = items[id];
  auto &p = s.player;
  int facing = p.face ? 1 : -1;
  if (def.kind == Bow || def.kind == Wand) {
    if (def.kind == Bow && count(s, Arrows) == 0) {
      notice(s, "没有箭了，可在工作台制作。");
      return;
    }
    if (def.kind == Wand && s.mana < 6) {
      notice(s, "魔力不足。");
      return;
    }
    for (auto &b : s.shots)
      if (!b.life) {
        float dx = s.target_x * 16 + 8 - p.x,
              dy = s.target_y * 16 + 8 - (p.y - 14),
              len = std::sqrt(dx * dx + dy * dy);
        if (len < 1) {
          dx = float(facing);
          dy = 0;
          len = 1;
        }
        b = {p.x,  p.y - 14, dx / len * 220,           dy / len * 220,
             1800, def.stat, uint8_t(def.kind == Wand)};
        if (def.kind == Bow)
          remove(s, Arrows, 1);
        else
          s.mana -= 6;
        s.cooldown = def.kind == Bow ? 380 : 270;
        s.dirty = true;
        return;
      }
    return;
  }
  s.cooldown = 270;
  for (auto &m : s.mobs)
    if (m.hp > 0 && std::fabs(m.x - p.x) < (m.type == 5 ? 66 : 47) &&
        std::fabs(m.y - p.y) < 40 && (m.x - p.x) * facing > -12) {
      m.hp -= def.kind == Sword ? def.stat : 3;
      m.vx = facing * 110;
      m.vy = -80;
      m.timer = 180;
      if (m.hp <= 0)
        kill(s, m);
    }
}
void tree(State &s, int x, int y) {
  int base = y;
  while (tile(s, x, base + 1) == Log)
    ++base;
  int top = y;
  while (tile(s, x, top - 1) == Log)
    --top;
  for (int yy = top; yy <= base; ++yy) {
    put(s, x, yy, Air);
    give(s, x * 16 + 8, yy * 16 + 8, Wood, 1);
  }
  for (int yy = top - 3; yy <= top + 3; ++yy)
    for (int xx = x - 3; xx <= x + 3; ++xx)
      if (tile(s, xx, yy) == Leaves) {
        put(s, xx, yy, Air);
        if (rng(s) % 6 == 0)
          give(s, xx * 16 + 8, yy * 16 + 8, Acorn, 1);
        if (rng(s) % 7 == 0)
          give(s, xx * 16 + 8, yy * 16 + 8, Berry, 1);
      }
  s.dirty = true;
}
void use(State &s, const Input &input, uint32_t ms) {
  int item = s.bag[s.selected].item;
  const auto &def = items[item];
  int x = s.target_x, y = s.target_y;
  float dx = x * 16 + 8 - s.player.x, dy = y * 16 + 8 - (s.player.y - 12);
  if (def.kind == Sword || def.kind == Bow || def.kind == Wand) {
    if (!s.cooldown)
      attack(s, item);
    return;
  }
  if (dx * dx + dy * dy > 96 * 96 || !inside(s, x, y)) {
    s.mining = 0;
    return;
  }
  uint8_t t = tile(s, x, y);
  if (item == Glass && (t == Water || t == Lava) && !s.cooldown) {
    if (add(s, t == Water ? WaterBucket : LavaBucket, 1)) {
      remove(s, Glass, 1);
      set_tile(s, x, y, Air);
      s.cooldown = 400;
      notice(s, "已用玻璃瓶装取液体，可再次放置。");
    }
    return;
  }
  if (def.kind == Pick || def.kind == Axe) {
    if (!t || t == Water || t == Lava) {
      s.mining = 0;
      return;
    }
    int tier =
        t == GoldOre ? 2
        : (t == CrystalOre || t == RubyOre || t == Obsidian || t == Deepstone)
            ? 3
            : 1;
    if (def.kind == Axe && t != Log && t != Leaves && t != Cactus &&
        t != Vine && t != Sapling) {
      s.mining = 0;
      return;
    }
    if (def.tier < tier) {
      if (!s.cooldown) {
        notice(s, "需要更高等级的镐。");
        s.cooldown = 1200;
      }
      return;
    }
    if (s.mine_x != x || s.mine_y != y) {
      s.mining = 0;
      s.mine_x = x;
      s.mine_y = y;
    }
    s.mining += uint16_t(ms * def.stat * (def.kind == Axe ? 3 : 1));
    int hardness = t == Log                                    ? 700
                   : t == Stone || t == Deepstone || is_ore(t) ? 500
                   : t == Obsidian                             ? 1000
                                                               : 220;
    if (s.mining >= hardness) {
      if (t == Log)
        tree(s, x, y);
      else {
        set_tile(s, x, y, Air);
        give(s, x * 16 + 8, y * 16 + 8, tile_drop(t), 1);
        if (t == Leaves && rng(s) % 3 == 0)
          give(s, x * 16 + 8, y * 16 + 8, Acorn, 1);
      }
      ++s.mined;
      s.mining = 0;
      s.cooldown = 80;
    }
    return;
  }
  s.mining = 0;
  if (s.cooldown)
    return;
  if (def.kind == Block || def.kind == Furniture) {
    if (t && t != Flower && t != Herb && t != Mushroom && t != Vine)
      return;
    if ((solid(def.tile) || def.tile == Door) &&
        !(def.tile == Plank && input.down) && occupied(s, x, y))
      return;
    bool support = tile(s, x - 1, y) || tile(s, x + 1, y) ||
                   tile(s, x, y - 1) || tile(s, x, y + 1) ||
                   s.walls[index(x, y)];
    if (!support) {
      notice(s, "靠近地面或已有方块放置。");
      s.cooldown = 400;
      return;
    }
    if (def.tile == Sapling) {
      if (tile(s, x, y + 1) != Grass && tile(s, x, y + 1) != Dirt)
        return;
    }
    if (def.tile == Plank && input.down) {
      s.walls[index(x, y)] = Plank;
      s.dirty = true;
    } else
      set_tile(s, x, y, def.tile);
    remove(s, item, 1);
    s.cooldown = 180;
    s.dirty = true;
    return;
  }
  if (def.kind == Food || def.kind == Potion) {
    activate(s, s.selected);
    s.cooldown = 400;
  }
}
void shelter(State &s) {
  for (int x = 34; x < 78; ++x) {
    s.heights[x] = 31;
    for (int y = 18; y < 39; ++y)
      put(s, x, y, y < 31 ? Air : y == 31 ? Grass : Dirt);
  }
  for (int y = 26; y <= 30; ++y)
    for (int x = 41; x <= 53; ++x) {
      s.walls[index(x, y)] = Plank;
      if (x == 41 || x == 53 || y == 26)
        put(s, x, y, Plank);
    }
  put(s, 53, 29, Door);
  put(s, 53, 30, Air);
  put(s, 44, 30, Bench);
  put(s, 46, 29, Torch);
  put(s, 49, 30, Bed);
  for (int x : {36, 57, 76}) {
    int tall = x == 57 ? 5 : 7;
    for (int y = 30; y >= 31 - tall; --y)
      put(s, x, y, Log);
    for (int yy = -2; yy <= 2; ++yy)
      for (int xx = -3; xx <= 3; ++xx)
        if (xx * xx + yy * yy < 12 && !tile(s, x + xx, 31 - tall + yy))
          put(s, x + xx, 31 - tall + yy, Leaves);
  }
  for (int x : {38, 39, 55, 60, 64, 75})
    put(s, x, 30, x % 2 ? Flower : Herb);
  // A window and a shallow starter shaft with a real copper seam.
  s.walls[index(47, 28)] = Glass;
  s.walls[index(48, 28)] = Glass;
  for (int y = 31; y < 41; ++y) {
    put(s, 71, y, Air);
    put(s, 72, y, Air);
    if (y % 3 == 0)
      put(s, 71, y, Platform);
  }
  for (int x = 68; x < 76; ++x)
    for (int y = 40; y < 43; ++y)
      put(s, x, y, Air);
  put(s, 69, 39, Torch);
  put(s, 74, 41, Chest);
  for (int x = 73; x < 77; ++x)
    for (int y = 43; y < 46; ++y)
      put(s, x, y, CopperOre);
}
} // namespace
uint32_t hash(uint32_t n) {
  n ^= n >> 16;
  n *= 0x7feb352du;
  n ^= n >> 15;
  n *= 0x846ca68bu;
  return n ^ (n >> 16);
}
uint32_t rng(State &s) {
  s.random ^= s.random << 13;
  s.random ^= s.random >> 17;
  s.random ^= s.random << 5;
  return s.random;
}
uint8_t tile(const State &s, int x, int y) {
  return x < 0 || x >= s.width || y >= s.height ? Deepstone
         : y < 0                             ? Air
                                             : s.tiles[index(x, y)];
}
bool set_tile(State &s, int x, int y, uint8_t value) {
  if (!inside(s, x, y) || value >= TileCount)
    return false;
  s.tiles[index(x, y)] = value;
  s.dirty = true;
  return true;
}
Biome biome_at(int x, int y, int width) {
  x = int(int64_t(x) * DefaultWorldW / width);
  return y >= 68   ? CrystalDepths
         : x < 105 ? Forest
         : x < 155 ? Meadow
         : x < 225 ? Desert
         : x < 290 ? Tundra
                   : Jungle;
}
const char *biome_name(Biome b, bool zh) {
  static const char *en[] = {"MEADOW",    "FERNWOOD",      "AMBER DUNES",
                             "FROSTFALL", "VERDANT WILDS", "LUMEN DEPTHS"};
  static const char *cn[] = {"花语草甸", "蕨木森林", "琥珀沙丘",
                             "霜落雪原", "繁茂雨林", "流光深境"};
  return zh ? cn[b] : en[b];
}
void generate(State &s, uint32_t seed) {
  s.seed = seed;
  s.random = seed ? seed : 1;
  for (int x = 0; x < s.width; ++x) {
    int h = 31 + int(3 * std::sin(x * .064f) + 2 * std::sin(x * .173f) +
                     3 * std::sin((x + seed % 100) * .025f));
    s.heights[x] = uint8_t(h);
    Biome b = biome_at(x, 0, s.width);
    for (int y = 0; y < s.height; ++y) {
      int t = Air;
      if (y >= h) {
        t = y == h      ? (b == Desert   ? Sand
                           : b == Tundra ? Snow
                                         : Grass)
            : y < h + 5 ? (b == Desert   ? Sand
                           : b == Tundra ? Snow
                                         : Dirt)
            : y > 75    ? Deepstone
                        : Stone;
        if (b == Tundra && y >= h + 3 && y <= h + 6)
          t = Ice;
        if (y > h + 6 && y < s.height - 3) {
          uint32_t n =
              hash(seed + uint32_t(x / 3) * 173u + uint32_t(y / 3) * 7919u);
          if (n % 13 == 0)
            t = CoalOre;
          else if (n % 13 == 1)
            t = CopperOre;
          else if (n % 17 == 0 && y > 44)
            t = IronOre;
          else if (n % 23 == 0 && y > 56)
            t = GoldOre;
          else if (n % 19 == 0 && y > 70)
            t = CrystalOre;
          else if (n % 43 == 0 && y > 79)
            t = RubyOre;
          if (y > 98 && n % 3 == 0)
            t = Obsidian;
        }
      }
      s.tiles[index(x, y)] = uint8_t(t);
      s.walls[index(x, y)] = y >= h ? uint8_t(y > 70 ? Deepstone : Stone) : 0;
    }
  }
  // Connected wandering caves, deterministic and reproducible from the world
  // seed.
  for (int cave = 0; cave < 43 * s.width * s.height / (DefaultWorldW * DefaultWorldH); ++cave) {
    float x = 8 + float(rng(s) % (s.width - 16)), y = 43 + float(rng(s) % (s.height - 54)),
          angle = float(rng(s) % 628) / 100;
    for (int step = 0; step < 55; ++step) {
      float radius = 2 + float(rng(s) % 11) / 10;
      for (int yy = int(y - radius); yy <= y + radius; ++yy)
        for (int xx = int(x - radius); xx <= x + radius; ++xx)
          if (inside(s, xx, yy) &&
              yy > s.heights[std::clamp(xx, 0, s.width - 1)] + 5 &&
              (xx - x) * (xx - x) + (yy - y) * (yy - y) < radius * radius)
            put(s, xx, yy, Air);
      angle += (int(rng(s) % 101) - 50) * .018f;
      x += std::cos(angle) * 1.5f;
      y += std::sin(angle) * .7f;
      x = std::clamp(x, 5.0f, float(s.width - 6));
      y = std::clamp(y, 40.0f, float(s.height - 8));
    }
  }
  for (int x = 6; x < s.width - 6; ++x) {
    int h = s.heights[x];
    Biome b = biome_at(x, 0, s.width);
    uint32_t n = hash(seed + x * 317);
    if (b != Desert && x % 9 == 0 && n % 3 && (b != Meadow || x % 27 == 0)) {
      int tall = 5 + n % 5;
      for (int y = h - 1; y >= h - tall; --y)
        put(s, x, y, Log);
      for (int yy = -3; yy <= 2; ++yy)
        for (int xx = -3; xx <= 3; ++xx)
          if (xx * xx + yy * yy < 13 && tile(s, x + xx, h - tall + yy) == Air)
            put(s, x + xx, h - tall + yy, Leaves);
      if (b == Jungle)
        for (int y = h - tall; y < h - 1; ++y)
          put(s, x + 2, y, Vine);
    } else if (b == Desert && x % 11 == 0) {
      for (int y = h - 1; y > h - 4; --y)
        put(s, x, y, Cactus);
    } else if (n % 4 == 0)
      put(s, x, h - 1,
          b == Desert   ? Cactus
          : b == Tundra ? Mushroom
          : n % 3 == 0  ? Flower
                        : Herb);
  }
  for (int y = 42; y < s.height - 5; ++y)
    for (int x = 3; x < s.width - 3; ++x) {
      if (tile(s, x, y) != Air)
        continue;
      uint32_t n = hash(seed + uint32_t(y * s.width + x));
      if (tile(s, x, y + 1) == Stone && n % 3 == 0) {
        put(s, x, y + 1, Moss);
        if (n % 2)
          put(s, x, y, Mushroom);
      }
      if (biome_at(x, 0, s.width) == Jungle && solid(tile(s, x, y - 1)) && n % 9 == 0)
        for (int h = 0; h < 3 && !tile(s, x, y + h); ++h)
          put(s, x, y + h, Vine);
    }
  for (int n = 0; n < 26 * s.width / DefaultWorldW; ++n) {
    int x = 10 + int(rng(s) % (s.width - 20)), y = 42 + int(rng(s) % (s.height - 54));
    while (y < s.height - 3 && tile(s, x, y) != Air)
      ++y;
    while (y < s.height - 3 && tile(s, x, y + 1) == Air)
      ++y;
    if (y < s.height - 3) {
      put(s, x, y, Chest);
      if (y > 70)
        put(s, x - 1, y, Torch);
    }
  }
  // Underground pools and a discoverable obsidian altar chamber.
  for (int x = 313 * s.width / DefaultWorldW; x < 330 * s.width / DefaultWorldW; ++x)
    for (int y = 79 * s.height / DefaultWorldH; y < 83 * s.height / DefaultWorldH; ++y) {
      put(s, x, y, y < 81 * s.height / DefaultWorldH ? Air : Water);
    }
  for (int x = 335 * s.width / DefaultWorldW; x < 350 * s.width / DefaultWorldW; ++x)
    for (int y = 98 * s.height / DefaultWorldH; y < 102 * s.height / DefaultWorldH; ++y)
      put(s, x, y, y < 100 * s.height / DefaultWorldH ? Air : Lava);
  for (int x = 188; x < 201; ++x)
    for (int y = 85; y <= 92; ++y)
      put(s, x, y, y == 92 ? Obsidian : Air);
  put(s, 194, 91, Portal);
  put(s, 190, 90, Torch);
  put(s, 198, 90, Torch);
  put(s, 197, 91, Chest);
  shelter(s);
}
void new_game(State &s, uint32_t seed, WorldSize size) {
  // Avoid a whole-State temporary on the small Wasm auxiliary stack.
  std::memset(&s, 0, sizeof(s));
  auto bounds = dimensions(size);
  s.width = bounds.width;
  s.height = bounds.height;
  s.hp = 100;
  s.mana = 60;
  s.day = 1;
  s.mine_x = s.mine_y = -1;
  generate(s, seed);
  s.home_x = 48;
  s.home_y = 31;
  s.player = {48 * 16.0f, 31 * 16.0f, 0, 0, 100, 0, 1};
  const Slot initial[] = {{WoodPick, 1},    {WoodAxe, 1},     {WoodSword, 1},
                          {Torches, 24},    {Wood, 24},       {Boards, 32},
                          {RedMushroom, 5}, {RecallPotion, 3}};
  std::copy(std::begin(initial), std::end(initial), s.bag);
  add(s, HunterBow, 1);
  add(s, Arrows, 40);
  add(s, Acorn, 4);
  s.equip[3] = BlueTunic;
  s.equip[4] = TealCape;
  s.equip[0] = StrawHat;
  for (int i = 0; i < 4; ++i)
    s.npcs[i] = {float((58 + i * 4) * 16),
                 31 * 16.0f,
                 0,
                 0,
                 100,
                 uint8_t(i),
                 uint8_t(i % 2)};
  s.clock = 90000;
  notice(s, "欢迎来到星壤！\n先拜访屋外向导，再探索右侧矿井。");
  s.dirty = true;
}
int count(const State &s, int id) {
  int n = 0;
  for (auto a : s.bag)
    if (a.item == id)
      n += a.count;
  return n;
}
bool add(State &s, int id, int n) {
  if (!pack(s.bag, BagSlots, id, n))
    return false;
  s.dirty = true;
  return true;
}
bool remove(State &s, int id, int n) {
  if (id <= 0 || id >= ItemCount || n < 0 || count(s, id) < n)
    return false;
  for (auto &a : s.bag)
    if (a.item == id) {
      int v = std::min(n, int(a.count));
      a.count -= v;
      n -= v;
      if (!a.count)
        a.item = 0;
    }
  s.dirty = true;
  return true;
}
bool equip(State &s, int slot) {
  if (slot < 0 || slot >= BagSlots || !s.bag[slot].item)
    return false;
  int id = s.bag[slot].item, pos = equip_slot(items[id].kind);
  if (pos < 0)
    return false;
  Slot candidate[BagSlots];
  std::copy(std::begin(s.bag), std::end(s.bag), candidate);
  if (!--candidate[slot].count)
    candidate[slot].item = 0;
  if (s.equip[pos] && !pack(candidate, BagSlots, s.equip[pos], 1))
    return false;
  std::copy(std::begin(candidate), std::end(candidate), s.bag);
  s.equip[pos] = uint8_t(id);
  s.dirty = true;
  notice(s, "装备已穿戴，外观与属性已更新。");
  return true;
}
bool unequip(State &s, int pos) {
  if (pos < 0 || pos >= EquipSlots || !s.equip[pos] || !add(s, s.equip[pos], 1))
    return false;
  s.equip[pos] = 0;
  s.dirty = true;
  return true;
}
bool store(State &s, int pos) {
  if (pos < 0 || pos >= BagSlots || !s.bag[pos].item)
    return false;
  if (!pack(s.chest, 12, s.bag[pos].item, s.bag[pos].count))
    return false;
  s.bag[pos] = {};
  s.dirty = true;
  return true;
}
bool withdraw(State &s, int pos) {
  if (pos < 0 || pos >= 12 || !s.chest[pos].item ||
      !add(s, s.chest[pos].item, s.chest[pos].count))
    return false;
  s.chest[pos] = {};
  return true;
}
bool nearby_storage(const State &s) {
  int px = ix(s.player.x), py = ix(s.player.y - 12);
  for (int y = py - 2; y <= py + 2; ++y)
    for (int x = px - 3; x <= px + 3; ++x)
      if (tile(s, x, y) == StorageChest)
        return true;
  return false;
}
int defense(const State &s) {
  int n = 0;
  for (int i = 0; i < 3; ++i)
    n += items[s.equip[i]].stat;
  return n;
}
bool near_station(const State &s, int kind) {
  if (kind == Hand)
    return true;
  int target = kind == Table   ? Bench
               : kind == Kiln  ? Furnace
               : kind == Forge ? Anvil
                               : Portal;
  for (int y = ix(s.player.y) - 4; y <= ix(s.player.y) + 2; ++y)
    for (int x = ix(s.player.x) - 5; x <= ix(s.player.x) + 5; ++x)
      if (tile(s, x, y) == target)
        return true;
  return false;
}
int station(const State &s) {
  for (int i = Altar; i >= Hand; --i)
    if (near_station(s, i))
      return i;
  return Hand;
}
bool can_craft(const State &s, int r) {
  if (r < 0 || r >= RecipeCount)
    return false;
  const auto &p = recipes[r];
  if (!near_station(s, p.station))
    return false;
  for (auto a : p.in)
    if (a.item && count(s, a.item) < a.count)
      return false;
  Slot copy[BagSlots];
  std::copy(std::begin(s.bag), std::end(s.bag), copy);
  for (auto a : p.in)
    if (a.item) {
      int n = a.count;
      for (auto &slot : copy)
        if (slot.item == a.item) {
          int v = std::min(n, int(slot.count));
          slot.count -= v;
          n -= v;
          if (!slot.count)
            slot.item = 0;
        }
    }
  return pack(copy, BagSlots, p.out, p.count);
}
bool craft(State &s, int r) {
  if (!can_craft(s, r))
    return false;
  for (auto a : recipes[r].in)
    if (a.item)
      remove(s, a.item, a.count);
  add(s, recipes[r].out, recipes[r].count);
  ++s.crafted;
  notice(s, "合成成功。");
  return true;
}
void respawn(State &s) {
  s.player.x = s.home_x * 16.0f;
  s.player.y = s.home_y * 16.0f;
  s.player.vx = s.player.vy = 0;
  for (int n = 0; n < s.height && !body_clear(s, s.player.x, s.player.y); ++n)
    s.player.y -= 16;
  if (!body_clear(s, s.player.x, s.player.y)) {
    s.player.x = 48 * 16;
    s.player.y = 26 * 16;
  }
  s.hp = 100;
  s.mana = 60;
  s.invulnerable = 2500;
  s.dirty = true;
}
bool activate(State &s, int slot) {
  if (slot < 0 || slot >= BagSlots || !s.bag[slot].item)
    return false;
  int id = s.bag[slot].item;
  if (equip_slot(items[id].kind) >= 0)
    return equip(s, slot);
  if (items[id].kind == Food) {
    if (s.hp == 100)
      return false;
    s.hp = std::min(100, int(s.hp) + items[id].stat);
  } else if (id == HealPotion) {
    if (s.hp == 100)
      return false;
    s.hp = std::min(100, int(s.hp) + 45);
  } else if (id == ManaPotion) {
    if (s.mana == 60)
      return false;
    s.mana = std::min(60, int(s.mana) + 30);
  } else if (id == RecallPotion) {
    respawn(s);
    notice(s, "星光将你带回家。");
  } else
    return false;
  remove(s, id, 1);
  return true;
}
int nearby_npc(const State &s) {
  int best = -1;
  float distance = 56;
  for (int i = 0; i < 4; ++i) {
    float d = std::fabs(s.player.x - s.npcs[i].x);
    if (d < distance && std::fabs(s.player.y - s.npcs[i].y) < 36) {
      best = i;
      distance = d;
    }
  }
  return best;
}
bool interact(State &s) {
  int px = ix(s.player.x), py = ix(s.player.y - 12);
  // Prefer the aimed furniture; nearby objects are a fallback for controllers.
  for (int pass = 0; pass < 2; ++pass)
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -3; dx <= 3; ++dx) {
        int x = pass ? px + dx : s.target_x, y = pass ? py + dy : s.target_y;
        if (!inside(s, x, y) || std::abs(x - px) > 5 || std::abs(y - py) > 4)
          continue;
        uint8_t t = tile(s, x, y);
        if (t == Chest) {
          const int loot[] = {CopperBar,  IronBar,    GoldBar,  Crystal,
                              Ruby,       Silk,       StarDust, HealPotion,
                              CloudBoots, IronHelmet, RedTunic, GlowCharm};
          uint32_t n = hash(s.seed + uint32_t(y * s.width + x));
          give(s, x * 16 + 8, y * 16, loot[n % 12],
               n % 12 >= 8 ? 1 : 3 + n % 4);
          give(s, x * 16 + 8, y * 16, Torches, 6);
          set_tile(s, x, y, Air);
          give(s, x * 16 + 8, y * 16, TreasureChest, 1);
          notice(s, "打开宝箱，获得了远行者留下的宝物。");
          return true;
        }
        if (t == Bed) {
          s.home_x = x;
          s.home_y = y + 1;
          s.dirty = true;
          notice(s, "已设置出生点，回城药水将带你回到这里。");
          return true;
        }
        if (t == Door) {
          set_tile(s, x, y, Air);
          s.walls[index(x, y)] = Door;
          notice(s, "木门已打开。");
          return true;
        }
        if (t == Air && s.walls[index(x, y)] == Door && !occupied(s, x, y) &&
            body_clear(s, x * 16 + 8, (y + 1) * 16)) {
          set_tile(s, x, y, Door);
          s.walls[index(x, y)] = Plank;
          notice(s, "木门已关闭。");
          return true;
        }
        if (t == Portal) {
          if (s.boss_defeated) {
            notice(s, "星核已重新点亮，世界属于你的下一次冒险。");
            return true;
          }
          for (auto &m : s.mobs)
            if (m.type == 5 && m.hp > 0) {
              notice(s, "守卫已经苏醒！");
              return true;
            }
          if (count(s, StarCore) == 0) {
            notice(s, "在祭坛合成星核，再互动唤醒守卫。");
            return true;
          }
          for (auto &m : s.mobs)
            if (m.hp <= 0) {
              remove(s, StarCore, 1);
              m = {s.player.x + 64, s.player.y - 30, 0, -80, 280, 5, 0};
              notice(s, "星核守卫苏醒了！");
              return true;
            }
        }
        if (!pass)
          break;
      }
  return nearby_npc(s) >= 0;
}
bool trade(State &s, int npc) {
  if (npc == 0) {
    if (!(s.quests & 1)) {
      give(s, s.player.x, s.player.y, Gel, 6);
      give(s, s.player.x, s.player.y, HealPotion, 3);
      s.quests |= 1;
      notice(s, "向导赠送了凝胶和药水。木材与凝胶能制作火把。");
      return true;
    }
    if (!(s.quests & 2) && s.mined >= 20) {
      give(s, s.player.x, s.player.y, CopperBar, 8);
      s.quests |= 2;
      notice(s, "完成采矿委托，获得八块铜锭。");
      return true;
    }
    if (!(s.quests & 4) && s.crafted >= 10) {
      give(s, s.player.x, s.player.y, StarDust, 8);
      s.quests |= 4;
      notice(s, "完成工匠委托，获得星尘。去寻找深处的祭坛吧。");
      return true;
    }
    notice(s, s.mined < 20 ? "向导委托：开采二十个方块。"
                           : "向导委托：完成十次合成。");
    return false;
  }
  Slot copy[BagSlots];
  std::copy(std::begin(s.bag), std::end(s.bag), copy);
  int cost = npc == 1   ? GoldBar
             : npc == 2 ? Copper
                        : Wildflower,
      n = npc == 1   ? 1
          : npc == 2 ? 9
                     : 3;
  int reward = npc == 1   ? Silk
               : npc == 2 ? IronBar
                          : HealPotion,
      qty = npc == 1   ? 8
            : npc == 2 ? 2
                       : 2;
  if (count(s, cost) < n) {
    notice(s, npc == 1   ? "商人：一块金锭换八份丝绸。"
              : npc == 2 ? "铁匠：九块铜矿换两块铁锭。"
                         : "园丁：三朵野花换两瓶生命药水。");
    return false;
  }
  remove(s, cost, n);
  if (!add(s, reward, qty)) {
    std::copy(std::begin(copy), std::end(copy), s.bag);
    notice(s, "背包已满，交易未进行。");
    return false;
  }
  notice(s, "交易完成。");
  return true;
}
void tick(State &s, const Input &in, uint32_t ms) {
  ms = std::min(ms, 40u);
  float dt = ms * .001f;
  auto &p = s.player;
  s.played += ms;
  s.clock += ms;
  if (s.clock >= 360000) {
    s.clock -= 360000;
    ++s.day;
  }
  s.cooldown = ms >= s.cooldown ? 0 : s.cooldown - ms;
  s.invulnerable = ms >= s.invulnerable ? 0 : s.invulnerable - ms;
  int speed = s.equip[5] ? 100 : 76;
  p.vx = float(in.move * speed);
  if (in.move)
    p.face = in.move > 0;
  if (p.grounded)
    s.jumps = 0;
  if (in.jump && !s.jump_down &&
      (p.grounded || (s.equip[5] == CloudBoots && s.jumps < 2))) {
    p.vy = -215;
    p.grounded = false;
    ++s.jumps;
  }
  s.jump_down = in.jump;
  float previous_x = p.x, previous_y = p.y;
  physics(s, p, dt, 10, 24, in.down);
  if (p.x != previous_x || p.y != previous_y)
    s.dirty = true;
  // Q8 travel distance: one gait frame per two pixels, no idle/wall treadmill.
  p.anim += uint16_t(std::fabs(p.x - previous_x) * 256);
  if (in.target_x >= 0) {
    s.target_x = in.target_x;
    s.target_y = in.target_y;
  } else {
    s.target_x = ix(p.x) + (in.aim_y   ? 0
                            : in.aim_x ? in.aim_x * 2
                            : p.face   ? 2
                                       : -2);
    s.target_y = ix(p.y) + (in.aim_y > 0 ? 0 : in.aim_y < 0 ? -3 : -1);
    if (!in.aim_y && !tile(s, s.target_x, s.target_y) &&
        solid(tile(s, s.target_x, s.target_y - 1)))
      --s.target_y;
  }
  if (in.use)
    use(s, in, ms);
  else
    s.mining = 0;
  uint8_t feet = tile(s, ix(p.x), ix(p.y - 3));
  if (feet == Lava)
    hurt(s, 18);
  if (feet == Water) {
    p.vy -= 330 * dt;
    if (in.jump)
      p.vy = -85;
  }
  if (in.interact)
    interact(s);
  s.regen += ms;
  if (s.regen >= 1500) {
    s.regen = 0;
    if (s.hp < 100 && !s.invulnerable)
      ++s.hp;
    if (s.mana < 60)
      s.mana = std::min(60, int(s.mana) + 2);
  }
  for (auto &d : s.drops)
    if (d.item) {
      float dx = p.x - d.x, dy = p.y - 10 - d.y;
      float reach = s.equip[7] == Magnet ? 100 : 28;
      if (dx * dx + dy * dy < reach * reach) {
        if (add(s, d.item, d.count)) {
          d.item = 0;
          continue;
        }
        d.x += dx * dt * 2;
      }
      d.vy = std::min(d.vy + 300 * dt, 100.0f);
      if (!solid(tile(s, ix(d.x), ix(d.y + d.vy * dt))))
        d.y += d.vy * dt;
      else
        d.vy = 0;
    }
  for (auto &n : s.npcs) {
    n.timer += ms;
    n.vx = n.timer % 8000 < 4000 ? 9 : -9;
    if (n.x < 56 * 16)
      n.vx = 12;
    if (n.x > 77 * 16)
      n.vx = -12;
    if (n.vx != 0)
      n.face = n.vx > 0;
    float old_npc_x = n.x;
    physics(s, n, dt);
    n.anim += uint16_t(std::fabs(n.x - old_npc_x) * 256);
  }
  for (auto &m : s.mobs)
    if (m.hp > 0) {
      m.timer += ms;
      m.anim += ms;
      float dx = p.x - m.x;
      m.face = dx > 0;
      if (std::fabs(dx) > 600 && m.type != 5) {
        m.hp = 0;
        continue;
      }
      if (m.type == 2) {
        m.vx = dx > 0 ? 38 : -38;
        m.vy = (p.y - 16 - m.y) * .9f + std::sin(m.anim * .009f) * 22;
        m.x += m.vx * dt;
        m.y += m.vy * dt;
      } else {
        if (m.grounded && m.timer > (m.type == 5 ? 440 : 900)) {
          m.vy = m.type == 5 ? -240 : -145;
          m.vx = (dx > 0 ? 1 : -1) * (m.type == 5 ? 85 : 32);
          m.timer = 0;
        }
        physics(s, m, dt,
                m.type == 5   ? 30
                : m.type == 1 ? 14
                              : 10,
                m.type == 5   ? 32
                : m.type == 1 ? 12
                              : 24);
      }
      if (std::fabs(dx) < (m.type == 5 ? 25 : 15) &&
          std::fabs(m.y - p.y) < (m.type == 5 ? 38 : 24))
        hurt(s, m.type == 5 ? 22 : m.type == 3 ? 14 : 8);
    }
  for (auto &b : s.shots)
    if (b.life) {
      b.life = ms >= b.life ? 0 : b.life - ms;
      float dx = b.vx * dt, dy = b.vy * dt;
      int steps = std::max(
          1, int(std::ceil(std::max(std::fabs(dx), std::fabs(dy)) / 3)));
      for (int n = 0; n < steps && b.life; ++n) {
        b.x += dx / steps;
        b.y += dy / steps;
        if (solid(tile(s, ix(b.x), ix(b.y)))) {
          b.life = 0;
          break;
        }
        for (auto &m : s.mobs)
          if (m.hp > 0 && std::fabs(m.x - b.x) < (m.type == 5 ? 20 : 10) &&
              b.y > m.y - (m.type == 5 ? 40 : 24) && b.y < m.y + 3) {
            m.hp -= b.damage;
            b.life = 0;
            if (m.hp <= 0)
              kill(s, m);
            break;
          }
      }
      if (!b.magic)
        b.vy += 110 * dt;
    }
  s.spawn_timer += ms;
  if (s.spawn_timer >= 3300) {
    s.spawn_timer = 0;
    int active = 0;
    for (auto m : s.mobs)
      if (m.hp > 0)
        ++active;
    if (active < 8)
      for (auto &m : s.mobs)
        if (m.hp <= 0) {
          int x = std::clamp(ix(p.x) +
                                 (rng(s) % 2 ? 1 : -1) * (10 + int(rng(s) % 7)),
                             3, s.width - 4),
              y = ix(p.y);
          if (y < int(s.heights[x]) + 5)
            y = s.heights[x] - 1;
          for (int n = 0; n < 8 && solid(tile(s, x, y)); ++n)
            --y;
          int type = ix(p.y) > 70       ? 4
                     : s.clock > 240000 ? 3
                     : rng(s) % 4 == 0  ? 2
                                        : 1;
          if (body_clear(s, x * 16 + 8, y * 16, 14, 24))
            m = {float(x * 16 + 8),
                 float(y * 16),
                 0,
                 0,
                 int16_t(type == 4   ? 55
                         : type == 3 ? 40
                                     : 24),
                 uint8_t(type),
                 0};
          break;
        }
  }
  // Slow, bounded regrowth. No full-world scan in the frame loop.
  if ((s.played / 40) % 50 == 0) {
    int x = 3 + int(rng(s) % (s.width - 6)), y = s.heights[x] - 1;
    if (tile(s, x, y) == Sapling) {
      bool room = true;
      for (int h = 1; h < 6; ++h)
        if (tile(s, x, y - h))
          room = false;
      if (room) {
        for (int h = 0; h < 5; ++h)
          put(s, x, y - h, Log);
        for (int yy = -2; yy <= 1; ++yy)
          for (int xx = -2; xx <= 2; ++xx)
            if (!tile(s, x + xx, y - 5 + yy))
              put(s, x + xx, y - 5 + yy, Leaves);
        s.dirty = true;
      }
    }
  }
}
namespace {
uint32_t checksum(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}
struct Writer {
  uint8_t *p;
  size_t cap, at = 0;
  bool ok = true;
  void u(uint32_t v, int n = 4) {
    for (int i = 0; i < n; ++i) {
      if (at >= cap) {
        ok = false;
        return;
      }
      p[at++] = uint8_t(v >> (8 * i));
    }
  }
  void bytes(const uint8_t *src, size_t n) {
    if (n > cap - at) {
      ok = false;
      return;
    }
    std::memcpy(p + at, src, n);
    at += n;
  }
};
struct Reader {
  const uint8_t *p;
  size_t cap, at = 0;
  bool ok = true;
  uint32_t u(int n = 4) {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) {
      if (at >= cap) {
        ok = false;
        return 0;
      }
      v |= uint32_t(p[at++]) << (i * 8);
    }
    return v;
  }
  void bytes(uint8_t *dst, size_t n) {
    if (n > cap - at) {
      ok = false;
      return;
    }
    std::memcpy(dst, p + at, n);
    at += n;
  }
};
} // namespace
size_t encode(const State &s, uint8_t *out, size_t capacity) {
  if (!out || !valid_dimensions(s.width, s.height) ||
      capacity < save_size(s.width, s.height))
    return 0;
  Writer w{out, capacity};
  w.u(0x31525353);
  w.u(1, 2);
  w.u(s.width, 2);
  w.u(s.height, 2);
  w.u(s.seed);
  w.u(s.random);
  w.u(s.clock);
  w.u(s.day);
  w.u(s.played);
  w.u(s.kills);
  w.u(s.mined);
  w.u(s.crafted);
  w.u(s.quests);
  w.u(uint32_t(std::clamp(s.player.x, 20.0f, float((s.width - 1) * 16 - 8)) *
               16));
  w.u(uint32_t(
      std::clamp(s.player.y, 24.0f, float((s.height - 1) * 16) - .0625f) * 16));
  w.u(s.home_x, 2);
  w.u(s.home_y, 2);
  w.u(s.hp, 2);
  w.u(s.mana, 2);
  w.u(s.selected, 1);
  w.u(s.boss_defeated, 1);
  for (auto a : s.bag) {
    w.u(a.item, 1);
    w.u(a.count, 2);
  }
  for (auto a : s.chest) {
    w.u(a.item, 1);
    w.u(a.count, 2);
  }
  w.bytes(s.equip, EquipSlots);
  w.bytes(s.heights, s.width);
  for (int y = 0; y < s.height; ++y)
    w.bytes(s.tiles + y * WorldW, s.width);
  for (int y = 0; y < s.height; ++y)
    w.bytes(s.walls + y * WorldW, s.width);
  // Keep uncollected loot across restarts as well as edited terrain.
  for (auto d : s.drops) {
    w.u(d.item, 1);
    w.u(d.count, 2);
    w.u(uint32_t(std::clamp(d.x, 0.0f, float(s.width * 16 - 1)) * 16));
    w.u(uint32_t(std::clamp(d.y, 0.0f, float(s.height * 16 - 1)) * 16));
  }
  if (!w.ok || capacity - w.at < 4)
    return 0;
  uint32_t h = checksum(out, w.at);
  w.u(h);
  return w.at;
}
bool inspect_save(const uint8_t *data, size_t size, SaveInfo &info) {
  if (!data || size < 1024 || size > SaveBytes)
    return false;
  Reader r{data, size - 4};
  uint32_t stored = uint32_t(data[size - 4]) | uint32_t(data[size - 3]) << 8 |
                    uint32_t(data[size - 2]) << 16 |
                    uint32_t(data[size - 1]) << 24;
  if (checksum(data, size - 4) != stored || r.u() != 0x31525353 || r.u(2) != 1)
    return false;
  SaveInfo next;
  next.width = uint16_t(r.u(2));
  next.height = uint16_t(r.u(2));
  if (!valid_dimensions(next.width, next.height) ||
      size != save_size(next.width, next.height)) return false;
  next.seed = r.u();
  r.u(); r.u();
  next.day = r.u();
  next.played = r.u();
  if (!next.day) return false;
  info = next;
  return true;
}
bool decode(State &s, const uint8_t *data, size_t size) {
  SaveInfo info;
  if (!inspect_save(data, size, info)) return false;
  Reader r{data, size - 4};
  r.at = 10;
  static State v;
  std::memset(&v, 0, sizeof(v));
  v.width = info.width;
  v.height = info.height;
  v.mine_x = v.mine_y = -1;
  v.seed = r.u();
  v.random = r.u();
  v.clock = r.u();
  v.day = r.u();
  v.played = r.u();
  v.kills = r.u();
  v.mined = r.u();
  v.crafted = r.u();
  v.quests = r.u();
  v.player.x = r.u() / 16.0f;
  v.player.y = r.u() / 16.0f;
  v.home_x = int16_t(r.u(2));
  v.home_y = int16_t(r.u(2));
  v.hp = r.u(2);
  v.mana = r.u(2);
  v.selected = r.u(1);
  v.boss_defeated = r.u(1);
  auto read_slot = [&](Slot &a) {
    a.item = uint8_t(r.u(1));
    a.count = uint16_t(r.u(2));
    return a.item < ItemCount && a.count <= 999 &&
           bool(a.item) == bool(a.count);
  };
  for (auto &a : v.bag)
    if (!read_slot(a))
      return false;
  for (auto &a : v.chest)
    if (!read_slot(a))
      return false;
  r.bytes(v.equip, EquipSlots);
  for (int i = 0; i < EquipSlots; ++i)
    if (v.equip[i] >= ItemCount ||
        (v.equip[i] && equip_slot(items[v.equip[i]].kind) != i))
      return false;
  r.bytes(v.heights, v.width);
  for (int y = 0; y < v.height; ++y)
    r.bytes(v.tiles + y * WorldW, v.width);
  for (int y = 0; y < v.height; ++y)
    r.bytes(v.walls + y * WorldW, v.width);
  for (int x = 0; x < v.width; ++x)
    if (v.heights[x] < 10 || v.heights[x] > 60)
      return false;
  for (auto t : v.tiles)
    if (t >= TileCount)
      return false;
  for (auto t : v.walls)
    if (t != 0 && t != Stone && t != Deepstone && t != Plank && t != Glass &&
        t != Door)
      return false;
  for (auto &d : v.drops) {
    d.item = r.u(1);
    d.count = r.u(2);
    d.x = r.u() / 16.0f;
    d.y = r.u() / 16.0f;
    if (d.item >= ItemCount || d.count > 999 || d.x >= v.width * 16 ||
        d.y >= v.height * 16)
      return false;
  }
  if (!r.ok || r.at != size - 4 || v.clock >= 360000 || !v.day || v.hp > 100 ||
      v.mana > 60 || v.selected >= HotSlots || v.boss_defeated > 1 ||
      !v.random || v.player.x < 16 || v.player.x >= (v.width - 1) * 16 ||
      v.player.y < 24 || v.player.y >= (v.height - 1) * 16 ||
      !inside(v, v.home_x, v.home_y))
    return false;
  for (int i = 0; i < 4; ++i)
    v.npcs[i] = {float((58 + i * 4) * 16),
                 31 * 16.0f,
                 0,
                 0,
                 100,
                 uint8_t(i),
                 uint8_t(i % 2)};
  v.player.face = 1;
  v.invulnerable = 1000;
  s = v;
  notice(s, "世界已载入，欢迎回家。");
  return true;
}
} // namespace starsoil
