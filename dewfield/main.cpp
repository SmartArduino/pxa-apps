// SPDX-License-Identifier: MIT
#include "art.hpp"
#include "core.hpp"
#include "living.hpp"
#include "collections.hpp"
#include "fish_art.hpp"
#include "palette.hpp"
#include "runtime.hpp"
#include "scene.hpp"
#include "terrain.hpp"
#include "title_art.hpp"
#include "ui_icons.hpp"
#include "ui_layout.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace {
using namespace pocket;
using namespace farm;
State state;
enum class Page {
  Farm,
  Shop,
  Bag,
  Guide,
  Sleep,
  Morning,
  Fishing,
  Letter,
  Settings,
  World,
  People,
  Story,
  Dialogue,
  Orders,
  Kitchen, Calendar, Forecast, Shipping, Collections
};
Page page = Page::Farm;
bool title_screen = true, save_locked = false, base_dirty = true,
     moving = false, route = false, route_action = false;
bool walking = false;
bool repeat_use = false;
uint32_t walk_distance = 0;
int camx, camy, dir = 2, stepx, stepy, targetx, targety, selected_row = 0;
int route_portal = -1;
bool approaching_portal = false;
uint32_t clock_acc = 0, move_acc = 0, anim_acc = 0, animation = 0, swing = 0,
         toast_ms = 0, morning_gold = 0;
FishingSession fishing;
int collection_category=0;
Page collection_parent=Page::Guide;
struct GatherEffect {int x=0,y=0,item=0,count=0;uint32_t remaining=0;};
GatherEffect gather_effects[4];
unsigned next_effect=0;
void item_icon(int,int,int);
void gather_effect(int x,int y,int item,int count) {
  gather_effects[next_effect++%4]={x*16+8,y*16+8,item,count,800};
}
char toast[96]{}, letter[512]{};
int following_npc = -1;
int talking_npc = -1, journey_region = -1, journey_x = 0, journey_y = 0;
bool journey_use = false;
void go_to(Region, int, int, bool);
void dialogue_action(int);
int touchx, touchy, stickx, sticky;
bool stick = false, touch_down = false;
int held_direction = -1;
int bag_selection = 0, drag_from = -1, drag_x = 0, drag_y = 0;
bool drag_active = false, action_hold = false, action_touch = false;
int hit_obstacle = -1, action_tool = 0;
int action_direction=2,action_grade=0;
art::tool_anim::Target action_target;
struct PendingTool {int x=0,y=0;bool active=false;} pending_tool;
bool committing_tool=false;
void action(int,int);
void cancel_tool() {pending_tool.active=false;swing=0;action_target.valid=false;}
uint32_t hit_ms = 0, deposit_ms=0;
int deposited_item=0;
int held_item() {
  return state.active_slot < InventorySize ? state.slots[state.active_slot] : 0;
}

constexpr int WorldPixelsW = MapW * Tile, WorldPixelsH = MapH * Tile;
alignas(4) uint16_t base[WorldPixelsW * WorldPixelsH];
alignas(4) uint16_t unlit_base[WorldPixelsW * WorldPixelsH];
terrain::Map terrain_map;
int cached_geometry=-1;
uint16_t light_palette[65536];
int cached_light=-1;
farm_scene::Queue world_objects;
uint8_t cached_plots[PlotCount]{};
uint8_t plot_visual(const Plot &p) {
  return uint8_t(bool(p.tilled) | (bool(p.wet) << 1) | (bool(p.crop) << 2));
}
bool plots_changed() {
  for (int i = 0; i < PlotCount; ++i)
    if (cached_plots[i] != plot_visual(state.plots[i]))
      return true;
  return false;
}
constexpr uint16_t Border = farm_palette::Ink, Paper = farm_palette::Paper,
                   Text = farm_palette::Ink, Muted = farm_palette::Muted,
                   Soft = farm_palette::Soft, Sage = farm_palette::Sage;
farm_ui::Layout layout() {
  auto insets = navigation_insets();
  return {WIDTH, HEIGHT, insets.left, insets.right, insets.bottom};
}
struct IconCanvas {
  void rect(int x, int y, int w, int h, uint16_t c) {
    pocket::rect(x, y, w, h, c);
  }
  void line(int x, int y, int xx, int yy, uint16_t c) {
    pocket::line(x, y, xx, yy, c);
  }
};
const char *tool_cn[] = {"锄头", "种子", "水壶", "采收",
                         "斧头", "镐子", "鱼竿"};
const char *tool_en[] = {"Hoe", "Seeds", "Water", "Harvest",
                         "Axe", "Pick",  "Rod"};
const char *seasons_cn[] = {"春", "夏", "秋", "冬"};
const char *seasons_en[] = {"Spring", "Summer", "Fall", "Winter"};
int season() { return season_of(state.day); }
void msg(const char *cn, const char *en) {
  std::snprintf(toast, sizeof(toast), "%s", tr(en, cn));
  toast_ms = 2200;
  redraw();
}
void changed() {
  sync_inventory(state);
  select_slot(state, state.active_slot == 255 ? -1 : state.active_slot);
  save();
  redraw();
}
void center_text(int x, int y, const char *s, uint16_t c = Text,
                 int scale = 1) {
  text(x - text_width(s, scale) / 2, y, s, c, scale);
}
void rounded_rect(int x, int y, int w, int h, uint16_t color, int radius = 6) {
  if (w <= 0 || h <= 0)
    return;
  constexpr uint8_t inset[9][8] = {{0},
                                   {0},
                                   {1, 0},
                                   {1, 0, 0},
                                   {2, 1, 0, 0},
                                   {3, 1, 1, 0, 0},
                                   {4, 2, 1, 1, 0, 0},
                                   {4, 3, 2, 1, 0, 0, 0},
                                   {5, 3, 2, 1, 1, 0, 0, 0}};
  int r = std::clamp(radius, 0, std::min({8, w / 2, h / 2}));
  rect(x, y + r, w, h - 2 * r, color);
  for (int yy = 0; yy < r; ++yy) {
    int skip = inset[r][yy];
    rect(x + skip, y + yy, w - 2 * skip, 1, color);
    rect(x + skip, y + h - 1 - yy, w - 2 * skip, 1, color);
  }
}
void panel(int x, int y, int w, int h) {
  rounded_rect(x + 1, y + 2, w, h, farm_palette::Shadow, 8);
  rounded_rect(x, y, w, h, Border, 8);
  rounded_rect(x + 1, y + 1, w - 2, h - 3, farm_palette::Wood, 7);
  rounded_rect(x + 3, y + 3, w - 6, h - 7, Paper, 5);
}
void button(int x, int y, int w, int h, const char *s, bool active = false) {
  rounded_rect(x, y, w, h, Border, 6);
  rounded_rect(x + 1, y + 1, w - 2, h - 3,
               active ? farm_palette::Selected : farm_palette::Button, 5);
  rect(x + 4, y + 1, w - 8, 1, farm_palette::Highlight);
  rect(x + 3, y + h - 3, w - 6, 1, farm_palette::WoodDark);
  if (w > 42) {
    rect(x + 5, y + h - 7, 5, 1, farm_palette::Grain);
    rect(x + w - 13, y + 5, 5, 1, farm_palette::Grain);
  }
  center_text(x + w / 2, y + (h - 12) / 2 - 1, s, Text);
}
void icon_button(farm_ui::Rect r, farm_ui::Icon icon) {
  button(r.x, r.y, r.w, r.h, "");
  IconCanvas canvas;
  farm_ui::icon(canvas, r.center_x() - 8, r.center_y() - 8, icon);
}
void icon_label_button(farm_ui::Rect r, farm_ui::Icon icon, const char *label,
                       bool active = false) {
  button(r.x, r.y, r.w, r.h, "", active);
  IconCanvas canvas;
  farm_ui::icon(canvas, r.x + 7, r.center_y() - 8, icon);
  center_text(r.x + (r.w + 20) / 2, r.y + (r.h - 12) / 2 - 1, label, Text);
}
constexpr uint16_t TitleBark = farm_palette::Ink,
                   TitleCream = farm_palette::Highlight;
