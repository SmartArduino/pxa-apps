// SPDX-License-Identifier: MIT
#include "actor_renderer.hpp"
#include "art.hpp"
#include "background.hpp"
#include "core.hpp"
#include "effects.hpp"
#include "font.hpp"
#include "runtime.hpp"
#include "terrain.hpp"
#include "text.hpp"
#include "trees.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace {
using namespace starsoil;
using pocket::Key;
using pocket::Phase;
using pocket::rgb;
constexpr uint16_t Ink = rgb(225, 226, 204), Muted = rgb(143, 163, 162),
                   GoldInk = rgb(229, 192, 120), Dark = rgb(23, 34, 44),
                   Panel = rgb(36, 51, 59), Teal = rgb(113, 184, 166),
                   Red = rgb(203, 117, 119);
State s;
Effects effects;
int draw_x = 0, draw_y = 0;
int width() { return pocket::WIDTH; }
int height() { return pocket::HEIGHT; }
int scene_bottom() { return height() - 43; }
int content_x() { return (width() - 288) / 2; }
int content_y() { return (height() - 224) / 2; }
void content_origin() {
  draw_x = content_x();
  draw_y = content_y();
}
void footer_origin() {
  draw_x = content_x();
  draw_y = height() - 224;
}
enum View { Title, World, Bag, Crafting, Gear, Map, Guide, Dialogue, Storage,
            CreateWorld, SeedEditor, DeleteWorld };
constexpr int RecipesPerPage = 4, RecipeRowHeight = 32;
View view = Title;
uint32_t animation = 0, autosave = 0, event_at = 0, last_event = 0, paint = 0;
int cx = 0, cy = 0, selected = 0, recipe = 0, recipe_page = 0, guide_page = 0,
    npc = 0;
int touch = -1, aim_x = -1, aim_y = -1;
bool touch_down = false, jump_edge = false, gesture_jump = false,
     use_blocked = false;
bool save_error = false;
bool preview_face = true;
bool session_active = false;
starsoil_actor::ActionClock tool_animation;
char message[128]{};
const char *tr(const char *en, const char *cn) { return pocket::tr(en, cn); }
const char *hint(const char *touch_en, const char *touch_cn,
                 const char *pad_en, const char *pad_cn) {
  return pocket::using_controller() ? tr(pad_en, pad_cn)
                                    : tr(touch_en, touch_cn);
}
uint16_t color(uint32_t c) { return rgb(c >> 16, (c >> 8) & 255, c & 255); }
uint16_t dim(uint16_t c, int light) {
  if (light >= 255)
    return c;
  return uint16_t(((((c >> 11) & 31) * light / 255) << 11) |
                  ((((c >> 5) & 63) * light / 255) << 5) |
                  ((c & 31) * light / 255));
}
uint16_t mix(uint16_t a, uint16_t b, int k) {
  unsigned ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31,
           br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return uint16_t(((ar * (255 - k) + br * k) / 255) << 11 |
                  ((ag * (255 - k) + bg * k) / 255) << 5 |
                  (ab * (255 - k) + bb * k) / 255);
}
void rect(int x, int y, int w, int h, uint16_t c) {
  pocket::rect(x + draw_x, y + draw_y, w, h, c);
}
void line(int x, int y, int xx, int yy, uint16_t c) {
  pocket::line(x + draw_x, y + draw_y, xx + draw_x, yy + draw_y, c);
}
void outline(int x, int y, int w, int h, uint16_t c) {
  pocket::outline(x + draw_x, y + draw_y, w, h, c);
}
void pixel(int x, int y, uint16_t c) {
  pocket::pixel(x + draw_x, y + draw_y, c);
}
void label(int x, int y, const char *txt, uint16_t c = Ink, int scale = 1) {
  pocket::text(x + draw_x, y + draw_y, txt, c, scale);
}
int text_width(const char *txt, int scale = 1) {
  return pocket::text_width(txt, scale);
}
void center(int x, int y, int w, const char *txt, uint16_t c = Ink,
            int scale = 1) {
  char fitted[256]{};
  if (text_width(txt, scale) > w - 4) {
    size_t size = 0;
    while (*txt) {
      const char *start = txt;
      pocket_ui::next_utf8(txt);
      size_t n = size_t(txt - start);
      if (size + n + 4 >= sizeof(fitted)) break;
      std::memcpy(fitted + size, start, n);
      std::memcpy(fitted + size + n, "…", 4);
      if (text_width(fitted, scale) > w - 4) {
        std::memcpy(fitted + size, "…", 4);
        break;
      }
      size += n;
    }
    txt = fitted;
  }
  label(x + (w - text_width(txt, scale)) / 2, y, txt, c, scale);
}
void wrap(int x, int y, int width, const char *txt, uint16_t c = Muted,
          int maxlines = 4) {
  char line[256]{};
  size_t used = 0;
  int row = 0;
  while (txt && *txt) {
    const char *start = txt;
    uint32_t cp = pocket_ui::next_utf8(txt);
    size_t n = size_t(txt - start);
    if (used + n + 1 >= sizeof(line)) break;
    std::memcpy(line + used, start, n);
    line[used + n] = 0;
    if (cp == '\n' || pocket::text_width(line) > width) {
      size_t total = used + n, split = used;
      if (cp != '\n')
        for (size_t i = 1; i < total; ++i)
          if (line[i] == ' ') split = i;
      char tail[256]{};
      size_t remaining = 0;
      if (cp != '\n') {
        size_t next = split == used && line[split] != ' ' ? used : split + 1;
        remaining = total - next;
        std::memcpy(tail, line + next, remaining);
      }
      line[split] = 0;
      label(x, y, line, c);
      y += pocket::text_line_height();
      if (++row >= maxlines)
        return;
      std::memcpy(line, tail, remaining + 1);
      used = remaining;
      continue;
    }
    used += n;
    line[used] = 0;
  }
  if (used) label(x, y, line, c);
}
void sprite(int x, int y, const uint16_t *data, int w, int h, bool flip = false,
            int light = 255, int scale = 1, bool clip = false) {
  for (int yy = 0; yy < h; ++yy) {
    int py = y + yy * scale;
    if (clip && (py < 25 || py >= scene_bottom()))
      continue;
    for (int xx = 0; xx < w; ++xx) {
      uint16_t c = data[yy * w + (flip ? w - 1 - xx : xx)];
      if (c)
        rect(x + xx * scale, py, scale, scale, dim(c, light));
    }
  }
}
void icon(int x, int y, int id, int light = 255) {
  if (id > 0 && id < ItemCount)
    sprite(x, y, art::icons[id], 16, 16, false, light);
}
void panel(int x, int y, int w, int h, uint16_t fill = Panel) {
  rect(x + 1, y + 2, w, h, Dark);
  rect(x, y, w, h, fill);
  outline(x, y, w, h, rgb(78, 94, 96));
  pixel(x, y, GoldInk);
  pixel(x + w - 1, y, GoldInk);
}
void button(int x, int y, int w, const char *txt, bool active = false) {
  panel(x, y, w, 20, active ? rgb(63, 92, 85) : Panel);
  center(x, y + (20 - pocket::text_height()) / 2, w, txt, active ? GoldInk : Ink);
}
void sound(int pitch) { pocket::tone(float(pitch), .045f); }
void notify(const char *en, const char *cn) {
  std::snprintf(message, sizeof(message), "%s", tr(en, cn));
  event_at = animation + 4200;
}
void set_view(View v) {
  view = v;
  touch = -1;
  touch_down = false;
  gesture_jump = false;
  aim_x = aim_y = -1;
  s.mining = 0;
  selected = v == Gear ? 0 : s.selected;
  use_blocked = pocket::held(Key::B);
  sound(430);
  pocket::redraw();
}
int light_at(int x, int y) {
  int surface = s.heights[std::clamp(x, 0, s.width - 1)];
  bool night = s.clock > 240000 || s.clock < 25000;
  int light =
      y <= surface ? night ? 112 : 255 : std::max(28, 190 - (y - surface) * 23);
  int px = int(s.player.x / 16), py = int(s.player.y / 16);
  int player_light = s.equip[7] == GlowCharm                       ? 8
                     : items[s.bag[s.selected].item].tile == Torch ? 7
                                                                   : 4;
  light = std::max(light, 230 - (std::abs(x - px) + std::abs(y - py)) * 255 /
                                    player_light);
  for (int yy = std::max(0, y - 5); yy <= std::min(s.height - 1, y + 5); ++yy)
    for (int xx = std::max(0, x - 5); xx <= std::min(s.width - 1, x + 5); ++xx)
      if (s.tiles[yy * WorldW + xx] == Torch ||
          s.tiles[yy * WorldW + xx] == Portal ||
          s.tiles[yy * WorldW + xx] == Lava)
        light =
            std::max(light, 255 - (std::abs(x - xx) + std::abs(y - yy)) * 38);
  return std::clamp(light, 28, 255);
}
void human(int x, int feet, int frame, bool face, const uint8_t *equipment,
           int held, int light = 255, int scale = 1, int skin = 0,
           bool using_item = false, int action_frame = -1) {
  starsoil_actor::Appearance a{x,         feet,         frame, scale,
                               light,     skin,         face,  using_item,
                               equipment, uint8_t(held)};
  a.action_frame = action_frame;
  starsoil_actor::draw(a, [](int px, int py, uint16_t c, starsoil_actor::Tag,
                             uint8_t) { pixel(px, py, c); });
}

