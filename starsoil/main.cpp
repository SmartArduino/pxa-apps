// SPDX-License-Identifier: MIT
#include "actor_renderer.hpp"
#include "art.hpp"
#include "background.hpp"
#include "background_cache.hpp"
#include "color_math.hpp"
#include "core.hpp"
#include "craft_filter.hpp"
#include "wall_art.hpp"
#include "door_art.hpp"
#include "effects.hpp"
#include "font.hpp"
#include "runtime.hpp"
#include "pointer_capture.hpp"
#include "scene_lighting.hpp"
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
starsoil_scene::Lighting lighting;
starsoil_tree::CrownCache tree_crowns;
#ifndef PXA_POCKET_STUDIO
starsoil_scene::BackgroundCache background_cache;
#endif
int draw_x = 0, draw_y = 0;
int width() { return pocket::WIDTH; }
int height() { return pocket::HEIGHT; }
constexpr int FooterInset = 26;
int scene_bottom() { return height() - 30 - FooterInset; }
int scene_top() { return pocket::has_display_cutouts() ? 0 : 25; }
int render_bottom() { return height(); }
int gear_row_height() { return height() < 280 ? 14 : 20; }
int bag_row_height() { return height() < 280 ? 18 : 24; }
int recipes_per_page() { return height() < 280 ? 3 : 4; }
int storage_row_height() { return height() < 280 ? 18 : 25; }
int content_x() { return (width() - 288) / 2; }
int content_y() { return (height() - 224) / 2; }
void content_origin() {
  draw_x = content_x();
  draw_y = content_y();
}
void footer_origin() {
  draw_x = content_x();
  draw_y = height() - 224 - FooterInset;
}
enum View { Title, World, Bag, Crafting, Gear, Map, Guide, Dialogue, Storage,
            CreateWorld, SeedEditor, DeleteWorld };
constexpr int RecipeRowHeight = 32;
constexpr int BestiaryPageSize = 4;
constexpr int GuidePages = 4 + (EnemyCount - 1 + BestiaryPageSize - 1) / BestiaryPageSize;
static_assert(art::CreatureCount == EnemyCount - 1);
View view = Title;
uint32_t animation = 0, autosave = 0, event_at = 0, last_event = 0, paint = 0;
int cx = 0, cy = 0, selected = 0, recipe = 0, recipe_page = 0, guide_page = 0,
    npc = 0;
int touch = -1, aim_x = -1, aim_y = -1;
bool touch_down = false, jump_edge = false, gesture_jump = false,
     use_blocked = false;
