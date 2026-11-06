// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include "core.hpp"
#include "world.hpp"
#include <algorithm>
#include <cstdint>
namespace farm_scene {
enum class Kind : uint8_t {
  Tree,
  Rock,
  Grass,
  House,
  Well,
  Chest,
  Board,
  Fence,
  Crop,
  Sprinkler,
  Villager,
  Chicken,
  Player,
  WorldNpc,
  Bed,
  Table,
  Forage
};
struct Rect {
  int x, y, w, h;
  bool intersects(const Rect &b) const {
    return x < b.x + b.w && x + w > b.x && y < b.y + b.h && y + h > b.y;
  }
};
struct Entry {
  Kind kind;
  int16_t x, y, depth;
  uint16_t order;
  uint8_t id;
  bool interactive;
  uint8_t growth = 2;
};
struct View {
  int direction = 2, frame = 0, swing = 0, tool = 0, season = 0;
  uint32_t animation = 0;
  int held_item = 0, grade = 0, seed = 0, hit_id = -1;
  uint32_t hit_ms = 0;
  bool fishing = false;
};
struct Placement {
  const sprites::Sprite *sprite = nullptr;
  int x = 0, y = 0;
};
// One physical foot anchor feeds culling, painting, contact and picking.
// Houses keep their map origin while their front step is the ground anchor.
inline Placement placement(const Entry &e) {
  switch (e.kind) {
  case Kind::WorldNpc:
    return {&art::villager_walking_sprite(e.id % 4, e.id / 4, e.growth), e.x,
            e.y};
  case Kind::Forage: {
    const sprites::Sprite *items[] = {&sprites::logs, &sprites::stones,
                                      &sprites::fish, &sprites::seeds};
    auto &f = farm::Foraging[e.id];
    auto *s = items[f.item == 17 ? 0 : f.item == 18 ? 1 : f.item == 16 ? 2 : 3];
    return {s, e.x - s->w / 2, e.y - s->h};
  }
  case Kind::Tree:
    return {&art::tree_sprite(e.x, e.y, 0, e.growth), e.x, e.y};
  case Kind::Rock:
    return {&art::rock_sprite(e.x, e.y), e.x, e.y};
  case Kind::Grass:
    return {&art::grass_sprite(e.x, e.y), e.x, e.y};
  case Kind::House:
    return {&sprites::cottage, e.x + 42, e.y + 61};
  case Kind::Well:
    return {&sprites::well, e.x, e.y};
  case Kind::Chest:
    return {&sprites::chest, e.x, e.y};
  case Kind::Board:
    return {&sprites::board, e.x, e.y};
  case Kind::Sprinkler:
    return {&sprites::sprinkler, e.x, e.y};
  case Kind::Villager:
    return {&sprites::villager, e.x, e.y};
  default:
    return {};
  }
}
inline Rect bounds(const Entry &e) {
  auto p = placement(e);
  if (p.sprite)
    return {p.x - p.sprite->anchor_x - 1, p.y - p.sprite->anchor_y,
            p.sprite->w + 2, p.sprite->h + 2};
  switch (e.kind) {
  case Kind::Bed:
    return {e.x - 18, e.y - 36, 36, 38};
  case Kind::Table:
    return {e.x - 19, e.y - 21, 38, 23};
  case Kind::Fence:
    return {e.x - 1, e.y, 53, 17};
  case Kind::Crop:
    return {e.x - 11, e.y - 22, 24, 25};
  case Kind::Chicken:
    return {e.x - 4, e.y - 12, 13, 14};
  case Kind::Player:
    return {e.x - 30, e.y - 60, 64, 84};
  default:
    return {0, 0, 0, 0};
  }
}
inline bool faded(const Entry &e, const farm::State &s) {
  if (e.kind != Kind::Tree || s.py >= e.depth)
    return false;
  auto p = placement(e);
  int item = s.active_slot < farm::HotbarSize ? s.slots[s.active_slot] : 0;
  bool carrying = item == 2 || item >= 8;
  int height = carrying ? 54 : 28;
  return Rect{int(s.px) - 10, int(s.py) - height, 20, height - 4}.intersects(
      {p.x - p.sprite->anchor_x + 1, p.y - p.sprite->anchor_y + 1,
       p.sprite->w - 2, p.sprite->h - std::max(3, int(p.sprite->h) / 3)});
}
struct Queue {
  static constexpr unsigned Capacity = 288;
  Entry entries[Capacity]{};
  unsigned count = 0;
  Rect viewport{0, 0, farm::MapW * 16, farm::MapH * 16};
  void add(Kind kind, int x, int y, int depth, int id = 0,
           bool interactive = false, int growth = 2) {
    Entry e{kind,
            int16_t(x),
            int16_t(y),
            int16_t(depth),
            uint16_t(next_order++),
            uint8_t(id),
            interactive,
            uint8_t(growth)};
    if (!bounds(e).intersects(viewport))
      return;
    // Capacity covers even a completely planted and irrigated full map.
    if (count >= Capacity)
      __builtin_trap();
    entries[count++] = e;
  }
  void sort() {
    std::sort(entries, entries + count, [](const Entry &a, const Entry &b) {
      return a.depth != b.depth ? a.depth < b.depth : a.order < b.order;
    });
  }
  void build(const farm::State &s, const View &v, Rect clip) {
    count = 0;
    next_order = 0;
    viewport = clip;
    if (s.region) {
      build_region(s, v);
      sort();
      return;
    }
    // Stable tie ordering is shared by rendering and picking.
    for (int y : {8 * 16, 17 * 16 + 7})
      for (int x : {10 * 16, 16 * 16})
        add(Kind::Fence, x, y, y + 15);
    for (int y : {14, 20})
      for (int x = 24; x < 30; x += 2) {
        add(Kind::Rock, x * 16, y * 16, y * 16);
        add(Kind::Grass, x * 16 + 10, y * 16 + 2, y * 16 + 2);
      }
    for (int x = 1; x < farm::MapW; x += 2) {
      add(Kind::Tree, x * 16, 29, 29, x);
      add(Kind::Tree, x * 16, farm::MapH * 16 - 2, farm::MapH * 16 - 2, x + 2);
    }
    for (int y = 4; y < farm::MapH - 2; y += 3) {
      add(Kind::Tree, 14, y * 16, y * 16, y);
      add(Kind::Tree, farm::MapW * 16 - 8, y * 16, y * 16, y + 1);
    }
    add(Kind::House, 4 * 16, 3 * 16, 3 * 16 + 61, 0, true);
    add(Kind::House, 23 * 16, 3 * 16, 3 * 16 + 61, 1, true);
    add(Kind::Well, 7 * 16 + 8, 8 * 16 + 12, 8 * 16 + 12, 0, true);
    add(Kind::Chest, 8 * 16 + 8, 9 * 16 + 12, 9 * 16 + 12, 0, true);
    add(Kind::Board, 21 * 16 + 8, 8 * 16 + 12, 8 * 16 + 12, 0, true);
    for (int i = 0; i < farm::PlotCount; ++i) {
      int x = (farm::PlotX + i % farm::PlotW) * 16 + 8,
          y = (farm::PlotY + i / farm::PlotW) * 16;
      if (s.plots[i].crop)
        add(Kind::Crop, soil::root_x(i), soil::root_y(i), soil::root_y(i), i);
      if (s.sprinklers[i / 8] & (1u << (i % 8)))
        add(Kind::Sprinkler, x, y + 15, y + 15, i);
    }
    for (int i = 0; i < 24; ++i) {
      auto &o = farm::Obstacles[i];
      int plot = farm::plot_index(o.x, o.y);
      bool stump = o.kind == 0 && s.cleared[i] && s.tree_age[i] < 2 &&
                   (plot < 0 || !s.plots[plot].tilled);
      if (s.cleared[i] && !stump)
        continue;
      int x = o.x * 16 + 8, y = o.y * 16 + (o.kind ? 12 : 13);
      add(o.kind == 0   ? Kind::Tree
          : o.kind == 1 ? Kind::Rock
                        : Kind::Grass,
          x, y, y, i, !stump, o.kind == 0 ? farm::tree_stage(s, i) : 2);
    }
    add(Kind::Villager, 22 * 16 + 5, 9 * 16 + 11, 9 * 16 + 11, 0, true);
    add(Kind::Chicken, 11 * 16 + int((v.animation / 4) % 12), 19 * 16 + 8,
        19 * 16 + 8, 0);
    add(Kind::Chicken, 14 * 16, 19 * 16 + 12, 19 * 16 + 12, 1);
    add(Kind::Player, s.px, s.py, s.py);
    sort();
  }
  void build_region(const farm::State &s, const View &v) {
    using farm::Region;
    if (s.region == uint8_t(Region::Home)) {
      add(Kind::Bed, 10 * 16 + 8, 9 * 16 + 15, 9 * 16 + 15, 0, true);
      add(Kind::Table, 19 * 16 + 8, 15 * 16 + 15, 15 * 16 + 15);
    } else {
      for (int x = 1; x < 32; x += 2) {
        add(Kind::Tree, x * 16, 29, 29);
        add(Kind::Tree, x * 16, 382, 382);
      }
      for (int y = 4; y < 22; y += 3) {
        if (y != 12) {
          add(Kind::Tree, 14, y * 16, y * 16);
          add(Kind::Tree, 504, y * 16, y * 16);
        }
      }
      if (s.region == uint8_t(Region::Town)) {
        if (s.story >= 5)
          add(Kind::Table, 15 * 16 + 8, 15 * 16, 15 * 16);
        add(Kind::House, 5 * 16, 3 * 16, 3 * 16 + 61, 1, true);
        add(Kind::House, 22 * 16, 3 * 16, 3 * 16 + 61, 2, true);
        add(Kind::Well, 16 * 16 + 8, 10 * 16 + 12, 10 * 16 + 12);
        for (int x : {11, 19}) {
          add(Kind::Grass, x * 16, 15 * 16, 15 * 16);
          add(Kind::Grass, x * 16, 9 * 16, 9 * 16);
        }
        for (int x : {10, 18})
          add(Kind::Fence, x * 16, 16 * 16, 16 * 16 + 15);
      } else if (s.region == uint8_t(Region::Forest)) {
        for (int y = 5; y <= 20; y += 5)
          for (int x = 6; x <= 24; x += 6)
            if (x != 12)
              add(Kind::Tree, x * 16 + 8, y * 16 + 13, y * 16 + 13);
        for (int i = 0; i < 9; ++i)
          add(Kind::Grass, (5 + i * 3) * 16 + 8, (i % 2 ? 17 : 8) * 16 + 12,
              (i % 2 ? 17 : 8) * 16 + 12);
      } else if (s.region == uint8_t(Region::Lake)) {
        for (int y : {6, 18})
          for (int x : {22, 27}) {
            add(Kind::Tree, x * 16, y * 16, y * 16);
            add(Kind::Grass, x * 16 + 8, y * 16 + 12, y * 16 + 12);
          }
        for (int x : {6, 12, 18})
          add(Kind::Rock, x * 16, 21 * 16, 21 * 16);
      }
    }
    for (unsigned i = 0; i < sizeof(farm::Foraging) / sizeof(*farm::Foraging);
         ++i) {
      auto &f = farm::Foraging[i];
      if (unsigned(f.region) == s.region && !(s.forage & (1u << i)))
        add(Kind::Forage, f.x * 16 + 8, f.y * 16 + 13, f.y * 16 + 13, i, true);
    }
    for (int id = 0; id < 4; ++id) {
      auto p = farm::npc_pose(s, id);
      if (unsigned(p.region) == s.region)
        add(Kind::WorldNpc, p.x, p.y, p.y, id + p.dir * 4, true,
            p.walking ? 1 + v.animation % 4 : 0);
    }
    add(Kind::Player, s.px, s.py, s.py);
    (void)v;
  }
  void shadows(art::Raster &r, const farm::State &s, const View &v = {}) const {
    for (unsigned i = 0; i < count; ++i) {
      const auto &e = entries[i];
      switch (e.kind) {
      case Kind::Player:
        art::person_shadow(r, e.x, e.y, v.direction, v.frame);
        break;
      case Kind::Chicken:
        art::chicken_shadow(r, e.x, e.y, v.animation / 2 + e.id);
        break;
      case Kind::Crop: {
        const auto &p = s.plots[e.id];
        const auto &c = farm::Crops[p.crop - 1];
        int stage = p.growth == 0        ? 0
                    : p.growth >= c.days ? 3
                                         : p.growth * 3 / c.days;
        art::crop_shadow(r, e.x, e.y, p.crop - 1, stage, p.growth >= c.days);
        break;
      }
      case Kind::Fence:
        art::fence_shadow(r, e.x, e.y, 48);
        break;
      default: {
        auto p = placement(e);
        if (p.sprite)
          art::contact_shadow(r, *p.sprite, p.x, p.y);
        break;
      }
      }
    }
  }
  void objects(art::Raster &r, const farm::State &s, const View &v) const {
    for (unsigned i = 0; i < count; ++i) {
      auto &e = entries[i];
      switch (e.kind) {
      case Kind::WorldNpc: {
        auto p = placement(e);
        art::grounded(r, *p.sprite, p.x, p.y);
        break;
      }
      case Kind::Bed:
        art::world_bed(r, e.x, e.y);
        break;
      case Kind::Table:
        art::world_table(r, e.x, e.y);
        break;
      case Kind::Forage: {
        auto p = placement(e);
        art::sprite(r, *p.sprite, p.x, p.y);
        break;
      }
      case Kind::Tree:
        art::tree(r, e.x, e.y, e.id, v.season, faded(e, s) ? 4 : 8, false,
                  e.growth);
        break;
      case Kind::Rock:
      case Kind::Grass:
      case Kind::Well:
      case Kind::Chest:
      case Kind::Board: {
        auto p = placement(e);
        art::grounded(r, *p.sprite, p.x, p.y);
        break;
      }
      case Kind::House:
        art::house(r, e.x, e.y, e.id == 1, false);
        break;
      case Kind::Fence:
        art::fence(r, e.x, e.y, 48);
        break;
      case Kind::Crop: {
        auto &p = s.plots[e.id];
        auto &c = farm::Crops[p.crop - 1];
        art::crop(r, e.x, e.y, p.crop - 1,
                  p.growth == 0        ? 0
                  : p.growth >= c.days ? 3
                                       : p.growth * 3 / c.days,
                  p.growth >= c.days, p.wet);
        break;
      }
      case Kind::Sprinkler:
        art::grounded(r, sprites::sprinkler, e.x, e.y);
        break;
      case Kind::Villager:
        art::person(r, e.x, e.y, 2, 0, true, 0, 0, false);
        break;
      case Kind::Chicken:
        art::chicken(r, e.x, e.y, v.animation / 2 + e.id);
        break;
      case Kind::Player:
        art::person(r, e.x, e.y, v.direction, v.frame, false, v.swing, v.tool,
                    false, v.held_item, v.grade, v.seed, v.fishing);
        break;
      }
      if ((e.kind == Kind::Tree || e.kind == Kind::Rock ||
           e.kind == Kind::Grass) &&
          e.interactive && e.id < 24 && v.hit_ms && v.hit_id == e.id &&
          s.damage[e.id]) {
        int hp = farm::obstacle_hp(e.id);
        if (e.kind == Kind::Tree && e.growth < 2)
          hp = e.growth == 0 ? 1 : 3;
        int yy = e.y - (e.kind == Kind::Tree  ? 15
                        : placement(e).sprite ? placement(e).sprite->h + 4
                                              : 14);
        r.rect(e.x - 6, yy, 12, 3, art::Ink);
        r.rect(e.x - 5, yy + 1, std::max(1, 10 * (hp - s.damage[e.id]) / hp), 1,
               farm_palette::Brass);
      }
    }
  }
  bool pick(int x, int y, const farm::State &s, int &tx, int &ty) const {
    for (unsigned at = count; at > 0; --at) {
      auto &e = entries[at - 1];
      if (!e.interactive)
        continue;
      auto p = placement(e);
      if (e.kind == Kind::Bed && bounds(e).intersects({x, y, 1, 1})) {
        tx = 10;
        ty = 9;
        return true;
      }
      if (!p.sprite)
        continue;
      if (e.kind == Kind::Tree && faded(e, s) &&
          y < p.y - std::max(3, int(p.sprite->h) / 3))
        continue;
      if (!art::hit(*p.sprite, x - p.x + p.sprite->anchor_x,
                    y - p.y + p.sprite->anchor_y))
        continue;
      if (e.kind == Kind::WorldNpc) {
        tx = e.x / 16;
        ty = e.y / 16;
      } else if (e.kind == Kind::Forage) {
        tx = farm::Foraging[e.id].x;
        ty = farm::Foraging[e.id].y;
      } else if (e.kind == Kind::House) {
        tx = s.region ? (e.id == 1 ? 7 : 24) : e.id ? 25 : 6;
        ty = 7;
      } else if (e.kind == Kind::Villager || e.kind == Kind::Board) {
        tx = 21;
        ty = 8;
      } else if (e.kind == Kind::Well) {
        tx = 7;
        ty = 8;
      } else if (e.kind == Kind::Chest) {
        tx = 8;
        ty = 9;
      } else {
        auto &o = farm::Obstacles[e.id];
        tx = o.x;
        ty = o.y;
      }
      return true;
    }
    return false;
  }

private:
  uint16_t next_order = 0;
};
} // namespace farm_scene