void sky() {
  int tx = std::clamp(int(s.player.x / 16), 0, s.width - 1);
  int depth = std::max(0, int(s.player.y / 16) - int(s.heights[tx]));
  starsoil_scene::draw(
      width(), scene_bottom(), cx, cy, int(s.heights[tx]) * 16 - cy, depth,
      biome_at(tx, int(s.player.y / 16), s.width), s.clock, animation,
      [](int x, int y, int w, int h, uint16_t c) { rect(x, y, w, h, c); },
      [](int x, int y, int xx, int yy, uint16_t c) { line(x, y, xx, yy, c); },
      [](int x, int y, uint16_t c) { pixel(x, y, c); });
}
bool connects(int t, int n) {
  return t == n ||
         (starsoil_terrain::terrain(t) && starsoil_terrain::terrain(n));
}
void scene() {
  cx = std::clamp(int(s.player.x) - width() / 2, 0, s.width * 16 - width());
  cy = std::clamp(int(s.player.y) - (scene_bottom() - 47), -114,
                  s.height * 16 - scene_bottom() + 1);
  sky();
  for (int y = std::max(0, (cy + 25) / 16);
       y <= std::min(s.height - 1, (cy + scene_bottom() - 1) / 16); ++y)
    for (int x = cx / 16; x <= std::min(s.width - 1, (cx + width() - 1) / 16);
         ++x) {
      int sx = x * 16 - cx, sy = y * 16 - cy, t = tile(s, x, y),
          light = light_at(x, y);
      if (s.walls[y * WorldW + x] == Plank || s.walls[y * WorldW + x] == Door ||
          s.walls[y * WorldW + x] == Glass) {
        int wall = s.walls[y * WorldW + x];
        uint16_t c = color(wall == Plank || wall == Door ? 0x675345
                           : wall == Glass               ? 0x7ca7ae
                           : y > 70                      ? 0x33334a
                                                         : 0x4b464d);
        rect(sx, std::max(sy, 25), 16,
             std::min(sy + 16, scene_bottom()) - std::max(sy, 25),
             dim(c, light));
        if (sy >= 25 && sy < scene_bottom() - 4) {
          if (wall == Glass) {
            line(sx, sy, sx + 15, sy, dim(color(0xc5ded3), light));
            line(sx + 1, sy + 2, sx + 5, sy + 2, dim(color(0xb4d8cd), light));
          } else {
            line(sx, sy + 15, sx + 15, sy + 15, dim(color(0x493e3d), light));
            line(sx + 7, sy, sx + 7, sy + 14, dim(color(0x51433e), light));
            line(sx + 2, sy + 3, sx + 2, sy + 11, dim(color(0x8c6d51), light));
            pixel(sx + 5, sy + 6, dim(color(0x9b7d5e), light));
          }
        }
      }
      auto tree_sample = [](int xx, int yy) { return tile(s, xx, yy); };
      bool natural_tree = starsoil_tree::draw_natural(
          x, y, sx, sy, biome_at(x, y, s.width), light, tree_sample,
          [](int xx, int yy, uint16_t c) {
            if (yy >= 25 && yy < scene_bottom())
              pixel(xx, yy, c);
          });
      if (t) {
        int mask = (connects(t, tile(s, x, y - 1)) ? 1 : 0) |
                   (connects(t, tile(s, x + 1, y)) ? 2 : 0) |
                   (connects(t, tile(s, x, y + 1)) ? 4 : 0) |
                   (connects(t, tile(s, x - 1, y)) ? 8 : 0);
        int region = biome_at(x, y, s.width);
        auto sample = [](int xx, int yy) { return tile(s, xx, yy); };
        const uint16_t *tile_art =
            starsoil_tree::art_for(t, x, y, region, sample);
        if (!tile_art)
          tile_art = art::tiles[t * 16 + mask];
        auto plot = [](int xx, int yy, uint16_t c) {
          if (yy >= 25 && yy < scene_bottom())
            pixel(xx, yy, c);
        };
        if (!natural_tree &&
            !starsoil_terrain::draw_terrain(x, y, sx, sy, light, sample, plot))
          sprite(sx, sy, tile_art, 16, 16, false, light, 1, true);
        if (is_ore(t) && sy >= 52 && sy < scene_bottom() - 16 &&
            (animation / 90 + hash(x + y * 384)) % 41 < 2) {
          int gx = sx + 3 + int(hash(x * 17 + y) % 10),
              gy = sy + 3 + int(hash(x + y * 31) % 10);
          pixel(gx, gy, Ink);
          pixel(gx - 1, gy, GoldInk);
          pixel(gx + 1, gy, GoldInk);
          pixel(gx, gy - 1, Teal);
          pixel(gx, gy + 1, Teal);
        }
        if (t == Torch) {
          int a = (animation / 130 + x) % 3;
          if (sy > 29 && sy < scene_bottom() - 10)
            pixel(sx + 6 + a, sy + 2 - a, Ink);
        }
        if ((t == Water || t == Lava) && sy >= 25 && sy < scene_bottom() - 2) {
          int offset = int(animation / 130 + x) % 8;
          line(sx + offset, sy + 2, sx + offset + 3, sy + 2, dim(Ink, light));
        }
        if (t == Grass && !tile(s, x, y - 1) && sy > 28) {
          for (int j = 0; j < 3; ++j) {
            int xx = sx + 2 + j * 5;
            line(xx, sy - 3 - int(hash(x + j) % 3),
                 xx + int(animation / 700 % 2), sy,
                 dim(color(0x80a569), light));
          }
        }
      }
      starsoil_terrain::draw_fillet(
          x, y, sx, sy, light, [](int xx, int yy) { return tile(s, xx, yy); },
          [](int xx, int yy, uint16_t c) {
            if (yy >= 25 && yy < scene_bottom())
              pixel(xx, yy, c);
          });
    }
  for (auto n : s.npcs) {
    int x = int(n.x) - cx, y = int(n.y) - cy;
    if (x < -20 || x > width() + 20 || y < 25 || y > scene_bottom() - 3)
      continue;
    uint8_t eq[8]{};
    eq[0] = n.type == 3 ? StrawHat : n.type == 2 ? IronHelmet : 0;
    eq[3] = n.type == 1 ? RedTunic : n.type == 3 ? SageRobe : BlueTunic;
    eq[4] = n.type == 0 ? TealCape : 0;
    human(x, y,
          starsoil_actor::locomotion_frame(n.grounded, std::fabs(n.vx) > 1,
                                           n.anim / 512),
          n.face, eq, n.type == 2 ? IronAxe : 0,
          light_at(int(n.x / 16), int(n.y / 16)), 1, n.type == 2 ? 2 : 1);
  }
  for (auto m : s.mobs)
    if (m.hp > 0) {
      int x = int(m.x) - cx, y = int(m.y) - cy;
      if (x < -48 || x > width() + 42 || y < 25 || y > scene_bottom())
        continue;
      int sc = m.type == 5 ? 2 : 1;
      sprite(x - 12 * sc, y - 24 * sc,
             art::creatures[(m.type - 1) * 6 + (m.anim / 120) % 6], 24, 24,
             !m.face, light_at(int(m.x / 16), int(m.y / 16)), sc, true);
    }
  for (auto d : s.drops)
    if (d.item) {
      int x = int(d.x) - cx, y = int(d.y) - cy;
      if (y > 25 && y < scene_bottom() - 12)
        icon(x - 8, y - 8, d.item, light_at(int(d.x / 16), int(d.y / 16)));
    }
  for (auto b : s.shots)
    if (b.life) {
      int x = int(b.x) - cx, y = int(b.y) - cy;
      if (y > 25 && y < scene_bottom() - 4) {
        rect(x - 1, y - 1, 3, 3, b.magic ? Teal : GoldInk);
        line(x, y, x - int(b.vx * .03f), y - int(b.vy * .03f), Ink);
      }
    }
  if (!s.invulnerable || s.invulnerable / 100 % 2)
    human(int(s.player.x) - cx, int(s.player.y) - cy,
          starsoil_actor::locomotion_frame(s.player.grounded,
                                           std::fabs(s.player.vx) > 1,
                                           s.player.anim / 512),
          s.player.face, s.equip, s.bag[s.selected].item,
          light_at(int(s.player.x / 16), int(s.player.y / 16)), 1, 0,
          tool_animation.active, tool_animation.frame());
  for (const auto &p : effects.particles)
    if (p.life) {
      int x = int(p.x) - cx, y = int(p.y) - cy;
      if (y >= 52 && y < scene_bottom() - 1)
        rect(x, y, p.life > 300 ? 2 : 1, 1,
             dim(color(p.color), std::min(255, int(p.life) * 2)));
    }
  int tx = s.target_x * 16 - cx, ty = s.target_y * 16 - cy;
  if (ty >= 25 && ty < scene_bottom() - 15) {
    outline(tx, ty, 16, 16, dim(GoldInk, 180));
    pixel(tx + 2, ty + 2, GoldInk);
    if (s.mining)
      rect(tx, ty + 15, std::min(16, int(s.mining) * 16 / 500), 2, Ink);
  }
}
void scene_labels() {
  for (const auto &n : s.npcs) {
    int x = int(n.x) - cx, y = int(n.y) - cy;
    if (nearby_npc(s) == n.type && x >= 0 && x < width() && y > 84 &&
        y < scene_bottom()) {
      panel(x - 7, y - 52, 14, 18);
      center(x - 7, y - 51, 14, "!", GoldInk);
    }
  }
  for (const auto &m : s.mobs)
    if (m.hp > 0 && m.type == 5) {
      panel(60, 79, 168, 14);
      rect(64, 84, std::max(0, int(m.hp)) * 160 / 280, 4, Red);
      label(62, 63, tr("STARHEART WARDEN", "星核守卫"), GoldInk);
    }
  for (const auto &p : effects.pops)
    if (p.life) {
      char b[64];
      if (p.item)
        std::snprintf(b, sizeof(b), "+%d %s", p.amount,
                      tr(items[p.item].en, items[p.item].zh));
      else
        std::snprintf(b, sizeof(b), "%d", p.amount);
      int x = std::clamp(int(p.x) - cx - text_width(b) / 2, 4,
                         std::max(4, width() - text_width(b) - 4));
      int y = int(p.y) - cy;
      if (y >= 52 && y < scene_bottom() - 12) {
        label(x + 1, y + 1, b, Dark);
        label(x, y, b, mix(color(p.color), Ink, 100));
      }
    }
}
void hud() {
  rect(0, 0, width(), 31, Dark);
  rect(0, 30, width(), 1, rgb(67, 88, 90));
  label(5, 0, "HP", Red);
  rect(22, 6, 62, 4, rgb(77, 56, 62));
  rect(22, 6, s.hp * 62 / 100, 4, Red);
  label(5, 15, "MP", Teal);
  rect(22, 21, 62, 3, rgb(49, 69, 81));
  rect(22, 21, s.mana * 62 / 60, 3, Teal);
  char b[48];
  std::snprintf(b, sizeof(b), "D%u  %02u:%02u", s.day,
                (s.clock / 15000 + 6) % 24, s.clock / 250 % 60);
  label(91, 1, b, GoldInk);
  label(91, 16,
        biome_name(biome_at(int(s.player.x / 16), int(s.player.y / 16), s.width),
                   !pocket::english()),
        Muted);
  center(width() - 88, 1, 64, tr("BAG", "背包"), Ink);
  center(width() - 88, 16, 64, tr("CRAFT", "合成"), Teal);
  label(width() - 19, 3, "II", GoldInk);
  for (int i = 0; i < HotSlots; ++i) {
    int x = 4 + i * 24;
    rect(x, 34, 22, 23, i == s.selected ? rgb(71, 89, 80) : Dark);
    outline(x, 34, 22, 23, i == s.selected ? GoldInk : rgb(76, 86, 87));
    icon(x + 3, 36, s.bag[i].item);
    if (s.bag[i].count > 1) {
      std::snprintf(b, sizeof(b), "%u", s.bag[i].count);
      label(x + 2, 42, b, Ink);
    }
  }
  int depth = std::max(
      0, int(s.player.y / 16) -
             int(s.heights[std::clamp(int(s.player.x / 16), 0, s.width - 1)]));
  std::snprintf(b, sizeof(b), "%dm", depth);
  label(width() - 41, 37, b, Ink);
  const char *goal;
  int progress = 0, total = 1;
  if (!(s.quests & 1))
    goal = tr("MEET ROWAN", "拜访向导");
  else if (!(s.quests & 2)) {
    goal = s.mined >= 20 ? tr("SEE ROWAN", "回访向导")
                         : tr("MINE ROCK", "开采方块");
    progress = std::min(20, int(s.mined));
    total = 20;
  } else if (!(s.quests & 4)) {
    goal = s.crafted >= 10 ? tr("SEE ROWAN", "回访向导")
                           : tr("MAKE ITEMS", "合成物品");
    progress = std::min(10, int(s.crafted));
    total = 10;
  } else
    goal = s.boss_defeated ? tr("EXPLORE ON", "继续探索")
                           : tr("FIND ALTAR", "寻找祭坛");
  panel(width() - 90, 62, 84, 37);
  label(width() - 86, 65, goal, GoldInk);
  if (total > 1) {
    std::snprintf(b, sizeof(b), "%d/%d", progress, total);
    label(width() - 86, 81, b, Muted);
  }
  rect(width() - 37, 93, 26, 2, rgb(56, 75, 79));
  rect(width() - 37, 93, progress * 26 / total, 2, Teal);
  rect(0, scene_bottom(), width(), 43, Dark);
  line(0, scene_bottom(), width() - 1, scene_bottom(), rgb(83, 105, 102));
  button(3 * width() / 288, height() - 26, 31 * width() / 288, "<",
         touch == 10);
  button(38 * width() / 288, height() - 26, 31 * width() / 288, ">",
         touch == 11);
  button(73 * width() / 288, height() - 26, 33 * width() / 288,
         tr("DOWN", "下"), touch == 12);
  button(110 * width() / 288, height() - 26, 42 * width() / 288,
         tr("TALK", "互动"), touch == 13);
  button(156 * width() / 288, height() - 26, 52 * width() / 288,
         tr("JUMP", "跳跃"), touch == 14);
  button(212 * width() / 288, height() - 26, 72 * width() / 288,
         tr("USE", "使用"), touch == 15);
}
void heading(const char *txt) {
  pocket::begin_ui();
  draw_x = draw_y = 0;
  pocket::clear(Dark);
  rect(0, 0, width(), 25, Panel);
  label(8, 6, txt, GoldInk);
  label(width() - 32, 6, "<", Ink);
  line(0, 24, width() - 1, 24, rgb(91, 113, 108));
  content_origin();
}
void tabs(View current) {
  draw_x = draw_y = 0;
  const View vs[] = {Bag, Crafting, Gear, Map, Guide};
  const char *en[] = {"BAG", "CRAFT", "GEAR", "MAP", "HELP"},
             *cn[] = {"背包", "合成", "装备", "地图", "指南"};
  for (int i = 0; i < 5; ++i) {
    int x = 5 + i * (width() - 8) / 5;
    int tab_width = (width() - 8) / 5 - 3;
    rect(x, 28, tab_width, 16, vs[i] == current ? rgb(64, 90, 82) : Panel);
    center(x, 29, tab_width, tr(en[i], cn[i]),
           vs[i] == current ? GoldInk : Muted);
  }
  content_origin();
}
#include "main_menu.inc"
void bag_draw() {
  heading(tr("SATCHEL & SUPPLIES", "旅人背包"));
  tabs(Bag);
  for (int i = 0; i < BagSlots; ++i) {
    int x = 6 + i % 8 * 25, y = 50 + i / 8 * 24;
    rect(x, y, 23, 22, i == selected ? rgb(66, 90, 82) : Panel);
    outline(x, y, 23, 22, i == selected ? GoldInk : rgb(64, 78, 84));
    icon(x + 3, y + 1, s.bag[i].item);
    if (s.bag[i].count > 1) {
      char b[12];
      std::snprintf(b, sizeof(b), "%u", s.bag[i].count);
      label(x + 1, y + 21 - pocket::text_height(), b);
    }
  }
  human(238, 126, art::StandFrame, preview_face, s.equip,
        s.bag[s.selected].item, 255, 2);
  center(210, 135, 75, tr("EQUIPPED", "实装预览"), Teal);
  char b[40];
  std::snprintf(b, sizeof(b), tr("DEF %d", "防御 %d"), defense(s));
  center(210, 151, 75, b, Muted);
  int id = s.bag[selected].item;
  footer_origin();
  label(7, 170, tr(items[id].en, items[id].zh), GoldInk);
  if (equip_slot(items[id].kind) >= 0)
    label(7, 187, hint("TAP AGAIN: EQUIP", "再次点击：穿戴到身上",
                       "A / TAP AGAIN: EQUIP", "A或再次点击：穿戴到身上"), Muted);
  else
    label(7, 187, hint("TAP AN ITEM, THEN USE", "点击物品后使用或放入快捷栏",
                       "A USE   Y TO HOTBAR", "A使用  Y放入当前快捷栏"), Muted);
  button(6, 204, 128, tr("USE / EQUIP", "使用 / 穿戴"), true);
  button(143, 204, 139, tr("TO HOTBAR", "放到当前快捷栏"));
}
void storage_draw() {
  heading(tr("TRAVELLER'S STORAGE", "远行者储物箱"));
  label(7, 30,
        tr("TAP BAG TO STORE; CHEST TO TAKE", "点左侧物品存入，点右侧物品取出"),
        Teal);
  for (int i = 0; i < BagSlots; ++i) {
    int x = 6 + i % 8 * 24, y = 50 + i / 8 * 25;
    panel(x, y, 22, 23);
    if (selected == i)
      outline(x, y, 22, 23, GoldInk);
    icon(x + 3, y + 2, s.bag[i].item);
    if (s.bag[i].count > 1) {
      char b[8];
      std::snprintf(b, sizeof(b), "%u", s.bag[i].count);
      label(x + 1, y + 21 - pocket::text_height(), b);
    }
  }
  for (int i = 0; i < 12; ++i) {
    int x = 216 + i % 2 * 33, y = 50 + i / 2 * 25;
    panel(x, y, 29, 23, rgb(74, 69, 54));
    if (selected == i + BagSlots)
      outline(x, y, 29, 23, GoldInk);
    icon(x + 6, y + 1, s.chest[i].item);
    if (s.chest[i].count > 1) {
      char b[8];
      std::snprintf(b, sizeof(b), "%u", s.chest[i].count);
      label(x + 1, y + 21 - pocket::text_height(), b);
    }
  }
  wrap(7, 174, 196,
       tr("Twelve shared slots.\nMove chests; items stay.",
          "储物箱共享十二格物资。\n移动箱子不会丢失物品。"),
       Muted, 2);
  label(8, 209, hint("TAP TO STORE / TAKE", "点击物品：存入或取出",
                     "PAD SELECT  A STORE  B BACK", "方向键选择 A存入 B返回"),
        GoldInk);
}
const char *station_name(int id) {
  const char *en[] = {"HAND", "WORKBENCH", "FURNACE", "ANVIL", "STAR ALTAR"},
             *cn[] = {"徒手", "工作台", "熔炉", "铁砧", "星核祭坛"};
  return tr(en[id], cn[id]);
}
void crafting_draw() {
  heading(tr("THE CRAFTING JOURNAL", "合成手册"));
  tabs(Crafting);
  int first = recipe_page * RecipesPerPage;
  char b[96];
  for (int row = 0; row < RecipesPerPage; ++row) {
    int r = first + row;
    if (r >= RecipeCount)
      break;
    int y = 49 + row * RecipeRowHeight;
    const auto &p = recipes[r];
    bool ready = can_craft(s, r);
    panel(6, y, 276, RecipeRowHeight - 2, r == recipe ? rgb(59, 81, 77) : Panel);
    icon(10, y + 3, p.out);
    label(31, y + 1, tr(items[p.out].en, items[p.out].zh),
          ready ? GoldInk : Ink);
    std::snprintf(b, sizeof(b), "x%u", p.count);
    label(178, y + 1, b, Muted);
    label(211, y + 1, station_name(p.station),
          near_station(s, p.station) ? Teal : Red);
    int at = 32;
    for (auto a : p.in)
      if (a.item) {
        std::snprintf(b, sizeof(b), "%u/%u ", count(s, a.item), a.count);
        label(at, y + 16, b, count(s, a.item) >= a.count ? Teal : Red);
        icon(at + text_width(b), y + 14, a.item);
        at += 74;
      }
  }
  footer_origin();
  const auto &p = recipes[recipe];
  int x = 7;
  for (auto a : p.in)
    if (a.item) {
      label(x, 178, tr(items[a.item].en, items[a.item].zh), Muted);
      x += 90;
    }
  button(6, 199, 43, "<");
  std::snprintf(b, sizeof(b), "%d/%d", recipe_page + 1,
                (RecipeCount + RecipesPerPage - 1) / RecipesPerPage);
  center(50, 204, 60, b, Muted);
  button(111, 199, 123, hint("CRAFT", "合成", "CRAFT   A", "合成   A"),
         can_craft(s, recipe));
  button(239, 199, 43, ">");
}
void gear_draw() {
  heading(tr("DRESS FOR THE JOURNEY", "旅人的衣橱"));
  tabs(Gear);
  panel(8, 49, 113, 126, rgb(29, 47, 57));
  line(18, 173, 112, 173, rgb(66, 85, 88));
  human(62, 173, art::StandFrame, preview_face, s.equip, s.bag[s.selected].item,
        255, 4);
  const char *en[] = {"HELM", "CHEST", "LEGS", "CLOTHES",
                      "CAPE", "BOOTS", "-",    "CHARM"},
             *cn[] = {"头盔", "胸甲", "护腿", "衣服",
                      "披风", "靴子", "",     "护符"};
  for (int i = 0; i < 8; ++i) {
    if (i == 6)
      continue;
    int y = 49 + (i < 6 ? i : i - 1) * 20;
    rect(129, y, 151, 17, selected == i ? rgb(59, 81, 77) : Panel);
    label(133, y + 1, tr(en[i], cn[i]), Muted);
    icon(175, y, s.equip[i]);
    label(194, y + 1,
          s.equip[i] ? tr(items[s.equip[i]].en, items[s.equip[i]].zh) : "--",
          s.equip[i] ? GoldInk : Muted);
  }
  footer_origin();
  char b[64];
  std::snprintf(b, sizeof(b), tr("ARMOR %d  /  HP %u", "防御 %d  /  生命 %u"),
                defense(s), s.hp);
  label(8, 180, b, GoldInk);
  label(192, 180,
        hint("TAP: TURN", "点击转身", "< > TURN", "左右转身"),
        Muted);
  button(8, 203, 272, hint("UNEQUIP SELECTED SLOT", "点击槽位：卸下装备",
                          "A UNEQUIP SELECTED SLOT", "A或点击槽位：卸下装备"));
}
void map_draw() {
  heading(tr("ATLAS OF STARSOIL", "星壤图鉴"));
  tabs(Map);
  // Show every world tile in the overview, with a cursor for discovery
  // coordinates.
  for (int yy = 0; yy < 112; ++yy)
    for (int xx = 0; xx < 272; ++xx) {
      int wx = xx * s.width / 272;
      int wy = yy * s.height / 112;
      int t = tile(s, wx, wy);
      uint16_t c = t                    ? color(items[tile_drop(t)].color)
                   : wy > s.heights[wx] ? rgb(37, 39, 55)
                                        : rgb(97, 139, 153);
      pixel(8 + xx, 49 + yy, c);
    }
  int px = 8 + int(s.player.x / 16) * 272 / s.width,
      py = 49 + int(s.player.y / 16) * 112 / s.height;
  outline(px - 2, py - 2, 5, 5, GoldInk);
  for (auto n : s.npcs)
    pixel(8 + int(n.x / 16) * 272 / s.width, 49 + int(n.y / 16) * 112 / s.height, Teal);
  char b[96];
  std::snprintf(b, sizeof(b),
                tr("SEED %u  /  DAY %u", "世界种子 %u  /  第%u天"), s.seed,
                s.day);
  label(8, 167, b, GoldInk);
  std::snprintf(
      b, sizeof(b),
      tr("MINED %u   CRAFTED %u   KILLS %u", "开采%u  合成%u  击败%u"), s.mined,
      s.crafted, s.kills);
  label(8, 182, b, Muted);
  wrap(8, 201, 270,
       tr("Crystal: Y68+. Altar: 194,91.", "晶洞在深处，祭坛194、91。"), Muted, 1);
}
void guide_draw() {
  heading(tr("A FIELD GUIDE", "远行指南"));
  tabs(Guide);
  const char *en[][8] = {
      {"01  YOUR FIRST EVENING", "Arrows move. A jumps. B uses items.",
       "X bag. Y craft. L/R select hotbar.",
       "UP/DOWN aim. Touch a tile to aim.",
       "Hold B: mine, chop, attack or place.",
       "SELECT / TALK: furniture and NPCs.",
       "Bag: tap twice or A to equip clothes.",
       "START pauses. Autosave every 8 sec."},
      {"02  FROM WOOD TO CRYSTAL", "Wood + stone: workbench and furnace.",
       "Ore + coal: ingots. Anvil: gear.",
       "Copper pick: gold; iron: crystal.",
       "Recipes list costs and stations.",
       "Gather flowers, fungi, vines, acorns.",
       "Plant acorns in grass or dirt.",
       "DOWN + place planks: build walls."},
      {"03  PEOPLE & ANCIENT LIGHT",
       "Guide: mine 20 blocks, craft 10.",
       "Merchant: 1 gold ingot > 8 silk.",
       "Smith: 9 copper ore > 2 iron ingots.",
       "Gardener: 3 flowers > 2 potions.",
       "SELECT / TALK: open hidden chests.",
       "Craft star heart at altar; summon!",
       "Warden drops: ember cape, star heart."},
      {"04  SURVIVE & BUILD", "TALK: doors. Jump through platforms.",
       "Hold DOWN to drop through platforms.",
       "Beds set home. Recall potions return.",
       "Water slows falls. Lava hurts.",
       "Bows need arrows. Wands use mana.",
       "Cloud boots: 2 jumps. Glow: light.",
       "Death sends you home, items kept."}};
  const char *cn[][8] = {
      {"一 · 初来星壤", "方向键移动，A跳跃，B使用。",
       "X背包，Y合成，L/R切换手持。", "上下键瞄准，触屏可点选目标。",
       "长按B采集、战斗，或放置方块。", "SELECT或互动：家具与NPC。",
       "背包双击穿戴，外观实时变化。", "START暂停；每八秒自动保存。"},
      {"二 · 从木材到晶体", "砍树挖石，建工作台与熔炉。",
       "矿石加煤炼锭，铁砧锻造装备。", "铜镐挖金矿，铁镐挖晶体。",
       "配方显示材料和所需设施。", "采集花草、蘑菇、藤蔓、橡子。",
       "橡子种在草地或泥土，随后长树。", "按住下键放木板，可搭背景墙。"},
      {"三 · 来客与古老流光", "向导：挖二十块，再合成十次。",
       "商人：一金锭换八份丝绸。", "铁匠：九铜矿换两铁锭。",
       "园丁：三野花换两生命药水。", "互动打开地下宝箱，寻找宝物。",
       "祭坛合成星核，再互动召唤守卫。", "击败守卫，获得星核和披风。"},
      {"四 · 生存与家园", "互动开关门；平台可从下方跳上。",
       "按住下键，可落到平台下方。", "与床互动设家，回城药水返回。",
       "水减缓下落；熔岩会造成伤害。", "弓消耗箭；星杖使用可恢复的魔力。",
       "云靴二段跳；辉光项链照亮周围。", "死亡回家，背包物品会保留。"}};
  const char *touch_en[] = {
      "01  YOUR FIRST EVENING",
      "Hold arrows to move. Tap JUMP.",
      "Open BAG / CRAFT at the top right.",
      "Tap to aim. Drag to change target.",
      "Hold USE: mine, chop, fight, place.",
      "Tap TALK: furniture and nearby NPCs.",
      "Bag: tap twice to equip clothes.",
      "Menu pauses. Autosave every 8 sec."};
  const char *touch_cn[] = {
      "一 · 初来星壤", "按住方向按钮移动，点击跳跃。",
      "右上角打开背包或合成手册。", "触屏点选目标，拖动切换目标。",
      "长按使用采集、战斗或放置。", "点击互动：家具与附近的NPC。",
      "背包双击穿戴，外观实时变化。", "菜单暂停；每八秒自动保存。"};
  if (guide_page < 4) {
    for (int i = 0; i < 8; ++i) {
      const char *line = tr(en[guide_page][i], cn[guide_page][i]);
      if (!pocket::using_controller()) {
        if (guide_page == 0)
          line = tr(touch_en[i], touch_cn[i]);
        else if (guide_page == 2 && i == 5)
          line = tr("Tap TALK to open underground chests.", cn[guide_page][i]);
      }
      wrap(8, 49 + i * 18, 270, line, i ? Muted : GoldInk, 1);
    }
  } else {
    const char *names[] = {tr("DEWDROP SLIME", "叶露史莱姆"),
                           tr("DUSK BAT", "暮翼蝠"), tr("MOSSLING", "苔木精"),
                           tr("GEM CRAWLER", "晶背爬兽"),
                           tr("WARDEN", "星核守卫")};
    for (int i = 0; i < 5; ++i) {
      int x = 6 + (i % 3) * 93, y = 49 + (i / 3) * 68;
      panel(x, y, 89, 63, rgb(29, 46, 57));
      sprite(x + 21, y + 1, art::creatures[i * 6 + (animation / 120) % 6], 24,
             24, false, 255, 2);
      center(x, y + 49, 89, names[i], i == 4 ? GoldInk : Teal);
    }
  }
  footer_origin();
  button(7, 200, 58, "<");
  char b[8];
  std::snprintf(b, sizeof(b), "%d / 5", guide_page + 1);
  center(72, 204, 144, b, GoldInk);
  button(224, 200, 58, ">");
}
const char *npc_name(int i) {
  const char *en[] = {"ROWAN / GUIDE", "MIRA / MERCHANT", "BRAM / SMITH",
                      "IRIS / GARDENER"},
             *cn[] = {"罗恩 · 向导", "米拉 · 商人", "布兰 · 铁匠",
                      "艾莉 · 园丁"};
  return tr(en[i], cn[i]);
}
void dialogue_draw() {
  heading(npc_name(npc));
  uint8_t eq[8]{};
  eq[3] = npc == 1 ? RedTunic : npc == 3 ? SageRobe : BlueTunic;
  eq[0] = npc == 2 ? IronHelmet : npc == 3 ? StrawHat : 0;
  human(44, 121, art::StandFrame, true, eq, 0, 255, 3, npc == 2 ? 2 : 1);
  const char *en[] = {
      "A home is only the beginning. Bring light into the old caves. Mine 20 "
      "blocks, then craft 10 times to help us rebuild.",
      "Every piece of cloth carries a story. One gold ingot for eight silk; a "
      "fine start for a cape or a new tunic.",
      "Copper is good. Iron is better. Bring me nine copper ore and I will "
      "trade you two iron ingots.",
      "Flowers for medicine. Bring three wildflowers and I will give you two "
      "healing potions. Plant acorns to keep the forest alive."};
  const char *cn[] = {
      "房子只是开始。带着火光去古老洞穴看看吧！先挖二十个方块，再完成十次合成，"
      "我会送你一些旅途需要的材料。",
      "每片布都有自己的故事。一块金锭换八份丝绸，做件新衣服或一条披风怎么样？",
      "铜不错，铁更好。带来九块铜矿，我会给你两块铁锭。记得在工作台旁造一座铁砧"
      "。",
      "让野花变成治愈的力量吧。三朵野花换两瓶生命药水。也别忘了种下橡子，让森林"
      "继续生长。"};
  wrap(94, 49, 180, tr(en[npc], cn[npc]), Ink, 7);
  button(12, 173, 264,
         npc == 0 ? hint("CLAIM / CHECK QUEST", "领取赠礼或查看委托",
                         "A CLAIM / CHECK QUEST", "A领取赠礼或查看委托")
                  : hint("TRADE", "交易", "A TRADE", "A交易"),
         true);
  button(12, 200, 264, hint("RETURN TO THE WORLD", "返回世界",
                           "B RETURN TO THE WORLD", "B返回世界"));
}
void swap_hotbar() {
  if (selected < 0 || selected >= BagSlots)
    return;
  std::swap(s.bag[selected], s.bag[s.selected]);
  s.dirty = true;
  sound(530);
  pocket::save();
}
void bag_activate() {
  if (activate(s, selected)) {
    sound(640);
    pocket::save();
  } else
    notify("Select a tool or move it to the hotbar.",
           "工具和方块请先放到快捷栏使用。");
}
void do_craft() {
  if (craft(s, recipe)) {
    sound(720);
    pocket::save();
  } else {
    notify("Missing ingredients, station, or bag space.",
           "请检查材料、所需设施和背包空间。");
    sound(180);
  }
}
void do_interact() {
  if (nearby_storage(s)) {
    set_view(Storage);
    selected = 0;
    return;
  }
  int who = nearby_npc(s);
  if (who >= 0) {
    npc = who;
    set_view(Dialogue);
  } else {
    interact(s);
    pocket::save();
  }
}
int hit(int x, int y) {
  if (x < 0 || y < 0 || x >= width() || y >= height())
    return -1;
  if (home_view()) return home_hit(x, y);
  if (view != World) {
    const int physical_y = y;
    if (y < 25 && x > width() - 48)
      return 99;
    if (view >= Bag && view <= Guide && y >= 28 && y < 44)
      return 100 + std::clamp((x - 5) * 5 / (width() - 8), 0, 4);
    x -= content_x();
    y -= content_y();
    if ((view == Bag && y + content_y() >= height() - 50) ||
        (view == Crafting && y + content_y() >= height() - 50) ||
        (view == Gear && y + content_y() >= height() - 26) ||
        (view == Guide && y + content_y() >= height() - 26))
      y += content_y() - (height() - 224);
    if (view == Bag) {
      if (x >= 210 && y >= 50 && y < 170)
        return 84;
      if (x >= 6 && x < 206 && y >= 50 && y < 170)
        return (y - 50) / 24 * 8 + (x - 6) / 25;
      if (y >= 204 && physical_y >= height() - 20)
        return x < 140 ? 80 : 81;
    }
    if (view == Crafting) {
      if (y >= 49 && y < 49 + RecipesPerPage * RecipeRowHeight)
        return 120 + recipe_page * RecipesPerPage + (y - 49) / RecipeRowHeight;
      if (y >= 199 && physical_y >= height() - 25)
        return x < 55 ? 90 : x > 235 ? 91 : 92;
    }
    if (view == Gear && x >= 129 && y >= 49 && y < 189) {
      int row = (y - 49) / 20;
      return 140 + (row < 6 ? row : 7);
    }
    if (view == Gear && y >= 203 && physical_y >= height() - 21)
      return 82;
    if (view == Gear && x < 126 && y >= 49 && y < 175)
      return 84;
    if (view == Guide && y >= 200 && physical_y >= height() - 24)
      return x < 80 ? 90 : x > 210 ? 91 : -1;
    if (view == Storage && x >= 6 && x < 198 && y >= 50 && y < 175)
      return 160 + (y - 50) / 25 * 8 + (x - 6) / 24;
    if (view == Storage && x >= 216 && x < 282 && y >= 50 && y < 200)
      return 200 + (y - 50) / 25 * 2 + (x - 216) / 33;
    if (view == Dialogue && y >= 173 && y < 193)
      return 83;
    if (view == Dialogue && y >= 200)
      return 99;
    return -1;
  }
  if (y < 31)
    return x >= width() - 88 && x < width() - 24 ? (y < 15 ? 20 : 21) : -1;
  if (y >= 34 && y < 57 && x < 196)
    return 30 + std::clamp((x - 4) / 24, 0, 7);
  if (x >= width() - 90 && y >= 62 && y < 99)
    return -1;
  if (y >= height() - 28)
    return x * 288 / width() < 35    ? 10
           : x * 288 / width() < 70  ? 11
           : x * 288 / width() < 108 ? 12
           : x * 288 / width() < 154 ? 13
           : x * 288 / width() < 210 ? 14
                                     : 15;
  if (y >= 57 && y < scene_bottom())
    return 16;
  return -1;
}
void click(int id) {
  if (home_view()) { home_click(id); return; }
  if (id == 99) {
    set_view(World);
    return;
  }
  if (id >= 100 && id < 105) {
    set_view(View(int(Bag) + id - 100));
    selected = 0;
    return;
  }
  if ((view == Gear || view == Bag) && id == 84) {
    preview_face = !preview_face;
    pocket::redraw();
    return;
  }
  if (view == Title && id == 1) {
    set_view(World);
    return;
  }
  if (view == Bag) {
    if (id >= 0 && id < 40) {
      if (selected == id)
        bag_activate();
      selected = id;
    } else if (id == 80)
      bag_activate();
    else if (id == 81)
      swap_hotbar();
  } else if (view == Crafting) {
    if (id >= 120 && id < 120 + RecipeCount) {
      int next = id - 120;
      if (recipe == next)
        do_craft();
      recipe = next;
    } else if (id == 90 || id == 91) {
      int pages = (RecipeCount + RecipesPerPage - 1) / RecipesPerPage;
      recipe_page = (recipe_page + (id == 90 ? pages - 1 : 1)) % pages;
      recipe = recipe_page * RecipesPerPage;
    } else if (id == 92)
      do_craft();
  } else if (view == Gear) {
    if (id >= 140 && id < 148) {
      selected = id - 140;
      if (unequip(s, selected))
        pocket::save();
    } else if (id == 82 && unequip(s, selected))
      pocket::save();
  } else if (view == Guide && (id == 90 || id == 91))
    guide_page = (guide_page + (id == 90 ? 4 : 1)) % 5;
  else if (view == Dialogue && id == 83) {
    trade(s, npc);
    pocket::save();
    sound(580);
  } else if (view == Storage && id >= 160 && id < 212) {
    bool ok = id < 200 ? store(s, id - 160) : withdraw(s, id - 200);
    if (ok) {
      pocket::save();
      sound(600);
    } else
      notify("No space for this stack.", "没有足够空间存放这一组物品。");
  } else if (view == World) {
    if (id == 20)
      set_view(Bag);
    else if (id == 21)
      set_view(Crafting);
    else if (id >= 30 && id < 38)
      s.selected = id - 30;
    else if (id == 13)
      do_interact();
  }
  pocket::redraw();
}
} // namespace
namespace game {
const char *title() { return "星壤物语 Starsoil"; }
const char *app_id() { return "pxa-starsoil"; }
void init() {
  effects = {};
  session_active = false;
  save_error = false;
  view = Title;
  animation = autosave = 0;
  tool_animation = {};
  last_event = event_at = 0;
  message[0] = 0;
}
void reset() {
  if (session_active) pocket::save();
  if (pocket::active_world_slot() >= 0) {
    world_selected = pocket::active_world_slot();
    int at = 0;
    while (at < world_count() && listed_world(at) != world_selected) ++at;
    world_page = at / 2;
  }
  set_view(Title);
}
void create_world(uint32_t seed, uint8_t size) {
  new_game(s, seed, WorldSize(size));
  effects = {};
  tool_animation = {};
  session_active = true;
  save_error = false;
  animation = autosave = 0;
  last_event = 0;
  event_at = 0;
  world_seed[0] = 0;
  set_view(World);
}
void world_deleted() {
  session_active = false;
  save_error = false;
  set_view(Title);
}
bool inspect_world(const uint8_t *data, size_t size, pocket::WorldInfo &info) {
  SaveInfo header;
  if (!inspect_save(data, size, header)) return false;
  info.width = header.width; info.height = header.height;
  info.seed = header.seed; info.day = header.day; info.played = header.played;
  info.valid = true;
  return true;
}
void key(Key k) {
  if (home_view()) { home_key(k); return; }
  if (k == Key::X) {
    set_view(view == Bag ? World : Bag);
    return;
  }
  if (k == Key::Y) {
    if (view == Bag)
      swap_hotbar();
    else
      set_view(view == Crafting ? World : Crafting);
    return;
  }
  if (k == Key::B && view != World) {
    set_view(World);
    return;
  }
  if (k == Key::SELECT) {
    if (view == World)
      do_interact();
    else
      set_view(Guide);
    return;
  }
  if (view == World) {
    if (k == Key::L)
      s.selected = (s.selected + 7) % 8;
    if (k == Key::R)
      s.selected = (s.selected + 1) % 8;
    if (k == Key::A)
      jump_edge = true;
    return;
  }
  if (k == Key::L || k == Key::R) {
    if (view >= Bag && view <= Guide)
      set_view(View(Bag + (int(view) - Bag + (k == Key::R ? 1 : 4)) % 5));
    return;
  }
  if (view == Bag) {
    if (k == Key::LEFT)
      selected = (selected + 39) % 40;
    if (k == Key::RIGHT)
      selected = (selected + 1) % 40;
    if (k == Key::UP)
      selected = (selected + 32) % 40;
    if (k == Key::DOWN)
      selected = (selected + 8) % 40;
    if (k == Key::A)
      bag_activate();
  }
  if (view == Crafting) {
    if (k == Key::UP)
      recipe = (recipe + RecipeCount - 1) % RecipeCount;
    if (k == Key::DOWN)
      recipe = (recipe + 1) % RecipeCount;
    if (k == Key::LEFT)
      recipe = (recipe + RecipeCount - RecipesPerPage) % RecipeCount;
    if (k == Key::RIGHT)
      recipe = (recipe + RecipesPerPage) % RecipeCount;
    recipe_page = recipe / RecipesPerPage;
    if (k == Key::A)
      do_craft();
  }
  if (view == Gear) {
    if (k == Key::LEFT || k == Key::RIGHT)
      preview_face = k == Key::RIGHT;
    if (k == Key::UP)
      selected = selected == 7 ? 5 : (selected + 7) % 8;
    if (k == Key::DOWN)
      selected = selected == 5 ? 7 : (selected + 1) % 8;
    if (k == Key::A && unequip(s, selected))
      pocket::save();
  }
  if (view == Guide) {
    if (k == Key::LEFT)
      guide_page = (guide_page + 4) % 5;
    if (k == Key::RIGHT)
      guide_page = (guide_page + 1) % 5;
  }
  if (view == Dialogue && k == Key::A) {
    trade(s, npc);
    pocket::save();
  }
  if (view == Storage) {
    if (k == Key::LEFT)
      selected = (selected + 51) % 52;
    if (k == Key::RIGHT)
      selected = (selected + 1) % 52;
    if (k == Key::UP)
      selected = (selected + 44) % 52;
    if (k == Key::DOWN)
      selected = (selected + 8) % 52;
    if (k == Key::A) {
      if (selected < 40)
        store(s, selected);
      else
        withdraw(s, selected - 40);
      pocket::save();
    }
  }
  pocket::redraw();
}
void pointer(Phase phase, int x, int y) {
  if (phase == Phase::CANCEL) {
    touch = -1;
    touch_down = false;
    aim_x = aim_y = -1;
    return;
  }
  if (phase == Phase::DOWN) {
    gesture_jump = false;
    touch = hit(x, y);
    touch_down = true;
    if (touch == 14)
      jump_edge = true;
    if (view == World && touch == 16) {
      aim_x = (x + cx) / 16;
      aim_y = (y + cy) / 16;
    }
    return;
  }
  if (phase == Phase::MOVE) {
    if (view == World && touch == 16) {
      aim_x = (x + cx) / 16;
      aim_y = (y + cy) / 16;
    }
    if (view == World && (touch == 10 || touch == 11)) {
      if (y < scene_bottom() - 4 && !gesture_jump) {
        jump_edge = true;
        gesture_jump = true;
      }
      if (y >= height() - 31)
        gesture_jump = false;
    }
    return;
  }
  if (phase == Phase::UP) {
    int was = touch;
    touch_down = false;
    touch = -1;
    if (was == hit(x, y) && was != 10 && was != 11 && was != 12 && was != 14 &&
        was != 15 && was != 16)
      click(was);
    aim_x = aim_y = -1;
  }
}
void tick(uint32_t ms) {
  ms = std::min(ms, 100u);
  animation += ms;
  paint += ms;
  if (view == World) {
    Input input;
    input.move =
        (pocket::held(Key::RIGHT) ? 1 : 0) - (pocket::held(Key::LEFT) ? 1 : 0);
    if (touch_down && touch == 10)
      input.move = -1;
    if (touch_down && touch == 11)
      input.move = 1;
    input.down = pocket::held(Key::DOWN) || (touch_down && touch == 12);
    input.jump =
        pocket::held(Key::A) || (touch_down && touch == 14) || jump_edge;
    if (!pocket::held(Key::B))
      use_blocked = false;
    input.use = (pocket::held(Key::B) && !use_blocked) ||
                (touch_down && (touch == 15 || touch == 16));
    input.aim_y = pocket::held(Key::UP) ? -1 : input.down ? 1 : 0;
    input.target_x = aim_x;
    input.target_y = aim_y;
    uint32_t remaining = ms;
    while (remaining) {
      uint32_t step = std::min(remaining, 20u);
      starsoil::tick(s, input, step);
      remaining -= step;
    }
    tool_animation.step(input.use, s.bag[s.selected].item, ms);
    jump_edge = false;
    effects.step(s, ms, true);
    autosave += ms;
    if (autosave >= 8000) {
      autosave = 0;
      if (s.dirty) {
        pocket::save();
        s.dirty = false;
      }
    }
  } else
    effects.step(s, ms, false);
  if (s.event_serial != last_event) {
    last_event = s.event_serial;
    std::snprintf(message, sizeof(message), "%s", s.event);
    event_at = animation + 4500;
  }
  if (paint >= 40) {
    paint = 0;
    pocket::redraw();
  }
}
void draw() {
  draw_x = draw_y = 0;
  if (home_view())
    draw_home();
  else if (view == World) {
    scene();
    pocket::begin_ui(true);
    scene_labels();
    hud();
    label(
        6, scene_bottom(),
        tr(items[s.bag[s.selected].item].en, items[s.bag[s.selected].item].zh),
        GoldInk);
    if (animation < event_at) {
      const int rows =
          std::strchr(message, '\n') || text_width(message) > 268 ? 2 : 1;
      const int height = rows * pocket::text_line_height() + 6,
                top = scene_bottom() - 2 - height;
      panel(5, top, width() - 10, height);
      wrap(10, top + 3, width() - 20, message, Ink, rows);
    }
  } else if (view == Bag)
    bag_draw();
  else if (view == Crafting)
    crafting_draw();
  else if (view == Gear)
    gear_draw();
  else if (view == Map)
    map_draw();
  else if (view == Guide)
    guide_draw();
  else if (view == Storage)
    storage_draw();
  else
    dialogue_draw();
  if (!home_view() && view != World && view != Map && view != Guide &&
      animation < event_at) {
    if (view == Bag || view == Crafting || view == Gear)
      footer_origin();
    int y = view == Crafting   ? 177
            : view == Bag      ? 187
            : view == Dialogue ? 157
                               : 180;
    rect(0, y, 288, pocket::text_height(), Dark);
    wrap(6, y, 276, message, GoldInk, 1);
  }
  draw_x = draw_y = 0;
  if (save_error || pocket::save_status() < 0) {
    rect(0, 0, width() - 24, pocket::text_height() + 2, Dark);
    label(5, 1, tr("SAVE ERROR: FILE PRESERVED", "存档失败：原文件已保留"),
          Red);
  }
}
size_t serialize(uint8_t *data, size_t capacity) {
  return !session_active || save_error ? 0 : encode(s, data, capacity);
}
bool load(const uint8_t *data, size_t size) {
  if (!decode(s, data, size)) {
    pocket::log("Starsoil: rejected corrupt save; original file preserved");
    return false;
  }
  session_active = true;
  save_error = false;
  effects = {};
  tool_animation = {};
  autosave = 0;
  set_view(World);
  last_event = 0;
  char status[120];
  std::snprintf(
      status, sizeof(status),
      "Starsoil: loaded seed=%u pos=%d,%d mined=%u crafted=%u defense=%d",
      s.seed, int(s.player.x), int(s.player.y), s.mined, s.crafted, defense(s));
  pocket::log(status);
  return true;
}
} // namespace game