starsoil_touch::Captures touches;
int stick_x = 62, stick_y = 0, stick_dx = 0, stick_dy = 0;
starsoil_touch::Layout touch_layout() { return {width(), height()}; }
bool save_error = false;
bool preview_face = true;
bool session_active = false;
bool smart_mode = true;
struct InventoryTouch {
  int source = -1, x = 0, y = 0, start_x = 0, start_y = 0;
  uint16_t elapsed = 0;
  bool dragging = false, cancelled = false;
} inventory_touch;
struct SceneTouch {
  static constexpr uint16_t HoldMs = 300;
  int start_x = 0, start_y = 0, world_x = 0, world_y = 0;
  uint16_t elapsed = 0;
  bool deferred = false, tap = false;
  bool ready() const { return !deferred || elapsed >= HoldMs; }
} scene_touch;
int last_bag_tap = -1;
int recipe_list[RecipeCount]{}, visible_recipes = RecipeCount;
bool craft_ready_only = false, craft_group_focus = false;
CraftGroup craft_category = CraftGroup::All;
uint32_t last_bag_tap_at = 0;
int inventory_slot_at(int x, int y);
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
  return starsoil_color::dim(c, unsigned(light));
}
uint16_t mix(uint16_t a, uint16_t b, int k) {
  return starsoil_color::mix(a, b, unsigned(k));
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
  if (scale == 1 && pocket::blit_pixels(x + draw_x, y + draw_y, data, w, h,
       flip, light, clip ? scene_top() : 0, clip ? render_bottom() : height())) return;
  for (int yy = 0; yy < h; ++yy) {
    int py = y + yy * scale;
    if (clip && (py < scene_top() || py >= render_bottom()))
      continue;
    for (int xx = 0; xx < w; ++xx) {
      uint16_t c = data[yy * w + (flip ? w - 1 - xx : xx)];
      if (c) {
        if (scale == 1) pixel(x + xx, py, dim(c, light));
        else rect(x + xx * scale, py, scale, scale, dim(c, light));
      }
    }
  }
}
void creature(int x, int y, int type, int frame, bool flip = false,
              int light = 255, int scale = 1, bool clip = false) {
  if (type <= NoEnemy || type >= EnemyCount) return;
  uint16_t palette[art::CreaturePaletteSize]{};
  for (int i = 1; i < art::CreaturePaletteSize; ++i)
    palette[i] = light == 255 ? art::creature_palettes[type-1][i]
                              : dim(art::creature_palettes[type-1][i], light);
  const auto *pixels = art::creature_pixels[(type-1)*art::CreatureFrames + frame%art::CreatureFrames];
  if (scale == 1 && pocket::blit_indexed_pixels(x + draw_x, y + draw_y, pixels, 24, 24,
       palette, flip, clip ? scene_top() : 0, clip ? render_bottom() : height())) return;
  for (int yy = 0; yy < 24; ++yy) {
    const int py = y + yy * scale;
    if (clip && (py < scene_top() || py >= render_bottom())) continue;
    for (int xx = 0; xx < 24; ++xx) {
      const uint8_t index = pixels[yy*24 + (flip ? 23-xx : xx)];
      if (index) rect(x + xx*scale, py, scale, scale, palette[index]);
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
void refresh_recipes() {
  visible_recipes = 0;
  int position = -1;
  for (int i = 0; i < RecipeCount; ++i)
    if (craft_matches(i,craft_category) && (!craft_ready_only || can_craft(s,i))) {
      if (i == recipe) position = visible_recipes;
      recipe_list[visible_recipes++] = i;
    }
  if (position < 0 && visible_recipes) {
    position = 0;
    recipe = recipe_list[0];
  }
  if (!visible_recipes) recipe = -1;
  recipe_page = position < 0 ? 0 : position / recipes_per_page();
}
int recipe_position() {
  for (int i = 0; i < visible_recipes; ++i) if (recipe_list[i] == recipe) return i;
  return 0;
}
void set_craft_group(CraftGroup group) {
  craft_category=group;refresh_recipes();event_at=0;
}
void toggle_craft_filter() {
  craft_ready_only = !craft_ready_only;
  refresh_recipes();
  event_at = 0;
}
void set_view(View v) {
  inventory_touch = {};
  scene_touch = {};
  last_bag_tap = -1;
  view = v;
  touch = -1;
  touch_down = false;
  touches.block();
  stick_dx = stick_dy = 0;
  gesture_jump = false;
  jump_edge = false;
  aim_x = aim_y = -1;
  s.mining = 0;
  selected = v == Gear ? 0 : s.selected;
  if (v == Crafting) { craft_group_focus=false;refresh_recipes(); }
  use_blocked = pocket::held(Key::B);
  sound(430);
  pocket::redraw();
}
int light_at(int x, int y) {
  return lighting.at(s, x, y);
}
void human(int x, int feet, int frame, bool face, const uint8_t *equipment,
           int held, int light = 255, int scale = 1, int skin = 0,
           bool using_item = false, int action_frame = -1, const State *aiming = nullptr) {
  starsoil_actor::Appearance a{x,         feet,         frame, scale,
                               light,     skin,         face,  using_item,
                               equipment, uint8_t(held)};
  a.action_frame = action_frame;
  if(aiming && items[held].kind==Bow) {
    const auto &p=aiming->player;
    auto d=starsoil_aim::direction(p.x,p.y,aiming->target_x*16+8,aiming->target_y*16+8,p.face,
                                 starsoil_aim::bob(p.grounded,p.vx,p.anim));
    a.aimed=true;a.aim_x=d.x;a.aim_y=d.y;
    a.bow_loaded=count(*aiming,Arrows)>0 && aiming->cooldown<280;
    a.bow_draw=a.bow_loaded?std::clamp((380-int(aiming->cooldown))*256/300,0,256):0;
  }
  starsoil_actor::draw(a, [](int px, int py, uint16_t c, starsoil_actor::Tag,
                             uint8_t) { pixel(px, py, c); });
}

void sky() {
  auto *canvas = pocket::framebuffer();
#ifndef PXA_POCKET_STUDIO
  const auto key = starsoil_scene::BackgroundKey::make(
      s, width(), render_bottom(), cx, cy, animation, scene_top());
  auto sparkles = [&] {
    starsoil_scene::draw_cave_sparkles(width(), render_bottom(), cx, cy,
      key.ground_y(), animation,
      [](int x, int y, uint16_t c) { pixel(x, y, c); },
      [canvas](int x, int y) { return canvas[y * width() + x]; }, scene_top());
  };
  if (background_cache.restore(key, canvas)) { sparkles(); return; }
#endif
  bool reading = false;
  pocket::begin_scene_batch();
  starsoil_scene::draw_world(
      s, width(), render_bottom(), cx, cy, animation,
      [](int x, int y, int w, int h, uint16_t c) { pocket::scene_rect(x+draw_x,y+draw_y,w,h,c); },
      [](int x, int y, int xx, int yy, uint16_t c) { pocket::scene_line(x+draw_x,y+draw_y,xx+draw_x,yy+draw_y,c); },
      [](int x, int y, uint16_t c) { pocket::scene_rect(x+draw_x,y+draw_y,1,1,c); },
      [canvas,&reading](int x, int y) {
        // Read/modify runs in the cave transition stay local after one barrier.
        if (!reading) { pocket::end_scene_batch(); reading = true; }
        return canvas[y * width() + x];
      }, scene_top(),
#ifdef PXA_POCKET_STUDIO
      true
#else
      false
#endif
      );
  pocket::end_scene_batch();
#ifndef PXA_POCKET_STUDIO
  background_cache.store(key, canvas);
  sparkles();
#endif
}
Input preview_input() {
  Input input;input.smart_cursor=s.smart_cursor;input.down=pocket::held(Key::DOWN) || (touches.owns(17) && stick_dy>14);
  return input;
}
void placement_ghost(const Placement &plan,bool wall,int tx,int ty) {
  const auto &def=items[s.bag[s.selected].item];
  auto blend=[](int x,int y,int w,int h,uint16_t c) {
    int top=std::max(y,scene_top()),bottom=std::min(y+h,render_bottom());
    if(top<bottom)pocket::blend_rect(x,top,w,bottom-top,c,90);
  };
  if(!plan.ok())return;
  if(wall) {
    starsoil_wall::draw(def.tile,15,s.target_x,s.target_y,tx,ty,255,blend);
    return;
  }
  if(def.tile==Door) {
    for(int y=0;y<32;++y)for(int x=0;x<16;++x) {
      auto c=starsoil_door::pixel(false,x,y);if(c)blend(tx+x,ty+y,1,1,c);
    }
    return;
  }
  if(def.tile>=38)return;
  const auto *image=art::tiles[def.tile*16];
  // Group equal horizontal pixels to keep the single ghost inexpensive.
  for(int y=0;y<16;++y)for(int x=0;x<16;) {
    int end=x+1;auto c=image[y*16+x];
    while(end<16 && image[y*16+end]==c)++end;
    if(c)blend(tx+x,ty+y,end-x,1,c);x=end;
  }
}
bool connects(int t, int n) {
  return t == n ||
         (starsoil_terrain::terrain(t) && starsoil_terrain::terrain(n));
}
void scene() {
  cx = std::clamp(int(s.player.x) - width() / 2, 0, s.width * 16 - width());
  cy = std::clamp(int(s.player.y) - (scene_bottom() - 47), -114,
                  s.height * 16 - scene_bottom() + 1);
  const int left = cx / 16, right = std::min(s.width - 1, (cx + width() - 1) / 16);
  const int top = std::max(0, (cy + scene_top()) / 16),
            bottom = std::min(s.height - 1, (cy + render_bottom() - 1) / 16);
  lighting.prepare(s, left, top, right - left + 1, bottom - top + 1);
  tree_crowns.prepare(left, top, right - left + 1, bottom - top + 1,
                     [](int x, int y) { return tile(s, x, y); });
  sky();
  for (int y = std::max(0, (cy + scene_top()) / 16);
       y <= std::min(s.height - 1, (cy + render_bottom() - 1) / 16); ++y)
    for (int x = cx / 16; x <= std::min(s.width - 1, (cx + width() - 1) / 16);
         ++x) {
      int sx = x * 16 - cx, sy = y * 16 - cy, t = tile(s, x, y),
          light = light_at(x, y);
      int wall=s.walls[y*WorldW+x];
      if(built_wall(uint8_t(wall))) {
        auto same=[&](int xx,int yy) {return xx>=0 && yy>=0 && xx<s.width && yy<s.height && s.walls[yy*WorldW+xx]==wall;};
        int mask=(same(x,y-1)?1:0)|(same(x+1,y)?2:0)|(same(x,y+1)?4:0)|(same(x-1,y)?8:0);
        starsoil_wall::draw(wall,mask,x,y,sx,sy,light,[](int xx,int yy,int w,int h,uint16_t c) {
          int top=std::max(yy,scene_top()),bottom=std::min(yy+h,render_bottom());
          if(top<bottom)rect(xx,top,w,bottom-top,c);
        });
      }
      auto tree_sample = [](int xx, int yy) { return tile(s, xx, yy); };
      auto crown = t == Air || t == Log || t == Leaves
                       ? tree_crowns.find(x, y, tree_sample) : starsoil_tree::Crown{};
      bool natural_tree = starsoil_tree::draw_natural(
          x, y, sx, sy, biome_at(x, y, s.width), light, tree_sample,
          [](int xx, int yy, uint16_t c) {
            if (yy >= scene_top() && yy < render_bottom())
              pixel(xx, yy, c);
          }, &crown);
      if (t) {
        int mask = (connects(t, tile(s, x, y - 1)) ? 1 : 0) |
                   (connects(t, tile(s, x + 1, y)) ? 2 : 0) |
                   (connects(t, tile(s, x, y + 1)) ? 4 : 0) |
                   (connects(t, tile(s, x - 1, y)) ? 8 : 0);
        int region = biome_at(x, y, s.width);
        auto sample = [](int xx, int yy) { return tile(s, xx, yy); };
        if (door(t)) {
          starsoil_door::draw(t, sx, sy, light, [](int xx, int yy, uint16_t c) {
            if (yy >= scene_top() && yy < render_bottom()) pixel(xx, yy, c);
          });
        } else {
        const uint16_t *tile_art =
            starsoil_tree::art_for(t, x, y, region, sample);
        if (!tile_art)
          tile_art = art::tiles[t * 16 + mask];
        if (!natural_tree) {
          if (starsoil_terrain::terrain(t)) {
            auto neighbors = starsoil_terrain::neighbors(x, y, sample);
            tile_art = art::tiles[t * 16 + neighbors.sides];
          }
          sprite(sx, sy, tile_art, 16, 16, false, light, 1, true);
        }
        }
        if (is_ore(t) && sy >= 52 && sy < render_bottom() - 16 &&
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
          if (sy > 29 && sy < render_bottom() - 10)
            pixel(sx + 6 + a, sy + 2 - a, Ink);
        }
        if ((t == Water || t == Lava) && sy >= scene_top() && sy < render_bottom() - 2) {
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
            if (yy >= scene_top() && yy < render_bottom())
              pixel(xx, yy, c);
          });
    }
  for (auto n : s.npcs) {
    int x = int(n.x) - cx, y = int(n.y) - cy;
    if (x < -20 || x > width() + 20 || y < scene_top() || y > render_bottom() - 3)
      continue;
    uint8_t eq[8]{};
    eq[0] = n.type == 3 ? StrawHat : n.type == 2 ? IronHelmet : 0;
    eq[3] = n.type == 1 ? RedTunic : n.type == 3 ? SageRobe : BlueTunic;
    eq[4] = n.type == 0 ? TealCape : 0;
    human(x, y,
          starsoil_actor::locomotion_frame(n.grounded, std::fabs(n.vx) > 1,
                                           n.anim / 512, n.vy),
          n.face, eq, n.type == 2 ? IronAxe : 0,
          light_at(int(n.x / 16), int(n.y / 16)), 1, n.type == 2 ? 2 : 1);
  }
  for (auto m : s.mobs)
    if (m.hp > 0) {
      int x = int(m.x) - cx, y = int(m.y) - cy;
      if (x < -48 || x > width() + 42 || y < scene_top() || y > render_bottom())
        continue;
      int sc = m.type == 5 ? 2 : 1;
      creature(x - 12 * sc, y - 24 * sc, m.type, m.anim / 120,
               !m.face, light_at(int(m.x / 16), int(m.y / 16)), sc, true);
      int charge = m.type == DuneBeetle ? int(m.timer) - 1000
                   : m.type == LanternMoth ? int(m.timer) - 1300
                   : m.type == ThornHornet ? int(m.timer) - 1500 : -1;
      const int windup = m.type == DuneBeetle ? 250 : 300;
      if (charge >= 0 && charge < windup && y - 27 >= scene_top())
        rect(x - 6, y - 27, 1 + charge * 11 / windup, 2, GoldInk);
    }
  for (const auto &shot : s.hostile_shots)
    if (shot.life) {
      const int x = int(shot.x) - cx, y = int(shot.y) - cy;
      if (x >= 4 && x < width() - 4 && y >= scene_top() + 4 && y < render_bottom() - 4) {
        line(x - int(shot.vx * .035f), y - int(shot.vy * .035f), x, y, GoldInk);
        rect(x - 1, y - 1, 2, 2, Teal);
      }
    }
  for (auto d : s.drops)
    if (d.item) {
      int x = int(d.x) - cx, y = int(d.y) - cy;
      if (y > scene_top() && y < render_bottom() - 12)
        icon(x - 8, y - 8, d.item, light_at(int(d.x / 16), int(d.y / 16)));
    }
  for (auto b : s.shots)
    if (b.life) {
      int x = int(b.x) - cx, y = int(b.y) - cy;
      if (y > scene_top() && y < render_bottom() - 4) {
        if(b.magic) {
          rect(x-1,y-1,3,3,Teal);
          line(x,y,x-int(b.vx*.03f),y-int(b.vy*.03f),Ink);
        } else {
          auto d=starsoil_aim::unit(b.vx,b.vy);
          auto at=[&](float along,float across) {return starsoil_actor::Point{
              x+int(std::lround(along*d.x-across*d.y)),y+int(std::lround(along*d.y+across*d.x))};};
          auto tail=at(-9,0),left=at(-3,-2),right=at(-3,2);
          line(x,y,tail.x,tail.y,color(0xd6c9a5));
          line(x,y,left.x,left.y,color(0xb6c6ca));line(x,y,right.x,right.y,color(0xb6c6ca));
          auto f=at(-7,-2),g=at(-7,2),tip=at(-5,0);
          line(tail.x,tail.y,f.x,f.y,GoldInk);line(tail.x,tail.y,g.x,g.y,GoldInk);
          line(f.x,f.y,tip.x,tip.y,GoldInk);line(g.x,g.y,tip.x,tip.y,GoldInk);
        }
      }
    }
  if (!s.invulnerable || s.invulnerable / 100 % 2)
    human(int(s.player.x) - cx, int(s.player.y) - cy,
          starsoil_actor::locomotion_frame(s.player.grounded,
                                           std::fabs(s.player.vx) > 1,
                                           s.player.anim / 512, s.player.vy),
          s.player.face, s.equip, s.bag[s.selected].item,
          light_at(int(s.player.x / 16), int(s.player.y / 16)), 1, 0,
          tool_animation.active, tool_animation.frame(), &s);
  for (const auto &p : effects.particles)
    if (p.life) {
      int x = int(p.x) - cx, y = int(p.y) - cy;
      if (y >= 52 && y < render_bottom() - 1)
        rect(x, y, p.life > 300 ? 2 : 1, 1,
             dim(color(p.color), std::min(255, int(p.life) * 2)));
    }
  int tx = s.target_x * 16 - cx, ty = s.target_y * 16 - cy;
  const auto &held=items[s.bag[s.selected].item];
  bool build=building(held.kind);int cursor_height=16;
  Placement plan{PlaceFailure::Blocked,s.target_y};
  if(build) {
    auto input=preview_input();plan=placement_preview(s,input,s.target_x,s.target_y);
    bool wall=held.kind==Wall || (held.tile==Plank && input.down);
    if(held.tile==Door && !wall) {ty=(plan.bottom-1)*16-cy;cursor_height=32;}
    placement_ghost(plan,wall,tx,ty);
  }
  if (ty >= scene_top() && ty < render_bottom()-1) {
    uint16_t cursor=build?(plan.ok()?Teal:Red):s.smart_cursor?(s.target_ready?Teal:Muted):GoldInk;
    outline(tx, ty, 16, std::min(cursor_height,render_bottom()-ty), dim(cursor,s.smart_cursor && !s.target_ready?90:180));
    pixel(tx + 2, ty + 2,cursor);
    if (s.mining)
      rect(tx, ty + 15, std::min(16, int(s.mining) * 16 / (items[s.bag[s.selected].item].kind==Hammer?300:mining_work(tile(s,s.target_x,s.target_y)))), 2, Ink);
  }
}
void scene_labels() {
  const auto &held=items[s.bag[s.selected].item];
  if(building(held.kind)) {
    auto plan=placement_preview(s,preview_input(),s.target_x,s.target_y);
    const char *en="PLACE",*cn="可放置";
    switch(plan.failure) {
      case PlaceFailure::None: {
        if(held.kind==Wall && built_wall(s.walls[s.target_y*WorldW+s.target_x])) {en="REPLACE WALL";cn="替换墙壁";}
        break;
      }
      case PlaceFailure::Duplicate:en="ALREADY BUILT";cn="已有相同墙";break;
      case PlaceFailure::Support:en="NEEDS SUPPORT";cn="需要支撑";break;
      case PlaceFailure::DoorSpace:en="2-CELL DOOR";cn="门需两格";break;
      case PlaceFailure::Space:en="NO REFUND SPACE";cn="回收空间不足";break;
      case PlaceFailure::Range:en="OUT OF REACH";cn="超出距离";break;
      default:en="BLOCKED";cn="无法放置";break;
    }
    int y=s.target_y*16-cy-15;
    if(y>70 && y<render_bottom()-16) {
      const char *text=tr(en,cn);int x=std::clamp(s.target_x*16-cx+8-text_width(text)/2,4,std::max(4,width()-text_width(text)-4));
      rect(x-2,y-1,text_width(text)+4,14,Dark);label(x,y,text,plan.ok()?Teal:Red);
    }
  }
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
void disc(starsoil_touch::Point p,int radius,bool active) {
  int span=radius;
  for(int dy=0;dy<=radius;++dy) {
    while(span*span+dy*dy>radius*radius)--span;
    for(int sign : {-1,1}) {
      if(!dy && sign==1)continue;
      int y=p.y+dy*sign;
      pocket::blend_rect(p.x-span,y,span*2+1,1,Dark,active?215:150);
      pixel(p.x-span,y,active?GoldInk:Muted);
      pixel(p.x+span,y,active?GoldInk:Muted);
    }
  }
}
void floating_controls() {
  if (pocket::using_controller()) return;
  auto layout=touch_layout();
  auto stick=touches.owns(17)?starsoil_touch::Point{stick_x,stick_y}:layout.stick();
  disc(stick,29,touches.owns(17));
  line(stick.x-21,stick.y,stick.x-17,stick.y,Muted);
  line(stick.x+17,stick.y,stick.x+21,stick.y,Muted);
  line(stick.x,stick.y+17,stick.x,stick.y+21,Muted);
  line(stick.x,stick.y-21,stick.x,stick.y-17,Muted);
  auto knob=starsoil_touch::Point{stick.x+stick_dx,stick.y+stick_dy};
  disc(knob,12,touches.owns(17));
  line(knob.x-3,knob.y,knob.x+3,knob.y,Teal);
  auto use=layout.use(),jump=layout.jump(),talk=layout.talk();
  disc(use,23,touches.owns(15)||touches.owns(16));
  icon(use.x-8,use.y-15,s.bag[s.selected].item);
  center(use.x-21,use.y+4,42,hint("USE","使用","B","B"),Ink);
  disc(jump,20,touches.owns(14));
  line(jump.x-6,jump.y-1,jump.x,jump.y-7,Ink);
  line(jump.x,jump.y-7,jump.x+6,jump.y-1,Ink);
  line(jump.x,jump.y-7,jump.x,jump.y+2,Ink);
  center(jump.x-19,jump.y+3,38,hint("JUMP","跳跃","A","A"),Ink);
  disc(talk,16,touches.owns(13));
  outline(talk.x-7,talk.y-6,14,10,Teal);
  line(talk.x-4,talk.y+4,talk.x-4,talk.y+8,Teal);
  line(talk.x-4,talk.y+8,talk.x,talk.y+4,Teal);
  pixel(talk.x-3,talk.y-1,Ink);pixel(talk.x,talk.y-1,Ink);pixel(talk.x+3,talk.y-1,Ink);
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
  // Keep touch targets below the system's top notification-gesture strip.
  center(width() - 116, 16, 36, tr("BAG", "背包"), Ink);
  center(width() - 80, 16, 40, tr("CRAFT", "合成"), Teal);
  for (int i = 0; i < HotSlots; ++i) {
    int x = 20 + i * 24;
    rect(x, 34, 22, 23, i == s.selected ? rgb(71, 89, 80) : Dark);
    outline(x, 34, 22, 23, i == s.selected ? GoldInk : rgb(76, 86, 87));
    icon(x + 3, 36, s.bag[i].item);
    if (s.bag[i].count > 1) {
      std::snprintf(b, sizeof(b), "%u", s.bag[i].count);
      label(x + 2, 42, b, Ink);
    }
  }
  if (s.selected >= HotSlots) {
    panel(216, 34, 22, 23, rgb(43, 72, 69));
    outline(216, 34, 22, 23, Teal);
    icon(219, 36, s.bag[s.selected].item);
    if (s.bag[s.selected].count > 1) {
      std::snprintf(b, sizeof(b), "%u", s.bag[s.selected].count);
      label(218, 42, b, Ink);
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
  floating_controls();
}
void heading(const char *txt) {
  pocket::begin_ui();
  draw_x = draw_y = 0;
  pocket::clear(Dark);
  rect(0, 0, width(), 28, Panel);
  label(8, 12, txt, GoldInk);
  label(width() - 52, 14, "<", Ink);
  line(0, 27, width() - 1, 27, rgb(91, 113, 108));
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
    int x = 6 + i % 8 * 25, y = 50 + i / 8 * bag_row_height();
    rect(x, y, 23, bag_row_height() - 1, i == selected ? rgb(66, 90, 82) : Panel);
    outline(x, y, 23, bag_row_height() - 1, i == selected ? GoldInk : rgb(64, 78, 84));
    if (inventory_touch.dragging && inventory_slot_at(inventory_touch.x, inventory_touch.y) == i)
      outline(x, y, 23, bag_row_height() - 1, Teal);
    if (i == s.selected) rect(x + 1, y + 1, 2, 3, Teal);
    icon(x + 3, y + 1, s.bag[i].item,
         inventory_touch.dragging && inventory_touch.source == i ? 100 : 255);
    if (s.bag[i].count > 1) {
      char b[12];
      std::snprintf(b, sizeof(b), "%u", s.bag[i].count);
      label(x + 1, y + bag_row_height() - 2 - pocket::text_height(), b);
    }
  }
  human(238, height() < 280 ? 118 : 126, art::StandFrame, preview_face, s.equip,
        s.bag[s.selected].item, 255, 2);
  center(210, height() < 280 ? 127 : 135, 75, tr("EQUIPPED", "实装预览"), Teal);
  char b[40];
  std::snprintf(b, sizeof(b), tr("DEF %d", "防御 %d"), defense(s));
  center(210, height() < 280 ? 143 : 151, 75, b, Muted);
  int id = s.bag[selected].item;
  footer_origin();
  label(7, 174, tr(items[id].en, items[id].zh), GoldInk);
  if (equip_slot(items[id].kind) >= 0)
    label(7, 187, hint("DOUBLE TAP: EQUIP; HOLD TO MOVE", "双击穿戴；长按拖动",
                       "A / TAP AGAIN: EQUIP", "A或再次点击：穿戴到身上"), Muted);
  else if(items[id].kind==Wall)
    label(7,187,tr("PLACE / REPLACE; HAMMER TO REMOVE", "放置或替换墙壁；锤子拆除"),Muted);
  else if(items[id].kind==Hammer)
    label(7,187,tr("HOLD: BACKING. TAP: INTERACT", "长按敲背景墙；轻点互动"),Muted);
  else
    label(7, 187, hint("TAP TO HOLD; LONG PRESS TO MOVE", "点选即可手持；长按拖动整理",
                       "A USE   Y TO HOTBAR", "A使用  Y放入当前快捷栏"), Muted);
  button(6, 204, 128, tr("USE / EQUIP", "使用 / 穿戴"), true);
  button(143, 204, 139, hint("SORT BAG", "整理背包", "SELECT SORT", "SELECT整理"));
}
void storage_draw() {
  heading(tr("TRAVELLER'S STORAGE", "远行者储物箱"));
  label(7, 30,
        tr("TAP TO STORE / TAKE; HOLD TO MOVE", "点按存取；长按拖动整理"),
        Teal);
  for (int i = 0; i < BagSlots; ++i) {
    int x = 6 + i % 8 * 24, y = 50 + i / 8 * storage_row_height();
    panel(x, y, 22, storage_row_height() - 2);
    if (selected == i)
      outline(x, y, 22, storage_row_height() - 2, GoldInk);
    if (inventory_touch.dragging && inventory_slot_at(inventory_touch.x, inventory_touch.y) == i)
      outline(x, y, 22, storage_row_height() - 2, Teal);
    icon(x + 3, y + 2, s.bag[i].item,
         inventory_touch.dragging && inventory_touch.source == i ? 100 : 255);
    if (s.bag[i].count > 1) {
      char b[8];
      std::snprintf(b, sizeof(b), "%u", s.bag[i].count);
      label(x + 1, y + storage_row_height() - 4 - pocket::text_height(), b);
    }
  }
  for (int i = 0; i < 12; ++i) {
    int x = 216 + i % 2 * 33, y = 50 + i / 2 * storage_row_height();
    panel(x, y, 29, storage_row_height() - 2, rgb(74, 69, 54));
    if (selected == i + BagSlots)
      outline(x, y, 29, storage_row_height() - 2, GoldInk);
    if (inventory_touch.dragging && inventory_slot_at(inventory_touch.x, inventory_touch.y) == i + BagSlots)
      outline(x, y, 29, storage_row_height() - 2, Teal);
    icon(x + 6, y + 1, s.chest[i].item,
         inventory_touch.dragging && inventory_touch.source == i + BagSlots ? 100 : 255);
    if (s.chest[i].count > 1) {
      char b[8];
      std::snprintf(b, sizeof(b), "%u", s.chest[i].count);
      label(x + 1, y + storage_row_height() - 4 - pocket::text_height(), b);
    }
  }
  footer_origin();
  label(7, 174, tr("Keeps hotbar + held item", "保留快捷栏与手持物"), Muted);
  button(6, 199, 88, hint("QUICK STACK", "快捷收纳", "Y STACK", "Y收纳"));
  button(100, 199, 88, hint("TAKE ALL", "全部取出", "SELECT TAKE", "SELECT取出"));
  button(194, 199, 88, hint("SORT CHEST", "整理箱子", "R SORT", "R整理"));
}
void inventory_drag_draw() {
  const auto &drag = inventory_touch;
  if (!drag.dragging || (view != Bag && view != Storage)) return;
  const auto &slot = drag.source < BagSlots ? s.bag[drag.source] : s.chest[drag.source - BagSlots];
  if (!slot.item) return;
  draw_x = draw_y = 0;
  int x = std::clamp(drag.x - 11, 2, width() - 25);
  int y = std::clamp(drag.y - 31, 45, height() - 27);
  panel(x, y, 23, 25, rgb(45, 67, 67));
  outline(x, y, 23, 25, GoldInk);
  icon(x + 3, y + 1, slot.item);
  if (slot.count > 1) {
    char count[8]; std::snprintf(count, sizeof(count), "%u", slot.count);
    label(x + 1, y + 24 - pocket::text_height(), count, Ink);
  }
}
const char *station_name(int id) {
  const char *en[] = {"HAND", "WORKBENCH", "FURNACE", "ANVIL", "STAR ALTAR"},
             *cn[] = {"徒手", "工作台", "熔炉", "铁砧", "星核祭坛"};
  return tr(en[id], cn[id]);
}
void crafting_draw() {
  heading(tr("CRAFTING", "合成手册"));
  tabs(Crafting);
  draw_x = draw_y = 0;
  char filter[48];
  std::snprintf(filter, sizeof(filter), craft_ready_only ? hint("READY %d", "可制作 %d", "SELECT READY %d", "SELECT可做 %d") : hint("ALL %d", "全部 %d", "SELECT ALL %d", "SELECT全部 %d"), visible_recipes);
  rect(width() - 162, 12, 98, 15, craft_ready_only ? rgb(64, 90, 82) : Dark);
  outline(width() - 162, 12, 98, 15, craft_ready_only ? Teal : Muted);
  center(width() - 162, 13, 98, filter, craft_ready_only ? Teal : Muted);
  content_origin();
  int first = recipe_page * recipes_per_page();
  char b[96];
  for (int row = 0; row < recipes_per_page(); ++row) {
    int position = first + row;
    if (position >= visible_recipes)
      break;
    int r = recipe_list[position];
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
  if (!visible_recipes)
    wrap(16, 75, 256, tr("No recipes are ready here. Switch to ALL to see ingredients and required stations.",
                           "当前没有可制作的配方。切换到全部，查看材料和所需设施。"), Muted, 3);
  footer_origin();
  const char *en[]={"ALL","BUILD","TOOLS","GEAR","SUPPLY"};
  const char *cn[]={"全部","建造","工具","装备","补给"};
  for(int group=0;group<int(CraftGroup::Count);++group) {
    int x=6+group*56;bool active=int(craft_category)==group;
    rect(x,174,52,17,active?rgb(63,92,85):Panel);
    outline(x,174,52,17,active?(craft_group_focus?GoldInk:Teal):rgb(64,78,84));
    center(x,176,52,tr(en[group],cn[group]),active?GoldInk:Muted);
  }
  button(6, 199, 43, "<");
  std::snprintf(b, sizeof(b), "%d/%d", visible_recipes ? recipe_page + 1 : 0,
                (visible_recipes + recipes_per_page() - 1) / recipes_per_page());
  center(50, 204, 60, b, Muted);
  button(111, 199, 76, hint("CRAFT", "合成", "CRAFT   A", "合成   A"),
         can_craft(s, recipe));
  button(193, 199, 41, hint("x5", "x5", "Y x5", "Y x5"),
         can_craft(s, recipe));
  button(239, 199, 43, ">");
}
void gear_draw() {
  heading(tr("DRESS FOR THE JOURNEY", "旅人的衣橱"));
  tabs(Gear);
  int preview_floor = height() < 280 ? 145 : 173;
  panel(8, 49, 113, preview_floor - 47, rgb(29, 47, 57));
  line(18, preview_floor, 112, preview_floor, rgb(66, 85, 88));
  human(62, preview_floor, art::StandFrame, preview_face, s.equip, s.bag[s.selected].item,
        255, height() < 280 ? 3 : 4);
  const char *en[] = {"HELM", "CHEST", "LEGS", "CLOTHES",
                      "CAPE", "BOOTS", "-",    "CHARM"},
             *cn[] = {"头盔", "胸甲", "护腿", "衣服",
                      "披风", "靴子", "",     "护符"};
  for (int i = 0; i < 8; ++i) {
    if (i == 6)
      continue;
    int y = 49 + (i < 6 ? i : i - 1) * gear_row_height();
    rect(129, y, 151, gear_row_height() - 1, selected == i ? rgb(59, 81, 77) : Panel);
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
       "UP/DOWN aim; diagonals dig slopes.",
       "Hold B: mine, chop, attack or place.",
       "Menu toggles the smart cursor.",
       "Bag: tap to hold; twice to use/equip.",
       "START pauses. Autosave every 8 sec."},
      {"02  FROM WOOD TO CRYSTAL", "Wood + stone: workbench and furnace.",
       "Ore + coal: ingots. Anvil: gear.",
       "Copper pick: gold; iron: crystal.",
       "UP at list top: recipe categories.",
       "Gather flowers, fungi, vines, acorns.",
       "Plant acorns in grass or dirt.",
       "Craft walls; hammers remove backing."},
      {"03  PEOPLE & ANCIENT LIGHT",
       "Guide: mine 20 blocks, craft 10.",
       "Merchant: 1 gold ingot > 8 silk.",
       "Smith: 9 copper ore > 2 iron ingots.",
       "Gardener: 3 flowers > 2 potions.",
       "SELECT / TALK: open hidden chests.",
       "Craft star heart at altar; summon!",
       "Warden drops: ember cape, star heart."},
      {"04  SURVIVE & BUILD", "Tap doors: open/close. Hold to mine.",
       "Built backing walls stop indoor spawns.",
       "Mosslings can open wooden doors.",
       "Beds set home. Recall potions return.",
       "DOWN: platforms. Water slows falls.",
       "Hold jump: higher. Cloud boots: 2 jumps.",
       "Death sends you home, items kept."}};
  const char *cn[][8] = {
      {"一 · 初来星壤", "方向键移动，A跳跃，B使用。",
       "X背包，Y合成，L/R切换手持。", "上下调方向，斜向移动可挖斜坡。",
       "长按B采集、战斗，或放置方块。", "智能选块接续采集，菜单可切换。",
       "背包点选手持，双击使用或穿戴。", "START暂停；每八秒自动保存。"},
      {"二 · 从木材到晶体", "砍树挖石，建工作台与熔炉。",
       "矿石加煤炼锭，铁砧锻造装备。", "铜镐挖金矿，铁镐挖晶体。",
       "配方顶部按上，可切换分类。", "采集花草、蘑菇、藤蔓、橡子。",
       "橡子种在草地或泥土，随后长树。", "合成背景墙，用锤子拆除回收。"},
      {"三 · 来客与古老流光", "向导：挖二十块，再合成十次。",
       "商人：一金锭换八份丝绸。", "铁匠：九铜矿换两铁锭。",
       "园丁：三野花换两生命药水。", "互动打开地下宝箱，寻找宝物。",
       "祭坛合成星核，再互动召唤守卫。", "击败守卫，获得星核和披风。"},
      {"四 · 生存与家园", "木门占两格，互动打开或关闭。",
       "人工背景墙可防止室内刷怪。", "苔木精会开门，其余普通怪不会。",
       "水减缓下落；熔岩会造成伤害。", "弓消耗箭；星杖使用可恢复的魔力。",
       "云靴二段跳；辉光项链照亮周围。", "死亡回家，背包物品会保留。"}};
  const char *touch_en[] = {
      "01  YOUR FIRST EVENING",
      "Left stick: move; push up to jump.",
      "Open BAG / CRAFT at the top right.",
      "Tap to aim. Drag to change target.",
      "Hold USE to mine, fight or build.",
      "Tap furniture; hold tools to remove.",
      "Bag: tap twice to equip clothes.",
      "Menu pauses. Autosave every 8 sec."};
  const char *touch_cn[] = {
      "一 · 初来星壤", "左摇杆移动，上推或点跳跃起跳。",
      "右上角打开背包或合成手册。", "触屏点选目标，拖动切换目标。",
      "长按使用采集、战斗或放置。", "工具轻点互动，长按采集或拆墙。",
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
    const int first = 1 + (guide_page - 4) * BestiaryPageSize;
    for (int i = 0; i < BestiaryPageSize && first + i < EnemyCount; ++i) {
      const int type = first + i, x = 6 + (i % 2) * 140, y = 44 + (i / 2) * 64;
      const auto &def = enemies[type];
      panel(x, y, 134, 59, rgb(29, 46, 57));
      center(x, y + 4, 134, tr(def.en, def.zh), type == Warden ? GoldInk : Teal);
      creature(x + 4, y + 26, type, animation / 120);
      label(x + 33, y + 23, tr(def.habitat_en, def.habitat_zh), Muted);
      label(x + 33, y + 39, tr(def.behavior_en, def.behavior_zh), Ink);
    }
  }
  footer_origin();
  button(7, 200, 58, "<");
  char b[8];
  std::snprintf(b, sizeof(b), "%d / %d", guide_page + 1, GuidePages);
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
  last_bag_tap = -1;
  if (selected < 0 || selected >= BagSlots)
    return;
  int target = s.selected < HotSlots ? s.selected : 0;
  std::swap(s.bag[selected], s.bag[target]);
  s.dirty = true;
  sound(530);
  pocket::save();
}
void bag_activate() {
  last_bag_tap = -1;
  auto kind = items[s.bag[selected].item].kind;
  if (activate(s, selected)) {
    sound(640);
    pocket::save();
    if (tool(kind) || kind == Sword || kind == Bow ||
        kind == Wand || building(kind))
      set_view(World);
  } else
    notify("This item cannot be used right now.", "这个物品暂时无法使用。");
}
void select_bag_item(int slot) {
  selected = slot;
  if (slot < 0 || slot >= BagSlots || s.selected == slot) return;
  s.selected = uint8_t(slot);
  s.dirty = true;
  pocket::save();
}
void inventory_action(int action) {
  last_bag_tap = -1;
  bool changed = false;
  int moved = 0;
  if (action == 81) {
    changed = sort_inventory(s);
    notify("Bag sorted. Hotbar and held stack kept.", "背包已整理，快捷栏和手持物保留原位。");
  } else if (action == 86 || action == 87) {
    moved = action == 86 ? quick_stack(s) : take_all(s);
    changed = moved > 0;
    if (changed) {
      std::snprintf(message, sizeof(message), action == 86 ? tr("STORED %d ITEMS", "已收纳 %d 件物品") : tr("TOOK %d ITEMS", "已取出 %d 件物品"), moved);
      event_at = animation + 3000;
    } else
      notify(action == 86 ? "No matches / chest full." : "Bag full / chest empty.",
             action == 86 ? "无可收纳物品，或箱子已满。" : "背包已满，或箱子已空。");
  } else if (action == 88) {
    changed = sort_inventory(s, true);
    notify("Chest sorted.", "储物箱已整理。");
  }
  if (changed) { sound(600); pocket::save(); }
  pocket::redraw();
}
void do_craft(int batches = 1) {
  int completed = craft_batch(s, recipe, batches);
  if (completed) {
    const auto &p = recipes[recipe];
    std::snprintf(message, sizeof(message), "+%u %s", unsigned(completed * p.count),
                  tr(items[p.out].en, items[p.out].zh));
    event_at = animation + 3000;
    last_event = s.event_serial;
    sound(720);
    pocket::save();
    refresh_recipes();
  } else {
    auto reason = craft_status(s, recipe);
    if (reason == CraftStatus::Station) {
      std::snprintf(message, sizeof(message), tr("NEED: %s", "需要靠近：%s"), station_name(recipes[recipe].station));
      event_at = animation + 4200;
    } else if (reason == CraftStatus::Ingredients) {
      for (auto a : recipes[recipe].in) if (a.item && count(s, a.item) < a.count) {
        std::snprintf(message, sizeof(message), tr("NEED %d MORE %s", "还缺 %d 份%s"), a.count - count(s, a.item), tr(items[a.item].en, items[a.item].zh));
        event_at = animation + 4200;
        break;
      }
    } else if (reason == CraftStatus::Space)
      notify("Free some bag space for the result.", "背包空间不足，请先腾出成品位置。");
    else notify("Switch to ALL to find more recipes.", "切换到全部，查看材料与所需设施。");
    sound(180);
  }
}
bool interactive_furniture(int t) {
  return door(t) || t == Chest || t == StorageChest || t == Bed ||
         t == Bench || t == Furnace || t == Anvil || t == Portal;
}
bool target_interaction(int world_x, int world_y) {
  int x = world_x / 16, y = world_y / 16;
  int t = tile(s, x, y);
  if (interactive_furniture(t)) {
    if (!interaction_range(s, x, y)) {
      notify("Move closer to interact.", "靠近后再点击互动。");
      return true;
    }
    s.target_x = x; s.target_y = y;
    if (t == StorageChest) { set_view(Storage); selected = 0; }
    else if (t == Bench || t == Furnace || t == Anvil) {
      int facility = t == Bench ? Table : t == Furnace ? Kiln : Forge;
      craft_ready_only = false;craft_category=CraftGroup::All;
      set_view(Crafting);
      for (int i = 0; i < RecipeCount; ++i)
        if (recipes[i].station == facility) { recipe = i; break; }
      refresh_recipes();
    } else if (interact_at(s, x, y)) pocket::save();
    return true;
  }
  for (int i = 0; i < 4; ++i) {
    const auto &person = s.npcs[i];
    if (person.hp <= 0 || std::fabs(world_x - person.x) > 12 ||
        world_y < person.y - 32 || world_y > person.y + 2) continue;
    if (!interaction_range(s, int(person.x / 16), int((person.y - 12) / 16)))
      notify("Move closer to talk.", "靠近后再点击交谈。");
    else { npc = i; set_view(Dialogue); }
    return true;
  }
  return false;
}
void do_interact() {
  if (interactive_furniture(tile(s, s.target_x, s.target_y)) &&
      interaction_range(s, s.target_x, s.target_y)) {
    target_interaction(s.target_x * 16 + 8, s.target_y * 16 + 8);
    return;
  }
  // A nearby NPC behind the player must not steal the door interaction.
  int px = int(s.player.x / 16), py = int((s.player.y - 12) / 16);
  for (int dx = 0; dx <= 3; ++dx)
    for (int dy = -1; dy <= 1; ++dy) {
      int x = px + (s.player.face ? dx : -dx), y = py + dy;
      if (!door(tile(s, x, y))) continue;
      s.target_x = x; s.target_y = y;
      if (interact(s)) pocket::save();
      return;
    }
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
    if (view == Crafting && y >= 14 && y < 28 && x >= width() - 162 && x < width() - 64)
      return 94;
    if (y >= 14 && y < 28 && x > width() - 64 && x < width() - 40)
      return 99;
    if (view >= Bag && view <= Guide && y >= 28 && y < 44)
      return 100 + std::clamp((x - 5) * 5 / (width() - 8), 0, 4);
    x -= content_x();
    y -= content_y();
    if (view == Storage && physical_y >= height() - 25 - FooterInset && physical_y < height() - 5 - FooterInset) {
      if (x >= 6 && x < 94) return 86;
      if (x >= 100 && x < 188) return 87;
      if (x >= 194 && x < 282) return 88;
      return -1;
    }
    if ((view == Bag && y + content_y() >= height() - 50 - FooterInset) ||
        (view == Crafting && y + content_y() >= height() - 50 - FooterInset) ||
        (view == Gear && y + content_y() >= height() - 26 - FooterInset) ||
        (view == Guide && y + content_y() >= height() - 26 - FooterInset))
      y += content_y() - (height() - 224 - FooterInset);
    if (view == Bag) {
      if (x >= 210 && y >= 50 && y < 170)
        return 84;
      if (x >= 6 && x < 206 && y >= 50 && y < 50 + 5 * bag_row_height())
        return (y - 50) / bag_row_height() * 8 + (x - 6) / 25;
      if (y >= 204 && physical_y >= height() - 20 - FooterInset)
        return x < 140 ? 80 : 81;
    }
    if (view == Crafting) {
      if(y>=174 && y<191 && x>=6 && x<282) {
        int group=(x-6)/56;
        return group<int(CraftGroup::Count) && (x-6)%56<52 ? 220+group : -1;
      }
      if (x >= 6 && x < 282 && y >= 49 && y < 49 + recipes_per_page() * RecipeRowHeight) {
        int position = recipe_page * recipes_per_page() + (y - 49) / RecipeRowHeight;
        return position < visible_recipes ? 120 + recipe_list[position] : -1;
      }
      if (y >= 199 && y < 219 && physical_y >= height() - 25 - FooterInset) {
        if (x >= 6 && x < 49) return 90;
        if (x >= 239 && x < 282) return 91;
        if (x >= 111 && x < 187) return 92;
        if (x >= 193 && x < 234) return 93;
      }
    }
    if (view == Gear && x >= 129 && y >= 49 && y < 49 + 7 * gear_row_height()) {
      int row = (y - 49) / gear_row_height();
      return 140 + (row < 6 ? row : 7);
    }
    if (view == Gear && y >= 203 && physical_y >= height() - 21 - FooterInset)
      return 82;
    if (view == Gear && x < 126 && y >= 49 && y < (height() < 280 ? 147 : 175))
      return 84;
    if (view == Guide && y >= 200 && physical_y >= height() - 24 - FooterInset)
      return x < 80 ? 90 : x > 210 ? 91 : -1;
    if (view == Storage && x >= 6 && x < 198 && y >= 50 && y < 50 + 5 * storage_row_height())
      return 160 + (y - 50) / storage_row_height() * 8 + (x - 6) / 24;
    if (view == Storage && x >= 216 && x < 282 && y >= 50 && y < 50 + 6 * storage_row_height())
      return 200 + (y - 50) / storage_row_height() * 2 + (x - 216) / 33;
    if (view == Dialogue && y >= 173 && y < 193)
      return 83;
    if (view == Dialogue && y >= 200)
      return 99;
    return -1;
  }
  if (y < 31)
    return y >= 14 && x >= width() - 116 && x < width() - 40
               ? (x < width() - 80 ? 20 : 21) : -1;
  if (y >= 34 && y < 57 && x >= 20 && x < 212)
    return 30 + std::clamp((x - 20) / 24, 0, 7);
  if (s.selected >= HotSlots && y >= 34 && y < 57 && x >= 216 && x < 238)
    return 20;
  if (x >= width() - 90 && y >= 62 && y < 99)
    return -1;
  int control = touch_layout().hit(x,y);
  if (control >= 0) return control;
  if (y >= 57 && y < height()) return 16;
  return -1;
}
int inventory_slot_at(int x, int y) {
  int id = hit(x, y);
  if (view == Bag && id >= 0 && id < BagSlots) return id;
  if (view == Storage && id >= 160 && id < 212) return id - 160;
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
      bool twice = last_bag_tap == id && animation - last_bag_tap_at <= 360;
      last_bag_tap = twice ? -1 : id;
      last_bag_tap_at = animation;
      select_bag_item(id);
      if (twice) bag_activate();
    } else if (id == 80)
      bag_activate();
    else if (id == 81)
      inventory_action(81);
  } else if (view == Crafting) {
    if (id >= 120 && id < 120 + RecipeCount) {
      int next = id - 120;
      if (recipe == next)
        do_craft();
      recipe = next;
    } else if (id == 90 || id == 91) {
      int pages = (visible_recipes + recipes_per_page() - 1) / recipes_per_page();
      if (!pages) return;
      recipe_page = (recipe_page + (id == 90 ? pages - 1 : 1)) % pages;
      recipe = recipe_list[recipe_page * recipes_per_page()];
    } else if (id == 92)
      do_craft();
    else if (id == 93)
      do_craft(5);
    else if (id == 94)
      toggle_craft_filter();
    else if(id>=220 && id<220+int(CraftGroup::Count)) {
      craft_group_focus=false;set_craft_group(CraftGroup(id-220));
    }
  } else if (view == Gear) {
    if (id >= 140 && id < 148) {
      selected = id - 140;
      if (unequip(s, selected))
        pocket::save();
    } else if (id == 82 && unequip(s, selected))
      pocket::save();
  } else if (view == Guide && (id == 90 || id == 91))
    guide_page = (guide_page + (id == 90 ? GuidePages - 1 : 1)) % GuidePages;
  else if (view == Dialogue && id == 83) {
    trade(s, npc);
    pocket::save();
    sound(580);
  } else if (view == Storage && id >= 86 && id <= 88) {
    inventory_action(id);
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
  touches.clear();
  scene_touch = {};
  stick_dx = stick_dy = 0;
  craft_ready_only = false;craft_group_focus=false;craft_category=CraftGroup::All;
  recipe = 0;
  refresh_recipes();
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
  if (inventory_touch.source >= 0) {
    inventory_touch = {};
    touch = -1; touch_down = false;
  }
  last_bag_tap = -1;
  if (home_view()) { home_key(k); return; }
  if (k == Key::X) {
    set_view(view == Bag ? World : Bag);
    return;
  }
  if (k == Key::Y) {
    if (view == Crafting)
      do_craft(5);
    else if (view == Bag)
      swap_hotbar();
    else if (view == Storage)
      inventory_action(86);
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
    else if (view == Bag)
      inventory_action(81);
    else if (view == Storage)
      inventory_action(87);
    else if (view == Crafting)
      toggle_craft_filter();
    else
      set_view(Guide);
    return;
  }
  if (view == World) {
    if (k == Key::L)
      s.selected = s.selected < HotSlots ? (s.selected + 7) % 8 : 7;
    if (k == Key::R)
      s.selected = s.selected < HotSlots ? (s.selected + 1) % 8 : 0;
    if (k == Key::A)
      jump_edge = true;
    return;
  }
  if (k == Key::L || k == Key::R) {
    if (view == Storage && k == Key::R) { inventory_action(88); return; }
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
    if (k == Key::LEFT || k == Key::RIGHT || k == Key::UP || k == Key::DOWN)
      select_bag_item(selected);
    if (k == Key::A)
      bag_activate();
  }
  if (view == Crafting) {
    if(craft_group_focus) {
      int step=k==Key::LEFT?-1:k==Key::RIGHT?1:0;
      if(step)set_craft_group(CraftGroup((int(craft_category)+step+int(CraftGroup::Count))%int(CraftGroup::Count)));
      if(k==Key::DOWN || k==Key::A)craft_group_focus=false;
      return;
    }
    if(k==Key::UP && (!visible_recipes || recipe_position()==0)) {craft_group_focus=true;return;}
    if (visible_recipes) {
      int step = k == Key::UP ? -1 : k == Key::DOWN ? 1 : k == Key::LEFT ? -recipes_per_page() : k == Key::RIGHT ? recipes_per_page() : 0;
      int position = (recipe_position() + step + visible_recipes * 4) % visible_recipes;
      recipe = recipe_list[position];
      recipe_page = position / recipes_per_page();
    }
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
      guide_page = (guide_page + GuidePages - 1) % GuidePages;
    if (k == Key::RIGHT)
      guide_page = (guide_page + 1) % GuidePages;
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
bool uses_ui_pointer(int x, int y) { return view != World || hit(x, y) != 16; }
bool input_active() {
  for (const auto &p : touches.slots) if (p.active) return true;
  for (unsigned i = 0; i < 12; ++i) if (pocket::held(Key(i))) return true;
  return false;
}
void cancel_input() {
  touches.clear();
  inventory_touch = {};
  scene_touch = {};
  touch=-1;touch_down=false;last_bag_tap=-1;
  jump_edge=gesture_jump=false;stick_dx=stick_dy=0;
  aim_x=aim_y=-1;use_blocked=true;
}
bool smart_cursor_enabled() { return smart_mode; }
void set_smart_cursor(bool enabled) {
  if(smart_mode==enabled)return;
  smart_mode=enabled;s.mining=0;s.smart_cursor=false;s.target_ready=true;
  cancel_input();pocket::redraw();
}
void menu_pointer(Phase phase, int x, int y) {
  if (phase == Phase::CANCEL) {

    inventory_touch = {};
    last_bag_tap = -1;
    touch = -1;
    touch_down = false;
    return;
  }
  if (phase == Phase::DOWN) {

    inventory_touch = {};
    gesture_jump = false;
    touch = hit(x, y);
    touch_down = true;
    int slot = inventory_slot_at(x, y);
    if (slot >= 0 && (slot < BagSlots ? s.bag[slot].item : s.chest[slot - BagSlots].item)) {
      inventory_touch.source = slot;
      inventory_touch.start_x = inventory_touch.x = x;
      inventory_touch.start_y = inventory_touch.y = y;
    }
    return;
  }
  if (phase == Phase::MOVE) {
    if (inventory_touch.source >= 0) {
      auto &drag = inventory_touch;
      drag.x = x; drag.y = y;
      if (!drag.dragging && (std::abs(x - drag.start_x) > 8 || std::abs(y - drag.start_y) > 8))
        drag.cancelled = true;
    }
    return;
  }
  if (phase == Phase::UP) {

    int was = touch;
    touch_down = false;
    touch = -1;
    if (inventory_touch.source >= 0) {
      auto drag = inventory_touch;
      inventory_touch = {};
      if (drag.dragging) {
        last_bag_tap = -1;
        int target = inventory_slot_at(x, y);
        if (move_stack(s, drag.source, target)) {
          selected = target;
          pocket::save(); sound(530);
        }
      } else if (!drag.cancelled && was == hit(x, y)) click(was);
      else last_bag_tap = -1;
      pocket::redraw();
      return;
    }
    bool world_control = view == World &&
        (was == 10 || was == 11 || was == 12 || was == 14 || was == 15 || was == 16);
    if (was == hit(x, y) && !world_control)
      click(was);
  }
}
void pointer(Phase phase, int x, int y, uint32_t id) {
  auto *p=touches.find(id);
  if (phase == Phase::CANCEL && p && (p->role == 14 || p->role == 17)) jump_edge = false;
  if(phase==Phase::DOWN) {
    p=touches.begin(id);
    if(!p)return;
    int role=view==World?hit(x,y):1;
    if(role!=13 && role!=14 && role!=15 && role!=16 && role!=17)role=1;
    // One owner per control, and one inventory/menu drag at a time.
    if(touches.owns(role)) { p->consumed=true; return; }
    p->role=role;p->x=x;p->y=y;
    if(role==1)menu_pointer(phase,x,y);
    else if(role==17) {
      stick_x=std::clamp(x,49,std::min(100,width()/3));
      stick_y=std::clamp(y,std::max(122,height()-116),height()-55);
      stick_dx=stick_dy=0;
      gesture_jump=false;
    } else if(role==14)jump_edge=true;
    else if(role==15)scene_touch.tap=false;
    else if(role==13)do_interact();
    else if(role==16) {
      aim_x=(x+cx)/16;aim_y=(y+cy)/16;
      auto kind=items[s.bag[s.selected].item].kind;
      scene_touch={x,y,x+cx,y+cy,0,tool(kind) ||
                   (kind==Wall && interactive_furniture(tile(s,aim_x,aim_y))),
                   !touches.owns(15) && !(pocket::held(Key::B) && !use_blocked)};
      if(!scene_touch.deferred && target_interaction(x+cx,y+cy))
        p->role=0; // This gesture is an interaction, never an attack.
    }
  } else {
    if(!p)return;
    p->x=x;p->y=y;
    if(p->role==1)menu_pointer(phase,x,y);
    else if(phase==Phase::MOVE && p->role==17) {
      int dx=x-stick_x,dy=y-stick_y;
      float length=std::sqrt(float(dx)*dx+float(dy)*dy);
      float scale=length>24?24/length:1;
      stick_dx=int(dx*scale);stick_dy=int(dy*scale);
      if(stick_dy < -14 && !gesture_jump) { jump_edge=true; gesture_jump=true; }
      if(stick_dy >= -10)gesture_jump=false;
    } else if(phase==Phase::MOVE && p->role==16) {
      aim_x=(x+cx)/16;aim_y=(y+cy)/16;
      if(std::abs(x-scene_touch.start_x)>8 || std::abs(y-scene_touch.start_y)>8)
        scene_touch.tap=false;
    }
    if(phase==Phase::UP || phase==Phase::CANCEL) {
      if(p->role==16) {
        // Tools wait for a deliberate hold. A short release only interacts
        // with the original target; dragging/cancellation never clicks it.
        auto gesture=scene_touch;
        scene_touch={};
        if(phase==Phase::UP && gesture.deferred && !gesture.ready() && gesture.tap &&
           hit(x,y)==16 && std::abs(x-gesture.start_x)<=8 && std::abs(y-gesture.start_y)<=8)
          target_interaction(gesture.world_x,gesture.world_y);
      }
      if(p->role==17)stick_dx=stick_dy=0;
      if(p->role==16 || p->role==0) {
        // A blocked secondary finger cannot reset another finger's target.
        if(!touches.owns(16) || p->role==16)aim_x=aim_y=-1;
      }
      *p={};
    }
  }
  pocket::redraw();
}
void tick(uint32_t ms) {
  ms = std::min(ms, 100u);
  animation += ms;
  paint += ms;
  if (touch_down && inventory_touch.source >= 0 && !inventory_touch.cancelled &&
      !inventory_touch.dragging) {
    inventory_touch.elapsed += uint16_t(ms);
    if (inventory_touch.elapsed >= 300) {
      inventory_touch.dragging = true;
      selected = inventory_touch.source;
      last_bag_tap = -1;
      sound(470);
    }
  }
  if (view == World) {
    Input input;
    input.move =
        (pocket::held(Key::RIGHT) ? 1 : 0) - (pocket::held(Key::LEFT) ? 1 : 0);
    if (touches.owns(17)) input.move = stick_dx > 7 ? 1 : stick_dx < -7 ? -1 : 0;
    input.down = pocket::held(Key::DOWN) || (touches.owns(17) && stick_dy > 14);
    input.jump = pocket::held(Key::A) || touches.owns(14) || jump_edge ||
                 (touches.owns(17) && stick_dy < -14);
    if (!pocket::held(Key::B)) use_blocked = false;
    const bool explicit_use = (pocket::held(Key::B) && !use_blocked) || touches.owns(15);
    const bool scene_held = touches.owns(16);
    if(explicit_use && scene_held)scene_touch.tap=false;
    input.aim_y = pocket::held(Key::UP) || (touches.owns(17) && stick_dy < -14) ? -1 : input.down ? 1 : 0;
    input.target_x = aim_x;
    input.target_y = aim_y;
    input.smart_cursor = smart_mode;
    uint32_t remaining = ms;
    while (remaining) {
      uint32_t step = std::min(remaining, 20u);
      input.use = explicit_use || (scene_held && scene_touch.ready());
      starsoil::tick(s, input, step);
      if(scene_held && scene_touch.deferred)
        scene_touch.elapsed=uint16_t(std::min<uint32_t>(SceneTouch::HoldMs,scene_touch.elapsed+step));
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
    paint %= 40;
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
    pocket::begin_controls();
    hud();
    bool boss_active=false;
    for(const auto &m:s.mobs)if(m.hp>0 && m.type==5)boss_active=true;
    if(!boss_active) center(20, 59, width() - 116,
           tr(items[s.bag[s.selected].item].en, items[s.bag[s.selected].item].zh), GoldInk);
    if (animation < event_at) {
      const int rows =
          std::strchr(message, '\n') || text_width(message) > 268 ? 2 : 1;
      const int toast_height = rows * pocket::text_line_height() + 6,
                top = std::max(62, height() - 147 - toast_height);
      panel(5, top, width() - 10, toast_height);
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
    if (view == Storage) {
      rect(0, 174, 198, pocket::text_height(), Dark);
      center(6, 174, 190, message, GoldInk);
    } else {
      rect(0, y, 288, pocket::text_height(), Dark);
      wrap(6, y, 276, message, GoldInk, 1);
    }
  }
  inventory_drag_draw();
  draw_x = draw_y = 0;
  if (save_error || pocket::save_status() < 0) {
    rect(0, 0, width() - 24, pocket::text_height() + 2, Dark);
    label(5, 1, tr("SAVE ERROR: FILE PRESERVED", "存档失败：原文件已保留"),
          Red);
  }
}
size_t serialize(uint8_t *data, size_t capacity, bool seal) {
  return !session_active || save_error ? 0 : encode(s, data, capacity, seal);
}
void describe_world(pocket::WorldInfo &info) {
  info = {true, session_active && !save_error, s.width, s.height, s.seed, s.day, s.played};
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
