// SPDX-License-Identifier: GPL-3.0-only
#include "../common/milky-frog/media.hpp"
#include "core.hpp"
#include "audio.hpp"
#include "game_input.hpp"
#include "render.hpp"
#include "ui_layout.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
using namespace md;
namespace {
State s;
enum View {
  Title,
  World,
  Sectors,
  Research,
  Guide,
  Paused,
  Options,
  About,
  NewConfirm,
  Units
};
View view = Title, returnView = Title;

bool hasSave = false, drag = false, down = false, erase = false;
int category = 0, direction = 0, selected = Empty, previewX = -1, previewY = -1,
    inspect = -1, guidePage = 0, sector = 0, researchPage = 0, lineX = -1,
    lineY = -1;
bool rallyPick = false;
bool buildOpen = false, toolsOpen = false, stockOpen = false, mapOpen = false;
md_ui::Layout controls() {
  return md_ui::layout(milky::bounds(), milky::menu_button());
}
void closePanels() { buildOpen = toolsOpen = stockOpen = false; }
int touchX = 0, touchY = 0, lastX = 0, lastY = 0, downX = 0, downY = 0,
    hoverX = -1, hoverY = -1;
float cameraX = 24.5f, cameraY = 35;
constexpr int MinZoom = 8, MaxZoom = 40;
int zoom = 16;
uint32_t pressTime = 0, zoomTime = 0;
uint32_t accum = 0, paint = 0, autoSave = 0, toastTime = 0, heldTime = 0;
char toast[128]{};
constexpr auto rgb = pocket::rgb;
constexpr uint16_t BG = rgb(23, 24, 28), Panel = rgb(37, 39, 45),
                   Border = rgb(74, 76, 84), Ink = rgb(230, 232, 235),
                   Muted = rgb(156, 160, 170), Gold = rgb(255, 211, 127),
                   Red = rgb(242, 95, 91), Green = rgb(142, 214, 130);
const char *tr(const char *en, const char *cn) { return pocket::tr(en, cn); }
int sw() { return pocket::WIDTH; }
int sh() { return pocket::HEIGHT; }
void label(int x, int y, int w, const char *txt, uint16_t c = Ink,
           int font = 12, int align = 0) {
  gfx::label(x, y, w, txt, c, font, align);
}
void center(int x, int y, int w, const char *txt, uint16_t c = Ink,
            int font = 12) {
  label(x, y, w, txt, c, font, 1);
}
void panel(int x, int y, int w, int h) {
  pocket::rect(x, y, w, h, Panel);
  pocket::outline(x, y, w, h, Border);
}
void button(int x, int y, int w, int h, const char *txt, bool active = false) {
  panel(x, y, w, h);
  if (active)
    pocket::rect(x, y + h - 2, w, 2, Gold);
  int font = h >= 40 ? 16 : 12;
  center(x + 2, y + (h - font) / 2, w - 4, txt, active ? Gold : Ink, font);
}
void iconButton(int x, int y, int w, int h, const art::Sprite &image,
                bool active = false) {
  panel(x, y, w, h);
  if (active)
    pocket::rect(x, y + h - 2, w, 2, Gold);
  int side = std::min(16, std::min(w - 4, h - 4));
  gfx::icon(image, x + (w - side) / 2, y + (h - side) / 2, side, side,
            active ? Gold : Ink);
}
bool inside(int x, int y, int xx, int yy, int w, int h) {
  return x >= xx && y >= yy && x < xx + w && y < yy + h;
}
void notify(const char *txt) {
  std::snprintf(toast, sizeof(toast), "%s", txt);
  toastTime = 2200;
}
void sound(unsigned id) {
  if (!s.muted)
    md_audio::effect(Sound(id), id == 2 ? 1 : 0);
}
const art::Sprite &blockArt(int b, int rotation = 0) {
  switch (b) {
  case Core:
    return art::core;
  case Drill:
    return art::drill_frame0;
  case Conveyor:
    return *art::belt_frames[0][0];
  case Router:
    return art::router;
  case Junction:
    return art::junction;
  case Duo:
    return art::duo;
  case Wall:
    return art::wall;
  case Press:
    return art::press;
  case Smelter:
    return art::smelter;
  case Generator:
    return art::generator;
  case Node:
    return art::node;
  case Battery:
    return art::battery;
  case Mender:
    return art::mender;
  case Arc:
    return art::arc;
  case Pneumatic:
    return art::pneumatic_frame0;
  case Kiln:
    return art::kiln;
  case Pump:
    return art::pump;
  case Conduit:
    return art::conduit;
  case LiquidRouter:
    return art::liquidrouter;
  case Tank:
    return art::tank;
  case GroundFactory:
    return *art::groundfactory_frames[rotation & 3];
  case AirFactory:
    return *art::airfactory_frames[rotation & 3];
  default:
    return art::wall;
  }
}
const art::Sprite &itemArt(int item) {
  switch (item) {
  case Copper:
    return art::itemcopper;
  case Lead:
    return art::itemlead;
  case Coal:
    return art::itemcoal;
  case Sand:
    return art::itemsand;
  case Graphite:
    return art::itemgraphite;
  case Metaglass:
    return art::itemglass;
  case Titanium:
    return art::itemtitanium;
  default:
    return art::itemsilicon;
  }
}
const char *itemName(int item) {
  const char *en[] = {"Copper",   "Lead",    "Coal",      "Sand",
                      "Graphite", "Silicon", "Metaglass", "Titanium"};
  const char *cn[] = {"铜", "铅", "煤", "沙", "石墨", "硅", "玻璃", "钛"};
  return tr(en[item], cn[item]);
}
const char *blockName(int b) { return tr(defs[b].en, defs[b].cn); }
milky::Rect unitRect(const art::UnitSprite &unit, int x, int y) {
  int w = unit.sourceWidth * zoom / 32, h = unit.sourceHeight * zoom / 32;
  return {x - w / 2, y - h / 2, w, h};
}
void drawUnit(const art::UnitSprite &unit, int x, int y, int rotation = 0) {
  auto r = unitRect(unit, x, y);
  gfx::sprite(unit.body, r.x, r.y, r.w, r.h, rotation);
}
int originX() { return sw() / 2 - int(cameraX * zoom); }
int originY() { return controls().centerY - int(cameraY * zoom); }
void tileAt(int x, int y, int &tx, int &ty) {
  tx = int(std::floor(float(x - originX()) / zoom));
  ty = int(std::floor(float(y - originY()) / zoom));
}
void resetUI() {
  game::cancel_touches();
  zoom = 16;
  zoomTime = 0;
  closePanels();
  mapOpen = false;
  selected = Empty;
  lineX = lineY = -1;
  rallyPick = false;
  previewX = previewY = -1;
  inspect = -1;
  cameraX = 24.5f;
  cameraY = 35;
  erase = false;
}
View parentView() {
  if (view == Paused)
    return World;
  if (view == About)
    return Options;
  if (view == Options || view == Sectors || view == Title)
    return Title;
  if (view == NewConfirm)
    return sector == 3 ? Title : Sectors;
  return returnView;
}
void newGame(bool sandbox) {
  auto research = s.researched;
  auto captured = s.captured;
  bool mute = s.muted;
  auto music = s.musicMode;
  s.generate(pocket::random(), uint8_t(sector > 2 ? 0 : sector), sandbox);
  s.researched = research;
  s.captured = captured;
  s.muted = mute;
  s.musicMode = music;
  hasSave = true;
  resetUI();
  view = World;
  pocket::save();
  pocket::log(sandbox ? "Mindustry: new sandbox" : "Mindustry: new survival");
}
int plannedCount() {
  if (previewX < 0 || lineX < 0)
    return 1;
  return std::min(W,
                  std::abs(previewX - lineX) + std::abs(previewY - lineY) + 1);
}
void rangeDraw(int type, float tileX, float tileY, uint16_t color) {
  float radius = blockRange(Block(type)) * zoom;
  if (radius <= 0)
    return;
  float cx = originX() + tileX * zoom, cy = originY() + tileY * zoom;
  constexpr int segments = 48;
  int lastX = int(std::lround(cx + radius)), lastY = int(std::lround(cy));
  for (int i = 1; i <= segments; ++i) {
    float angle = i * 6.28318530718f / segments;
    int x = int(std::lround(cx + radius * std::cos(angle))),
        y = int(std::lround(cy + radius * std::sin(angle)));
    pocket::line(lastX, lastY, x, y, color);
    lastX = x;
    lastY = y;
  }
}
void worldDraw(bool title = false) {
  int ox = originX(), oy = originY();
  pocket::clear(BG);
  gfx::terrain(s, ox, oy, zoom, title);
  if (!title) {
    if (selected && previewX >= 0)
      rangeDraw(selected, previewX + defs[selected].size * .5f,
                previewY + defs[selected].size * .5f,
                s.canBuild(Block(selected), previewX, previewY) ? Green : Red);
    else if (inspect >= 0 && s.buildings[inspect].type) {
      auto &b = s.buildings[inspect];
      rangeDraw(b.type, b.x + defs[b.type].size * .5f,
                b.y + defs[b.type].size * .5f, Gold);
    }
  }
  if (!title && inspect >= 0 && s.buildings[inspect].type == Node) {
    auto &node = s.buildings[inspect];
    for (int j = 0; j < MaxBuildings; ++j) {
      auto &b = s.buildings[j];
      if (j == inspect || !b.type)
        continue;
      if (powerConnects(node, b))
        pocket::line(ox + int((node.x + .5f) * zoom),
                     oy + int((node.y + .5f) * zoom),
                     ox + int((b.x + defs[b.type].size * .5f) * zoom),
                     oy + int((b.y + defs[b.type].size * .5f) * zoom),
                     b.powered ? Gold : Muted);
    }
  }
  // All shadows precede all building bodies. Drawing a shadow immediately
  // before each body lets later neighbours cut across already drawn belts.
  for (const auto &b : s.buildings) if (b.type && !b.buildState) {
    int size = defs[b.type].size * zoom, px = ox + b.x * zoom,
        py = oy + b.y * zoom;
    if (px < sw() && py < sh() && px + size > 0 && py + size > 0)
      pocket::rect(px + 2, py + 3, size, size, rgb(28, 28, 33));
  }
  for (int i = 0; i < MaxBuildings; ++i) {
    auto &b = s.buildings[i];
    if (!b.type)
      continue;
    int size = defs[b.type].size * zoom, px = ox + b.x * zoom,
        py = oy + b.y * zoom;
    if (px > sw() || py > sh() || px + size < 0 || py + size < 0)
      continue;
    if (b.buildState) {
      uint16_t color = b.buildState == 3 ? Red : Gold;
      if (b.type == Conveyor) {
        auto t = s.conveyorTiling(b.x, b.y, b.dir, true);
        gfx::sprite(*art::belt_frames[t.shape][0], px, py, size, size,
                    b.dir, 110, 0, t.flip);
      } else gfx::sprite(blockArt(b.type, b.dir), px, py, size, size,
                          b.type == Conduit ? b.dir : 0, 110);
      pocket::outline(px, py, size, size, color);
      if (b.buildState != 1) {
        pocket::rect(px, py + size - 3, size, 3, Border);
        pocket::rect(px, py + size - 3, size * b.progress / BuildComplete, 3,
                     color);
      }
      if (s.builder == i) {
        int ax = ox + int(s.playerX * zoom), ay = oy + int(s.playerY * zoom);
        pocket::line(ax, ay, px, py, color);
        pocket::line(ax, ay, px + size, py + size, color);
      }
      continue;
    }
    auto tiling = b.type == Conveyor ? s.conveyorTiling(b.x, b.y, b.dir)
                                     : ConveyorTiling{};
    bool drill = b.type == Drill || b.type == Pneumatic;
    const art::Sprite &image =
        b.type == Conveyor
            ? *art::belt_frames[tiling.shape][b.conveyorClog > 10 ? 0 : (s.ticks * 18 / 25) % 4]
        : drill
            ? *(b.type == Drill
                    ? art::drill_frames
                    : art::pneumatic_frames)[int(b.drillRotation * 8 / 90) % 8]
            : blockArt(b.type, b.dir);
    gfx::sprite(image, px, py, size, size,
                (b.type == Conveyor || b.type == Conduit) ? b.dir : 0,
                title ? 130 : 255, 0, tiling.flip);
    if (b.water && (zoom >= 12 || i == inspect)) {
      int cap = liquidCapacity(Block(b.type));
      pocket::rect(px, py + size - 3, size, 3, Border);
      pocket::rect(px, py + size - 3, size * b.water / cap, 3,
                   rgb(90, 163, 230));
    }
    if (auto duration = processTime(Block(b.type));
        duration && b.progress && (i == inspect || (zoom >= 12 && !selected))) {
      pocket::rect(px, py + size - 2, size * b.progress / duration, 2, Green);
    }
    if (drill && b.ore.count) {
      constexpr uint16_t colors[Items] = {
          rgb(217, 157, 115), rgb(140, 127, 169), rgb(39, 39, 39),
          rgb(247, 203, 164), rgb(178, 198, 210), rgb(83, 86, 92),
          rgb(235, 238, 245), rgb(141, 161, 227)};
      gfx::icon(art::drillitem, px, py, size, size, colors[b.ore.item]);
    }
    if (!title && b.hp < defs[b.type].hp) {
      pocket::rect(px, py + size - 2, size, 2, Red);
      pocket::rect(px, py + size - 2, size * b.hp / defs[b.type].hp, 2, Green);
    }
    if (!title &&
        (b.type == Smelter || b.type == Arc || b.type == Mender ||
         b.type == Kiln || b.type == GroundFactory || b.type == AirFactory) &&
        powerDemand(b) && !b.powered)
      center(px, py, size, "!", Gold, 10);
  }
  // Items share a layer above every conveyor body, like the original belt
  // renderer, rather than being clipped by a later neighbouring building.
  for (const auto &b : s.buildings)
    if (!b.buildState && (b.type == Conveyor || b.type == Router))
      for (int j = 0; j < Items; ++j) if (b.inv[j])
        gfx::sprite(itemArt(j), ox + b.x * zoom + zoom / 4,
                    oy + b.y * zoom + zoom / 4, zoom / 2, zoom / 2);
  for (auto &e : s.enemies)
    if (e.hp) {
      int px = ox + int(e.x * zoom), py = oy + int(e.y * zoom);
      drawUnit(e.kind ? art::crawler_unit : art::dagger_unit, px, py, 2);
      auto r = unitRect(e.kind ? art::crawler_unit : art::dagger_unit, px, py);
      pocket::rect(px - zoom / 2, r.y - 3, zoom, 2, Red);
    }
  for (auto &b : s.shots)
    if (b.life) {
      auto c = b.hostile ? Red : Gold;
      pocket::line(ox + int(b.x * zoom), oy + int(b.y * zoom),
                   ox + int(b.tx * zoom), oy + int(b.ty * zoom), c);
    }
  for (auto &u : s.units)
    if (u.hp) {
      int px = ox + int(u.x * zoom), py = oy + int(u.y * zoom);
      drawUnit(u.kind ? art::flare_unit : art::dagger_unit, px, py);
      auto r = unitRect(u.kind ? art::flare_unit : art::dagger_unit, px, py);
      pocket::rect(px - zoom / 2, r.y - 3, zoom, 2, Green);
    }
  if (!title && s.unitCount()) {
    int px = ox + int(s.rallyX * zoom), py = oy + int(s.rallyY * zoom);
    pocket::line(px - 4, py, px + 4, py, Green);
    pocket::line(px, py - 4, px, py + 4, Green);
  }
  if (!title)
    drawUnit(art::alpha_unit, ox + int(s.playerX * zoom),
             oy + int(s.playerY * zoom));
  if (!title && s.mining) {
    pocket::line(ox + int(s.playerX * zoom), oy + int(s.playerY * zoom),
                 ox + int((s.mineX + .5f) * zoom),
                 oy + int((s.mineY + .5f) * zoom), Gold);
    pocket::outline(ox + s.mineX * zoom, oy + s.mineY * zoom, zoom, zoom, Gold);
  }
  if (!title && s.carriedCount) {
    int px = ox + int(s.playerX * zoom), py = oy + int(s.playerY * zoom);
    int side = std::clamp(zoom / 2, 8, 14);
    gfx::sprite(itemArt(s.carriedItem), px + zoom / 2, py, side, side);
    char amount[20];
    std::snprintf(amount, sizeof(amount), "%u/%u", s.carriedCount,
                  CarryCapacity);
    center(px - 25, py + zoom, 50, amount, Gold, 12);
  }
}
void minimap() {
  auto r = controls().map;
  int scale = r.w / W;
  panel(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
  gfx::minimap(s, r.x, r.y, scale);
  for (auto &e : s.enemies)
    if (e.hp)
      pocket::rect(r.x + int(e.x) * scale, r.y + int(e.y) * scale, 2 * scale,
                   2 * scale, Red);
  pocket::outline(
      r.x + std::clamp(int(cameraX) - sw() / zoom / 2, 0, W - 1) * scale,
      r.y + std::clamp(int(cameraY) - sh() / zoom / 2, 0, H - 1) * scale,
      std::min(W, sw() / zoom) * scale, std::min(H, sh() / zoom) * scale,
      Muted);
}
int menuTop() { return controls().pause.bottom() + 8; }
int menuFooter() { return milky::bounds().bottom() - controls().button - 4; }
milky::Rect menuRow(int index) {
  auto b = milky::bounds();
  int n = controls().button;
  int w = std::min(320, b.w - 8);
  return {b.x + (b.w - w) / 2, menuTop() + index * (n + 7), w, n};
}
void header(const char *name) {
  auto r = controls().pause;
  iconButton(r.x, r.y, r.w, r.h, art::icon_undo);
  label(r.right() + 8, r.y + (r.h - 16) / 2,
        milky::menu_button().x - r.right() - 12, name, Gold, 16);
}
void button(milky::Rect r, const char *name, bool active = false) {
  button(r.x, r.y, r.w, r.h, name, active);
}
void iconButton(milky::Rect r, const art::Sprite &image, bool active = false) {
  iconButton(r.x, r.y, r.w, r.h, image, active);
}
void inventoryDraw() {
  auto r = controls().inventory;
  panel(r.x, r.y, r.w, r.h);
  for (int i = 0; i < Items; ++i) {
    int x = r.x + 6 + (i % 4) * (r.w - 12) / 4;
    int y = r.y + 8 + (i / 4) * 32;
    gfx::sprite(itemArt(i), x, y, 15, 15);
    char buf[20];
    std::snprintf(buf, sizeof(buf), "%u", s.stock[i]);
    label(x + 18, y + 1, (r.w - 12) / 4 - 20, buf, Ink, 12);
  }
  char buf[80];
  std::snprintf(
      buf, sizeof(buf),
      tr("Power %u / %u  ·  Units %d/%d", "电力 %u / %u · 部队 %d/%d"),
      s.powerProduced, s.powerUsed, s.unitCount(), MaxUnits);
  label(r.x + 7, r.bottom() - 22, r.w - 14, buf, Gold, 12);
}
void resources() {
  auto l = controls();
  iconButton(l.pause, art::icon_pause);
  char buf[64];
  if (s.alive())
    std::snprintf(buf, sizeof(buf), tr("%u · %d enemies", "波 %u · 敌 %d"),
                  s.wave, s.alive());
  else
    std::snprintf(buf, sizeof(buf), tr("%u · %us >", "波 %u · %us >"), s.wave,
                  (s.countdown + 19) / 20);
  button(l.wave, buf, s.alive() != 0);
  int core = s.core();
  if (core >= 0)
    pocket::rect(l.wave.x + 2, l.wave.bottom() - 3,
                 (l.wave.w - 4) * s.buildings[core].hp / defs[Core].hp, 2,
                 Gold);
  panel(l.stock.x, l.stock.y, l.stock.w, l.stock.h);
  int count = std::clamp((l.stock.w - 10) / 48, 1, 3);
  int cell = (l.stock.w - 8) / count;
  for (int i = 0; i < count; ++i) {
    int x = l.stock.x + 4 + i * cell, y = l.stock.y + (l.stock.h - 14) / 2;
    gfx::sprite(itemArt(i), x, y, 12, 12);
    std::snprintf(buf, sizeof(buf), s.stock[i] >= 1000 ? "%.1fk" : "%.0f",
                  s.stock[i] >= 1000 ? s.stock[i] / 1000.f : float(s.stock[i]));
    label(x + 13, y, cell - 13, buf, Ink, 12);
  }
}
int buildList(int *out) {
  int n = 0;
  for (int b = Drill; b < Blocks; ++b)
    if (defs[b].category == category)
      out[n++] = b;
  return n;
}
void buildPanel() {
  auto l = controls();
  panel(l.drawer.x, l.drawer.y, l.drawer.w, l.drawer.h);
  const art::Sprite *cats[] = {&art::icon_production, &art::icon_distribution,
                               &art::icon_turret,     &art::icon_crafting,
                               &art::icon_power,      &art::icon_liquid,
                               &art::icon_units};
  for (int i = 0; i < 7; ++i)
    iconButton(l.tabs[i], *cats[i], i == category);
  int entries[Blocks], count = buildList(entries);
  for (int i = 0; i < count; ++i) {
    int b = entries[i];
    auto r = md_ui::item(l, i, count);
    bool available = s.unlocked(Block(b));
    if (b == selected)
      pocket::rect(r.x, r.y, r.w, r.h, rgb(69, 62, 45));
    int size = std::min(28, r.h - 20);
    gfx::sprite(blockArt(b), r.x + (r.w - size) / 2, r.y + 3, size, size, 0,
                available ? 255 : 80);
    center(r.x + 1, r.bottom() - 17, r.w - 2,
           available ? blockName(b) : tr("Locked", "未解锁"),
           available ? Ink : Muted, 12);
  }
}
void toolPanel() {
  auto l = controls();
  panel(l.tools.x, l.tools.y, l.tools.w, l.tools.h);
  const char *names[] = {tr("Research", "科技研究"),
                         tr("Units", "部队指挥"),
                         tr("Zoom +", "放大 +"),
                         tr("Zoom -", "缩小 -"),
                         tr("Dismantle", "拆除模式"),
                         s.muted ? tr("Sound: off", "音效：关")
                                 : tr("Sound: on", "音效：开")};
  for (int i = 0; i < 6; ++i)
    button(l.tool[i], names[i], i == 4 && erase);
}
void hud() {
  auto l = controls();
  resources();
  if (mapOpen)
    minimap();
  else if (selected || inspect >= 0)
    button(l.mapToggle, tr("Map", "地图"));
  char buf[180];
  if (buildOpen)
    buildPanel();
  if (toolsOpen)
    toolPanel();
  if (stockOpen)
    inventoryDraw();
  if (selected && !buildOpen) {
    button(l.dock[0], blockName(selected), true);
    iconButton(l.dock[1], art::icon_rotate);
    iconButton(l.dock[2], art::icon_check, previewX >= 0);
    iconButton(l.dock[3], art::icon_cancel);
    int count = plannedCount();
    size_t used = 0;
    buf[0] = 0;
    if (count > 1)
      used = size_t(std::snprintf(buf, sizeof(buf), "%dx  ", count));
    bool affordable = true;
    for (int i = 0; i < Items; ++i) {
      unsigned cost = defs[selected].cost[i] * count;
      if (!cost)
        continue;
      if (!s.sandbox && s.stock[i] < cost)
        affordable = false;
      int n = std::snprintf(buf + used, sizeof(buf) - used, "%s%u ",
                            itemName(i), cost);
      if (n < 0 || size_t(n) >= sizeof(buf) - used)
        break;
      used += size_t(n);
    }
    panel(l.info.x, l.info.y, l.info.w, l.info.h);
    label(l.info.x + 4, l.info.y + 4, l.info.w - 8, buf,
          affordable ? Muted : Red, 12);
  } else if (inspect >= 0 && s.buildings[inspect].type && !buildOpen) {
    auto &b = s.buildings[inspect];
    button(l.dock[0], tr("Build", "建造"));
    button(l.dock[1], b.buildState     ? tr("Alpha", "机器人")
                      : b.type == Core ? tr("Deposit", "存入")
                      : s.carriedCount ? tr("Deliver", "交付")
                                       : tr("Supply", "供料"));
    button(l.dock[2],
           b.buildState ? tr("Cancel plan", "取消计划") : tr("Remove", "拆除"));
    iconButton(l.dock[3], art::icon_cancel);
    int total = 0;
    for (auto v : b.inv)
      total += v;
    if (b.buildState) {
      bool missing = false;
      for (int j = 0; j < Items; ++j)
        missing |= !s.sandbox && b.inv[j] < defs[b.type].cost[j] && !s.stock[j];
      bool far =
          !s.sandbox &&
          std::hypot(s.playerX - b.x - defs[b.type].size * .5f,
                     s.playerY - b.y - defs[b.type].size * .5f) > BuildRange;
      const char *status = b.buildState == 3      ? tr("Removing", "拆除中")
                           : s.builder == inspect ? tr("Building", "建造中")
                           : far     ? tr("Out of range", "超出范围")
                           : missing ? tr("Needs materials", "缺少材料")
                                     : tr("Queued", "排队中");
      std::snprintf(buf, sizeof(buf),
                    tr("%s · %s %u%% · Plans %u", "%s · %s %u%% · 队列 %u"),
                    blockName(b.type), status,
                    b.progress * 100u / BuildComplete, s.buildCount);
    } else if (b.type == Drill || b.type == Pneumatic) {
      std::snprintf(buf, sizeof(buf),
                    tr("%s %.2f/s · Water %.1f", "%s %.2f/秒 · 水 %.1f"),
                    b.ore.count ? itemName(b.ore.item) : "-", b.drillRate,
                    b.water / 10.f);
    } else if (liquidCapacity(Block(b.type)))
      std::snprintf(buf, sizeof(buf),
                    tr("%s · HP %u · Water %.1f", "%s · HP %u · 水 %.1f"),
                    blockName(b.type), b.hp, b.water / 10.f);
    else if (auto duration = processTime(Block(b.type))) {
      const auto *r = recipe(Block(b.type));
      const char *status =
          r && b.inv[r->output] >= 10 ? tr("Output full", "产物已满")
          : !canProcess(b)            ? tr("Needs inputs", "缺少原料")
          : b.progress == duration    ? tr("Waiting for exit", "等待出兵")
          : b.type != Press && !b.powerEfficiency ? tr("No power", "缺少电力")
          : b.type != Press && b.powerEfficiency < FullPower
              ? tr("Low power", "供电不足")
              : tr("Working", "生产中");
      if (b.type == Press)
        std::snprintf(buf, sizeof(buf), "%s %u%% · %s", blockName(b.type),
                      b.progress * 100u / duration, status);
      else
        std::snprintf(buf, sizeof(buf),
                      tr("%s %u%% · Power %u%% · %s", "%s %u%% · 电 %u%% · %s"),
                      blockName(b.type), b.progress * 100u / duration,
                      b.powerEfficiency * 100u / FullPower, status);
    } else
      std::snprintf(buf, sizeof(buf), "%s · HP %u · %d", blockName(b.type),
                    b.hp, total);
    panel(l.info.x, l.info.y, l.info.w, l.info.h);
    label(l.info.x + 4, l.info.y + 4, l.info.w - 8, buf, Ink, 12);
  } else {
    button(l.dock[0], buildOpen ? tr("Close", "收起") : tr("Build", "建造"),
           buildOpen);
    button(l.dock[1], tr("Core", "核心"));
    button(l.dock[2], tr("Map", "地图"), mapOpen);
    button(l.dock[3], erase ? tr("Cancel", "取消拆除") : tr("Tools", "工具"),
           toolsOpen || erase);
  }
  if (zoomTime) {
    int x = sw() / 2 - 32, y = l.pause.bottom() + 5;
    panel(x, y, 64, 22);
    std::snprintf(buf, sizeof(buf), "%d%%", zoom * 100 / 16);
    center(x + 2, y + 4, 60, buf, Gold, 12);
  }
  if (rallyPick) {
    panel(l.info.x, l.info.y, l.info.w, l.info.h);
    center(l.info.x + 3, l.info.y + 4, l.info.w - 6,
           tr("Tap a rally point", "点击地图设置集结点"), Green, 12);
  }
  if (previewX >= 0 && selected) {
    int startX = lineX >= 0 ? lineX : previewX,
        startY = lineY >= 0 ? lineY : previewY;
    int dx = (previewX > startX) - (previewX < startX),
        dy = (previewY > startY) - (previewY < startY),
        n = std::abs(previewX - startX) + std::abs(previewY - startY) + 1;
    for (int i = 0; i < std::min(n, W); ++i) {
      int tx = startX + i * dx, ty = startY + i * dy,
          size = defs[selected].size * zoom, x = originX() + tx * zoom,
          y = originY() + ty * zoom;
      bool valid = s.canBuild(Block(selected), tx, ty);
      if (selected == Conveyor) {
        auto t = s.conveyorTiling(tx, ty, direction, true);
        gfx::sprite(*art::belt_frames[t.shape][0], x, y, size, size,
                    direction, 170, 0, t.flip);
      } else gfx::sprite(blockArt(selected, direction), x, y, size, size,
                          selected == Conduit ? direction : 0, 170);
      pocket::outline(x, y, size, size, valid ? Green : Red);
    }
  }
  if (toastTime) {
    int y = (buildOpen ? l.drawer.y : toolsOpen ? l.tools.y : l.info.y) - 26;
    panel(l.info.x, y, l.info.w, 22);
    center(l.info.x + 3, y + 4, l.info.w - 6, toast, Gold, 12);
  }
}
void titleDraw() {
  worldDraw(true);
  int x = std::max(9, sw() / 2 - 106), w = std::min(212, sw() - 18);
  int y = std::max(4, (sh() - 222) / 2);
  panel(x - 7, y, w + 14, 216);
  center(x, y + 13, w, "M I N D U S T R Y", Gold, 21);
  center(x, y + 40, w, tr("PXA EDITION · UNOFFICIAL", "PXA 重制版 · 非官方"),
         Muted, 10);
  const char *buttons[] = {
      hasSave ? tr("Continue", "继续游戏") : tr("Play", "开始游戏"),
      tr("Campaign", "战役"), tr("Sandbox", "沙盒"),
      tr("How to play", "玩法指南"), tr("Settings / About", "设置 / 关于")};
  for (int i = 0; i < 5; ++i)
    button(x, y + 61 + i * 28, w, 24, buttons[i], i == 0);
}
int researchRows() { return std::max(1, (menuFooter() - 4 - menuTop()) / 32); }
int researchColumns() { return milky::bounds().w >= 380 ? 2 : 1; }
int researchPages() {
  int capacity = researchRows() * researchColumns();
  return (Blocks - Press + capacity - 1) / capacity;
}
void researchDraw() {
  header(tr("Research", "科技研究"));
  int cols = researchColumns(), rows = researchRows(), capacity = cols * rows;
  researchPage = std::clamp(researchPage, 0, researchPages() - 1);
  for (int i = 0; i < capacity; ++i) {
    int b = Press + researchPage * capacity + i;
    if (b >= Blocks)
      break;
    auto bounds = milky::bounds();
    int x = bounds.x + 4 + (i % cols) * (bounds.w - 8) / cols,
        y = menuTop() + (i / cols) * 32, w = (bounds.w - 8) / cols - 4;
    panel(x, y, w, 30);
    gfx::sprite(blockArt(b), x + 3, y + 3, 20, 20, 0,
                s.unlocked(Block(b)) ? 255 : 120);
    label(x + 28, y + 3, w - 32, blockName(b),
          s.unlocked(Block(b)) ? Green : Gold, 11);
    char cost[120]{};
    size_t used = 0;
    if (s.unlocked(Block(b)))
      std::snprintf(cost, sizeof(cost), "%s", tr("Unlocked", "已解锁"));
    else {
      for (int j = 0; j < Items; ++j) {
        auto value = researchCost(Block(b), Item(j));
        if (!value)
          continue;
        int n = std::snprintf(cost + used, sizeof(cost) - used, "%s %u ",
                              itemName(j), value);
        if (n < 0 || size_t(n) >= sizeof(cost) - used)
          break;
        used += size_t(n);
      }
    }
    label(x + 28, y + 17, w - 32, cost, Muted, 9);
  }
  auto b = milky::bounds();
  int footer = menuFooter(), n = controls().button;
  button(b.x + 4, footer, 55, n, "<");
  char page[30];
  std::snprintf(page, sizeof(page), "%d / %d", researchPage + 1,
                researchPages());
  center(b.x + 64, footer + (n - 12) / 2, b.w - 128, page, Muted);
  button(b.right() - 59, footer, 55, n, ">");
}
void unitMenu() {
  header(tr("Unit command", "部队指挥"));
  auto b = milky::bounds();
  int n = controls().button, top = menuTop();
  char buf[80];
  std::snprintf(buf, sizeof(buf), tr("Units %d / %d", "部队 %d / %d"),
                s.unitCount(), MaxUnits);
  center(b.x + 4, top, b.w - 8, buf, Gold, 16);
  button(b.x + 4, top + 20, b.w - 8, n, tr("Defend core", "保卫核心"),
         s.command == Command::Defend);
  button(b.x + 4, top + 20 + n + 8, b.w - 8, n,
         tr("Set rally point", "设置集结点"), s.command == Command::Rally);
  button(b.x + 4, top + 20 + 2 * (n + 8), b.w - 8, n,
         tr("Attack enemies", "进攻敌人"), s.command == Command::Attack);
  label(
      b.x + 4, b.bottom() - 34, b.w - 8,
      tr("Factories need power and silicon.", "工厂需要供电和硅；陆军另需铅。"),
      Muted, 12);
}
void guideDraw() {
  header(tr("Field guide", "作战指南"));
  const char *cn[][5] = {
      {"01  从矿脉开始", "核心旁的钻头已接通传送带。",
       "在铜、铅、煤矿上放置机械钻头。", "方向箭头决定传送带输出方向。",
       "物品进入核心后才成为建造资源。"},
      {"02  生产与能源", "压缩机：2 煤 → 1 石墨。",
       "冶炼厂：1 煤 + 2 沙 → 1 硅。", "火力发电机需要持续输入煤。",
       "用电力节点连接发电与耗电建筑。"},
      {"03  保卫核心", "双管炮需要铜或石墨弹药。",
       "传送带供料；也可选中后点供料。", "铜墙阻挡敌人，修复器需要供电。",
       "守住 10 / 15 / 20 波即可占领区域。"},
      {"04  触屏与手柄", "平移视角，机器人会自动跟随。",
       "长按建筑选同类，再选址确认。", "传送带、导管、铜墙可拖动铺线。",
       "工具面板：科技、部队、缩放、拆除。"},
      {"05  玻璃与供水", "窑炉：铅 + 沙 → 玻璃，需要供电。",
       "水面放机械泵，用导管接到钻头。", "水量会守恒，导管方向决定输出。",
       "选导管拖动，可一次铺设整条直线。"},
      {"06  制造与指挥", "陆军：10 铅 + 10 硅 → 尖刀。",
       "空军：15 硅 → 星辉；每批 15 秒。", "每座工厂耗电 72；单发电机不足。",
       "部队面板：守卫、集结、进攻。"}};
  const char *en[][5] = {
      {"01  Start with ore", "Your first drill already feeds the core.",
       "Place drills over copper, lead or coal.",
       "Conveyor arrows set the output direction.",
       "Deliver items to the core to build."},
      {"02  Production and power", "Press: 2 coal -> 1 graphite.",
       "Smelter: 1 coal + 2 sand -> 1 silicon.",
       "Feed coal to the combustion generator.",
       "Link power consumers with power nodes."},
      {"03  Defend the core", "Duo turrets consume copper or graphite.",
       "Use conveyors, or select and Supply.",
       "Walls block enemies. Menders use power.",
       "Survive 10 / 15 / 20 waves to capture."},
      {"04  Touch and controller", "Pan the view; Alpha follows its center.",
       "Hold a block to select the same type.",
       "Drag conveyors, pipes or walls in a line.",
       "Tools: tech, units, zoom and remove."},
      {"05  Glass and water", "Kiln: lead + sand -> glass; needs power.",
       "Put pumps on water; pipe it to drills.",
       "Water is conserved. Pipes are directed.",
       "Drag a conduit preview to place a line."},
      {"06  Build and command", "Dagger: 10 lead + 10 silicon.",
       "Flare: 15 silicon. A batch takes 15 sec.",
       "Each factory uses 72 power per second.",
       "Units menu: defend, rally or attack."}};
  for (int i = 0; i < 5; ++i)
    label(milky::bounds().x + 4, menuTop() + i * 25, milky::bounds().w - 8,
          pocket::english() ? en[guidePage][i] : cn[guidePage][i],
          i ? Ink : Gold, i ? 12 : 16);
  auto b = milky::bounds();
  int n = controls().button, footer = menuFooter();
  button(b.x + 4, footer, 64, n, "<");
  char page[16];
  std::snprintf(page, sizeof(page), "%d / 6", guidePage + 1);
  center(b.x + 72, footer + (n - 12) / 2, b.w - 144, page, Muted);
  button(b.right() - 68, footer, 64, n, ">");
}
void menus() {
  if (view == Units) {
    unitMenu();
    return;
  }
  if (view == Research) {
    researchDraw();
    return;
  }
  if (view == Guide) {
    guideDraw();
    return;
  }
  if (view == Sectors) {
    header(tr("Serpulo · Deploy", "塞普罗 · 部署"));
    const char *cn[] = {"零号地区", "冰冻森林", "陨石带"},
               *en[] = {"Ground Zero", "Frozen Forest", "The Craters"};
    for (int i = 0; i < 3; ++i) {
      auto b = milky::bounds();
      int y = menuTop() + i * 47;
      panel(b.x + 4, y, b.w - 8, 40);
      if (i == sector)
        pocket::rect(b.x + 4, y + 38, b.w - 8, 2, Gold);
      center(b.x + 8, y + 4, b.w - 16, pocket::english() ? en[i] : cn[i],
             i == sector ? Gold : Ink, 16);
      char buf[40];
      std::snprintf(buf, sizeof(buf), tr("Survive %d waves", "守住 %d 波"),
                    10 + i * 5);
      if (s.captured & (1u << i))
        std::snprintf(buf, sizeof(buf), "%s", tr("Captured", "已占领"));
      center(b.x + 8, y + 25, b.w - 16, buf, Muted, 12);
    }
    return;
  }
  header(view == Paused    ? tr("Paused", "已暂停")
         : view == Options ? tr("Settings", "设置")
         : view == About   ? tr("About", "关于")
                           : tr("New game", "新游戏"));
  if (view == Paused) {
    const char *rows[] = {tr("Resume", "继续游戏"), tr("Research", "科技研究"),
                          tr("How to play", "玩法指南"),
                          tr("Save and menu", "保存并回主菜单")};
    for (int i = 0; i < 4; ++i)
      button(menuRow(i), rows[i], i == 0);
  }
  if (view == Options) {
    auto b = milky::bounds();
    int n = controls().button, top = menuTop();
    button(b.x + 4, top, b.w - 8, n,
           pocket::english() ? "Language: English" : "语言：简体中文");
    button(b.x + 4, top + n + 12, b.w - 8, n,
           s.muted ? tr("Sound: OFF", "音效：关")
                   : tr("Sound: ON", "音效：开"));
    button(b.x + 4, top + 2 * (n + 12), b.w - 8, n,
           s.musicMode == 2 ? tr("Music: OFF", "音乐：关") :
           s.musicMode == 1 ? tr("Music: continuous", "音乐：连续") :
                             tr("Music: original", "音乐：原版"));
    button(b.x + 4, top + 3 * (n + 12), b.w - 8, n,
           tr("About / Licenses", "关于 / 许可"));
  }
  if (view == About) {
    auto b = milky::bounds();
    center(b.x + 4, menuTop() + 4, b.w - 8, "Mindustry · PXA 0.11", Gold, 18);
    const char *lines[] = {
        tr("Original game and artwork: Anuken & contributors",
           "原作与素材：Anuken 及贡献者"),
        tr("Independent C++ remake. GPL-3.0.", "独立 C++ 重制 · GPL-3.0"),
        tr("Factory, water and unit subset of v154.",
           "工厂、水路与基础单位玩法"),
        tr("No Java, multiplayer or full tech tree yet.",
           "尚未实现联机、完整科技树及全内容"),
        "github.com/Anuken/Mindustry"};
    for (int i = 0; i < 5; ++i)
      center(b.x + 4, menuTop() + 36 + i * 23, b.w - 8, lines[i], Muted, 12);
  }
  if (view == NewConfirm) {
    center(
        milky::bounds().x + 4, menuTop() + 20, milky::bounds().w - 8,
        tr("Replace the current saved game?", "将覆盖当前存档，开始新战役？"),
        Gold);
    auto b = milky::bounds();
    int n = controls().button;
    button(b.x + 4, menuTop() + 70, b.w / 2 - 8, n, tr("Cancel", "取消"));
    button(b.x + b.w / 2 + 4, menuTop() + 70, b.w / 2 - 8, n,
           tr("Start", "开始"));
  }
}
void buildNow() {
  if (previewX < 0)
    return;
  int i = lineX >= 0
              ? s.queueLine(Block(selected), lineX, lineY, previewX, previewY,
                            direction)
              : s.queueBuild(Block(selected), previewX, previewY, direction);
  if (i >= 0) {
    lineX = lineY = -1;
    previewX = previewY = -1;
  }
  if (i < 0)
    notify(tr("Blocked, insufficient resources, or building limit",
              "无法建造：地形、资源不足或建筑上限"));
  else {
    sound(0);
    pocket::save();
    pocket::log("Mindustry: building queued");
  }
}
void tap(int x, int y) {
  sound(0);
  if (view == Title) {
    int yy = std::max(4, (sh() - 222) / 2);
    int row = (y - yy - 61) / 28;
    if (y < yy + 61 || row > 4)
      return;
    if (row == 0) {
      if (hasSave) {
        view = World;
        resetUI();
        cameraX = s.playerX;
        cameraY = s.playerY;
      } else
        view = Sectors;
    }
    if (row == 1)
      view = Sectors;
    if (row == 2) {
      if (hasSave) {
        returnView = Title;
        sector = 3;
        view = NewConfirm;
      } else
        newGame(true);
    }
    if (row == 3) {
      returnView = Title;
      view = Guide;
    }
    if (row == 4) {
      returnView = Title;
      view = Options;
    }
    return;
  }
  if (view != World) {
    if (controls().pause.contains(x, y)) {
      view = parentView();
      return;
    }
    if (view == Sectors) {
      int i = (y - menuTop()) / 47;
      if (y >= menuTop() && i >= 0 && i < 3) {
        sector = i;
        if (hasSave)
          view = NewConfirm;
        else
          newGame(false);
      }
      return;
    }
    if (view == NewConfirm) {
      if (y >= menuTop() + 70 && y < menuTop() + 70 + controls().button) {
        if (x < sw() / 2)
          view = parentView();
        else
          newGame(sector == 3);
      }
      return;
    }
    if (view == Units) {
      int top = menuTop() + 20, n = controls().button;
      if (y >= top && y < top + n) {
        s.command = Command::Defend;
        s.rallyX = 24.5f;
        s.rallyY = 31.5f;
        view = World;
      } else if (y >= top + n + 8 && y < top + 2 * n + 8) {
        s.command = Command::Rally;
        rallyPick = true;
        view = World;
      } else if (y >= top + 2 * (n + 8) && y < top + 2 * (n + 8) + n) {
        s.command = Command::Attack;
        view = World;
      }
      pocket::save();
      return;
    }
    if (view == Research) {
      if (y >= menuFooter()) {
        researchPage =
            (researchPage + (x < sw() / 2 ? researchPages() - 1 : 1)) %
            researchPages();
        return;
      }
      auto bounds = milky::bounds();
      int cols = researchColumns(), rows = researchRows(),
          row = (y - menuTop()) / 32,
          col = (x - bounds.x - 4) / ((bounds.w - 8) / cols);
      if (y >= menuTop() && row < rows && x >= bounds.x + 4 && col >= 0 &&
          col < cols) {
        int b = Press + researchPage * rows * cols + row * cols + col;
        if (b < Blocks && !s.unlocked(Block(b))) {
          if (s.research(Block(b))) {
            md_audio::effect(Sound::Unlock, 0);
            pocket::save();
            notify(tr("Technology unlocked", "科技已解锁"));
          } else
            notify(tr("Research prerequisites or gather materials",
                      "先解锁前置科技，并补足所需材料"));
        }
      }
      return;
    }
    if (view == Guide) {
      if (y >= menuFooter())
        guidePage = (guidePage + (x < sw() / 2 ? 5 : 1)) % 6;
      return;
    }
    if (view == Paused) {
      int i = (y - menuTop()) / (controls().button + 7);
      if (i >= 0 && i < 4 && menuRow(i).contains(x, y)) {
        if (i == 0)
          view = World;
        if (i == 1) {
          returnView = Paused;
          view = Research;
        }
        if (i == 2) {
          returnView = Paused;
          view = Guide;
        }
        if (i == 3) {
          view = Title;
          pocket::save();
          pocket::log("Mindustry: saved to menu");
        }
      }
      return;
    }
    if (view == Options) {
      int top = menuTop(), n = controls().button;
      if (y >= top && y < top + n) {
        gfx::toggle_language();
        pocket::save();
      }
      if (y >= top + n + 12 && y < top + 2 * n + 12) {
        s.muted = !s.muted;
        pocket::save();
        milky::volume(s.muted ? 0 : 65, 55);
      }
      if (y >= top + 2 * (n + 12) && y < top + 2 * (n + 12) + n) {
        s.musicMode = uint8_t((s.musicMode + 1) % 3);
        pocket::save();
      }
      if (y >= top + 3 * (n + 12) && y < top + 3 * (n + 12) + n) {
        returnView = Options;
        view = About;
      }
      return;
    }
    return;
  }
  if (s.won || s.lost) {
    if (inside(x, y, sw() / 2 - 90, sh() / 2 + 22, 180, 29)) {
      view = Title;
      pocket::save();
    }
    return;
  }
  auto l = controls();
  if (l.pause.contains(x, y)) {
    closePanels();
    view = Paused;
    pocket::log("Mindustry: paused");
    return;
  }
  if (l.wave.contains(x, y)) {
    if (s.startWave()) {
      pocket::save();
      pocket::log("Mindustry: wave started");
    }
    return;
  }
  if (l.stock.contains(x, y)) {
    bool open = !stockOpen;
    closePanels();
    stockOpen = open;
    return;
  }
  if (!mapOpen && (selected || inspect >= 0) && l.mapToggle.contains(x, y)) {
    closePanels();
    mapOpen = true;
    return;
  }
  if (mapOpen && l.map.contains(x, y)) {
    cameraX = std::clamp(float(x - l.map.x) * W / l.map.w, 3.f, 45.f);
    cameraY = std::clamp(float(y - l.map.y) * H / l.map.h, 3.f, 45.f);
    mapOpen = false;
    return;
  }
  if (buildOpen && l.drawer.contains(x, y)) {
    for (int i = 0; i < 7; ++i)
      if (l.tabs[i].contains(x, y)) {
        category = i;
        return;
      }
    int list[Blocks], n = buildList(list);
    for (int i = 0; i < n; ++i)
      if (md_ui::item(l, i, n).contains(x, y)) {
        int b = list[i];
        if (s.unlocked(Block(b))) {
          selected = b;
          inspect = -1;
          erase = false;
          buildOpen = false;
          previewX = previewY = lineX = lineY = -1;
        } else {
          closePanels();
          notify(tr("Unlock this block in Research", "先在科技研究中解锁"));
          returnView = World;
          view = Research;
        }
        return;
      }
    return;
  }
  if (toolsOpen && l.tools.contains(x, y)) {
    for (int i = 0; i < 6; ++i)
      if (l.tool[i].contains(x, y)) {
        if (i < 2) {
          closePanels();
          inspect = -1;
          returnView = World;
          view = i == 0 ? Research : Units;
          if (i == 0)
            pocket::log("Mindustry: research opened");
        } else if (i == 2 || i == 3) {
          zoom = std::clamp(zoom + (i == 2 ? 4 : -4), MinZoom, MaxZoom);
          zoomTime = 900;
          char message[120];
          std::snprintf(message, sizeof(message),
                        "Mindustry: view zoom=%d camera=%.3f,%.3f", zoom,
                        cameraX, cameraY);
          pocket::log(message);
        } else if (i == 4) {
          erase = !erase;
          inspect = -1;
          closePanels();
        } else {
          s.muted = !s.muted;
          milky::volume(s.muted ? 0 : 65, 55);
          pocket::save();
        }
        return;
      }
    return;
  }
  for (int i = 0; i < 4; ++i)
    if (l.dock[i].contains(x, y)) {
      if (i == 0) {
        bool open = !buildOpen;
        closePanels();
        buildOpen = open;
        return;
      }
      if (selected && !buildOpen) {
        if (i == 1)
          direction = (direction + 1) & 3;
        if (i == 2)
          buildNow();
        if (i == 3) {
          selected = Empty;
          previewX = previewY = lineX = lineY = -1;
        }
      } else if (inspect >= 0 && !buildOpen) {
        if (i == 1) {
          if (s.buildings[inspect].buildState) {
            cameraX = s.playerX;
            cameraY = s.playerY;
          } else
            s.supply(inspect);
          pocket::save();
        }
        if (i == 2) {
          s.queueRemove(inspect);
          inspect = -1;
          pocket::save();
        }
        if (i == 3)
          inspect = -1;
      } else if (i == 1) {
        cameraX = 24.5f;
        cameraY = 35;
      } else if (i == 2) {
        closePanels();
        mapOpen = !mapOpen;
      } else if (erase)
        erase = false;
      else {
        bool open = !toolsOpen;
        closePanels();
        toolsOpen = open;
      }
      return;
    }
  if (buildOpen || toolsOpen || stockOpen) {
    closePanels();
    return;
  }
  if ((selected || inspect >= 0 || rallyPick) && l.info.contains(x, y))
    return;
  int tx, ty;
  tileAt(x, y, tx, ty);
  if (tx < 0 || ty < 0 || tx >= W || ty >= H)
    return;
  if (rallyPick) {
    s.rallyX = tx + .5f;
    s.rallyY = ty + .5f;
    rallyPick = false;
    pocket::save();
    return;
  }
  int b = s.at(tx, ty);
  if (selected) {
    previewX = tx;
    previewY = ty;
    return;
  }
  if (erase) {
    if (s.queueRemove(b))
      pocket::save();
    return;
  }
  inspect = b;
  if (b < 0) {
    if (s.canMine(tx, ty)) {
      if (s.mineX == tx && s.mineY == ty && pressTime < 350)
        s.stopMining();
      else if (std::hypot(s.playerX - tx - .5f, s.playerY - ty - .5f) <= MineRange)
        s.mine(tx, ty);
      else notify(tr("Pan closer to mine", "移动视角，让机器人靠近矿点"));
    } else {
      s.stopMining();
      if (s.tiles[ty * W + tx].ore < Items)
        notify(
            tr("Alpha cannot mine this ore", "此矿物需要更高等级的采矿设备"));
    }
    pocket::save();
  }
}
} // namespace
namespace game {
const char *title() { return "Mindustry PXA"; }
const char *app_id() { return "pxa-mindustry"; }
void init() {
  static const char *const sounds[] = {
    "assets/click.s16", "assets/wave.s16", "assets/shoot.s16", "assets/explode.s16",
    "assets/place.s16", "assets/break.s16", "assets/unit.s16", "assets/unlock.s16",
    "assets/alpha.s16", "assets/arc.s16", "assets/unitshot.s16", "assets/heal.s16",
    "assets/buildloop.s16", "assets/mineloop.s16", "assets/drillloop.s16",
    "assets/smeltloop.s16", "assets/factoryloop.s16"};
  static_assert(sizeof(sounds) / sizeof(*sounds) == unsigned(Sound::Count));
  milky::sounds(sounds, unsigned(Sound::Count));
  milky::volume(65, 55);
  s.generate(154, 0, false);
  resetUI();
  view = Title;
  auto l = controls();
  auto b = milky::bounds();
  char geometry[160];
  std::snprintf(geometry, sizeof(geometry),
                "Mindustry UI bounds=%d,%d,%d,%d button=%d dock=%d,%d,%d,%d",
                b.x, b.y, b.w, b.h, l.button, l.dock[0].x, l.dock[0].y,
                l.dock[0].w, l.dock[0].h);
  pocket::log(geometry);
  pocket::log("Mindustry: ready version=0.11.0");
}
void reset() { newGame(false); }
void key(pocket::Key k) {
  using pocket::Key;
  if (k == Key::B) {
    cancel_touches();
    if (view == World) {
      s.stopMining();
      closePanels();
      selected = Empty;
      previewX = previewY = lineX = lineY = -1;
      inspect = -1;
      rallyPick = false;
    } else
      view = parentView();
  } else if (view == Title && k == Key::A) {
    if (hasSave)
      view = World;
    else
      newGame(false);
  } else if (view == World) {
    if (k == Key::UP)
      cameraY -= 2;
    if (k == Key::DOWN)
      cameraY += 2;
    if (k == Key::LEFT)
      cameraX -= 2;
    if (k == Key::RIGHT)
      cameraX += 2;
    if (k == Key::X)
      direction = (direction + 1) & 3;
    if (k == Key::Y)
      s.startWave();
    if (k == Key::L)
      category = (category + 6) % 7;
    if (k == Key::R)
      category = (category + 1) % 7;
    if (k == Key::A) {
      if (!selected) {
        int list[Blocks];
        buildList(list);
        selected = list[0];
      } else if (previewX < 0) {
        previewX = int(cameraX);
        previewY = int(cameraY);
      } else
        buildNow();
    }
  }
  cameraX = std::clamp(cameraX, 2.f, 46.f);
  cameraY = std::clamp(cameraY, 2.f, 46.f);
  pocket::redraw();
}
bool mapTouch(int x, int y) {
  if (view != World || buildOpen || toolsOpen || stockOpen)
    return false;
  if (milky::menu_button().contains(x, y))
    return false;
  auto l = controls();
  if (l.pause.contains(x, y) || l.wave.contains(x, y) || l.stock.contains(x, y))
    return false;
  for (auto r : l.dock)
    if (r.contains(x, y))
      return false;
  if (mapOpen && l.map.contains(x, y))
    return false;
  if (!mapOpen && (selected || inspect >= 0) && l.mapToggle.contains(x, y))
    return false;
  if ((selected || inspect >= 0 || rallyPick) && l.info.contains(x, y))
    return false;
  return x >= 0 && y >= 0 && x < sw() && y < sh();
}
void pointer(pocket::Phase phase, int x, int y) {
  using pocket::Phase;
  if (phase == Phase::DOWN) {
    down = true;
    drag = false;
    downX = lastX = x;
    downY = lastY = y;
    heldTime = pressTime = 0;
    if (mapTouch(x, y) &&
        (selected == Conveyor || selected == Conduit || selected == Wall)) {
      tileAt(x, y, lineX, lineY);
      previewX = lineX;
      previewY = lineY;
    }
  }
  if (phase == Phase::MOVE && down) {
    if (std::abs(x - downX) + std::abs(y - downY) > 6)
      drag = true;
    if (drag && mapTouch(downX, downY) && lineX >= 0) {
      tileAt(x, y, previewX, previewY);
      if (std::abs(previewX - lineX) >= std::abs(previewY - lineY)) {
        previewY = lineY;
        direction = previewX >= lineX ? 0 : 2;
      } else {
        previewX = lineX;
        direction = previewY >= lineY ? 3 : 1;
      }
      previewX = std::clamp(previewX, 2, W - 3);
      previewY = std::clamp(previewY, 2, H - 3);
    } else if (drag && mapTouch(downX, downY)) {
      cameraX = std::clamp(cameraX - float(x - lastX) / zoom, 2.f, 46.f);
      cameraY = std::clamp(cameraY - float(y - lastY) / zoom, 2.f, 46.f);
    }
    lastX = x;
    lastY = y;
  }
  if (phase == Phase::UP && down) {
    down = false;
    if (!drag && std::abs(x - downX) + std::abs(y - downY) < 10)
      tap(x, y);
  }
  if (phase == Phase::CANCEL) {
    down = false;
    drag = false;
    lineX = lineY = -1;
    previewX = previewY = -1;
    heldTime = pressTime = 0;
  }
  pocket::redraw();
}
namespace {
struct Contact {
  uint8_t id = 0;
  int x = 0, y = 0;
  bool active = false, map = false;
};
Contact contacts[2];
bool multi = false, navigating = false, spanReady = false;
float startSpan = 0, startZoom = 16, anchorX = 0, anchorY = 0;
int contactCount() { return int(contacts[0].active) + int(contacts[1].active); }
void rebaseGesture() {
  float x = (contacts[0].x + contacts[1].x) * .5f;
  float y = (contacts[0].y + contacts[1].y) * .5f;
  startSpan = std::hypot(float(contacts[0].x - contacts[1].x),
                         float(contacts[0].y - contacts[1].y));
  spanReady = startSpan >= 12;
  startZoom = float(zoom);
  anchorX = (x - originX()) / zoom;
  anchorY = (y - originY()) / zoom;
}
void updateGesture() {
  if (!navigating || contactCount() != 2)
    return;
  float x = (contacts[0].x + contacts[1].x) * .5f;
  float y = (contacts[0].y + contacts[1].y) * .5f;
  float distance = std::hypot(float(contacts[0].x - contacts[1].x),
                              float(contacts[0].y - contacts[1].y));
  if (!spanReady) {
    rebaseGesture();
    return;
  }
  float requested = startZoom * distance / startSpan;
  zoom = std::clamp(int(std::lround(requested)), MinZoom, MaxZoom);
  cameraX = std::clamp(anchorX + (sw() * .5f - x) / zoom, 2.f, 46.f);
  cameraY = std::clamp(anchorY + (controls().centerY - y) / zoom, 2.f, 46.f);
  zoomTime = 900;
  if (requested < MinZoom || requested > MaxZoom)
    rebaseGesture();
}
} // namespace
bool touch_active() { return contactCount() != 0; }
void cancel_touches() {
  for (auto &c : contacts)
    c = Contact{};
  multi = navigating = spanReady = false;
  pointer(pocket::Phase::CANCEL, lastX, lastY);
}
void touch(pocket::Phase phase, uint8_t id, int x, int y) {
  using pocket::Phase;
  Contact *point = nullptr;
  for (auto &c : contacts)
    if (c.active && c.id == id)
      point = &c;
  if (phase == Phase::CANCEL) {
    if (point)
      cancel_touches();
    return;
  }
  if (phase == Phase::DOWN) {
    if (point)
      return;
    for (auto &c : contacts)
      if (!c.active) {
        point = &c;
        break;
      }
    if (!point)
      return;
    *point = {id, x, y, true, mapTouch(x, y) && !s.lost && !s.won};
    if (contactCount() == 1) {
      pointer(phase, x, y);
      return;
    }
    if (!multi) {
      multi = true;
      navigating = contacts[0].map && contacts[1].map;
      pointer(Phase::CANCEL, x, y);
    }
    if (navigating)
      rebaseGesture();
    return;
  }
  if (!point)
    return;
  int dx = x - point->x, dy = y - point->y;
  point->x = x;
  point->y = y;
  if (multi) {
    if (phase == Phase::MOVE && navigating) {
      if (contactCount() == 2)
        updateGesture();
      else {
        cameraX = std::clamp(cameraX - float(dx) / zoom, 2.f, 46.f);
        cameraY = std::clamp(cameraY - float(dy) / zoom, 2.f, 46.f);
      }
    }
    if (phase == Phase::UP) {
      point->active = false;
      if (!touch_active()) {
        if (navigating) {
          char message[120];
          std::snprintf(message, sizeof(message),
                        "Mindustry: view zoom=%d camera=%.3f,%.3f", zoom,
                        cameraX, cameraY);
          pocket::log(message);
        }
        multi = navigating = spanReady = false;
      }
    }
    pocket::redraw();
    return;
  }
  pointer(phase, x, y);
  if (phase == Phase::UP)
    point->active = false;
}
void audioStep() {
  bool inGame = hasSave && (view == World || view == Paused || view == Units ||
    ((view == Research || view == Guide) && (returnView == World || returnView == Paused)));
  int core = s.core();
  bool damaged = core >= 0 && s.buildings[core].hp * 100 < defs[Core].hp * 85;
  md_audio::scene(!inGame ? md_audio::Scene::Menu :
    damaged ? md_audio::Scene::Dark : md_audio::Scene::Calm, s.musicMode);
  bool active = view == World && !s.lost && !s.won;
  if (!active) {
    s.soundCount = 0;
    md_audio::stop(3); md_audio::stop(4);
    return;
  }
  auto volumeAt = [](float x, float y, unsigned base = 100) {
    // PXA mono output: retain distance attenuation around the camera.
    float d = std::hypot(x - cameraX, y - cameraY);
    return unsigned(std::max(0.f, 1.f - d / 28.f) * base);
  };
  const SoundEvent *weapons = nullptr, *world = nullptr;
  unsigned weaponVolume = 0, worldPriority = 0;
  for (unsigned i = 0; i < s.soundCount; ++i) {
    auto &e = s.soundEvents[i];
    if (e.kind == Sound::Wave) md_audio::wave();
    unsigned v = e.kind == Sound::Wave || e.kind == Sound::Unlock ? 100 : volumeAt(e.x, e.y);
    bool shot = e.kind == Sound::DuoShot || e.kind == Sound::AlphaShot ||
      e.kind == Sound::ArcShot || e.kind == Sound::UnitShot;
    if (shot) { if (v > weaponVolume) { weapons = &e; weaponVolume = v; } }
    else {
      unsigned priority = v + (e.kind == Sound::Wave ? 400 : e.kind == Sound::Explosion ? 200 : 100);
      if (v && priority > worldPriority) { world = &e; worldPriority = priority; }
    }
  }
  if (weapons) md_audio::effect(weapons->kind, 1, weaponVolume);
  if (world) md_audio::effect(world->kind, 2,
    world->kind == Sound::Wave || world->kind == Sound::Unlock ? 100 : volumeAt(world->x, world->y));
  s.soundCount = 0;
  if (s.builder >= 0) md_audio::loop(Sound::BuildLoop, 3, volumeAt(s.playerX, s.playerY, 60));
  else if (s.mining) md_audio::loop(Sound::MineLoop, 3, volumeAt(s.playerX, s.playerY, 60));
  else md_audio::stop(3);
  Sound machine = Sound::DrillLoop; unsigned loudest = 0;
  for (auto &b : s.buildings) if (b.type && !b.buildState) {
    Sound loop = Sound::Count; unsigned base = 0;
    if ((b.type == Drill || b.type == Pneumatic) && b.drillRate > 0) { loop = Sound::DrillLoop; base = 8; }
    else if (canProcess(b) && (b.type == Press || b.powerEfficiency)) {
      if (b.type == Smelter || b.type == Kiln) { loop = Sound::SmeltLoop; base = 7; }
      else if ((b.type == GroundFactory || b.type == AirFactory) && b.progress < FactoryTime && s.unitCount() < MaxUnits) {
        loop = Sound::FactoryLoop; base = 10;
      }
    }
    auto v = volumeAt(b.x + defs[b.type].size * .5f, b.y + defs[b.type].size * .5f, base);
    if (loop != Sound::Count && v > loudest) { machine = loop; loudest = v; }
  }
  if (loudest) md_audio::loop(machine, 4, loudest); else md_audio::stop(4);
}
void tick(uint32_t ms) {
  bool animated = view == World && !s.lost && !s.won;
  bool hadToast = toastTime != 0;
  zoomTime = zoomTime > ms ? zoomTime - ms : 0;
  toastTime = toastTime > ms ? toastTime - ms : 0;
  paint += ms;
  if (view == World && !s.lost && !s.won) {
    accum += ms;
    while (accum >= 50) {
      s.followView(cameraX, cameraY);
      s.step();
      accum -= 50;
    }
    if (down && !drag && selected == Empty && mapTouch(downX, downY)) {
      heldTime += ms;
      pressTime += ms;
      int x, y;
      tileAt(downX, downY, x, y);
      int id = s.at(x, y);
      if (pressTime >= 500 && !erase && !rallyPick && id >= 0 &&
          s.buildings[id].type >= Drill) {
        auto &b = s.buildings[id];
        selected = b.type;
        direction = b.dir;
        category = defs[b.type].category;
        inspect = -1;
        previewX = previewY = lineX = lineY = -1;
        down = false;
        drag = true;
        heldTime = pressTime = 0;
        notify(tr("Same block selected", "已选择同类建筑"));
        pocket::log("Mindustry: block copied");
      } else if (heldTime >= 350 && id < 0) {
        if (std::hypot(s.playerX - x - .5f, s.playerY - y - .5f) <= MineRange)
          s.mine(x, y);
        heldTime = 0;
      }
    }
    autoSave += ms;
    if (autoSave >= 5000) {
      autoSave = 0;
      pocket::save();
    }
  }
  if (paint >= 33) {
    paint %= 33;
#ifdef MINDUSTRY_PROFILE
    // Profiling holds the same frame cadence in both renderers, including idle
    // menus.
    pocket::redraw();
#else
    if (animated || (hadToast && !toastTime))
      pocket::redraw();
#endif
  }
  audioStep();
}
void draw() {
  if (view == Title) {
    titleDraw();
    return;
  }
  if (view != World) {
    pocket::clear(BG);
    menus();
    if (toastTime) {
      panel(8, sh() - 46, sw() - 16, 22);
      center(10, sh() - 41, sw() - 20, toast, Gold, 11);
    }
    return;
  }
  worldDraw();
  hud();
  if (s.lost || s.won) {
    int y = sh() / 2 - 36;
    panel(sw() / 2 - 110, y, 220, 94);
    center(sw() / 2 - 105, y + 12, 210,
           s.won ? tr("Sector captured", "区域已占领")
                 : tr("Core destroyed", "核心已摧毁"),
           s.won ? Gold : Red, 20);
    button(sw() / 2 - 90, sh() / 2 + 22, 180, 29,
           tr("Main menu", "返回主菜单"));
  }
}
size_t serialize(uint8_t *data, size_t capacity) {
  if (!hasSave)
    return 0;
  return s.save(data, capacity);
}
void load(const uint8_t *data, size_t size) {
  if (s.load(data, size)) {
    hasSave = true;
    resetUI();
    milky::volume(s.muted ? 0 : 65, 55);
    pocket::log("Mindustry: save restored");
  } else if (size)
    pocket::log("Mindustry: invalid save ignored");
}
} // namespace game