void title_shape(int x, int y, int w, int h, uint16_t c) {
  rect(x + 3, y, w - 6, h, c);
  rect(x + 1, y + 1, w - 2, h - 2, c);
  rect(x, y + 3, w, h - 6, c);
}
void title_wood(farm_ui::Rect r, bool light = false, bool pressed = false) {
  int x = r.x, y = r.y + int(pressed), w = r.w, h = r.h;
  title_shape(x + 1, y + (pressed ? 1 : 3), w, h, art::rgb(65, 47, 30));
  title_shape(x, y, w, h, TitleBark);
  title_shape(x + 1, y + 1, w - 2, h - 3, farm_palette::WoodDark);
  title_shape(x + 3, y + 3, w - 6, h - 7,
              light ? farm_palette::Button : farm_palette::Wood);
  rect(x + 5, y + 2, w - 10, 1, farm_palette::Brass);
  rect(x + 3, y + 4, 1, h - 9, art::rgb(213, 150, 72));
  rect(x + 4, y + h - 5, w - 8, 2, art::rgb(130, 76, 36));
  // Low-contrast grain in short, irregular clusters like the cottage timber.
  for (int row = 0; row < (h - 10) / 8; ++row) {
    int yy = y + 7 + row * 8;
    for (int i = 0; i < 5; ++i) {
      auto hash = art::hash(row + 31, i + (light ? 7 : 2));
      int xx = x + 7 + int(hash % unsigned(w - 27));
      rect(xx, yy + int((hash >> 9) & 1), 3 + int((hash >> 5) % 6), 1,
           light ? art::rgb(190, 129, 63) : farm_palette::Grain);
    }
  }
}
void title_sprig(int x, int y, int direction = 1) {
  line(x, y, x + direction * 9, y - 8, art::rgb(70, 90, 38));
  IconCanvas canvas;
  art::leaf(canvas, x + direction * 2, y - 3, direction);
  art::leaf(canvas, x + direction * 5, y - 6, -direction);
}
void title_overlay() {
  auto logo = layout().title_logo(), enter = layout().title_enter();
  for (int x : {logo.x + 25, logo.x + logo.w - 27}) {
    rect(x, 0, 2, logo.y + 2, TitleBark);
    rect(x, 0, 1, logo.y, art::rgb(192, 155, 92));
    rect(x - 1, logo.y - 2, 4, 3, art::rgb(115, 76, 41));
  }
  title_wood(logo);
  center_text(WIDTH / 2 + 1, logo.y + 11, "晴露农场", TitleBark, 2);
  center_text(WIDTH / 2, logo.y + 10, "晴露农场", TitleCream, 2);
  center_text(WIDTH / 2, logo.y + 43, "D E W F I E L D",
              art::rgb(231, 184, 108));
  title_sprig(logo.x + 6, logo.y + logo.h - 5);
  title_sprig(logo.x + logo.w - 6, logo.y + 14, -1);
  bool pressed = touch_down && enter.contains(touchx, touchy);
  title_wood(enter, true, pressed);
  int label_y = enter.y + 8 + int(pressed);
  center_text(WIDTH / 2 + 5, label_y, tr("ENTER THE FARM", "走进晴露谷"),
              TitleBark);
  title_sprig(enter.x + 11, enter.y + 20 + int(pressed));
  line(enter.x + enter.w - 16, enter.y + 11, enter.x + enter.w - 12,
       enter.y + 15, TitleBark);
  line(enter.x + enter.w - 12, enter.y + 15, enter.x + enter.w - 16,
       enter.y + 19, TitleBark);
  const char *caption =
      tr("A little farm. A slower life.", "种下希望，收获慢时光。");
  int caption_y=layout().title_caption_y();
  for (int dx : {-1, 1})
    center_text(WIDTH / 2 + dx, caption_y, caption, TitleBark);
  center_text(WIDTH / 2, caption_y+1, caption, TitleBark);
  center_text(WIDTH / 2, caption_y, caption, TitleCream);
}
void draw_plots(bool restore) {
  // Restore only the farm's ground before baking changed soil/pockets into the
  // existing map cache. Walking does not recalculate any cultivated pixels.
  if (restore)
    for (int y = PlotY * Tile; y < (PlotY + PlotH) * Tile; ++y)
      for (int x = PlotX * Tile; x < (PlotX + PlotW) * Tile; ++x)
        unlit_base[y * WorldPixelsW + x] = terrain_map.pixel(x, y, season());
  art::Raster r{unlit_base, WorldPixelsW, WorldPixelsH};
  terrain::plots(r, state);
  for (int i = 0; i < PlotCount; ++i)
    cached_plots[i] = plot_visual(state.plots[i]);
  if(restore)for(int y=PlotY*Tile;y<(PlotY+PlotH)*Tile;++y)
    for(int x=PlotX*Tile;x<(PlotX+PlotW)*Tile;++x) {
      unsigned at=y*WorldPixelsW+x;auto c=unlit_base[at];
      base[at]=cached_light>0?light_palette[c]:c;
    }
}
void tint_base() {
  if(cached_light>0)for(unsigned i=0;i<WorldPixelsW*WorldPixelsH;++i)
    base[i]=light_palette[unlit_base[i]];
  else std::memcpy(base,unlit_base,sizeof(base));
}
void draw_base() {
  art::Raster r{unlit_base, WorldPixelsW, WorldPixelsH};
  int se = season();
  if(!indoors(state.region)) {
    int geometry=state.region+(state.story>=3?256:0);
    if(geometry!=cached_geometry){terrain_map.build(state);cached_geometry=geometry;}
    terrain_map.draw(r,se);
  }
  if (!state.region) {
    // Landing, reeds and stones along the pond.
    r.rect(23 * 16, 17 * 16, 28, 3 * 16, Border);
    for (int k = 0; k < 8; ++k) {
      r.rect(23 * 16 + 1, 17 * 16 + k * 6, 26, 5, art::rgb(190, 142, 75));
      r.rect(23 * 16 + 2, 17 * 16 + k * 6, 24, 1, art::Gold);
    }
    draw_plots(false);
  } else if (indoors(state.region)) {
    r.rect(0,0,512,384,art::rgb(65,63,47));
    art::world_room(r,state.region==unsigned(Region::SeedShop),state.region==unsigned(Region::Lodge));
  } else {
    if (state.region == uint8_t(Region::Forest)) {
      for (int tx : {3, 4, 5})
        for (int yy = 12 * 16; yy < 13 * 16; ++yy) {
          if (state.story < 3 && yy % 8 > 2)
            continue;
          r.rect(tx * 16, yy, 16, 1,
                 yy % 5 == 0 ? Border : art::rgb(183, 135, 72));
        }
    }
    if (state.region == uint8_t(Region::Lake)) {
      r.rect(21 * 16, 11 * 16, 3 * 16, 2 * 16, Border);
      for (int y = 11 * 16 + 1; y < 13 * 16; y += 5)
        r.rect(21 * 16 + 1, y, 3 * 16 - 2, 4, art::rgb(187, 140, 76));
    }
    if (state.region == uint8_t(Region::Town) && state.story >= 5) {
      for (int x = 11 * 16; x <= 20 * 16; x += 8) {
        r.line(x, 11 * 16, x + 8, 11 * 16 + (x / 8 % 2 ? 1 : -1), Border);
        r.rect(x + 2, 11 * 16 + 3, 3, 4, art::Gold);
      }
    }
  }
  for (const auto &portal : Portals)
    if (unsigned(portal.from)==state.region) {
      int x=portal.x*16+8,y=portal.y*16+8;
      if(indoors(state.region)) art::world_rug(r,x-14,y-4,28,11);
      else if(indoors(uint8_t(portal.to))) {
        r.rect(x-10,y-2,21,5,art::rgb(135,124,91));
        r.rect(x-9,y-2,19,1,art::rgb(204,181,133));
        r.rect(x-7,y,6,1,art::rgb(164,146,105));
      }
    }
  tint_base();
  base_dirty = false;
}
void camera() {
  camx = std::clamp(int(state.px) - WIDTH / 2, 0,
                    std::max(0, WorldPixelsW - WIDTH));
  camy = std::clamp(int(state.py) - (HEIGHT - 8) / 2, 0,
                    std::max(0, WorldPixelsH - HEIGHT));
}
void scene() {
  auto tone=ambient_light(state);
  int light=tone.key();
  bool relight=light!=cached_light;
  if(light!=cached_light) {
    cached_light=light;
    if(light)for(unsigned c=0;c<65536;++c)light_palette[c]=apply_light(c,tone);
  }
  if (base_dirty)
    draw_base();
  else {
    if(!state.region && plots_changed())draw_plots(true);
    if(relight)tint_base();
  }
  camera();
  auto *p = framebuffer();
  auto outside=indoors(state.region)?art::rgb(65,63,47):art::Sky;
  copy_background(base, WorldPixelsW, WorldPixelsH, camx, camy, cached_light>0?light_palette[outside]:outside);
  art::Raster r{p, WIDTH, HEIGHT, camx, camy};
  r.tint=cached_light>0?light_palette:nullptr;
  profile(DrawStage::Ground);
  for (int i = 0; i < (indoors(state.region) ? 0 : 35); ++i) {
    int x = (state.region == uint8_t(Region::Lake) ? 5 : 24) * 16 +
            int(art::hash(i, 3) % 88),
        y = 14 * 16 + int(art::hash(i, 7) % 110);
    x += (animation + i) % 7;
    for (int k = 0; k < 3 + int(art::hash(i, 2) % 5); ++k)
      if (terrain_map.material(x + k, y) == terrain::Pond &&
          terrain_map.distance(x + k, y, true, 4) >= 4)
        r.rect(x + k, y, 1, 1, art::rgb(143, 196, 189));
  }
  farm_scene::View view{swing?action_direction:dir,
                        walking ? int((walk_distance / 4) % 8) + 1 : 0,
                        int(swing),
                        swing ? action_tool : int(state.tool),
                        season(),
                        animation,
                        swing ? 0 : held_item(),
                        swing?action_grade:int(state.tool_grade[state.tool]),
                        int(state.selected),
                        hit_obstacle,
                        hit_ms,
                        page == Page::Fishing};
  view.subminute_ms=clock_acc;
  view.tool_target=action_target;
  profile(DrawStage::Water);
  world_objects.build(state, view, {camx, camy, WIDTH, HEIGHT});
  profile(DrawStage::Queue);
  world_objects.shadows(r, state, view);
  profile(DrawStage::Shadows);
  int fx = state.px / 16 + (dir == 1) - (dir == 3),
      fy = state.py / 16 + (dir == 2) - (dir == 0);
  if (route) {
    fx = targetx;
    fy = targety;
  }
  int oi = state.region ? -1 : obstacle_index(fx, fy);
  if (route || (!state.region && plot_index(fx, fy) >= 0) ||
      (oi >= 0 && !state.cleared[oi])) {
    for (int dx : {0, 13})
      for (int dy : {0, 13}) {
        r.rect(fx * 16 + dx, fy * 16 + dy, 3, 1, art::Cream);
        r.rect(fx * 16 + dx + (dx ? 2 : 0), fy * 16 + dy, 1, 3, art::Cream);
      }
  }
  world_objects.objects(r, state, view);
  if(deposit_ms) {
    int t=360-deposit_ms,xx=(int(state.px)*(360-t)+(8*16+8)*t)/360;
    int yy=((int(state.py)-36)*(360-t)+(9*16-6)*t)/360-t*(360-t)/5000;
    art::carry_item(r,xx,yy+31,deposited_item,state.selected);
  }
  profile(DrawStage::Bodies);
  // Rain uses a small fixed particle budget and no full-screen blend.
  if (state.rain && !indoors(state.region))
    for (int i = 0; i < 22; ++i) {
      int x = (i * 71 + animation * 2) % WIDTH,
          y = (i * 43 + animation * 5) % HEIGHT;
      r.line(camx + x, camy + y, camx + x - 2, camy + y + 4,
             art::rgb(174, 203, 173));
    }
  if(!indoors(state.region)&&state.weather==unsigned(Weather::Snow))
    for(int i=0;i<24;++i)r.rect(camx+(i*71+animation)%WIDTH,camy+(i*43+animation*2)%HEIGHT,1+(i%3==0),1,art::Cream);
  if(!indoors(state.region)&&state.weather==unsigned(Weather::Wind))
    for(int i=0;i<10;++i)r.line(camx+(i*59+animation*4)%WIDTH,camy+(i*47+animation)%HEIGHT,camx+(i*59+animation*4)%WIDTH+2,camy+(i*47+animation)%HEIGHT-1,art::Leaf);
  for(const auto &effect:gather_effects)if(effect.remaining) {
    int x=effect.x-camx,y=effect.y-camy-24-int((800-effect.remaining)/45);
    if(x>=12 && x<WIDTH-26 && y>=2 && y<HEIGHT-16) {
      item_icon(x-5,y+6,effect.item);
      char amount[16];std::snprintf(amount,sizeof(amount),"+%d",effect.count);
      text(x+6,y,amount,farm_palette::Highlight);
    }
  }
  profile(DrawStage::Effects);
}
void stop_motion() {
  route = false;
  moving = false;
  walking = false;
  move_acc = 0;
  route_portal = -1;
  approaching_portal = false;
  state.px = state.px / 16 * 16 + 8;
  state.py = state.py / 16 * 16 + 8;
  stick = false;
  held_direction = -1;
  action_hold = false;
  repeat_use = false;
}
void open_page(Page p) {
  cancel_tool();
  stop_motion();
  page = p;
  selected_row = p==Page::Calendar?season_day(state.day)-1:0;
  if(p==Page::Collections)collection_category=0;
  if (p == Page::Bag) {
    bag_selection = state.active_slot < InventorySize ? state.active_slot : 0;
    selected_row=-1;
  }
  redraw();
}
void open_collections(int category=0,int index=0,Page parent=Page::Guide) {
  open_page(Page::Collections);collection_category=category;selected_row=index;collection_parent=parent;
}
void feedback(Result result) {
  switch (result) {
  case Result::Sleep:
    open_page(Page::Sleep);
    return;
  case Result::Shop:
    open_page(Page::Shop);
    return;
  case Result::Talk: {
    uint32_t before = state.quests; // quest has already been evaluated by act()
    (void)before;
    open_page(Page::Letter);
    const char *cn =
        state.quests == 0 ? "想尝尝你的新鲜蔬菜！\n收获 3 份作物，奖励 60 金。"
        : state.quests == 1
            ? "农场越来越有生气了。\n累计出货 120 金，奖励 100 金。"
        : state.quests == 2
            ? "可以帮我带 3 条鲜鱼吗？\n交付后奖励 140 金。"
            : "谢谢你，让晴露谷热闹起来！\n去田里享受慢慢生长的日子吧。";
    const char *en =
        state.quests == 0   ? "Harvest 3 crops.\nReward: 60 gold."
        : state.quests == 1 ? "Earn 120 gold from shipping.\nReward: 100 gold."
        : state.quests == 2 ? "Bring me 3 fresh fish.\nReward: 140 gold."
                            : "Thank you for helping our village!";
    std::snprintf(letter, sizeof(letter), "%s", tr(en, cn));
    changed();
    return;
  }
  case Result::Fish:
    open_page(Page::Fishing);
    action_tool = 6;
    swing = art::tool_anim::duration(6);
    action_direction=dir;action_grade=0;
    fishing.begin(state);
    changed();
    return;
  case Result::FullFish:
    msg("鲜鱼已经装满，先出货或交付委托吧", "Fish stack full. Ship or deliver some first");
    return;
  case Result::Tilled:
    msg("松软的土地，准备播种", "Soil ready for seeds");
    break;
  case Result::Planted:
    msg("种下了！每天记得浇水", "Planted! Water it daily");
    break;
  case Result::Watered:
    msg("浇水完成", "Watered");
    break;
  case Result::Harvested:
    msg("收获了！放进出货箱吧", "Harvested! Use shipping crate");
    break;
  case Result::Struck: {
    char cn[80], en[80];
    int hits = strikes_left(state, hit_obstacle, Tool(state.tool));
    std::snprintf(cn, sizeof(cn), "还需 %d 次，继续挥动工具", hits);
    std::snprintf(en, sizeof(en), "%d swings left", hits);
    msg(cn, en);
    break;
  }
  case Result::Cleared:
    msg("收集材料，腾出新田地", "Materials collected");
    break;
  case Result::NoSeeds:
    msg("种子用完了，去商店补货", "Buy more seeds at the shop");
    break;
  case Result::NoEnergy:
    msg("体力不足，回小屋睡一觉吧", "Low energy. Rest at home");
    break;
  case Result::NoWater:
    msg("水壶空了，去井边或池塘补水", "Refill at the well or pond");
    break;
  case Result::Refilled:
    msg("清水已装满", "Water refilled");
    break;
  case Result::NotReady:
    msg("还在生长，浇水后睡一觉", "Still growing. Water and rest");
    break;
  case Result::Blocked:
    msg("树用斧头，石头用镐，杂草用锄头",
        "Axe: trees. Pick: rocks. Hoe: weeds");
    break;
  case Result::Ship:
    msg("出货已入箱，明早收到货款", "Shipped! Paid tomorrow morning");
    break;
  default:
    return;
  }
  if(result==Result::NoEnergy||result==Result::NoWater||result==Result::NoSeeds) {
    action_hold=false;cancel_tool();changed();return;
  }
  action_tool = result == Result::Harvested||result==Result::Refilled ? 3 : result==Result::Planted?1:state.tool;
  if(!committing_tool) {
    swing = art::tool_anim::duration(action_tool, state.tool_grade[action_tool]);
    action_direction=dir;action_grade=state.tool_grade[action_tool];
    moving=walking=false;move_acc=0;
  }
  if (result == Result::NoEnergy || result == Result::NoWater ||
      result == Result::NoSeeds)
    action_hold = false;
  changed();
}
void action(int x, int y) {
  if (swing&&!committing_tool)
    return;
  if(!committing_tool) {
    auto tool=Tool(state.tool);
    if(tool==Tool::Hoe||tool==Tool::Axe||tool==Tool::Pick||tool==Tool::Water) {
      State preview=state;auto result=act(preview,x,y);
      if(result==Result::Tilled||result==Result::Watered||result==Result::Struck||result==Result::Cleared) {
        action_tool=state.tool;action_grade=state.tool_grade[state.tool];action_direction=dir;
        action_target={x*16+8,y*16+8-art::tool_anim::surface_offset(action_tool),true};pending_tool={x,y,true};
        swing=art::tool_anim::duration(action_tool,action_grade);
        moving=walking=false;move_acc=0;redraw();return;
      }
    }
    action_target={x*16+8,y*16+2,true};
  }
  uint32_t oldq = state.quests;
  int npc = npc_at(state, x, y);
  int offered_item=held_item();
  int crop=plot_index(x,y),forage=forage_at(state,x,y);
  int gathered_item=crop>=0&&!state.region&&state.plots[crop].crop?11+state.plots[crop].crop:
    forage>=0?Foraging[forage].item:0;
  unsigned old_count=item_count(state,gathered_item);
  bool was_known=discovered_item(state,gathered_item);
  auto res = act(state, x, y);
  if(res==Result::Harvested || res==Result::Foraged)
    gather_effect(x,y,gathered_item,int(item_count(state,gathered_item)-old_count));
  if(res==Result::Ship){deposited_item=offered_item;deposit_ms=360;}
  if (res == Result::Travel) {
    for(auto &effect:gather_effects)effect.remaining=0;
    stop_motion();
    swing = 0;
    hit_ms = 0;
    base_dirty = true;
    move_acc = 0;
    page = Page::Farm;
    msg(region_name(Region(state.region)),
        region_name(Region(state.region), true));
    changed();
    if (journey_region >= 0) {
      int region = journey_region;
      journey_region = -1;
      go_to(Region(region), journey_x, journey_y, journey_use);
    }
    return;
  }
  if (res == Result::Dialogue) {
    journey_region = following_npc = -1;
    open_page(Page::Dialogue);
    talking_npc = npc;
    chat(state, npc);
    bool personal = personal_event(state, npc);
    std::snprintf(letter, sizeof(letter), "%s",
                  npc_line(state, npc, personal, english()));
    changed();
    return;
  }
  if (res == Result::Locked) {
    journey_region = -1;
    msg("旧桥断了，打开手册查看修桥目标",
        "Bridge broken. Check your story journal");
    return;
  }
  if (res == Result::Foraged) {
    msg(was_known?"拾起了路边的物资，明天再来看看":"新发现已收进收藏册！",was_known?"Supplies gathered. More tomorrow":"New discovery in your collection!");
    changed();
    return;
  }
  if(res==Result::Calendar || res==Result::Forecast || res==Result::EmptyShipping) {
    open_page(res==Result::Calendar?Page::Calendar:res==Result::Forecast?Page::Forecast:Page::Shipping);return;
  }
  if(res==Result::Closed) {msg("尚未营业：种子铺 09–17，周三休息；会馆 08–20","Closed: seeds 09-17, Wed off; lodge 08-20");return;}
  if(res==Result::WrongSeason) {msg("这类种子不适合当前季节","These seeds need another season");return;}
  if (res == Result::Orders || res == Result::Kitchen) {
    open_page(res == Result::Orders ? Page::Orders : Page::Kitchen);
    return;
  }
  if (state.quests != oldq)
    msg("委托完成！奖励已到账", "Quest complete! Reward received");
  if (res == Result::Struck || res == Result::Cleared) {
    hit_obstacle = obstacle_index(x, y);
    hit_ms = 500;
  }
  feedback(res);
  if(res==Result::Harvested&&!was_known)msg("首次收获！收藏册已记录这种作物", "First harvest! Added to your collection");
}
void begin_route(int x, int y, bool use) {
  if (x < 0 || y < 0 || x >= MapW || y >= MapH)
    return;
  route = true;
  route_action = use;
  targetx = x;
  targety = y;
  route_portal=-1;approaching_portal=false;
  if(const auto*p=portal_at(state,x,y)) {
    route_portal=int(p-Portals);approaching_portal=true;
    auto front=portal_approach(*p);targetx=front.x;targety=front.y;
    route_action=false;
  }
  redraw();
}
void facing_action() {
  if (page != Page::Fishing && swing)
    return;
  if (page == Page::Fishing) {
    if(fishing.phase==FishingSession::Phase::Caught || fishing.phase==FishingSession::Phase::Escaped) {
      if(state.energy && state.fish<999 && spend_energy(state,4)) {
        fishing.begin(state);changed();
      } else {page=Page::Farm;msg("先整理背包或吃点东西，再来钓鱼吧", "Ship your catch or eat before fishing again");}
    } else {
      bool caught=fishing.press(state);
      if(caught)changed();else redraw();
    }
    return;
  }
  int front_x=state.px/16+(dir==1)-(dir==3),front_y=state.py/16+(dir==2)-(dir==0);
  if(!state.region&&front_x==8&&front_y==9) {action(front_x,front_y);return;}
  if (food_energy(held_item())) {
    if (eat(state, held_item())) {
      msg("吃好了，体力恢复了", "A good meal. Energy restored");
      changed();
    } else msg("体力充足，留着稍后享用吧", "Energy is full. Save it for later");
    return;
  }
  if (held_item() >= 12) {
    msg("物品可出货，材料可用于制作",
        "Ship produce; use materials for crafting");
    return;
  }
  int x = state.px / 16 + (dir == 1) - (dir == 3),
      y = state.py / 16 + (dir == 2) - (dir == 0);
  action(x, y);
}
void navigate(int d) {
  if(swing)return;
  journey_region = following_npc = -1;
  route = false;
  route_portal=-1;approaching_portal=false;
  if (moving)
    return;
  dir = d;
  if(const auto*p=portal_at(state,state.px/16,state.py/16);
     p && d==portal_heading(*p)) {
    action(p->x,p->y);return;
  }
  int x = state.px / 16 + (d == 1) - (d == 3),
      y = state.py / 16 + (d == 2) - (d == 0);
  if (walkable(state, x, y)) {
    stepx = x * 16 + 8;
    stepy = y * 16 + 8;
    moving = true;
  }
  redraw();
}
void sleep_now(bool forced = false) {
  cancel_tool();
  for(auto &effect:gather_effects)effect.remaining=0;
  morning_gold = farm::sleep(state,forced);
  state.region = uint8_t(Region::Home);
  state.px = 16 * 16 + 8;
  state.py = 17 * 16 + 8;
  state.visited |= 1u << state.region;
  journey_region = -1;
  base_dirty = true;
  page = Page::Morning;
  clock_acc = 0;
  changed();
}
int panel_left() { return (WIDTH - 228) / 2; }
int panel_top() { return (HEIGHT - 210) / 2; }
void tool_icon(int x, int y, int which, bool active) {
  const sprites::Sprite *icons[] = {
      &sprites::hoe, &sprites::seeds, &sprites::can, &sprites::basket,
      &sprites::axe, &sprites::pick,  &sprites::rod};
  auto &s = *icons[which];
  pocket::sprite(x - s.w / 2, y - s.h / 2, s);
  if ((which == 4 || which == 5) && state.tool_grade[which]) {
    uint16_t metal = state.tool_grade[which] == 1 ? art::rgb(190, 118, 66)
                                                  : art::rgb(191, 210, 203);
    rect(x + 5, y - 9, 4, 4, metal);
    rect(x + 5, y - 9, 3, 1, art::Cream);
  }
  (void)active;
}
void item_label(char *b, size_t n, int item, bool quantity = true) {
  if (!item) {
    std::snprintf(b, n, "%s", tr("Empty hands", "空手"));
    return;
  }
  if (item <= 7 && item != 2) {
    int t = item - 1, grade = state.tool_grade[t];
    std::snprintf(b, n, "%s%s",
                  grade == 1   ? tr("Copper ", "铜")
                  : grade == 2 ? tr("Iron ", "铁")
                               : "",
                  english() ? tool_en[t] : tool_cn[t]);
    return;
  }
  int seed = item == 2                 ? state.selected
             : item >= 8 && item <= 11 ? item - 8
                                       : -1;
  const char *name = seed >= 0    ? tr(Crops[seed].en, Crops[seed].cn)
                     : item <= 15 ? tr(Crops[item - 12].en, Crops[item - 12].cn)
                     : item == 16 ? tr("Fish", "鲜鱼")
                     : item == 17 ? tr("Wood", "木材")
                     : item == 18 ? tr("Stone", "石料")
                                  : item>=24?seasonal_name(item,english()):tr(Foods[item - 19].en, Foods[item - 19].cn);
  unsigned count = seed >= 0 ? state.seeds[seed] : item_count(state, item);
  std::snprintf(b, n, "%s%s", name, seed >= 0 ? tr(" seeds", "种子") : "");
  if (quantity) {
    char label[72];
    std::snprintf(label, sizeof(label), "%s x%u", b, count);
    std::snprintf(b, n, "%s", label);
  }
}
void item_icon(int x, int y, int item) {
  if (!item)
    return;
  if (item <= 7) {
    tool_icon(x, y, item - 1, false);
    return;
  }
  if (item <= 11) {
    pocket::sprite(x - 8, y - 10, sprites::seeds);
    constexpr uint16_t accents[] = {
        art::rgb(224, 183, 139), art::rgb(197, 151, 76), art::rgb(205, 83, 42),
        art::rgb(224, 142, 45)};
    rect(x + 3, y - 7, 4, 4, accents[item - 8]);
  } else if (item <= 15) {
    IconCanvas c;
    art::produce(c, x, y + 2, item - 12);
  } else if (item <= 18) {
    const sprites::Sprite *resource[] = {&sprites::fish, &sprites::logs,
                                         &sprites::stones};
    auto &sprite = *resource[item - 16];
    pocket::sprite(x - sprite.w / 2, y - sprite.h / 2, sprite);
  } else { IconCanvas c; art::food(c, x, y + 6, item); }
}
void inventory_cell(farm_ui::Rect cell, int slot, bool bag) {
  int item = state.slots[slot];
  bool active = slot == state.active_slot;
  rounded_rect(cell.x, cell.y, cell.w, cell.h,
               active ? farm_palette::Brass : farm_palette::Frame);
  rounded_rect(cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2,
               active              ? farm_palette::Selected
               : slot < HotbarSize ? farm_palette::Soft
                                   : farm_palette::Paper);
  if (bag && slot == bag_selection)
    outline(cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2, Sage);
  if (drag_active && drag_from == slot)
    return;
  item_icon(cell.center_x(), cell.y + 12, item);
  if (item >= 8) {
    char count[16];
    std::snprintf(count, sizeof(count), "%u",
                  item == 2 ? state.seeds[state.selected]
                            : item_count(state, item));
    int w = text_width(count);
    rect(cell.x + cell.w - w - 2, cell.y + cell.h - 12, w + 1, 12,
         farm_palette::Paper);
    text(cell.x + cell.w - w - 2, cell.y + cell.h - 12, count, Text);
  }
}
void drag_overlay() {
  if (!drag_active || drag_from < 0)
    return;
  int x = std::clamp(drag_x, 14, WIDTH - 14),
      y = std::clamp(drag_y, 14, HEIGHT - 14);
  rounded_rect(x - 13, y - 14, 27, 28, farm_palette::Shadow);
  rounded_rect(x - 13, y - 15, 26, 27, farm_palette::Selected);
  item_icon(x, y - 2, state.slots[drag_from]);
}
void setting_action(int index) {
  if (index == 0)
    state.show_joystick = !state.show_joystick;
  if (index == 1)
    state.show_action = !state.show_action;
  if (index == 2) {
    state.music = !state.music;
    music_enabled(state.music);
  }
  if (index == 3)
    state.auto_run = !state.auto_run;
  if (index == 4)
    switch_language();
  changed();
}
void cycle_seeds() {
  for(int next=1;next<=4;++next) {
    int seed=(state.selected+next)%4;
    if(state.seeds[seed]){equip_item(state,8+seed);changed();return;}
  }
  select_slot(state,-1);changed();
}
void tool_upgrade_action() {
  int item = state.slots[bag_selection];
  if (item != 5 && item != 6)
    return;
  if (upgrade_tool(state, Tool(item - 1)))
    msg("工具升级完成", "Tool upgraded");
  else
    msg("金币、木材或石料不足", "Need more gold, wood or stone");
  changed();
}
int wrapped(const char *source, char lines[][128], int max_lines, int width) {
  int row = 0, len = 0, used = 0;
  lines[0][0] = 0;
  while (*source && row < max_lines) {
    if (*source == '\n') {
      source++;
      lines[row][len] = 0;
      if (++row == max_lines)
        break;
      len = used = 0;
      lines[row][0] = 0;
      continue;
    }
    if (static_cast<unsigned char>(*source) < 128 && *source != ' ') {
      const char *end = source;
      while (*end && *end != ' ' && *end != '\n' &&
             static_cast<unsigned char>(*end) < 128)
        ++end;
      int word = int(end - source) * 6;
      if (used && word <= width && used + word > width) {
        lines[row][len] = 0;
        if (++row == max_lines)
          break;
        len = used = 0;
        lines[row][0] = 0;
      }
    }
    const char *start = source;
    pocket_ui::next_utf8(source);
    int bytes = int(source - start);
    char one[5]{};
    std::memcpy(one, start, bytes);
    int advance = text_width(one);
    if (used + advance > width) {
      lines[row][len] = 0;
      if (++row == max_lines)
        break;
      len = used = 0;
      lines[row][0] = 0;
    }
    if (!used && *start == ' ')
      continue;
    if (len + bytes >= 127)
      break;
    std::memcpy(lines[row] + len, start, bytes);
    len += bytes;
    lines[row][len] = 0;
    used += advance;
  }
  return std::min(row + 1, max_lines);
}
void fit_text(int x, int y, const char *s, int width, uint16_t color = Text) {
  char b[128]{};
  int n = 0, used = 0;
  if (text_width(s) <= width) {
    text(x, y, s, color);
    return;
  }
  while (*s) {
    const char *at = s;
    pocket_ui::next_utf8(s);
    int bytes = int(s - at);
    char one[5]{};
    std::memcpy(one, at, bytes);
    int w = text_width(one);
    if (used + w + 12 > width || n + bytes >= 120)
      break;
    std::memcpy(b + n, at, bytes);
    n += bytes;
    used += w;
  }
  std::memcpy(b + n, "…", 4);
  text(x, y, b, color);
}
void paragraph(int x, int y, const char *s, int width, int max_lines,
               uint16_t color = Text) {
  char lines[5][128]{};
  int n = wrapped(s, lines, std::min(max_lines, 5), width);
  for (int i = 0; i < n; ++i)
    text(x, y + i * 15, lines[i], color);
}
void coin(int x, int y) {
  rounded_rect(x, y, 7, 7, art::rgb(153, 105, 45));
  rounded_rect(x + 1, y, 5, 6, art::Gold);
  rect(x + 2, y + 1, 2, 3, art::Cream);
}
void weather_icon(int x, int y) {
  if(state.weather==unsigned(Weather::Snow)) {
    line(x+7,y,x+7,y+12,art::Water);line(x+1,y+6,x+13,y+6,art::Water);
    line(x+3,y+2,x+11,y+10,art::Water);line(x+3,y+10,x+11,y+2,art::Water);
  } else if(state.weather==unsigned(Weather::Wind)) {
    for(int i=0;i<3;++i)line(x,y+i*4+2,x+12-i*2,y+i*4+2,Sage);
  } else if (state.rain) {
    rounded_rect(x, y + 2, 14, 6, art::rgb(139, 165, 158));
    rounded_rect(x + 3, y, 8, 7, art::rgb(183, 199, 178));
    for (int i = 0; i < 3; ++i)
      line(x + 3 + i * 4, y + 10, x + 2 + i * 4, y + 12,
           art::rgb(79, 137, 151));
  } else {
    rounded_rect(x + 4, y + 3, 7, 7, art::Gold);
    rect(x + 6, y, 2, 2, art::Gold);
    rect(x + 6, y + 11, 2, 2, art::Gold);
    rect(x + 1, y + 5, 2, 2, art::Gold);
    rect(x + 12, y + 5, 2, 2, art::Gold);
    rect(x + 5, y + 4, 2, 2, Paper);
  }
}
int caption_width() {
  char b[80];
  item_label(b, sizeof(b), held_item());
  return std::clamp(text_width(b) + 18, 66, 146);
}
struct Notice {
  farm_ui::Rect box{};
  char lines[2][128]{};
  int count = 0;
};
Notice notice() {
  Notice n;
  if (!toast_ms && !route)
    return n;
  const char *s =
      toast_ms ? toast : tr("Walking to destination…", "正在前往目的地…");
  n.count = wrapped(s, n.lines, 2, WIDTH - 38);
  int width = 0;
  for (int i = 0; i < n.count; ++i)
    width = std::max(width, text_width(n.lines[i]));
  int w = std::clamp(width + 24, 80, WIDTH - 12), h = n.count * 13 + 8,
      x = (WIDTH - w) / 2, y = 70;
  farm_scene::Rect actor{int(state.px) - camx - 11, int(state.py) - camy - 34,
                         22, 37};
  if (actor.intersects({x, y, w, h}))
    y = layout().belt().y - h - 2;
  n.box = {x, y, w, h};
  return n;
}
int hud_opacity(farm_ui::Rect box,int normal=8) {
  int height=held_item()>=8?54:33;
  farm_scene::Rect actor{int(state.px)-camx-12,int(state.py)-camy-height,24,height+3};
  return actor.intersects({box.x,box.y,box.w,box.h})?std::min(normal,3):normal;
}
struct HudLayer {
  explicit HudLayer(farm_ui::Rect box,int normal=8) {
    begin_hud(box.x-4,box.y-4,box.w+8,box.h+9,hud_opacity(box,normal));
  }
  ~HudLayer(){end_hud();}
};
void notification() {
  auto n = notice();
  if (!n.count)
    return;
  auto r = n.box;
  HudLayer layer(r);
  panel(r.x, r.y, r.w, r.h);
  rect(r.x + 5, r.y + 5, 2, n.count * 13 - 1, Sage);
  for (int i = 0; i < n.count; ++i)
    text(r.x + 12, r.y + 3 + i * 13, n.lines[i], Text);
}
void hud() {
  auto ui = layout();
  auto calendar = ui.calendar();
  {
  HudLayer layer(calendar);
  panel(calendar.x, calendar.y, calendar.w, calendar.h);
  char b[96];
  std::snprintf(b, sizeof(b), "%s %u",
                english() ? seasons_en[season()] : seasons_cn[season()],
                season_day(state.day));
  text(12, 9, b, Text);
  std::snprintf(b, sizeof(b), "%02u:%02u", (state.minute / 60) % 24,
                state.minute % 60);
  text(110 - text_width(b), 9, b, Muted);
  coin(13, 26);
  std::snprintf(b, sizeof(b), "%u G", state.gold);
  text(24, 23, b, Text);
  weather_icon(91, 22);
  }
  char b[96];
  const farm_ui::Icon icons[] = {farm_ui::Icon::Backpack,
                                 farm_ui::Icon::Journal, farm_ui::Icon::Pause};
  for (int i = 0; i < 3; ++i) {
    HudLayer layer(ui.header_button(i));
    icon_button(ui.header_button(i), icons[i]);
  }
  if (state.region) {
    HudLayer layer({6,41,112,18});
    panel(6, 41, 112, 18);
    fit_text(12, 44, region_name(Region(state.region), english()), 100, Sage);
  }
  auto vitals = ui.vitals();
  {
  HudLayer layer(vitals);
  panel(vitals.x, vitals.y, vitals.w, vitals.h);
  for (int row = 0; row < 2; ++row) {
    int y = vitals.y + 4 + row * 12, x = vitals.x + 6;
    if (row == 0) {
      rect(x, y + 3, 5, 3, Sage);
      rect(x + 2, y + 1, 4, 3, Sage);
      line(x, y + 7, x + 5, y + 1, art::Leaf);
    } else {
      rect(x + 2, y + 1, 2, 2, art::rgb(94, 150, 164));
      rect(x + 1, y + 3, 4, 4, art::rgb(94, 150, 164));
      rect(x + 2, y + 4, 1, 2, Paper);
    }
    int amount = row ? state.water : state.energy,
        maximum = row ? state.capacity : 100;
    rect(x + 10, y + 3, 33, 5, farm_palette::Frame);
    rect(x + 11, y + 4, 31, 3, Soft);
    if (amount)
      rect(x + 11, y + 4, std::max(1, 31 * amount / maximum), 3,
           row ? farm_palette::Water : state.energy<20?art::rgb(181,83,53):farm_palette::Sage);
    std::snprintf(b, sizeof(b), "%u", amount);
    text(vitals.x + vitals.w - 7 - text_width(b), y - 1, b,
         row ? farm_palette::Water : Sage);
  }
  }
  auto belt = ui.belt();
  {
  HudLayer layer(belt);
  panel(belt.x, belt.y, belt.w, belt.h);
  for (int i = 0; i < HotbarSize; ++i)
    inventory_cell(ui.tool(i), i, false);
  }
  if (!toast_ms && !route) {
    auto label = ui.caption(caption_width());
    HudLayer layer(label);
    panel(label.x, label.y, label.w, label.h);
    item_label(b, sizeof(b), held_item());
    center_text(label.center_x(), label.y + 2, b, Sage);
  }
  if (!using_controller() && state.show_joystick) {
    auto joystick = ui.joystick();
    HudLayer layer(joystick,6);
    int cx = joystick.center_x(), cy = joystick.center_y(),
        px = stick ? 20 : 18, py = 0, error = 1 - px;
    while (px >= py) {
      for (int sign : {-1, 1}) {
        rect(cx + sign * (px + 1), cy + py, 1, 1, Border);
        rect(cx + sign * (px + 1), cy - py, 1, 1, Border);
        rect(cx + sign * py, cy + px + 1, 1, 1, Border);
        rect(cx + sign * py, cy - px - 1, 1, 1, Border);
        rect(cx + sign * px, cy + py, 1, 1, Paper);
        rect(cx + sign * px, cy - py, 1, 1, Paper);
        rect(cx + sign * py, cy + px, 1, 1, Paper);
        rect(cx + sign * py, cy - px, 1, 1, Paper);
      }
      ++py;
      if (error < 0)
        error += 2 * py + 1;
      else {
        --px;
        error += 2 * (py - px) + 1;
      }
    }
    if (stick) {
      line(cx - 14, cy, cx + 14, cy, Paper);
      line(cx, cy - 14, cx, cy + 14, Paper);
      rounded_rect(stickx - 4, sticky - 4, 9, 9, Border);
      rounded_rect(stickx - 3, sticky - 3, 7, 7, art::Gold);
    } else {
      line(cx - 4, cy, cx + 4, cy, Paper);
      line(cx, cy - 4, cx, cy + 4, Paper);
    }
  }
  if (!using_controller() && state.show_action) {
    HudLayer layer(ui.action(),5);
    icon_button(ui.action(), farm_ui::Icon::Use);
  }
  notification();
}
void collection_action() {
  if(collection_category!=3)return;
  if(claim_collection_reward(state,selected_row)) {
    changed();msg("徽章奖励已领取，继续探索晴露谷吧！", "Badge reward claimed. Keep exploring!");
  } else msg("完成目标后可以领取，已领过的不会重复发放", "Reach the goal to claim each badge once");
}
void draw_collections(int x,int y) {
  const char *tabs[]={tr("Crops","农作"),tr("Forage","山野"),tr("Fish","鱼类"),tr("Badges","徽章")};
  auto ui=layout();char b[128];IconCanvas canvas;
  for(int i=0;i<4;++i) {auto r=ui.collection_tab(i);button(r.x,r.y,r.w,r.h,tabs[i],collection_category==i);}
  int count=collection_category==3?MilestoneCount:collection_size(collection_category);
  for(int i=0;i<count;++i) {
    auto r=ui.collection_cell(collection_category,i);
    rounded_rect(r.x,r.y,r.w,r.h,selected_row==i?farm_palette::Selected:Soft,4);
    if(collection_category==3) {
      auto m=milestone(state,i);bool claimed=state.collection_rewards&(1u<<i);
      fit_text(r.x+5,r.y+2,english()?m.en:m.cn,82,claimed?Muted:Sage);
      std::snprintf(b,sizeof(b),"%d/%d  %s%dG",std::min(m.goal,m.progress),m.goal,claimed?"":"+",m.reward);
      fit_text(r.x+5,r.y+16,b,90,claimed?Muted:Text);
      if(claimed) {line(r.x+87,r.y+8,r.x+90,r.y+11,Sage);line(r.x+90,r.y+11,r.x+95,r.y+5,Sage);}
    } else {
      bool known=collected(state,collection_category,i);
      if(collection_category==2) {
        art::fish_portrait(canvas,r.x+4,r.y+7,i,1,known);
        std::snprintf(b,sizeof(b),"%s",english()?FishSpeciesList[i].en:FishSpeciesList[i].cn);
      } else {
        int item=collection_item(collection_category,i);
        if(known)item_icon(r.x+12,r.y+12,item);else center_text(r.x+12,r.y+6,"?",Muted);
        item_label(b,sizeof(b),item,false);
      }
      fit_text(r.x+24,r.y+6,b,72,known?Sage:Muted);
    }
  }
  if(collection_category==3) {
    auto m=milestone(state,selected_row);auto r=ui.confirm(173,25);
    bool claimed=state.collection_rewards&(1u<<selected_row);
    button(r.x,r.y,r.w,r.h,claimed?tr("CLAIMED","奖励已领取"):m.progress>=m.goal?tr("CLAIM REWARD","领取徽章奖励"):tr("KEEP EXPLORING","继续探索，完成目标"),!claimed&&m.progress>=m.goal);
  } else {
    if(collection_category==2) {
      const auto &f=FishSpeciesList[selected_row];
      char best[48],skill[48];int level=fishing_level(state),goal=level==0?8:level==1?25:level==2?60:0;
      if(state.fish_records[selected_row])std::snprintf(best,sizeof(best),english()?"Best %ucm":"记录%u厘米",state.fish_records[selected_row]);
      else std::snprintf(best,sizeof(best),"%s",tr("Not caught","尚未钓获"));
      if(goal)std::snprintf(skill,sizeof(skill),english()?"Lv%d %u/%d":"钓技%d %u/%d",level+1,state.fish_caught,goal);
      else std::snprintf(skill,sizeof(skill),"%s",tr("Lv4 / MAX","钓技4 · 满级"));
      std::snprintf(b,sizeof(b),"%s · %s",best,skill);
      fit_text(x+12,y+174,b,202,Sage);
      fit_text(x+12,y+190,english()?f.hint_en:f.hint_cn,202,Muted);
    } else {
      int item=collection_item(collection_category,selected_row);
      if(collection_category==0) {
        if(item<=15)std::snprintf(b,sizeof(b),english()?"Grow %s; %u watered nights":"种下%s，浇水照料%u夜",english()?Crops[item-12].en:Crops[item-12].cn,Crops[item-12].days);
        else std::snprintf(b,sizeof(b),"%s",tr("Cook at your cottage stove","在农舍灶台制作后即可收录"));
      } else {
        const Forage *source=nullptr;for(const auto &f:Foraging)if(f.item==item){source=&f;break;}
        char available[64]{};size_t at=0;
        for(int season=0;season<4;++season)if(source&&(source->seasons&(1u<<season)))
          at+=std::snprintf(available+at,sizeof(available)-at,"%s%s",at?" ":"",english()?seasons_en[season]:seasons_cn[season]);
        std::snprintf(b,sizeof(b),"%s\n%s",source?region_name(source->region,english()):"",available);
      }
      paragraph(x+12,y+174,b,202,2,Muted);
    }
  }
}
void draw_fishing(int x,int y) {
  using Phase=FishingSession::Phase;
  bool caught=fishing.phase==Phase::Caught,escaped=fishing.phase==Phase::Escaped;
  IconCanvas canvas;char b[96];
  if(caught) {
    rounded_rect(x+20,y+38,188,51,Soft,6);
    art::fish_portrait(canvas,x+88,y+46,fishing.species,3);
    const auto &f=FishSpeciesList[fishing.species];
    center_text(x+114,y+96,english()?f.en:f.cn,Sage);
    std::snprintf(b,sizeof(b),english()?"%d cm   Fish +%d":"%d 厘米   鲜鱼 +%d",fishing.length,fishing.count);
    center_text(x+114,y+114,b);
    fit_text(x+18,y+134,fishing.discovered?tr("NEW DISCOVERY! Check your journal","新发现！已记入晴露收藏册"):fishing.record?tr("A new personal length record!","刷新了这种鱼的最大长度记录！"):tr("Fresh fish: ship, gift or deliver","鲜鱼可以出货、送礼或交付委托"),194,Sage);
    if(fishing.perfect)center_text(x+114,y+151,fishing.count==2?tr("PERFECT! Double catch","完美收竿！双份渔获"):tr("PERFECT! Fish stack full","完美收竿！鲜鱼已经装满"),farm_palette::WoodDark);
    icon_label_button({x+12,y+174,100,25},farm_ui::Icon::Journal,tr("COLLECTION","收藏册"));
    icon_label_button({x+117,y+174,100,25},farm_ui::Icon::Rod,tr("CAST AGAIN","再钓一竿"));
    return;
  }
  if(escaped) {
    art::fish_portrait(canvas,x+90,y+50,std::max(0,fishing.species),3,false);
    center_text(x+114,y+98,tr("The fish slipped away","鱼溜走了，下次还有机会"),Muted);
    paragraph(x+20,y+123,tr("Reel on the bite, then stop the marker inside the green zone.","浮漂下沉时提竿，再让指针停在绿色区域。每条鱼有两次收线机会。"),188,3,Muted);
    icon_label_button(layout().confirm(174,25),farm_ui::Icon::Rod,tr("TRY AGAIN","再试一次"));
    return;
  }
  bool reeling=fishing.phase==Phase::Reeling,bite=fishing.phase==Phase::Bite;
  rounded_rect(x+19,y+40,190,66,farm_palette::Water,5);
  for(int i=0;i<7;++i)rect(x+30+i*24,y+72+(i%2)*6,12,1,art::rgb(149,197,175));
  if(reeling)art::fish_portrait(canvas,x+90+fishing.cursor()/70,y+66,fishing.species,2);
  else {
    int bob=int(fishing.elapsed/250)%2;
    line(x+75,y+43,x+116,y+78,Paper);
    rect(x+113,y+(bite?91:77+bob),6,5,Border);
    rect(x+114,y+(bite?91:77+bob),4,2,Paper);
    rect(x+114,y+(bite?93:79+bob),4,2,farm_palette::Brass);
    if(bite)center_text(x+142,y+50,"!",Paper,2);
  }
  if(reeling) {
    int start=x+25,width=178;
    rounded_rect(start-2,y+114,width+4,18,Border,3);
    rect(start,y+117,width,12,Soft);
    int lo=(fishing.center-fishing.radius)*width/1000,hi=(fishing.center+fishing.radius)*width/1000;
    rect(start+lo,y+117,hi-lo,12,Sage);
    rect(start+(fishing.center-55)*width/1000,y+117,110*width/1000,12,farm_palette::GrassLight);
    int marker=start+fishing.cursor()*width/1000;
    rect(marker-1,y+112,3,22,Paper);
    std::snprintf(b,sizeof(b),english()?"Stop in green / %d tries":"停在绿区收线 · 剩%d次",fishing.attempts);
    center_text(x+114,y+142,b,Sage);
  } else center_text(x+114,y+126,bite?tr("BITE! Set the hook!","咬钩了！快提竿！"):tr("Wait for the float to dip...","等浮漂下沉，再提竿…"),bite?Sage:Muted);
  icon_label_button(layout().confirm(174,25),farm_ui::Icon::Rod,reeling?tr("REEL IN","收线"):tr("SET HOOK","提竿"),bite||reeling);
}
void modal() {
  int x = panel_left(), y = panel_top();
  panel(x, y, 228, 210);
  rounded_rect(x + 7, y + 5, 214, 25, farm_palette::Button);
  const char *name =
      page == Page::Shop       ? tr("DEWFIELD SEEDS", "晴露种子铺")
      : page == Page::Bag      ? tr("BAG / TOP ROW HOTBAR", "背包 · 首排物品栏")
      : page == Page::World    ? tr("VALLEY MAP", "晴露谷地图")
      : page == Page::People   ? tr("VILLAGERS", "村民与日程")
      : page == Page::Story    ? tr("STORY JOURNAL", "故事手册")
      : page == Page::Dialogue ? npc_name(talking_npc, english())
      : page == Page::Settings ? tr("SETTINGS", "设置")
      : page == Page::Orders   ? tr("VILLAGE REQUESTS", "晴露镇每日委托")
      : page == Page::Calendar ? tr("SEASON CALENDAR", "晴露谷日历")
      : page == Page::Forecast ? tr("WEATHER CHANNEL", "农舍天气频道")
      : page == Page::Shipping ? tr("SHIPPING BIN", "出货箱")
      : page == Page::Collections ? tr("VALLEY COLLECTION", "晴露收藏册")
      : page == Page::Kitchen  ? tr("COTTAGE KITCHEN", "农舍小厨房")
      : page == Page::Guide    ? tr("FARM JOURNAL", "农场手册")
      : page == Page::Fishing  ? tr("A QUIET MOMENT", "池畔垂钓")
      : page == Page::Letter   ? tr("A LETTER FROM LIN", "林婆婆的来信")
      : page == Page::Sleep    ? tr("HOME, SWEET HOME", "回到温暖的小屋")
                               : tr("A NEW MORNING", "又是美好的一天");
  char title[64];
  if(page==Page::Calendar) {
    std::snprintf(title,sizeof(title),english()?"%s / Year %u":"%s · 第%u年日历",english()?seasons_en[season()]:seasons_cn[season()],(state.day-1)/112+1);name=title;
  }
  if(page==Page::Collections) {
    std::snprintf(title,sizeof(title),english()?"COLLECTION %d/%d":"晴露收藏册 %d/%d",discovered_total(state),CollectionTotal);name=title;
  }
  center_text(x + 114, y + 10, name);
  icon_button(layout().close(), farm_ui::Icon::Close);
  line(x + 12, y + 31, x + 215, y + 31, art::Earth);
  char b[96];
  if(page==Page::Collections)draw_collections(x,y);
  else if(page==Page::Calendar) {
    const char *days[]={"M","T","W","T","F","S","S"};
    for(int i=0;i<7;++i)center_text(x+27+i*29,y+40,days[i],Muted);
    for(int day=1;day<=28;++day) {
      int xx=x+14+(day-1)%7*29,yy=y+57+(day-1)/7*25;
      rounded_rect(xx,yy,26,22,day==selected_row+1?Sage:Soft,3);
      std::snprintf(b,sizeof(b),"%d",day);center_text(xx+13,yy+4,b,day==selected_row+1?Paper:Text);
      constexpr int birthdays[]={7,12,19,25};
      if(day==birthdays[season()])rect(xx+11,yy+18,4,2,art::Gold);
    }
    fit_text(x+12,y+161,calendar_event((state.day-1)/28*28+selected_row+1,english()),202,Sage);
    fit_text(x+12,y+181,tr("28 days / season; dots: birthdays", "每季28天 · 金点标记生日"),202,Muted);
  } else if(page==Page::Forecast) {
    auto w=weather_on(state.day==65535?1:state.day+1);
    IconCanvas rr;art::world_appliance(rr,x+114,y+83,true);
    std::snprintf(b,sizeof(b),"%s%s",tr("Tomorrow: ","明天天气："),weather_name(w,english()));
    center_text(x+114,y+101,b,Sage);
    paragraph(x+14,y+126,wet_weather(w)?tr("Rain waters outdoor crops. Pack a meal for a forest walk.","雨水会浇湿露天农田。\n带上食物，去林间走走吧。"):
      w==Weather::Snow?tr("Snow does not water crops. Winter roots grow in the woods.","积雪不会浇水。\n冬日林间可以找到冬根菜。"):
      tr("Remember to water your crops. The forecast stays the same all day.","记得给作物浇水。\n明天醒来，天气就和预报一致。"),200,4,Muted);
  } else if(page==Page::Shipping) {
    paragraph(x+14,y+45,tr("Hold an item and put it in the bin. One item per tap; payment arrives next morning.","拿起要卖的物品，朝出货箱放入。每次放一份，次日结算；不会出售背包其他东西。"),200,4);
    std::snprintf(b,sizeof(b),"%s %uG",tr("Pending:","待结算："),state.shipping);center_text(x+114,y+118,b,Sage);
    if(state.last_shipped) {item_label(b,sizeof(b),state.last_shipped,false);fit_text(x+14,y+143,b,200);}
    auto rr=layout().confirm();button(rr.x,rr.y,rr.w,rr.h,tr("Retrieve last item", "取回最后放入的物品"));
  } else if (page == Page::Shop) {
    for (int i = 0; i < 4; ++i) {
      int yy = y + 39 + i * 27;
      rounded_rect(x + 11, yy - 1, 206, 26, Soft);
      pocket::sprite(x + 14, yy + 2, sprites::seeds);
      text(x + 37, yy, tr(Crops[i].en, Crops[i].cn));
      std::snprintf(b, sizeof(b), "%uG / %u%s", Crops[i].price, Crops[i].days,
                    tr("days", "天"));
      text(x + 37, yy + 12, b, Muted);
      auto r = layout().shop_buy(i);
      button(r.x, r.y, r.w, r.h, crop_in_season(state.day,i)?tr("BUY", "购买"):tr("OFF", "非当季"),
             using_controller() && selected_row == i);
    }
    std::snprintf(b, sizeof(b), "%s %uG", tr("Gold:", "余额："), state.gold);
    fit_text(x + 12, y + 153, toast_ms ? toast : b, 204,
             toast_ms ? Sage : Muted);
    button(x + 12, y + 174, 202, 24,
           state.capacity < 60 ? tr("Bigger can  180G", "升级水壶 180G / 60 格")
                               : tr("Watering can upgraded", "水壶已升级"));
  } else if (page == Page::Bag) {
    for (int i = 0; i < InventorySize; ++i)
      inventory_cell(layout().bag_slot(i), i, true);
    int item = state.slots[bag_selection];
    if (item == 5 || item == 6) {
      auto cost = tool_cost(state, Tool(item - 1));
      if (cost.gold) {
        std::snprintf(b, sizeof(b), "%uG %s%u %s%u", cost.gold, tr("W", "木"),
                      cost.wood, tr("S", "石"), cost.stone);
        fit_text(x + 12, y + 155, b, 143, Muted);
        auto r = layout().bag_upgrade();
        button(r.x, r.y, r.w, r.h, tr("UPGRADE", "升级"));
      } else {
        item_label(b, sizeof(b), item, false);
        fit_text(x + 12, y + 155, b, 202, Sage);
      }
    } else {
      item_label(b, sizeof(b), item);
      fit_text(x + 12, y + 155, b, 202, Muted);
    }
    icon_label_button(layout().bag_action(0), farm_ui::Icon::Journal,
                      tr("JOURNAL", "农场手册"),
                      using_controller() && selected_row == 0);
    icon_label_button(layout().bag_action(1), farm_ui::Icon::Sprinkler,
                      tr("SPRINKLER", "造洒水器"),
                      using_controller() && selected_row == 1);
  } else if (page == Page::Settings) {
    const char *labels[] = {
        tr("Virtual joystick", "虚拟摇杆"), tr("Action button", "悬浮按钮"),
        tr("Background music", "背景音乐"), tr("Auto run", "自动跑步"),
        tr("Language", "语言")};
    bool on[] = {state.show_joystick, state.show_action, state.music, state.auto_run};
    for (int i = 0; i < 5; ++i) {
      auto r = layout().setting(i);
      button(r.x, r.y, r.w, r.h, "", using_controller() && selected_row == i);
      text(r.x + 9, r.y + 7, labels[i], Text);
      const char *value = i == 4  ? tr("English", "中文")
                          : on[i] ? tr("ON", "开")
                                  : tr("OFF", "关");
      text(r.x + r.w - 9 - text_width(value), r.y + 7, value, Sage);
    }
  } else if (page == Page::Guide) {
    const char *labels[] = {
        tr("HOME", "小屋休息"),   tr("SEED SHOP", "购买种子"),
        tr("SHIPPING", "出货箱"), tr("WELL", "井边补水"),
        tr("POND", "池塘钓鱼"),   tr("VILLAGER", "村民委托"),
        tr("REQUESTS", "每日委托"), tr("KITCHEN", "厨房料理")};
    for (int i = 0; i < 8; ++i)
      button(x + 12 + (i % 2) * 105, y + 40 + (i / 2) * 29, 100, 25, labels[i],
             selected_row == i);
    const char *tabs[] = {tr("MAP", "地图"), tr("PEOPLE", "村民"),
                          tr("STORY", "剧情"),tr("FINDS","收藏")};
    for (int i = 0; i < 4; ++i) {
      auto r=layout().journal_tab(i);button(r.x,r.y,r.w,r.h,tabs[i]);
    }
    if(int ready=ready_collection_rewards(state)) {
      std::snprintf(b,sizeof(b),english()?"%d badge rewards ready / Y: finds":"%d枚徽章可领奖 · 收藏册查看",ready);
      fit_text(x+12,y+191,b,202,Sage);
    } else fit_text(x + 12, y + 191, story_goal(state, english()), 202, Muted);
  } else if (page == Page::Orders) {
    for (int i=0;i<NpcCount;++i) {
      auto order = daily_order(state,i); int yy=y+36+i*33;
      rounded_rect(x+12,yy,202,31,Soft,4);
      item_icon(x+25,yy+14,order.item);
      text(x+40,yy+2,npc_name(i,english()));
      std::snprintf(b,sizeof(b),"+%uG",order.reward);
      text(x+102,yy+2,b,Sage);
      char label[64];item_label(label,sizeof(label),order.item,false);
      std::snprintf(b,sizeof(b),"%s %u/%u",label,item_count(state,order.item),order.count);
      fit_text(x+40,yy+16,b,118,Muted);
      button(x+163,yy+5,48,22,order.done?tr("DONE","已交"):tr("GIVE","交付"),
             using_controller()&&selected_row==i);
    }
    bool here=state.region==unsigned(Region::Town)&&near(state,18,10);
    button(x+12,y+170,202,23,here?tr("BACK TO JOURNAL","返回手册"):
           tr("WALK TO NOTICEBOARD","前往镇上公告栏"),using_controller()&&selected_row==4);
    fit_text(x+12,y+196,toast_ms?toast:tr("New requests every morning","每日刷新 · 交付获得金币和好感"),202,Muted);
  } else if (page == Page::Kitchen) {
    for(int i=0;i<2;++i) {
      const auto&r=Recipes[i];int yy=y+39+i*59;
      rounded_rect(x+12,yy,202,55,Soft,5);item_icon(x+26,yy+18,r.item);
      item_label(b,sizeof(b),r.item,false);fit_text(x+43,yy+4,b,114);
      std::snprintf(b,sizeof(b),"+%u %s",Foods[r.item-19].energy,tr("energy","体力"));
      text(x+43,yy+19,b,Sage);
      char first[40],second[40];item_label(first,sizeof(first),r.first,false);item_label(second,sizeof(second),r.second,false);
      std::snprintf(b,sizeof(b),"%s %u/%u + %s %u/%u",first,item_count(state,r.first),r.first_count,second,item_count(state,r.second),r.second_count);
      fit_text(x+20,yy+39,b,185,Muted);
      button(x+163,yy+7,48,24,tr("COOK","制作"),using_controller()&&selected_row==i);
    }
    bool here=state.region==unsigned(Region::Home)&&near(state,21,10);
    button(x+12,y+164,202,24,here?tr("BACK TO JOURNAL","返回手册"):
           tr("WALK TO COTTAGE STOVE","前往农舍灶台"),using_controller()&&selected_row==2);
    paragraph(x+12,y+192,toast_ms?toast:tr("Hold food and use it to eat","拿起食物，点操作按钮即可食用"),202,1,Muted);
  } else if (page == Page::World) {
    for (int i = 0; i < RegionCount; ++i) {
      auto r = layout().destination(i);
      button(r.x, r.y, r.w, r.h, region_name(Region(i), english()),
             selected_row == i);
    }
    paragraph(x + 12, y + 162,
              tr("Choose a destination to walk there. The lake opens after "
                 "repairing the bridge.",
                 "选择目的地会沿路走过去。林间旧桥修好后，才能到达镜光湖。"),
              202, 3, Muted);
  } else if (page == Page::People) {
    for (int i = 0; i < 4; ++i) {
      auto p = npc_pose(state, i);
      int yy = y + 36 + i * 33;
      button(x + 12, yy, 202, 30, "", selected_row == i);
      text(x + 18, yy + 3, npc_name(i, english()), Text);
      std::snprintf(b, sizeof(b), "%u/4", state.bonds[i] / 10);
      text(x + 200 - text_width(b), yy + 3, b, Sage);
      fit_text(x + 18, yy + 16, region_name(p.region, english()), 160, Muted);
    }
    fit_text(
        x + 12, y + 179,
        tr("Choose a friend to walk to them", "点村民名字，沿路前往他的位置"),
        202, Muted);
  } else if (page == Page::Story) {
    fit_text(x + 12, y + 40, story_title(state, english()), 202, Sage);
    paragraph(x + 12, y + 61, story_line(state, english()), 202, 6, Text);
    paragraph(x + 12, y + 143, story_goal(state, english()), 202, 3, Muted);
    button(x + 24, y + 177, 180, 25, tr("BACK", "返回手册"));
  } else if (page == Page::Dialogue) {
    const auto &portrait = art::villager_sprite(talking_npc, 2);
    pocket::sprite(x + 15, y + 35, portrait);
    text(x + 47, y + 38, npc_job(talking_npc, english()), Sage);
    std::snprintf(b, sizeof(b), "%s %u/4", tr("Hearts", "好感"),
                  state.bonds[talking_npc] / 10);
    text(x + 47, y + 55, b, Muted);
    paragraph(x + 14, y + 78, letter, 200, 6, Text);
    const char *labels[] = {tr("STORY", "聊近况"), tr("GIFT", "送礼物"),
                            tr("BYE", "回头见")};
    for (int i = 0; i < 3; ++i)
      button(x + 12 + i * 69, y + 177, 65, 25, labels[i], selected_row == i);
  } else if (page == Page::Sleep) {
    center_text(x + 114, y + 62,
                tr("Rest until tomorrow?", "睡到明天早晨吗？"));
    center_text(
        x + 114, y + 86,
        tr("Watered crops grow overnight.", "浇过水的作物会在夜里生长。"),
        Muted);
    std::snprintf(b, sizeof(b), "%s %uG", tr("Shipping:", "待结算："),
                  state.shipping);
    center_text(x + 114, y + 110, b);
    button(x + 24, y + 160, 180, 30, tr("SLEEP", "晚安，晴露谷"), true);
  } else if (page == Page::Morning) {
    std::snprintf(b, sizeof(b), "%s %u", tr("DAY", "第"), state.day);
    center_text(x + 114, y + 47, b, Text, 2);
    std::snprintf(b, sizeof(b), "+ %u G", morning_gold);
    center_text(x + 114, y + 86, b, art::Green, 2);
    center_text(x + 114, y + 120,
                tr("Energy and water restored", "体力和水壶已恢复"), Muted);
    center_text(
        x + 114, y + 139,
        state.rain
            ? tr("Rain waters every crop today", "今天下雨，田地已自动浇水")
            : tr("A good day to grow something", "今天也适合种下新的希望"),
        Muted);
    button(x + 24, y + 169, 180, 28, tr("GOOD MORNING", "开始新的一天"), true);
  } else if (page == Page::Letter) {
    paragraph(x + 14, y + 48, letter, 200, 4, Text);
    std::snprintf(b, sizeof(b), "%s %u / 3", tr("Friendship", "友好度"),
                  state.friendship);
    text(x + 14, y + 111, b, Muted);
    text(x + 14, y + 137,
         tr("Come back when you are ready.", "完成目标后，再来找我吧。"),
         Muted);
    button(x + 24, y + 171, 180, 27, tr("SEE YOU", "回头见"));
  } else if (page == Page::Fishing) {
    draw_fishing(x,y);
  }
}
void scene_target(int wx, int wy, int &tx, int &ty) {
  world_objects.pick(wx, wy, state, tx, ty);
}
void go_to(Region region, int x, int y, bool use) {
  page = Page::Farm;
  stop_motion();
  if (state.region == unsigned(region)) {
    journey_region = -1;
    begin_route(x, y, use);
    return;
  }
  int portal = next_portal(state, region);
  if (portal < 0)
    return;
  journey_region = int(region);
  journey_x = x;
  journey_y = y;
  journey_use = use;
  begin_route(Portals[portal].x, Portals[portal].y, true);
}
void guide_route(int i) {
  if (i == 6 || i == 7) { open_page(i == 6 ? Page::Orders : Page::Kitchen); return; }
  if (i < 0 || i >= 6) return;
  following_npc = -1;
  const int goals[][2] = {{10, 9}, {15, 10}, {8, 9}, {7, 8}, {24, 17}, {21, 8}};
  if (i == 4)
    equip_item(state, 7);
  Region r = i == 0 ? Region::Home : i == 1 ? Region::SeedShop : Region::Farm;
  go_to(r, goals[i][0], goals[i][1], true);
}
void order_action(int choice) {
  bool here=state.region==unsigned(Region::Town)&&near(state,18,10);
  if (!here) { go_to(Region::Town,18,10,true); return; }
  if (choice == 4) { open_page(Page::Guide); return; }
  if (deliver_order(state,choice)) msg("交付完成，金币和好感已增加", "Delivered! Gold and friendship gained");
  else msg("物品不足，或这份委托今天已完成", "Need the items; each request is once a day");
  changed();
}
void kitchen_action(int choice) {
  bool here=state.region==unsigned(Region::Home)&&near(state,21,10);
  if (!here) { go_to(Region::Home,21,10,true); return; }
  if (choice == 2) { open_page(Page::Guide); return; }
  if (cook(state,choice)) msg("料理完成！拿起食物可以恢复体力", "Cooked! Hold food and use it to eat");
  else msg("材料不足，或料理已装满背包", "Need ingredients and space for the meal");
  changed();
}
void dialogue_action(int choice) {
  if (talking_npc < 0 || talking_npc >= 4)
    return;
  if (choice == 0) {
    if (advance_story(state, talking_npc)) {
      base_dirty = true;
      std::snprintf(letter, sizeof(letter), "%s",
                    story_completion(state.story, english()));
    } else
      std::snprintf(letter, sizeof(letter), "%s\n%s",
                    story_line(state, english()), story_goal(state, english()));
  } else if (choice == 1) {
    if (gift(state, talking_npc, held_item())) {
      bool personal = personal_event(state, talking_npc);
      std::snprintf(letter, sizeof(letter), "%s",
                    personal ? npc_line(state, talking_npc, true, english())
                             : tr("Thank you! I'll treasure this.",
                                  "谢谢你！这份礼物我会好好珍惜。"));
    } else
      std::snprintf(letter, sizeof(letter), "%s",
                    tr("Hold a gift; one gift per friend each day.",
                       "先在背包拿起礼物。每位村民每天可收一份。"));
  } else {
    page = Page::Farm;
  }
  changed();
}
void bag_action(int index) {
  if (index == 0) {
    open_page(Page::Guide);
    return;
  }
  int i = state.region ? -1 : plot_index(state.px / 16, state.py / 16);
  if (craft_sprinkler(state, i))
    msg("洒水器已放置，明早自动浇水", "Sprinkler placed. Active tomorrow");
  else
    msg("站在田里，需要 8 木材和 6 石头",
        "Stand in field. Need 8 wood + 6 stone");
  changed();
  page = Page::Farm;
}
void shop_action(int index) {
  if (index < 4) {
    if (buy_seed(state, index))
      msg("种子已放进背包", "Seeds added to backpack");
    else
      msg("金币不足", "Not enough gold");
  } else {
    if (upgrade(state))
      msg("水壶升级完成", "Watering can upgraded");
    else
      msg("需要 180 金币", "You need 180 gold");
  }
  changed();
}
void panel_tap(int px, int py) {
  auto m=layout().modal();
  if(page==Page::Calendar && px>=m.x+14 && px<m.x+217 && py>=m.y+57 && py<m.y+157) {
    selected_row=(py-m.y-57)/25*7+(px-m.x-14)/29;redraw();return;
  }
  if(page==Page::Shipping && layout().confirm().contains(px,py)) {if(unship(state))changed();return;}
  auto ui = layout();
  if (!ui.modal().contains(px, py))
    return;
  if (ui.close().contains(px, py)) {
    if(page==Page::Fishing)cancel_tool();
    page = page==Page::Collections?collection_parent:Page::Farm;
    return;
  }
  if(page==Page::Collections) {
    for(int i=0;i<4;++i)if(ui.collection_tab(i).contains(px,py)) {
      collection_category=i;selected_row=0;redraw();return;
    }
    int count=collection_category==3?MilestoneCount:collection_size(collection_category);
    for(int i=0;i<count;++i)if(ui.collection_cell(collection_category,i).contains(px,py)) {
      selected_row=i;redraw();return;
    }
    if(collection_category==3&&ui.confirm(173,25).contains(px,py))collection_action();
  } else if(page==Page::Fishing) {
    if(fishing.phase==FishingSession::Phase::Caught) {
      if(farm_ui::Rect{m.x+12,m.y+174,100,25}.contains(px,py))open_collections(2,fishing.species,Page::Fishing);
      else if(farm_ui::Rect{m.x+117,m.y+174,100,25}.contains(px,py))facing_action();
    } else if(ui.confirm(174,25).contains(px,py))facing_action();
  } else if (page == Page::Orders) {
    for(int i=0;i<4;++i) if(farm_ui::Rect{ui.modal().x+163,ui.modal().y+41+i*33,48,22}.contains(px,py)) {
      order_action(i);return;
    }
    if(farm_ui::Rect{ui.modal().x+12,ui.modal().y+170,202,23}.contains(px,py)) order_action(4);
  } else if (page == Page::Kitchen) {
    for(int i=0;i<2;++i) if(farm_ui::Rect{ui.modal().x+163,ui.modal().y+46+i*59,48,24}.contains(px,py)) {
      kitchen_action(i);return;
    }
    if(farm_ui::Rect{ui.modal().x+12,ui.modal().y+164,202,24}.contains(px,py)) kitchen_action(2);
  } else if (page == Page::Dialogue) {
    for (int i = 0; i < 3; ++i)
      if (farm_ui::Rect{ui.modal().x + 12 + i * 69, ui.modal().y + 177, 65, 25}
              .contains(px, py)) {
        dialogue_action(i);
        return;
      }
  } else if (page == Page::World) {
    for (int i = 0; i < RegionCount; ++i)
      if (ui.destination(i).contains(px, py)) {
        following_npc = -1;
        go_to(Region(i),
              i == 4   ? 16
              : i == 3 ? 24
                       : 15,
              i == 4 ? 17 : 12, false);
        return;
      }
  } else if (page == Page::People) {
    for (int i = 0; i < 4; ++i)
      if (farm_ui::Rect{ui.modal().x + 12, ui.modal().y + 36 + i * 33, 202, 30}
              .contains(px, py)) {
        following_npc = i;
        auto p = npc_pose(state, i);
        go_to(p.region, p.x / 16, p.y / 16, true);
        return;
      }
  } else if (page == Page::Story) {
    if (ui.confirm(177, 25).contains(px, py))
      open_page(Page::Guide);
  } else if (page == Page::Shop) {
    for (int i = 0; i < 4; ++i)
      if (ui.shop_buy(i).contains(px, py)) {
        shop_action(i);
        return;
      }
    if (ui.upgrade().contains(px, py))
      shop_action(4);
  } else if (page == Page::Settings) {
    for (int i = 0; i < 5; ++i)
      if (ui.setting(i).contains(px, py)) {
        setting_action(i);
        return;
      }
  } else if (page == Page::Bag) {
    if (ui.bag_upgrade().contains(px, py)) {
      tool_upgrade_action();
      return;
    }
    for (int i = 0; i < 2; ++i)
      if (ui.bag_action(i).contains(px, py)) {
        bag_action(i);
        return;
      }
  } else if (page == Page::Guide) {
    for (int i = 0; i < 4; ++i)
      if (ui.journal_tab(i).contains(px,py)) {
        if(i==3)open_collections();else open_page(i == 0 ? Page::World : i == 1 ? Page::People : Page::Story);
        return;
      }
    for (int i = 0; i < 8; ++i)
      if (ui.destination(i).contains(px, py)) {
        guide_route(i);
        return;
      }
  } else if (page == Page::Sleep && ui.confirm(160, 30).contains(px, py))
    sleep_now();
  else if (page == Page::Morning && ui.confirm().contains(px, py))
    page = Page::Farm;
  else if (page == Page::Letter && ui.confirm(171, 27).contains(px, py))
    page = Page::Farm;
}
} // namespace
namespace game {
const char *title() { return "晴露农场 Dewfield"; }
const char *app_id() { return "dewfield"; }
bool input_active() { return moving || route || stick || touch_down; }
bool back() {
  if (!title_screen && page != Page::Farm) {
    stop_motion();
    cancel_tool();
    page = page==Page::Collections?collection_parent:Page::Farm;
    redraw();
    return true;
  }
  return false;
}
void init() {
  fresh(state);
  scene_surface(false);
  music_enabled(state.music);
}
void settings() { open_page(Page::Settings); }
void reset() { /* Restart is deliberately not a save-deleting action. */ }
size_t serialize(uint8_t *d, size_t n) {
  if (save_locked)
    return 0;
  return encode(state, d, n);
}
void load(const uint8_t *d, size_t n) {
  if (!decode(state, d, n)) {
    save_locked = true;
    msg("存档损坏，已保护原存档", "Save damaged. Original protected");
  }
  music_enabled(state.music);
  base_dirty = true;
}
bool menu_hit(int x, int y) {
  return !title_screen && page == Page::Farm &&
         layout().header_button(2).contains(x, y);
}
void draw_menu(bool paused, int row, bool exiting) {
  if (!paused)
    return;
  begin_ui();
  shade();
  int x = (WIDTH - 192) / 2, y = (HEIGHT - 180) / 2;
  panel(x, y, 192, 180);
  center_text(WIDTH / 2, y + 12, tr("PAUSED", "农场小憩"));
  const char *labels[] = {
      tr("CONTINUE", "继续游戏"), tr("SAVE PROGRESS", "保存进度"),
      tr("SAVE AND EXIT", "保存并退出"), tr("SETTINGS", "设置")};
  for (int i = 0; i < 4; ++i)
    button(x + 8, y + 40 + i * 30, 176, 27, labels[i], i == row);
  if (exiting)
    center_text(WIDTH / 2, y + 162, tr("Saving...", "保存中…"));
}
void key(Key k) {
  if (title_screen) {
    if (k == Key::A || k == Key::B) {
      title_screen = false;
      scene_surface(true);
      msg("欢迎！点开背包里的农场手册", "Welcome! Open the farm journal");
    }
    return;
  }
  if (page == Page::Fishing) {
    if(k==Key::B) {cancel_tool();page=Page::Farm;redraw();}
    else if(k==Key::X && fishing.phase==FishingSession::Phase::Caught)open_collections(2,fishing.species,Page::Fishing);
    else if (k == Key::A)
      facing_action();
    return;
  }
  if(page==Page::Collections) {
    int count=collection_category==3?MilestoneCount:collection_size(collection_category);
    if(k==Key::L||k==Key::R) {
      collection_category=(collection_category+(k==Key::L?3:1))%4;selected_row=0;
    } else if(k==Key::LEFT && selected_row%2)--selected_row;
    else if(k==Key::RIGHT && selected_row%2==0 && selected_row+1<count)++selected_row;
    else if(k==Key::UP && selected_row>=2)selected_row-=2;
    else if(k==Key::DOWN && selected_row+2<count)selected_row+=2;
    else if(k==Key::A)collection_action();
    else if(k==Key::B)page=collection_parent;
    redraw();return;
  }
  if (page != Page::Farm) {
    if(page==Page::Guide&&k==Key::Y){open_collections();return;}
    if(page==Page::Forecast||page==Page::Calendar||page==Page::Shipping) {
      if(page==Page::Calendar&&(k==Key::LEFT||k==Key::RIGHT||k==Key::UP||k==Key::DOWN)) {
        selected_row=std::clamp(selected_row+(k==Key::LEFT?-1:k==Key::RIGHT?1:k==Key::UP?-7:7),0,27);redraw();return;
      }
      if(k==Key::A&&page==Page::Shipping) {if(unship(state))changed();}
      else if(k==Key::A||k==Key::B)page=Page::Farm;
      redraw();return;
    }
    if (page == Page::Guide &&
        (k == Key::L || k == Key::R || k == Key::SELECT)) {
      open_page(k == Key::L   ? Page::World
                : k == Key::R ? Page::People
                              : Page::Story);
      return;
    }
    if (k == Key::B) {
      page = Page::Farm;
      return;
    }
    if (page == Page::Guide || page == Page::World) {
      if (k == Key::UP && selected_row >= 2)
        selected_row -= 2;
      if (k == Key::DOWN && selected_row + 2 < (page == Page::World ? RegionCount : 8))
        selected_row += 2;
      if (k == Key::LEFT && (selected_row % 2))
        --selected_row;
      if (k == Key::RIGHT && !(selected_row % 2) && selected_row + 1 < (page == Page::World ? RegionCount : 8))
        ++selected_row;
    } else if (page == Page::People || page == Page::Dialogue) {
      int count = page == Page::People ? 4 : 3;
      if (k == Key::UP || k == Key::LEFT)
        selected_row = (selected_row + count - 1) % count;
      if (k == Key::DOWN || k == Key::RIGHT)
        selected_row = (selected_row + 1) % count;
    } else if (page == Page::Orders || page == Page::Kitchen) {
      int count=page==Page::Orders?5:3;
      if(k==Key::UP||k==Key::LEFT) selected_row=(selected_row+count-1)%count;
      if(k==Key::DOWN||k==Key::RIGHT) selected_row=(selected_row+1)%count;
    } else if (page == Page::Settings) {
      if (k == Key::UP)
        selected_row = (selected_row + 4) % 5;
      if (k == Key::DOWN)
        selected_row = (selected_row + 1) % 5;
    } else if (page == Page::Bag) {
      if (k == Key::SELECT)
        tool_upgrade_action();
      if(k==Key::Y){open_page(Page::Guide);return;}
      if(selected_row<0) {
        if(k==Key::LEFT&&bag_selection%7) --bag_selection;
        if(k==Key::RIGHT&&bag_selection%7<6) ++bag_selection;
        if(k==Key::UP&&bag_selection>=7) bag_selection-=7;
        if(k==Key::DOWN) {
          if(bag_selection<21)bag_selection+=7;
          else selected_row=bag_selection%7>=3?1:0;
        }
      } else {
        if(k==Key::LEFT)selected_row=0;
        if(k==Key::RIGHT)selected_row=1;
        if(k==Key::UP){bag_selection=21+bag_selection%7;selected_row=-1;}
      }
    } else {
      if (k == Key::UP)
        selected_row = std::max(0, selected_row - 1);
      if (k == Key::DOWN)
        selected_row = std::min(4, selected_row + 1);
    }
    if (k == Key::A) {
      if (page == Page::World) {
        following_npc = -1;
        go_to(Region(selected_row),
              selected_row == 4   ? 16
              : selected_row == 3 ? 24
                                  : 15,
              selected_row == 4 ? 17 : 12, false);
      } else if (page == Page::People) {
        following_npc = selected_row;
        auto p = npc_pose(state, selected_row);
        go_to(p.region, p.x / 16, p.y / 16, true);
      } else if (page == Page::Orders) order_action(selected_row);
      else if (page == Page::Kitchen) kitchen_action(selected_row);
      else if (page == Page::Dialogue)
        dialogue_action(selected_row);
      else if (page == Page::Story)
        open_page(Page::Guide);
      else if (page == Page::Shop) {
        shop_action(selected_row);
      } else if (page == Page::Guide)
        guide_route(selected_row);
      else if (page == Page::Sleep)
        sleep_now();
      else if (page == Page::Settings)
        setting_action(selected_row);
      else if (page == Page::Bag) {
        if(selected_row>=0)bag_action(selected_row);
        else {select_slot(state,state.active_slot==bag_selection?-1:bag_selection);changed();}
      }
      else
        page = Page::Farm;
    }
    redraw();
    return;
  }
  if (unsigned(k) < 4) {
    navigate(int(k));
    return;
  }
  if (k == Key::A || k == Key::B) {
    repeat_use=held_item()<12;
    facing_action();
  }
  else if(swing&&(k==Key::L||k==Key::R||k==Key::SELECT))return;
  else if (k == Key::X)
    open_page(Page::Bag);
  else if (k == Key::Y)
    open_page(Page::Guide);
  else if (k == Key::L) {
    select_slot(state,
                (int(state.active_slot < 7 ? state.active_slot : 0) + 6) % 7);
    changed();
  } else if (k == Key::R) {
    select_slot(state,
                (int(state.active_slot < 7 ? state.active_slot : 6) + 1) % 7);
    changed();
  } else if (k == Key::SELECT) {
    cycle_seeds();
  }
}
void pointer(Phase phase, int x, int y) {
  if (phase == Phase::CANCEL) {
    touch_down = false;
    drag_from = -1;
    drag_active = false;
    action_hold = action_touch = false;
    stick = false;
    held_direction = -1;
    if (title_screen)
      redraw();
    return;
  }
  if (phase == Phase::DOWN) {
    touch_down = true;
    touchx = x;
    touchy = y;
    drag_from = -1;
    drag_active = false;
    if (title_screen) {
      redraw();
      return;
    }
    auto ui = layout();
    if(page==Page::Farm&&swing&&ui.belt().contains(x,y)) {touch_down=false;return;}
    if (page == Page::Bag) {
      for (int i = 0; i < InventorySize; ++i)
        if (ui.bag_slot(i).contains(x, y)) {
          bag_selection = i;
          drag_from = i;
          drag_x = x;
          drag_y = y;
          redraw();
          return;
        }
    }
    if (page == Page::Farm) {
      for (int i = 0; !swing && i < HotbarSize; ++i)
        if (ui.tool(i).contains(x, y)) {
          drag_from = i;
          drag_x = x;
          drag_y = y;
          return;
        }
      if (state.show_joystick && ui.joystick().contains(x, y)) {
        stick = true;
        stickx = x;
        sticky = y;
        route = false;
      } else if (state.show_action && ui.action().contains(x, y)) {
        repeat_use=held_item()<12;
        action_hold = action_touch = true;
        facing_action();
      }
    }
    return;
  }
  if (phase == Phase::MOVE) {
    if (drag_from >= 0) {
      drag_x = x;
      drag_y = y;
      if (state.slots[drag_from] &&
          std::abs(x - touchx) + std::abs(y - touchy) >= 5)
        drag_active = true;
      redraw();
      return;
    }
    if (stick) {
      auto joystick = layout().joystick();
      stickx =
          std::clamp(x, joystick.center_x() - 20, joystick.center_x() + 20);
      sticky =
          std::clamp(y, joystick.center_y() - 20, joystick.center_y() + 20);
    }
    return;
  }
  if (phase != Phase::UP || !touch_down)
    return;
  touch_down = false;
  if (action_touch) {
    repeat_use=false;
    action_hold = action_touch = false;
    return;
  }
  if (drag_from >= 0) {
    int source = drag_from;
    drag_from = -1;
    if (drag_active) {
      int destination = -1;
      for (int i = 0; i < (page == Page::Bag ? InventorySize : HotbarSize); ++i)
        if ((page == Page::Bag ? layout().bag_slot(i) : layout().tool(i))
                .contains(x, y))
          destination = i;
      if (destination >= 0 && swap_slots(state, source, destination))
        changed();
    } else if (page == Page::Farm && layout().tool(source).contains(x, y)) {
      select_slot(state, state.active_slot == source ? -1 : source);
      changed();
    } else if (page == Page::Bag && source < InventorySize &&
               layout().bag_slot(source).contains(x, y)) {
      select_slot(state, state.active_slot == source ? -1 : source);
      changed();
    }
    drag_active = false;
    redraw();
    return;
  }
  if (stick) {
    stick = false;
    held_direction = -1;
    return;
  }
  if (std::abs(x - touchx) + std::abs(y - touchy) > 18)
    return;
  if (title_screen) {
    title_screen = false;
    scene_surface(true);
    if(state.day==1&&state.minute<=365&&!state.harvested)msg("点地块自动操作，背包里有手册", "Tap tiles to farm. Journal in bag");
    else toast_ms=0;
    redraw();
    return;
  }
  if (page != Page::Farm) {
    panel_tap(x, y);
    redraw();
    return;
  }
  auto ui = layout();
  if (ui.header_button(0).contains(x, y)) {
    open_page(Page::Bag);
    return;
  }
  if (ui.header_button(1).contains(x, y)) {
    open_page(Page::Guide);
    return;
  }
  if(ui.calendar().contains(x,y)){open_page(Page::Calendar);return;}
  if (ui.vitals().contains(x, y) ||
      ui.header_button(2).contains(x, y))
    return;
  if (ui.belt().contains(x, y))
    return;
  if ((toast_ms || route) && notice().box.contains(x, y)) {
    if (toast_ms) {
      toast_ms = 0;
      redraw();
    }
    return;
  }
  if (!toast_ms && !route && ui.caption(caption_width()).contains(x, y)) {
    if (held_item() >= 8 && held_item() <= 11)cycle_seeds();
    return;
  }
  if (state.show_action && ui.action().contains(x, y)) {
    facing_action();
    return;
  }
  int tx = (x + camx) / 16, ty = (y + camy) / 16;
  journey_region = following_npc = -1;
  scene_target(x + camx, y + camy, tx, ty);
  int pi = plot_index(tx, ty);
  bool interactive =
      (!state.region && pi >= 0) || !walkable(state, tx, ty) ||
      portal_at(state,tx,ty) || npc_at(state,tx,ty)>=0 ||
      forage_at(state,tx,ty)>=0 ||
      (!state.region && ty==7 && tx==6) ||
      (state.region==unsigned(Region::Town) && tx==5 && ty==7);
  begin_route(tx, ty, interactive);
}
void tick(uint32_t elapsed) {
  for(auto &effect:gather_effects)if(effect.remaining) {
    effect.remaining=effect.remaining>elapsed?effect.remaining-elapsed:0;redraw();
  }
  if(deposit_ms) {deposit_ms=deposit_ms>elapsed?deposit_ms-elapsed:0;redraw();}
  if (hit_ms) {
    hit_ms = hit_ms > elapsed ? hit_ms - elapsed : 0;
    redraw();
  }

  if (toast_ms) {
    bool expires = toast_ms <= elapsed;
    toast_ms = expires ? 0 : toast_ms - elapsed;
    if (expires)
      redraw();
  }
  if (title_screen)
    return;
  if (swing) {
    swing = swing > elapsed ? swing - elapsed : 0;
    if(pending_tool.active && swing<=unsigned(art::tool_anim::impact_remaining(action_tool,action_grade))) {
      auto pending=pending_tool;pending_tool.active=false;
      committing_tool=true;action(pending.x,pending.y);committing_tool=false;
    }
    redraw();
  }
  if (page == Page::Fishing) {
    auto phase=fishing.phase;fishing.tick(elapsed);
    if(phase!=FishingSession::Phase::Caught&&phase!=FishingSession::Phase::Escaped)redraw();
    return;
  }
  if (page != Page::Farm)
    return;
  // NPCs share the player's continuous movement clock. Refresh while a
  // visible villager walks; keep the low idle rate when everyone is still.
  for(int id=0;id<NpcCount;++id) {
    auto p=npc_pose(state,id,clock_acc);
    if(unsigned(p.region)==state.region && p.walking && p.x>=camx-24 && p.x<camx+WIDTH+24 && p.y>=camy-32 && p.y<camy+HEIGHT+32) {redraw();break;}
  }
  if(state.overwork>=10) { stop_motion();sleep_now(true);msg("体力透支，醒来时已回到家中","Overworked. You woke up at home");return; }
  if(!action_hold && !held(Key::A))repeat_use=false;
  if ((action_hold || held(Key::A)) && repeat_use && !swing)
    facing_action();
  clock_acc += elapsed;
  if (clock_acc >= 500) {
    clock_acc -= 500;
    ++state.minute;
    if(state.minute==1440)msg("已经午夜，早点休息体力恢复更多","Midnight. Sleep soon to recover energy");
    if(state.minute==1500)msg("凌晨一点，两点就会昏睡","1 AM. You will pass out at 2 AM");
    if (state.minute >= 1560) {
      stop_motion();
      sleep_now(true);
      msg("夜深了，农夫也需要休息", "Time to rest");
      return;
    }
    if (state.minute % 20 == 0)
      save();
    redraw();
  }
  anim_acc += elapsed;
  if (anim_acc >= 160) {
    anim_acc -= 160;
    ++animation;
    redraw();
  }
  bool was_walking = walking;
  walking = false;
  int d = -1;
  for (int i = 0; i < 4; ++i)
    if (held(Key(i)))
      d = i;
  if (stick) {
    auto joystick = layout().joystick();
    int dx = stickx - joystick.center_x(), dy = sticky - joystick.center_y();
    if (std::abs(dx) > 5 || std::abs(dy) > 5)
      d = std::abs(dx) > std::abs(dy) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
  }
  if (d >= 0 && !moving && !swing)
    navigate(d);
  if (route && !moving && following_npc >= 0) {
    auto p = npc_pose(state, following_npc);
    if (unsigned(p.region) != state.region) {
      // Preserve the approach/threshold stages while tracking someone in
      // another region. Replanning on every tick would restart the doorway.
      if(journey_region!=int(p.region))go_to(p.region,p.x/16,p.y/16,true);
      else {journey_x=p.x/16;journey_y=p.y/16;}
    }
    else {
      journey_region = -1;
      route_portal=-1;approaching_portal=false;route_action=true;
      targetx = p.x / 16;
      targety = p.y / 16;
    }
  }
  if (route && !moving && !swing) {
    int tx = state.px / 16, ty = state.py / 16;
    if ((route_action && near(state, targetx, targety)) ||
        (!route_action && tx == targetx && ty == targety)) {
      if(route_portal>=0 && approaching_portal) {
        const auto &p=Portals[route_portal];
        approaching_portal=false;targetx=p.x;targety=p.y;
        dir=portal_heading(p);redraw();return;
      }
      if (route_action && swing) {
        redraw();
        return;
      }
      bool use = route_action;
      route = false;
      dir = std::abs(targetx - tx) > std::abs(targety - ty)
                ? (targetx > tx ? 1 : 3)
                : (targety > ty ? 2 : 0);
      if (use)
        action(targetx, targety);
    } else {
      int nx, ny;
      if (next_step(state, targetx, targety, nx, ny, route_action)) {
        stepx = nx * 16 + 8;
        stepy = ny * 16 + 8;
        dir = nx != tx ? (nx > tx ? 1 : 3) : (ny > ty ? 2 : 0);
        moving = true;
      } else {
        route = false;
        msg("暂时走不到，先清理挡路的杂物",
            "Path blocked. Clear obstacles first");
      }
    }
  }
  if (moving && !swing) {
    move_acc += elapsed * movement_speed(state);
    int amount = move_acc / 1000;
    move_acc %= 1000;
    int before_x = state.px, before_y = state.py;
    int dx = stepx - state.px, dy = stepy - state.py;
    if (dx)
      state.px += std::clamp(dx, -amount, amount);
    if (dy)
      state.py += std::clamp(dy, -amount, amount);
    int traveled =
        std::abs(int(state.px) - before_x) + std::abs(int(state.py) - before_y);
    walk_distance += traveled;
    // Carry distance through a tile boundary instead of losing the final
    // pixels every step, especially when frame delivery is uneven.
    if ((route || d >= 0) && amount > traveled)
      move_acc += unsigned(amount - traveled) * 1000;
    walking = traveled > 0;
    if (state.px == stepx && state.py == stepy) {
      moving = false;
      if (const auto *p=portal_at(state,state.px/16,state.py/16);
          p && crossed_portal(*p,before_x,before_y,state.px,state.py,dir)) {
        action(state.px / 16, state.py / 16);
        return;
      }
      save();
    }
    redraw();
  }
  if (walking != was_walking)
    redraw();
}
void draw() {
  if (title_screen) {
    cover_background(title_art::pixels, title_art::Width, title_art::Height);
    title_overlay();
    return;
  }
  scene();
  if (page != Page::Farm)
    shade();
  begin_ui();
  if (page == Page::Farm)
    hud();
  else {
    modal();
  }
  drag_overlay();
  if (save_locked) {
    panel(4, 39, WIDTH - 8, 21);
    center_text(
        WIDTH / 2, 43,
        tr("Save damaged: original protected", "存档损坏：原文件已保护"));
  }
}
} // namespace game
