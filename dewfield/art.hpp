// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include "palette.hpp"
#include "soil.hpp"
#include "sprites.hpp"
#include "animal_sprites.hpp"
#include "mine_sprites.hpp"
#include "mines.hpp"
#include "livestock.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
namespace art {
constexpr uint16_t rgb(int r, int g, int b) {
  return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3);
}
constexpr uint16_t Ink = farm_palette::Ink, Earth = rgb(177, 119, 63),
                   Cream = farm_palette::Highlight, Gold = farm_palette::Brass,
                   Green = farm_palette::GrassDark,
                   Leaf = farm_palette::GrassLight, Dark = rgb(57, 78, 36),
                   Water = farm_palette::Water, Sky = rgb(175, 214, 183);
struct Raster {
  uint16_t *p;
  int w, h, ox = 0, oy = 0;
  const uint16_t *tint=nullptr;
  void rect(int x, int y, int ww, int hh, uint16_t c) {
    if(tint)c=tint[c];
    x -= ox;
    y -= oy;
    int ex = std::clamp(x + ww, 0, w), ey = std::clamp(y + hh, 0, h);
    for (int yy = std::max(y, 0); yy < ey; ++yy)
      std::fill(p + yy * w + std::clamp(x, 0, w), p + yy * w + ex, c);
  }
  void outline(int x, int y, int w, int h, uint16_t c) {
    rect(x, y, w, 1, c);
    rect(x, y + h - 1, w, 1, c);
    rect(x, y, 1, h, c);
    rect(x + w - 1, y, 1, h, c);
  }
  void line(int x, int y, int xx, int yy, uint16_t c) {
    int dx = std::abs(xx - x), dy = -std::abs(yy - y), sx = x < xx ? 1 : -1,
        sy = y < yy ? 1 : -1, e = dx + dy;
    for (;;) {
      rect(x, y, 1, 1, c);
      if (x == xx && y == yy)
        break;
      int v = 2 * e;
      if (v >= dy) {
        e += dy;
        x += sx;
      }
      if (v <= dx) {
        e += dx;
        y += sy;
      }
    }
  }
};
inline uint32_t hash(int x, int y) {
  uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
template <class Canvas>
inline void flower(Canvas &r, int x, int y, uint16_t c) {
  r.rect(x, y, 1, 5, Green);
  r.rect(x - 2, y, 5, 2, c);
  r.rect(x, y - 2, 1, 5, c);
  r.rect(x, y, 1, 1, Gold);
}
inline void grass(Raster &r, int x, int y, int season) {
  uint16_t base = season == 2   ? rgb(166, 169, 83)
                  : season == 3 ? rgb(181, 204, 182)
                                : rgb(145, 171, 83);
  r.rect(x, y, 16, 16, base);
  auto n = hash(x, y);
  for (int k = 0; k < 5; ++k) {
    int xx = x + int((n >> (k * 3)) % 14),
        yy = y + int((n >> (k * 2 + 4)) % 13);
    r.rect(xx, yy, 2, 1, season == 3 ? rgb(162, 187, 171) : rgb(123, 154, 73));
    r.rect(xx + 1, yy - 1, 1, 1, season == 3 ? Cream : rgb(165, 183, 93));
  }
}
inline uint16_t blend(uint16_t a, uint16_t b, int alpha) {
  int rb = (((a & 0xf81f) * (8 - alpha) + (b & 0xf81f) * alpha) >> 3) & 0xf81f;
  int g = (((a & 0x07e0) * (8 - alpha) + (b & 0x07e0) * alpha) >> 3) & 0x07e0;
  return uint16_t(rb | g);
}
inline void sprite(Raster &r, const sprites::Sprite &s, int x, int y,
                   int alpha = 8, int first_row = 0, int last_row = 255) {
  x -= r.ox;
  y -= r.oy;
  if (x >= r.w || y >= r.h || x + s.w <= 0 || y + s.h <= 0)
    return;
  for (unsigned i = 0; i < s.count; ++i) {
    const auto &span = s.spans[i];
    if (span.y < first_row)
      continue;
    if (span.y >= last_row)
      break;
    int yy = y + span.y, xx = x + span.x;
    if (yy < 0 || yy >= r.h)
      continue;
    int left = std::max(0, xx), right = std::min(r.w, xx + span.count);
    if (left >= right)
      continue;
    const uint16_t *src = s.pixels + span.offset + left - xx;
    uint16_t *dst = r.p + yy * r.w + left;
    if(r.tint) {
      for(int j=0;j<right-left;++j) dst[j]=alpha==8?r.tint[src[j]]:blend(dst[j],r.tint[src[j]],alpha);
    } else if (alpha == 8)
      std::copy_n(src, right - left, dst);
    else
      for (int j = 0; j < right - left; ++j)
        dst[j] = blend(dst[j], src[j], alpha);
  }
}
inline constexpr uint16_t ShadowTint = rgb(59, 48, 30);
inline void shadow_strip(Raster &r, int x, int y, int width, int alpha) {
  x -= r.ox;
  y -= r.oy;
  if (y < 0 || y >= r.h)
    return;
  int left = std::max(0, x), right = std::min(r.w, x + width);
  auto *row = r.p + y * r.w;
  for (int xx = left; xx < right; ++xx)
    row[xx] = blend(row[xx], ShadowTint, alpha);
}
inline void shadow_profile(Raster &r, const sprites::Shadow &profile, int x,
                           int y) {
  for (unsigned i = 0; i < profile.count; ++i) {
    const auto &span = profile.spans[i];
    shadow_strip(r, x + span.x, y + span.y, span.count, span.alpha);
  }
}
inline void contact_shadow(Raster &r, const sprites::Sprite &s, int x, int y,
                           bool mirrored = false) {
  if (!s.shadow)
    return;
  if (!mirrored)
    shadow_profile(r, *s.shadow, x - s.anchor_x, y - s.anchor_y);
  else
    for (unsigned i = 0; i < s.shadow->count; ++i) {
      const auto &span = s.shadow->spans[i];
      shadow_strip(r, x + s.anchor_x - span.x - span.count,
                   y - s.anchor_y + span.y, span.count, span.alpha);
    }
}
inline void mirrored_sprite(Raster &r, const sprites::Sprite &s, int x, int y,
                            int first_row) {
  x -= r.ox;
  y -= r.oy;
  for (unsigned i = 0; i < s.count; ++i) {
    const auto &span = s.spans[i];
    int yy = y + span.y;
    if (span.y < first_row || yy < 0 || yy >= r.h)
      continue;
    int left = x + s.w - span.x - span.count;
    for (int j = std::max(0, -left); j < span.count && left + j < r.w; ++j)
      r.p[yy * r.w + left + j] = s.pixels[span.offset + span.count - 1 - j];
  }
}
inline void grounded(Raster &r, const sprites::Sprite &s, int x, int y) {
  sprite(r, s, x - s.anchor_x, y - s.anchor_y);
}
// Three shallow rows for procedural stems and posts, centred at their actual
// floor contact. Wide diamond-shaped blobs are inappropriate for small bases.
inline void contact_patch(Raster &r, int x, int y, int width, int alpha = 2) {
  int left = x - width / 2;
  shadow_strip(r, left - 1, y - 1, width + 2, 1);
  shadow_strip(r, left, y, width, alpha);
  shadow_strip(r, left, y + 1, width, 1);
}
inline const sprites::Sprite &tree_sprite(int x, int y, int season = 0, int stage = 2, int forced_kind = -1) {
  int kind = forced_kind >= 0 ? forced_kind & 3 : farm::scenery_variant(x / 16, y / 16);
  const sprites::Sprite *seeds[] = {&sprites::tree_seed0,&sprites::tree_seed1,&sprites::tree_seed2,&sprites::tree_seed3};
  if(stage==3) return sprites::tree_stump;
  if(stage==0) return *seeds[kind];
  const sprites::Sprite *adult[4][4] = {
    {&sprites::oak,&sprites::oak_summer,&sprites::oak_fall,&sprites::oak_winter},
    {&sprites::birch,&sprites::birch_summer,&sprites::birch_fall,&sprites::birch_winter},
    {&sprites::pine,&sprites::pine,&sprites::pine,&sprites::pine},
    {&sprites::orchard,&sprites::orchard_summer,&sprites::orchard_fall,&sprites::orchard_winter}};
  const sprites::Sprite *young[4][4] = {
    {&sprites::oak_young,&sprites::oak_summer_young,&sprites::oak_fall_young,&sprites::oak_winter_young},
    {&sprites::birch_young,&sprites::birch_summer_young,&sprites::birch_fall_young,&sprites::birch_winter_young},
    {&sprites::pine_young,&sprites::pine_young,&sprites::pine_young,&sprites::pine_young},
    {&sprites::orchard_young,&sprites::orchard_summer_young,&sprites::orchard_fall_young,&sprites::orchard_winter_young}};
  return *(stage==1?young[kind][season&3]:adult[kind][season&3]);
}
inline const sprites::Sprite &rock_sprite(int x,int y) {
  const sprites::Sprite *all[]={&sprites::rocks,&sprites::rock_granite,&sprites::rock_slate,&sprites::rock_weathered};
  return *all[farm::scenery_variant(x/16,y/16)];
}
inline const sprites::Sprite &grass_sprite(int x,int y) {
  const sprites::Sprite *all[]={&sprites::grass,&sprites::clover,&sprites::daisies,&sprites::herbs};
  return *all[farm::scenery_variant(x/16,y/16)];
}
inline void tree(Raster &r, int x, int y, int variant = 0, int season = 0,
                 int alpha = 8, bool draw_shadow = true, int stage = 2, int forced_kind = -1) {
  const auto &s=tree_sprite(x,y,season,stage,forced_kind);
  int trunk=std::max(3,int(s.h)/3);
  if(draw_shadow) contact_shadow(r,s,x,y);
  sprite(r,s,x-s.anchor_x,y-s.anchor_y,alpha,0,s.h-trunk);
  sprite(r,s,x-s.anchor_x,y-s.anchor_y,8,s.h-trunk,s.h);
  (void)variant;
}
inline bool hit(const sprites::Sprite &s, int x, int y) {
  if (x < 0 || y < 0 || x >= s.w || y >= s.h)
    return false;
  for (unsigned i = 0; i < s.count; ++i) {
    auto &span = s.spans[i];
    if (span.y == y && x >= span.x && x < span.x + span.count)
      return true;
  }
  return false;
}
inline void rock(Raster &r, int x, int y) {
  contact_shadow(r, sprites::rocks, x, y);
  grounded(r, sprites::rocks, x, y);
}
inline void weed(Raster &r, int x, int y) {
  contact_shadow(r, sprites::grass, x, y);
  grounded(r, sprites::grass, x, y);
}
inline void chest(Raster &r) {
  int x = 8 * 16 + 8, y = 9 * 16 + 12;
  contact_shadow(r, sprites::chest, x, y);
  grounded(r, sprites::chest, x, y);
}
inline void well(Raster &r) {
  int x = 7 * 16 + 8, y = 8 * 16 + 12;
  contact_shadow(r, sprites::well, x, y);
  grounded(r, sprites::well, x, y);
}
inline void board(Raster &r) {
  int x = 21 * 16 + 8, y = 8 * 16 + 12;
  contact_shadow(r, sprites::board, x, y);
  grounded(r, sprites::board, x, y);
}
inline void fence(Raster &r, int x, int y, int length) {
  r.rect(x, y + 3, length, 2, rgb(119, 82, 44));
  r.rect(x, y + 9, length, 2, rgb(154, 109, 54));
  for (int i = 0; i <= length; i += 16) {
    r.rect(x + i, y, 3, 15, Ink);
    r.rect(x + i, y, 2, 13, rgb(208, 163, 86));
    r.rect(x + i, y, 2, 2, Cream);
  }
}
inline void house(Raster &r, int x, int y, bool shop = false,
                  bool draw_shadow = true) {
  if (draw_shadow)
    contact_shadow(r, sprites::cottage, x + 42, y + 61);
  grounded(r, sprites::cottage, x + 42, y + 61);
  if (shop) {
    r.rect(x + 52, y + 27, 18, 11, Ink);
    r.rect(x + 53, y + 28, 16, 9, rgb(102, 140, 85));
    r.rect(x + 59, y + 30, 4, 5, Cream);
    r.rect(x + 60, y + 31, 2, 3, Gold);
    r.rect(x + 58, y + 29, 6, 2, Leaf);
  }
}
template <class Canvas>
inline void leaf(Canvas &r, int x, int y, int flip = 1) {
  r.rect(x, y, 4, 2, rgb(48, 94, 44));
  r.rect(x + flip, y - 1, 4, 2, rgb(77, 128, 48));
  r.rect(x + flip * 2, y - 2, 3, 2, rgb(116, 160, 59));
  r.rect(x + flip * 2, y - 2, 2, 1, rgb(153, 180, 76));
}
template <class Canvas> inline void produce(Canvas &r, int x, int y, int type) {
  if (type == 0) {
    r.rect(x - 3, y - 3, 7, 5, rgb(152, 86, 71));
    r.rect(x - 2, y - 4, 5, 6, rgb(245, 213, 170));
    r.rect(x - 2, y - 3, 3, 3, rgb(255, 237, 196));
    r.rect(x - 1, y + 2, 3, 2, rgb(215, 133, 116));
    r.rect(x, y + 4, 1, 2, rgb(231, 167, 139));
    leaf(r, x - 5, y - 8);
    leaf(r, x + 2, y - 9, -1);
    r.line(x, y - 5, x + 1, y - 12, Green);
  } else if (type == 1) {
    for (int i = 0; i < 3; ++i) {
      int xx = x - 5 + i * 4, yy = y - 1 - (i % 2) * 2;
      r.rect(xx, yy, 5, 3, rgb(128, 86, 44));
      r.rect(xx + 1, yy - 1, 4, 3, rgb(209, 167, 89));
      r.rect(xx + 1, yy - 1, 2, 1, rgb(239, 198, 122));
      r.rect(xx + 3, yy, 1, 1, rgb(161, 116, 56));
    }
    leaf(r, x - 6, y - 6);
    leaf(r, x + 3, y - 7, -1);
    flower(r, x - 2, y - 12, rgb(226, 211, 182));
  } else if (type == 2) {
    for (int i = 0; i < 3; ++i) {
      int xx = x - 5 + (i % 2) * 7, yy = y - 10 + (i / 2) * 8;
      r.rect(xx, yy - 1, 4, 5, rgb(137, 56, 40));
      r.rect(xx - 1, yy, 6, 3, rgb(181, 65, 40));
      r.rect(xx, yy - 1, 4, 4, rgb(231, 96, 56));
      r.rect(xx, yy, 2, 1, rgb(255, 169, 91));
      r.rect(xx + 1, yy - 2, 2, 2, Green);
    }
  } else {
    leaf(r, x - 8, y - 3);
    leaf(r, x + 5, y - 4, -1);
    r.rect(x - 5, y - 6, 11, 8, rgb(156, 81, 34));
    r.rect(x - 4, y - 8, 9, 11, rgb(207, 113, 36));
    r.rect(x - 2, y - 9, 5, 12, rgb(232, 151, 46));
    r.rect(x - 5, y - 4, 1, 4, rgb(241, 161, 48));
    r.rect(x - 3, y - 6, 1, 7, rgb(250, 181, 64));
    r.rect(x + 1, y - 7, 1, 9, rgb(186, 91, 29));
    r.rect(x + 4, y - 5, 1, 5, rgb(173, 87, 29));
    r.rect(x - 1, y - 11, 3, 3, rgb(74, 108, 45));
    r.rect(x, y - 12, 3, 2, rgb(108, 139, 53));
  }
}

// A leaf grows from (x,y); its stem and stepped blade remain connected.
inline void crop_leaf(Raster &r, int x, int y, int side) {
  constexpr int left[] = {3, 1, 1, 0}, right[] = {4, 5, 4, 2};
  for (int row = 0; row < 4; ++row) {
    int a = left[row], b = right[row];
    r.rect(x + (side > 0 ? a : -b), y - 3 + row, b - a + 1, 1,
           rgb(58, 91, 39));
  }
  r.rect(x + (side > 0 ? 2 : -4), y - 2, 3, 1, rgb(101, 140, 52));
  r.rect(x + (side > 0 ? 3 : -4), y - 3, 2, 1, rgb(148, 169, 72));
  r.line(x, y, x + side * 4, y - 2, rgb(81, 119, 45));
}
inline void crop_tomato(Raster &r, int x, int y) {
  r.rect(x - 2, y - 1, 5, 3, rgb(131, 57, 36));
  r.rect(x - 1, y - 2, 3, 4, rgb(206, 76, 40));
  r.rect(x - 1, y - 1, 2, 1, rgb(242, 135, 65));
  r.rect(x - 1, y - 3, 3, 1, rgb(75, 109, 40));
}
inline void crop(Raster &r, int x, int y, int type, int stage, bool ready,
                 bool wet = false) {
  const auto &earth = soil::colors(wet);
  constexpr uint16_t stem = rgb(63, 94, 39);
  r.rect(x - 1, y - 3, 2, 3, stem);
  if (stage == 0 && !ready) {
    r.rect(x - 3, y - 4, 3, 2, rgb(96, 137, 48));
    r.rect(x + 1, y - 5, 3, 2, rgb(105, 145, 51));
    r.rect(x - 3, y - 4, 2, 1, rgb(153, 173, 76));
    r.rect(x + 2, y - 5, 2, 1, rgb(153, 173, 76));
  } else if (stage == 1 && !ready) {
    r.rect(x - 1, y - 7, 2, 7, stem);
    crop_leaf(r, x, y - 3, -1);
    crop_leaf(r, x, y - 5, 1);
  } else if (type == 0) {
    // Turnip: a leafy crown and only the shoulder of the root above soil.
    crop_leaf(r, x, y - 4, -1);
    crop_leaf(r, x, y - 4, 1);
    crop_leaf(r, x, y - 7, -1);
    crop_leaf(r, x, y - 8, 1);
    r.rect(x - 1, y - 8, 2, 7, stem);
    if (ready) {
      r.rect(x - 3, y - 4, 7, 3, rgb(161, 101, 84));
      r.rect(x - 2, y - 5, 5, 4, rgb(236, 215, 170));
      r.rect(x - 2, y - 4, 2, 2, rgb(250, 233, 193));
      r.rect(x + 1, y - 3, 2, 2, rgb(205, 159, 133));
      r.rect(x - 2, y - 1, 5, 1, earth.clod);
    }
  } else if (type == 1) {
    // Potatoes stay underground. The planted crop is a branching leafy bush.
    r.rect(x - 1, y - 10, 2, 10, stem);
    crop_leaf(r, x - 1, y - 3, -1);
    crop_leaf(r, x + 1, y - 4, 1);
    crop_leaf(r, x, y - 7, -1);
    crop_leaf(r, x, y - 9, 1);
    if (ready) {
      r.line(x - 1, y - 6, x - 3, y - 10, stem);
      r.rect(x - 4, y - 11, 3, 1, rgb(235, 218, 164));
      r.rect(x - 3, y - 12, 1, 3, rgb(235, 218, 164));
      r.rect(x - 3, y - 11, 1, 1, rgb(188, 142, 64));
    }
  } else if (type == 2) {
    // Tomato: tied to a planted stake, with fruit attached to vine branches.
    r.rect(x + 3, y - 19, 2, 19, rgb(112, 79, 44));
    r.rect(x + 4, y - 18, 1, 17, rgb(178, 132, 69));
    r.rect(x - 1, y - 15, 2, 15, stem);
    crop_leaf(r, x, y - 5, -1);
    crop_leaf(r, x, y - 10, 1);
    crop_leaf(r, x, y - 14, -1);
    r.rect(x, y - 11, 5, 1, rgb(179, 147, 85));
    if (ready) {
      r.line(x, y - 12, x - 4, y - 10, stem);
      crop_tomato(r, x - 4, y - 8);
      crop_tomato(r, x + 3, y - 12);
      r.line(x, y - 7, x + 2, y - 6, stem);
      crop_tomato(r, x + 2, y - 4);
    } else {
      r.rect(x - 4, y - 11, 3, 1, rgb(218, 176, 73));
      r.rect(x - 3, y - 12, 1, 3, rgb(218, 176, 73));
    }
  } else {
    // Pumpkin: a spreading vine and a heavy fruit resting directly on soil.
    r.rect(x, y - 8, 1, 8, stem);
    r.line(x, y - 1, x - 4, y - 4, stem);
    r.line(x, y - 1, x + 4, y - 5, stem);
    crop_leaf(r, x - 2, y - 4, -1);
    crop_leaf(r, x + 2, y - 5, 1);
    crop_leaf(r, x, y - 8, -1);
    if (ready) {
      r.rect(x + 1, y - 10, 2, 4, rgb(74, 98, 41));
      r.rect(x - 2, y - 8, 6, 8, rgb(177, 96, 37));
      r.rect(x - 4, y - 7, 9, 6, rgb(199, 113, 39));
      r.rect(x - 5, y - 6, 11, 4, rgb(187, 101, 34));
      r.rect(x - 3, y - 7, 6, 6, rgb(221, 139, 48));
      r.rect(x - 2, y - 7, 1, 5, rgb(244, 173, 71));
      r.rect(x + 1, y - 6, 1, 5, rgb(180, 95, 35));
      r.rect(x + 4, y - 5, 1, 3, rgb(153, 80, 32));
      r.rect(x - 3, y - 1, 7, 1, rgb(152, 86, 36));
    }
  }
  // The last stem pixel takes the earth's reflected color; no exposed roots.
  if (!(ready && (type == 0 || type == 3)))
    r.rect(x - 1, y - 1, 2, 1, earth.clod);
}
inline const sprites::Sprite &person_sprite(int dir, int frame,
                                            bool npc = false) {
  const sprites::Sprite *elder[]={&sprites::villager_3_2,&sprites::villager_3_1,&sprites::villager_3_0,&sprites::villager_3_3};
  const sprites::Sprite *standing[] = {&sprites::north, &sprites::east,
                                       &sprites::south, &sprites::west};
  const sprites::Sprite *walking[4][4] = {
      {&sprites::north0, &sprites::north1, &sprites::north2, &sprites::north3},
      {&sprites::east0, &sprites::east1, &sprites::east2, &sprites::east3},
      {&sprites::south0, &sprites::south1, &sprites::south2, &sprites::south3},
      {&sprites::west0, &sprites::west1, &sprites::west2, &sprites::west3}};
  return *(npc     ? elder[dir&3]
           : frame ? walking[dir & 3][(frame - 1) & 3]
                   : standing[dir & 3]);
}
#include "world_art.hpp"
#include "gait.hpp"
#include "livestock_art.hpp"
#include "mine_art.hpp"
inline void person_shadow(Raster &r, int x, int y, int dir, int frame,
                          bool npc = false) {
  if(npc)gait::shadow(r,x,y,dir,frame,&villager_rig(3,dir));
  else if (frame)
    gait::shadow(r, x, y, dir, frame);
  else
    contact_shadow(r, person_sprite(dir, 0, npc), x, y);
}
#include "tool_animation.hpp"
inline void carry_item(Raster&r,int x,int y,int item,int selected) {
  if(item==2 || (item>=8 && item<=11)) {
    sprite(r,sprites::seed_handbag,x-sprites::seed_handbag.w/2,y-30-sprites::seed_handbag.h);
    (void)selected;
  } else if(item>=12 && item<=15) {
    constexpr int bottom[]={5,1,2,2};
    produce(r,x,y-31-bottom[item-12],item-12);
  }
  else if(item>=16 && item<=18) {
    const sprites::Sprite *items[]={&sprites::fish,&sprites::logs,&sprites::stones};
    const auto &s=*items[item-16];sprite(r,s,x-s.w/2,y-30-s.h);
  } else if (item >= 19 && item <= 45) food(r,x,y-31,item);
}
inline void person(Raster &r, int x, int y, int dir, int frame,
                   bool npc = false, int swing = 0, int tool = 0,
                   bool draw_shadow = true,int item = 0,int grade = 0,int selected = 0,bool fishing = false,tool_anim::Target target={}) {
  if(draw_shadow) person_shadow(r,x,y,dir,frame,npc);
  if(npc) {gait::draw(r,x,y,dir,frame,false,&villager_rig(3,dir));return;}
  if(swing) tool_anim::use(r,x,y,dir,frame,tool,swing,grade,fishing,target);
  else if(item==37){tool_anim::use(r,x,y,dir,frame,7,0,grade,false);}
  else if(item==2 || item>=8) {
    gait::draw(r,x,y,dir,frame,true);
    carry_item(r,x,y,item,selected);
  } else if(item && item!=4) {
    tool_anim::use(r,x,y,dir,frame,item-1,0,grade,fishing);
  } else if(frame) gait::draw(r,x,y,dir,frame);
  else grounded(r,person_sprite(dir,0),x,y);
}
inline void fence_shadow(Raster &r, int x, int y, int length) {
  for (int i = 0; i <= length; i += 16)
    contact_patch(r, x + i + 1, y + 15, 3, 2);
}
inline void crop_shadow(Raster &r, int x, int y, int type, int stage,
                        bool ready) {
  contact_patch(r, x, y, ready && type == 3 ? 7 : ready && type == 0 ? 5 : 2);
  if (type == 2 && stage >= 2)
    contact_patch(r, x + 4, y, 2);
}

inline void chicken_shadow(Raster &r, int x, int y, int frame) {
  shadow_strip(r, x - 3, y - 1, 9, 1);
  // The left planted foot is always on the ground; the right alternates.
  // Disjoint spans avoid darkening the same floor pixel twice.
  shadow_strip(r, x - 3, y, 1, 1);
  shadow_strip(r, x - 2, y, 1, 3);
  shadow_strip(r, x - 1, y, 3, 1);
  shadow_strip(r, x + 2, y, 1, (frame & 1) ? 3 : 1);
  shadow_strip(r, x + 3, y, 2, 1);
}
inline void chicken(Raster &r, int x, int y, int f) {
  y -= 3; // Feet end at ground y-1, matching the character/prop convention.
  r.rect(x - 3, y - 4, 7, 5, Cream);
  r.rect(x + 2, y - 7, 4, 5, rgb(255, 245, 205));
  r.rect(x + 3, y - 8, 2, 2, rgb(199, 79, 46));
  r.rect(x + 4, y - 6, 1, 1, Ink);
  r.rect(x + 6, y - 4, 2, 1, Gold);
  r.rect(x - 2, y + 1, 1, 2, Gold);
  r.rect(x + 2, y + f % 2, 1, 2, Gold);
}
} // namespace art
