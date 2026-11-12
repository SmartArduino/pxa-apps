// SPDX-License-Identifier: MIT
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-studio/studio.hpp"
#include "art.hpp"
#include "audio.hpp"
#include "core.hpp"
#include "map_gestures.hpp"
#include "map_raster.hpp"
#include "pinch_tracker.hpp"
#include "turn_log.hpp"
#include "visible_tiles.hpp"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#ifdef UNCIV_TEST_TACTICS
#include "tests/tactics_fixture.hpp"
#endif
namespace {
using namespace unciv;
using namespace pocket;
State state;
enum Screen {
  Title,
  Map,
  Technology,
  CityView,
  Empire,
  Policies,
  Help,
  NewGame,
  Result,
  Production,
  BattleView,
  CityEconomy,
  Journal,
  CityTiles,
  PurchaseConfirm,
  UnitView,
  PromotionView,
  UpgradeView,
  ProductionQueue,
  PillageView
};
Screen screen = Title;
int map_size = 1, city_tab = 0, map_tile_width = 48, inspected_job = Warrior;
int empire_tab = 2, empire_page = 0;
int battle_target = -1, journal_page = 0, work_tile = -1, queue_page = 0;
TurnLog turn_log;
const int tile_widths[] = {32, 48, 64};
int tile_width() {
  return screen == CityTiles
             ? std::clamp((HEIGHT - 120) * 8 / 35 / 2 * 2, 22, 48)
             : map_tile_width;
}
bool pinching = false;
struct ZoomReference {
  int size, pan_x, pan_y, x, y;
} zoom_reference;
int tech_slots[] = {Agriculture, Writing,   Iron,        Pottery,
                    Bronze,      Education, Mining,      Wheel,
                    Engineering, Archery,   Mathematics, Astronomy};
int slot_tech(int n) { return tech_slots[std::clamp(n, 0, TECHS - 1)]; }
int city_job(int n) { return city_tab ? 6 + n : n; }
uint64_t pan_started = 0, pan_render_begin = 0;
unsigned pan_frame_begin = 0;
int cursor = 143, selected = -1, selected_city = -1, focus = 0, pan_x = 0,
    pan_y = 0;
int down_x, down_y, old_pan_x, old_pan_y, pressed = -1;
int preview_tile = -1;
uint32_t state_revision = 0;
struct Movement {
  uint32_t revision = ~0u;
  int unit = -2;
  int16_t cost[N], previous[N];
  bool attacks[N];
} movement_cache;
struct Objects {
  uint32_t revision = ~0u;
  int8_t units[N], cities[N], claims[N];
  uint8_t borders[N], work[N];
} object_cache;
bool dragging = false, touch = false, started = false;
const char *toast = nullptr;
uint32_t toast_end = 0;
constexpr uint16_t SEA = rgb(17, 36, 57), DARK = rgb(12, 25, 42),
                   PAPER = rgb(231, 237, 244), GOLD = rgb(226, 185, 100),
                   MINT = rgb(132, 209, 182), RED = rgb(238, 133, 124),
                   GREY = rgb(137, 161, 170);
constexpr uint16_t colors[3] = {rgb(112, 177, 236), rgb(219, 115, 99),
                                rgb(191, 162, 231)};
const char *kind_name(int k) {
  if (k == NoJob)
    return tr("Choose production", "选择生产");
  const char *en[] = {"Settler", "Warrior",   "Scout",    "Worker",
                      "Archer",  "Swordsman", "Monument", "Granary",
                      "Library", "Barracks"};
  const char *zh[] = {"开拓者", "勇士",   "侦察兵", "工人",   "弓箭手",
                      "剑士",   "纪念碑", "粮仓",   "图书馆", "兵营"};
  return tr(en[k], zh[k]);
}
const char *tech_name(int k) {
  const char *en[] = {"Agriculture",  "Pottery",   "Mining",      "Archery",
                      "Writing",      "Bronze",    "The Wheel",   "Mathematics",
                      "Iron Working", "Education", "Engineering", "Astronomy"};
  const char *zh[] = {"农业", "制陶", "采矿", "弓术", "文字",   "青铜冶炼",
                      "轮子", "数学", "铁器", "教育", "工程学", "天文学"};
  return tr(en[k], zh[k]);
}
const char *tech_detail(int k) {
  const char *en[] = {"Farms",
                      "Granary",
                      "Hill mines",
                      "Archers",
                      "Library",
                      "Barracks",
                      "Engineering path",
                      "Archer strength +3",
                      "Swordsmen",
                      "Science +3 / city",
                      "Astronomy path",
                      "Complete all: victory"};
  const char *zh[] = {"平地农田",    "解锁粮仓",      "丘陵矿山",
                      "解锁弓箭手",  "解锁图书馆",    "解锁兵营",
                      "通向工程学",  "弓箭攻击力 +3", "解锁剑士",
                      "每城科研 +3", "通向天文学",    "全科技达成胜利"};
  return tr(en[k], zh[k]);
}
const char *terrain_name(int t) {
  const char *en[] = {"Grassland", "Plains", "Desert", "Hill",
                      "Mountain",  "Forest", "Coast",  "Ocean"};
  const char *zh[] = {"草原", "平原", "沙漠", "丘陵",
                      "山脉", "森林", "海岸", "海洋"};
  return tr(en[t], zh[t]);
}
const char *nation(int p) {
  const char *en[] = {"China", "Rome", "India"};
  const char *zh[] = {"中华", "罗马", "印度"};
  return tr(en[p], zh[p]);
}
const char *city_name(int c) {
  if (c < 0 || c >= CITIES)
    return "";
  int owner = state.cities[c].owner;
  for (int p = 0; p < 3; p++)
    if (state.cities[c].tile == state.capitals[p]) {
      const char *en[] = {"Chang'an", "Rome", "Delhi"};
      const char *zh[] = {"长安", "罗马城", "德里"};
      return tr(en[p], zh[p]);
    }
  return owner == 0   ? tr("New City", "新城")
         : owner == 1 ? tr("Roman Colony", "罗马殖民地")
                      : tr("Indian Colony", "印度殖民地");
}
void label(int x, int y, int w, const char *s, uint16_t color = PAPER,
           int scale = 1, unsigned align = 0) {
  if (!map_raster::text(x, y, w, s, color, scale, align))
    studio::label(x, y, w, s, color, scale, align);
}
void icon(const art::Sprite &im, int x, int y, int size, uint16_t tint = 0) {
  map_raster::icon(x, y, size, im.rgb, im.alpha, im.w, im.h, tint);
}
void box(int x, int y, int w, int h, const char *s, bool active = false,
         uint16_t color = colors[0]) {
  // Explicit pixel typography keeps controls on one line at high screen DPI.
  uint16_t fill = active ? color : rgb(29, 49, 74);
  studio::card(x, y, w, h, fill, studio::mix(fill, 0xffff, active ? 120 : 32),
               4);
  label(x + 3, y + (h - 10) / 2, w - 6, s, active ? DARK : PAPER, 1, 1);
}
void notify(const char *en, const char *zh) {
  toast = tr(en, zh);
  toast_end = millis() + 2500;
  redraw();
}
void record(const char *event) {
  char b[180];
  int c = 0, u = 0;
  for (auto &v : state.cities)
    if (v.tile >= 0 && v.owner == 0)
      c++;
  for (auto &v : state.units)
    if (v.tile >= 0 && v.owner == 0)
      u++;
  snprintf(b, sizeof(b),
           "Unciv: event=%s turn=%u cities=%d units=%d tech=%u screen=%d",
           event, state.turn, c, u, state.civ[0].tech, int(screen));
  log(b);
}
void changed(const char *event) {
  ++state_revision;
  preview_tile = -1;
  save();
  redraw();
  record(event);
}
const Movement &movement() {
  auto &m = movement_cache;
  if (m.revision == state_revision && m.unit == selected)
    return m;
  m.revision = state_revision;
  m.unit = selected;
  std::fill(m.cost, m.cost + N, int16_t(-1));
  std::fill(m.previous, m.previous + N, int16_t(-1));
  std::fill(m.attacks, m.attacks + N, false);
  if (selected >= 0 && state.units[selected].tile >= 0 &&
      state.units[selected].owner == 0 && state.result == Playing) {
    reachable(state, selected, m.cost, m.previous);
    for (int t = 0; t < N; ++t)
      if (inside(state, t))
        m.attacks[t] = can_attack(state, selected, t);
  }
  return m;
}
const Objects &objects() {
  auto &m = object_cache;
  if (m.revision == state_revision)
    return m;
  m.revision = state_revision;
  std::fill(m.units, m.units + N, int8_t(-1));
  std::fill(m.cities, m.cities + N, int8_t(-1));
  for (int i = 0; i < UNITS; ++i)
    if (state.units[i].tile >= 0)
      m.units[state.units[i].tile] = i;
  for (int i = 0; i < CITIES; ++i)
    if (state.cities[i].tile >= 0)
      m.cities[state.cities[i].tile] = i;
  std::fill(m.work, m.work + N, uint8_t(0));
  territory(state, m.claims);
  for (int i = 0; i < CITIES; ++i)
    if (state.cities[i].tile >= 0) {
      int16_t worked[18];
      int count = worked_tiles(state, i, worked);
      for (int n = 0; n < count; ++n)
        m.work[worked[n]] = tile_locked(state, i, worked[n]) ? 3 : 1;
    }
  for (int t = 0; t < N; ++t) {
    m.borders[t] = 0;
    int c = m.claims[t];
    if (c < 0)
      continue;
    for (int d = 0; d < 6; ++d) {
      int n = neighbor(t, d), other = inside(state, n) ? m.claims[n] : -1;
      if (other < 0 || state.cities[other].owner != state.cities[c].owner)
        m.borders[t] |= 1u << d;
    }
  }
  return m;
}
int bottom() { return HEIGHT - 80; }
int center_x(int t) { return t % W * (tile_width() * 3 / 4); }
int center_y(int t) {
  return t / W * (tile_width() * 7 / 8) + (t % W & 1) * (tile_width() * 7 / 16);
}
int origin_x() { return WIDTH / 2 - center_x(cursor) + pan_x; }
int origin_y() { return (40 + bottom()) / 2 - center_y(cursor) + pan_y; }
void apply_zoom(const pinch_tracker::Camera &camera) {
  map_tile_width = camera.size;
  pan_x = std::clamp(camera.pan_x, -state.width * tile_width(),
                     state.width * tile_width());
  pan_y = std::clamp(camera.pan_y, -state.height * tile_width(),
                     state.height * tile_width());
  redraw();
}
void center(int t) {
  cursor = t;
  pan_x = pan_y = 0;
}
void open(Screen s) {
  toast = nullptr;
  preview_tile = -1;
  screen = s;
  focus = 0;
  if (s == Empire)
    empire_page = 0;
  if (s == CityView) {
    int job = state.cities[selected_city].job;
    city_tab = job >= KINDS && job < JOBS;
    focus = job == NoJob ? Warrior : job - (city_tab ? KINDS : 0);
  }
  if (s == ProductionQueue) {
    queue_page = 0;
    focus = 0;
  }
  if (s == CityTiles) {
    center(state.cities[selected_city].tile);
    selected = -1;
    work_tile = state.cities[selected_city].tile;
  }
  if (s == Technology) {
    for (int n = 0; n < TECHS; n++)
      if (slot_tech(n) == state.civ[0].research)
        focus = n;
  }
  sound::play(sound::Paper);
  redraw();
}
void go_back() {
  if (screen == ProductionQueue) {
    open(CityView);
    return;
  }
  if (screen == PromotionView || screen == UpgradeView ||
      screen == PillageView) {
    open(UnitView);
    return;
  }
  if (screen == PurchaseConfirm) {
    open(CityView);
    return;
  }
  if (screen == CityTiles) {
    open(CityEconomy);
    return;
  }
  if (screen == CityEconomy) {
    open(CityView);
    return;
  }
  if (screen == Production) {
    open(CityView);
    city_tab = inspected_job >= KINDS;
    focus = inspected_job - (city_tab ? KINDS : 0);
    return;
  }
  if (screen == Help) {
    open(Title);
    return;
  }
  if (!started) {
    open(Title);
    return;
  }
  if (screen == Map) {
    selected = -1;
    redraw();
  } else if (screen == Title)
    open(Map);
  else
    open(Map);
}
void next_unit() {
  int next = selected;
  for (int k = 1; k <= UNITS; k++) {
    int i = (next + k + UNITS) % UNITS;
    if (state.units[i].tile >= 0 && state.units[i].owner == 0 &&
        state.units[i].moves && !state.units[i].fortified &&
        !state.units[i].work_left) {
      selected = i;
      selected_city = -1;
      center(state.units[i].tile);
      sound::play(sound::Click);
      redraw();
      return;
    }
  }
  notify("No awake units waiting", "暂无待行动单位，可结束回合");
}
void new_game() {
  turn_log.clear();
  uint32_t seed = pocket::random();
  unciv::start(state, seed, map_size);
  int start = int(state.height / 2) * W + 3;
  selected = unit_at(state, start);
  selected_city = -1;
  center(start);
  started = true;
  screen = Map;
  toast = nullptr;
  changed("new");
}
void choose(int tile) {
  if (tile < 0 || tile >= N || !(state.tiles[tile].seen & 1))
    return;
  if (selected >= 0 && state.units[selected].tile >= 0) {
    auto &u = state.units[selected];
    if (tile == u.tile) {
      selected = -1;
      selected_city = city_at(state, tile);
      if (selected_city >= 0 && state.cities[selected_city].owner != 0)
        selected_city = -1;
      redraw();
      return;
    }
    if (tile != u.tile) {
      if (can_attack(state, selected, tile)) {
        battle_target = tile;
        open(BattleView);
        return;
      }
      if (move(state, selected, tile)) {
        sound::play(sound::Move);
        cursor = tile;
        pan_x = pan_y = 0;
        selected_city = -1;
        changed("move");
        return;
      }
      int other = unit_at(state, tile), city = city_at(state, tile);
      if ((other < 0 || state.units[other].owner != 0) &&
          (city < 0 || state.cities[city].owner != 0)) {
        if (!can_enter(state, 0, tile))
          notify("Closed borders: declare war to enter",
                 "边界关闭，宣战后才能进入");
        else if (!u.moves)
          notify("No movement points remaining.", "本回合行动力已用完。");
        else
          notify("Cannot reach this tile this turn.", "本回合无法到达此格。");
        return;
      }
    }
  }
  int u = unit_at(state, tile), c = city_at(state, tile);
  if (c >= 0 && state.cities[c].owner == 0 && cursor == tile &&
      selected_city == c) {
    open(CityView);
    return;
  }
  cursor = tile;
  pan_x = pan_y = 0;
  selected = u >= 0 && state.units[u].owner == 0 ? u : -1;
  selected_city = c >= 0 && state.cities[c].owner == 0 ? c : -1;
  sound::play(sound::Click);
  redraw();
}
void primary() {
  if (selected_city >= 0 && state.cities[selected_city].tile == cursor) {
    open(CityView);
    return;
  }
  if (selected < 0 || state.units[selected].tile < 0) {
    notify("Select a unit or one of your cities", "请先选择己方单位或城市");
    return;
  }
  auto &u = state.units[selected];
  if (u.order != Manual && cancel_order(state, selected)) {
    changed("cancel-order");
    notify("Manual control restored.", "已恢复手动操作。");
    return;
  }
  if (u.fortified && wake(state, selected)) {
    changed("wake");
    notify("Unit awakened.", "单位已唤醒。");
    return;
  }
  if (u.kind == Settler) {
    if (found(state, selected)) {
      selected_city = city_at(state, cursor);
      selected = -1;
      sound::play(sound::City);
      notify("City founded.", "城市已建立。");
      changed("found");
    } else
      notify("Need land, a move and 4 hexes from any city",
             "建城需行动点，且距其他城市至少四格");
  } else if (u.kind == Worker) {
    if (u.work_left && cancel_work(state, selected)) {
      changed("cancel-work");
      notify("Construction cancelled.", "已停止作业。");
    } else if (improve(state, selected)) {
      sound::play(sound::Click);
      changed("improve");
      notify("Construction started.", "已开始作业，每回合推进一次。");
    } else
      notify("Improve nearby land; hill mines need Mining",
             "改良己方城市两格内土地；矿山需要采矿");
  } else if (fortify(state, selected)) {
    sound::play(sound::Click);
    changed("fortify");
    notify("Fortified.", "已驻防。");
  }
}
void finish_turn() {
  if (state.result != Playing) {
    open(Result);
    return;
  }
  for (int i = 0; i < CITIES; ++i)
    if (state.cities[i].tile >= 0 && state.cities[i].owner == 0 &&
        state.cities[i].job == NoJob) {
      selected_city = i;
      selected = -1;
      center(state.cities[i].tile);
      open(CityView);
      return;
    }
  auto old_tech = state.civ[0].tech;
  State before = state;
  unciv::end_turn(state);
  turn_log.collect(before, state);
  if (selected >= 0 && state.units[selected].tile < 0)
    selected = -1;
  if (selected_city >= 0 && state.cities[selected_city].owner != 0)
    selected_city = -1;
  sound::play(old_tech != state.civ[0].tech ? sound::City : sound::Click);
  changed("turn");
  if (state.result != Playing)
    open(Result);
  else if (old_tech != state.civ[0].tech)
    notify("Research completed.", "研究完成。");
}
int tile_at(int x, int y) {
  if (y < 40 || y >= bottom())
    return -1;
  int best = -1, dist = 99999;
  for (int t = 0; t < N; t++) {
    if (!inside(state, t))
      continue;
    int dx = x - center_x(t) - origin_x(), dy = y - center_y(t) - origin_y(),
        d = dx * dx + dy * dy;
    if (d < dist) {
      best = t;
      dist = d;
    }
  }
  return dist < tile_width() * tile_width() / 3 ? best : -1;
}
int grid_y() { return screen == CityView ? 107 : 50; }
int queue_rows() { return std::clamp((HEIGHT - 164) / 28, 2, QUEUE); }
int journal_rows() { return std::clamp((HEIGHT - 98) / 34, 2, 10); }
int overview_rows() { return std::clamp((HEIGHT - 127) / 32, 2, 10); }
int owned_id(bool units, int index) {
  for (int i = 0; i < (units ? UNITS : CITIES); ++i) {
    bool owned = units
                     ? state.units[i].tile >= 0 && state.units[i].owner == 0
                     : state.cities[i].tile >= 0 && state.cities[i].owner == 0;
    if (owned && index-- == 0)
      return i;
  }
  return -1;
}
int owned_count(bool units) {
  int count = 0;
  while (owned_id(units, count) >= 0)
    ++count;
  return count;
}
struct TitleLayout {
  int x, y, width, row_height, gap, count;
};
TitleLayout title_layout() {
  int count = started ? 3 : 2;
  int height = HEIGHT < 280 ? 32 : 36, gap = 8;
  int width = std::min(WIDTH - 48, 304);
  int total = count * (height + gap) - gap;
  int y = std::min((HEIGHT + 68 - total) / 2, HEIGHT - 28 - total);
  return {(WIDTH - width) / 2, y, width, height, gap, count};
}
int grid_step() {
  int rows = screen == CityView ? 3 : 4;
  return std::clamp(
      (HEIGHT - grid_y() - (screen == Technology ? 69 : 42)) / rows, 22, 46);
}
int hit(int x, int y) {
  if (x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT)
    return -1;
  if (screen == Title) {
    const auto menu = title_layout();
    if (x >= menu.x && x < menu.x + menu.width) {
      for (int row = 0; row < menu.count; ++row) {
        int top = menu.y + row * (menu.row_height + menu.gap);
        if (y >= top && y < top + menu.row_height)
          return row;
      }
    }
    return -1;
  }
  if (screen == Map) {
    if (x >= WIDTH - 67 && x < WIDTH - 2 && y >= bottom() - 45 &&
        y < bottom() - 3) {
      int tx = std::clamp((x - WIDTH + 65) * state.width / 59, 0,
                          int(state.width) - 1),
          ty = std::clamp((y - bottom() + 43) * state.height / 36, 0,
                          int(state.height) - 1);
      return 1000 + ty * W + tx;
    }
    if (selected >= 0 && state.units[selected].tile >= 0 && y >= bottom() + 3 &&
        y < bottom() + 21 && x >= WIDTH - 44)
      return 18;
    if (y >= HEIGHT - 29)
      return 10 + std::min(3, x * 4 / WIDTH);
    if (y < 24 && x >= WIDTH - 80 && x < WIDTH - 40)
      return 14;
    if (y >= 22 && y < 40 && x >= WIDTH - 81 && x < WIDTH - 58)
      return 17;
    if (y >= 22 && y < 40 && x >= WIDTH - 54)
      return x < WIDTH - 27 ? 15 : 16;
    return -1;
  }
  if (screen == Empire && empire_tab == 1 && y >= 5 && y < 28 &&
      x >= WIDTH - 88 && x < WIDTH - 40)
    return 104;
  if (screen == CityView && y >= 5 && y < 28 && x >= WIDTH - 140 &&
      x < WIDTH - 94)
    return 105;
  if (screen == CityView && y >= 5 && y < 28 && x >= WIDTH - 88 &&
      x < WIDTH - 40)
    return 102;
  if (screen == CityView && y >= 82 && y < 103)
    return x < WIDTH / 2 ? 100 : 101;
  if (screen == Production && y >= HEIGHT - 79 && y < HEIGHT - 56)
    return 92;
  if ((screen == Technology || screen == Production || screen == BattleView ||
       screen == PurchaseConfirm || screen == PromotionView ||
       screen == UpgradeView || screen == PillageView) &&
      y >= HEIGHT - 53 && y < HEIGHT - 30)
    return 91;
  if (screen == CityTiles && y >= HEIGHT - 28)
    return x < WIDTH / 3 ? 110 : x < WIDTH * 2 / 3 ? 111 : 90;
  if (y >= HEIGHT - 28)
    return screen == CityView && x >= WIDTH / 2 ? 91 : 90;
  if (screen == CityEconomy && y >= HEIGHT - 53 && y < HEIGHT - 30)
    return 103;
  if (screen == CityTiles) {
    if (y >= HEIGHT - 28)
      return x < WIDTH / 3 ? 110 : x < WIDTH * 2 / 3 ? 111 : 90;
    return -1;
  }
  if (screen == CityEconomy && y >= 49 && y < 76)
    return std::min(3, x * 4 / WIDTH);
  if (screen == Journal) {
    if (y >= HEIGHT - 53 && y < HEIGHT - 30)
      return x < WIDTH / 2 ? 110 : 111;
    if (y >= 44 && y < 44 + journal_rows() * 34)
      return journal_page * journal_rows() + (y - 44) / 34;
  }
  if (screen == ProductionQueue) {
    if (y >= HEIGHT - 107 && y < HEIGHT - 84)
      return x < WIDTH / 2 ? 114 : 115;
    if (y >= HEIGHT - 81 && y < HEIGHT - 58)
      return 110 + std::min(2, x * 3 / WIDTH);
    if (y >= HEIGHT - 53 && y < HEIGHT - 30)
      return 113;
    if (y >= 50 && y < 50 + queue_rows() * 28)
      return queue_page * queue_rows() + (y - 50) / 28;
    return -1;
  }
  if (screen == UnitView) {
    if (y >= 88 && y < 114)
      return x < WIDTH / 2 ? 0 : 1;
    if (y >= 120 && y < 146)
      return x < WIDTH / 2 ? 2 : 3;
    if (y >= 155 && y < 181)
      return x < WIDTH / 2 ? 4 : 5;
  }
  if (screen == PromotionView && y >= 54) {
    int step = std::clamp((HEIGHT - 130) / 2, 46, 62);
    if (y < 54 + step * 2)
      return std::min(1, x * 2 / WIDTH) + (y - 54) / step * 2;
  }
  if (screen == Technology || screen == CityView) {
    int cols = screen == Technology ? 3 : 2,
        rows = screen == Technology ? 4 : 3;
    if (y >= grid_y() && y < grid_y() + grid_step() * rows && x >= 8 &&
        x < WIDTH - 8)
      return ((y - grid_y()) / grid_step()) * cols +
             std::min(cols - 1, (x - 8) * cols / (WIDTH - 16));
  }
  if (screen == Empire) {
    if (y >= 42 && y < 64)
      return 100 + std::min(2, (x - 8) * 3 / (WIDTH - 16));
    if (empire_tab == 2) {
      if (y >= 70 && y < 158)
        return (y - 70) / 44;
      if (y >= 165 && y < 190)
        return 2;
    } else {
      if (y >= 68 && y < 68 + overview_rows() * 32)
        return empire_page * overview_rows() + (y - 68) / 32;
      if (y >= HEIGHT - 53 && y < HEIGHT - 30)
        return x < WIDTH / 2 ? 110 : 111;
    }
  }
  if (screen == Policies) {
    if (y >= 48 && y < 156)
      return (y - 48) / 36;
  }
  if (screen == NewGame) {
    int top = HEIGHT / 2 - 20;
    if (y >= top && y < top + 30)
      return 100 + std::min(2, x * 3 / WIDTH);
    if (y >= top + 55 && y < top + 85)
      return x < WIDTH / 2 ? 0 : 1;
  }
  if (screen == Result && y >= HEIGHT - 66 && y < HEIGHT - 34)
    return 0;
  if (screen == Help && y >= HEIGHT - 60 && y < HEIGHT - 32)
    return x < WIDTH / 2 ? 0 : 1;
  return -1;
}
void activate(int a) {
  if (screen == Title) {
    if (a == 0) {
      if (started) {
        open(Map);
        record("continue");
      } else
        open(NewGame);
    }
    if (a == 1 && started)
      open(NewGame);
    else if (a == (started ? 2 : 1))
      open(Help);
    return;
  }
  if (screen == Map) {
    if (a >= 1000) {
      center(a - 1000);
      redraw();
      return;
    }
    if (a == 10)
      next_unit();
    if (a == 11)
      primary();
    if (a == 12)
      open(Technology);
    if (a == 13)
      finish_turn();
    if (a == 14)
      open(Empire);
    if (a == 18 && selected >= 0) {
      open(UnitView);
      return;
    }
    if (a == 17) {
      journal_page = 0;
      turn_log.unread = 0;
      open(Journal);
    }
    if (a == 15 || a == 16) {
      int width = tile_width(), target = width;
      if (a == 16) {
        for (int value : tile_widths)
          if (value > width) {
            target = value;
            break;
          }
      } else {
        for (int i = 2; i >= 0; --i)
          if (tile_widths[i] < width) {
            target = tile_widths[i];
            break;
          }
      }
      int cx = WIDTH / 2, cy = (40 + bottom()) / 2;
      apply_zoom(
          pinch_tracker::zoom(width, pan_x, pan_y, cx, cy, cx, cy, cx, cy,
                              uint32_t(uint64_t(target) * 65536 / width)));
    }
    return;
  }
  if (a == 90) {
    go_back();
    return;
  }
  if (screen == Empire && empire_tab == 1 && a == 104) {
    int unit = owned_id(true, focus);
    if (unit >= 0) {
      selected = unit;
      selected_city = -1;
      center(state.units[unit].tile);
      open(UnitView);
    }
    return;
  }
  if (screen == UnitView) {
    if (selected < 0 || state.units[selected].tile < 0) {
      open(Map);
      return;
    }
    auto &u = state.units[selected];
    bool done = false;
    if (a == 0)
      done = u.fortified ? wake(state, selected) : fortify(state, selected);
    if (a == 1)
      done = u.order == Healing ? cancel_order(state, selected)
                                : heal(state, selected);
    if (a == 2)
      done = unciv::wait(state, selected);
    if (a == 3) {
      open(PromotionView);
      return;
    }
    if (a == 4) {
      open(UpgradeView);
      return;
    }
    if (a == 5) {
      open(PillageView);
      return;
    }
    if (done) {
      changed("unit-order");
      open(Map);
    } else
      notify("No movement, full health or construction in progress",
             "行动力不足、生命已满，或正在作业");
    return;
  }
  if (screen == PromotionView) {
    if (a >= 0 && a < PROMOTIONS) {
      focus = a;
      redraw();
    }
    if (a == 91) {
      if (promote(state, selected, focus)) {
        sound::play(sound::City);
        changed("promotion");
        open(UnitView);
      } else
        notify("This promotion is unavailable", "经验不足，或该晋升已达到上限");
    }
    return;
  }
  if (screen == UpgradeView && a == 91) {
    if (upgrade(state, selected)) {
      sound::play(sound::City);
      changed("upgrade");
      open(UnitView);
    } else
      notify("Need technology, gold, full movement and own territory",
             "需要科技、金币、完整行动力和己方领土");
    return;
  }
  if (screen == PillageView && a == 91) {
    if (pillage(state, selected)) {
      sound::play(sound::Battle);
      changed("pillage");
      open(Map);
      notify("Improvement pillaged: +25 gold, up to 25 HP restored",
             "已掠夺改良：金币+25，最多恢复25生命");
    } else
      notify("Need movement and an enemy or neutral improvement",
             "需要行动力，并位于敌方或中立改良地块");
    return;
  }
  if (screen == BattleView && a == 91) {
    auto forecast = battle_preview(state, selected, battle_target);
    if (forecast.possible && attack(state, selected, battle_target)) {
      sound::play(forecast.ranged ? sound::Arrow : sound::Battle);
      if (state.units[selected].tile < 0)
        selected = -1;
      center(selected >= 0 ? state.units[selected].tile : battle_target);
      selected_city = -1;
      changed("attack");
    }
    open(state.result == Playing ? Map : Result);
    return;
  }
  if (screen == CityView && a == 102) {
    open(CityEconomy);
    focus = state.cities[selected_city].focus;
    return;
  }
  if (screen == CityEconomy && a == 103) {
    open(CityTiles);
    return;
  }
  if (screen == CityTiles) {
    if (a == 110 && auto_assign(state, selected_city))
      changed("auto-assign");
    if (a == 111) {
      if (lock_tile(state, selected_city, work_tile,
                    !tile_locked(state, selected_city, work_tile)))
        changed("citizen-lock");
      else
        notify("Choose a workable tile; unlock a citizen if all are locked",
               "选择可工作地块；人口已锁满时先解锁");
    }
    return;
  }
  if (screen == CityEconomy && a >= 0 && a < FOCUSES) {
    if (city_focus(state, selected_city, a)) {
      focus = a;
      changed("city-focus");
    }
    return;
  }
  if (screen == Journal) {
    int pages =
        std::max(1, (turn_log.count + journal_rows() - 1) / journal_rows());
    if (a == 110 || a == 111) {
      journal_page = (journal_page + (a == 111 ? 1 : pages - 1)) % pages;
      focus = journal_page * journal_rows();
      redraw();
      return;
    }
    if (a >= 0 && a < turn_log.count) {
      const auto &e = turn_log.entries[a];
      if (e.type == TurnLog::Research) {
        open(Technology);
        return;
      }
      if (inside(state, e.tile)) {
        selected = unit_at(state, e.tile);
        if (selected >= 0 && state.units[selected].owner != 0)
          selected = -1;
        selected_city = city_at(state, e.tile);
        if (selected_city >= 0 && state.cities[selected_city].owner != 0)
          selected_city = -1;
        center(e.tile);
        open(Map);
      }
    }
    return;
  }
  if (screen == CityView && a == 105) {
    open(ProductionQueue);
    return;
  }
  if (screen == Production && a == 92) {
    if (enqueue(state, selected_city, inspected_job)) {
      changed("enqueue");
      open(ProductionQueue);
    } else
      notify("Queue full, duplicate building, or requirements unmet",
             "队列已满、建筑重复，或条件未满足");
    return;
  }
  if (screen == ProductionQueue) {
    auto &c = state.cities[selected_city];
    if (a >= 0 && a < c.queued) {
      focus = a;
      redraw();
      return;
    }
    if (a == 110 || a == 111) {
      int direction = a == 110 ? -1 : 1;
      if (reorder_queue(state, selected_city, focus, direction)) {
        focus += direction;
        queue_page = focus / queue_rows();
        changed("reorder-queue");
      }
      return;
    }
    if (a == 112) {
      if (remove_queue(state, selected_city, focus)) {
        focus = std::clamp(focus, 0, std::max(0, int(c.queued) - 1));
        queue_page = focus / queue_rows();
        changed("remove-queue");
      }
      return;
    }
    if (a == 113) {
      if (repeat_production(state, selected_city))
        changed("repeat-production");
      else
        notify("Repeat needs a unit and an empty queue",
               "需先清空后续队列，才能连续生产当前单位");
      return;
    }
    if (a == 114 || a == 115) {
      int pages = std::max(1, (c.queued + queue_rows() - 1) / queue_rows());
      queue_page = (queue_page + (a == 115 ? 1 : pages - 1)) % pages;
      focus = queue_page * queue_rows();
      redraw();
      return;
    }
    return;
  }
  if (screen == Production && a == 91) {
    if (produce(state, selected_city, inspected_job)) {
      changed("production");
      open(CityView);
      notify("Production selected.", "已选择生产项目。");
    } else
      notify("Already built, or technology required",
             "已经建造，或需要先研究对应科技");
    return;
  }
  if (screen == CityView && a == 91) {
    if (state.cities[selected_city].job == NoJob) {
      notify("Choose production first", "请先选择生产项目");
      return;
    }
    open(PurchaseConfirm);
    return;
  }
  if (screen == PurchaseConfirm && a == 91) {
    if (purchase(state, selected_city)) {
      sound::play(sound::City);
      changed("purchase");
      open(CityView);
      notify("Production completed.", "生产完成。");
    } else
      notify("Need more gold or an empty adjacent hex",
             "金币不足，或城市周围没有空位");
    return;
  }
  if (screen == CityView && (a == 100 || a == 101)) {
    city_tab = a - 100;
    focus = 0;
    redraw();
    return;
  }
  if (screen == Technology && a == 91) {
    if (research(state, 0, slot_tech(focus))) {
      changed("research");
    }
    return;
  }
  if (screen == Technology && a >= 0 && a < TECHS) {
    focus = a;
    redraw();
    return;
  } else if (screen == CityView && a >= 0 && a < (city_tab ? 4 : 6)) {
    inspected_job = city_job(a);
    open(Production);
  } else if (screen == Empire) {
    if (a >= 100 && a <= 102) {
      empire_tab = a - 100;
      empire_page = focus = 0;
      redraw();
      return;
    }
    if (empire_tab < 2) {
      int rows = overview_rows(), count = owned_count(empire_tab == 1);
      if (a == 110 || a == 111) {
        empire_page = std::clamp(empire_page + (a == 110 ? -1 : 1), 0,
                                 std::max(0, (count - 1) / rows));
        focus = empire_page * rows;
        redraw();
        return;
      }
      int id = owned_id(empire_tab == 1, a);
      if (id < 0)
        return;
      if (empire_tab == 1) {
        selected = id;
        selected_city = -1;
        center(state.units[id].tile);
        open(Map);
      } else {
        selected = -1;
        selected_city = id;
        center(state.cities[id].tile);
        open(CityView);
      }
      return;
    }
    if (a < 2 && a >= 0) {
      int p = a + 1;
      bool declare = !war(state, 0, p);
      if (diplomacy(state, 0, p, declare)) {
        changed(declare ? "war" : "peace");
        notify(declare ? "War declared." : "Ten-turn peace treaty.",
               declare ? "已宣战。" : "已签订十回合和平条约。");
      } else
        notify("Treaty active, or nation eliminated",
               "和平条约尚未到期，或文明已覆灭");
    }
    if (a == 2)
      open(Policies);
  } else if (screen == Policies && a >= 0 && a < 3) {
    if (policy(state, 0, a)) {
      sound::play(sound::City);
      changed("policy");
    } else
      notify("Need more culture, or already adopted",
             "文化不足，或已经采用此政策");
  } else if (screen == NewGame) {
    if (a >= 100 && a <= 102) {
      map_size = a - 100;
      redraw();
      return;
    }
    if (a == 0)
      open(Title);
    if (a == 1)
      new_game();
  } else if (screen == Result && a == 0)
    open(NewGame);
  else if (screen == Help && (a == 0 || a == 1)) {
    if (a == 0)
      sound::toggle();
    else
      sound::toggle_music();
    redraw();
  }
}
void sprite(const art::Sprite &im, int x, int y, int w, int h, int dark = 0,
            bool clip = true) {
  map_raster::sprite(x, y, w, h, im.rgb, im.alpha, im.w, im.h, dark,
                     clip ? 40 : 0, clip ? bottom() : HEIGHT);
}
void hex(int x, int y, int radius, uint16_t color, int width = 1) {
  int half = radius * 7 / 8;
  int xx[6] = {x - radius, x - radius / 2, x + radius / 2,
               x + radius, x + radius / 2, x - radius / 2};
  int yy[6] = {y, y - half, y - half, y, y + half, y + half};
  for (int i = 0; i < 6; i++)
    map_raster::stroke(xx[i], yy[i], xx[(i + 1) % 6], yy[(i + 1) % 6], width,
                       color, 40, bottom());
}
void tile_overlay(const art::Sprite &mask, int t, uint16_t color, int opacity) {
  int tw = tile_width(), th = tw * 7 / 8;
  map_raster::overlay(center_x(t) + origin_x() - tw / 2,
                      center_y(t) + origin_y() - th / 2, tw, th, mask.alpha,
                      mask.w, mask.h, color, opacity, 40, bottom());
}
void draw_route(const Movement &m) {
  if (!inside(state, preview_tile) || m.cost[preview_tile] <= 0)
    return;
  int t = preview_tile;
  for (int steps = 0; steps < N && m.previous[t] >= 0; ++steps) {
    int from = m.previous[t], x = center_x(from) + origin_x(),
        y = center_y(from) + origin_y(), x2 = center_x(t) + origin_x(),
        y2 = center_y(t) + origin_y();
    map_raster::stroke(x, y, x2, y2, 4, DARK, 40, bottom());
    map_raster::stroke(x, y, x2, y2, 2, GOLD, 40, bottom());
    if (t == preview_tile) {
      int dx = x2 - x, dy = y2 - y, norm = std::max(abs(dx), abs(dy));
      if (norm) {
        int bx = x2 - dx * 7 / norm, by = y2 - dy * 7 / norm;
        map_raster::stroke(x2, y2, bx - dy * 4 / norm, by + dx * 4 / norm, 2,
                           GOLD, 40, bottom());
        map_raster::stroke(x2, y2, bx + dy * 4 / norm, by - dx * 4 / norm, 2,
                           GOLD, 40, bottom());
      }
    }
    t = from;
  }
}
void draw_map(bool reveal = false) {
  clear(SEA);
  const auto &index = objects();
  const art::Sprite *terrains[] = {
      &art::grassland, &art::plains, &art::desert, &art::hill,
      &art::mountain,  &art::forest, &art::coast,  &art::ocean};
  const art::Sprite *units[] = {&art::settler, &art::warrior, &art::scout,
                                &art::worker,  &art::archer,  &art::swordsman};
  int ox = origin_x(), oy = origin_y(), tw = tile_width(), th = tw * 7 / 8;
  int tiles[N];
  int count = visible_tiles(state, ox, oy, tw, WIDTH, 40, bottom(), tiles);
  const Movement *range =
      !reveal && screen == Map && !dragging && !pinching && selected >= 0
          ? &movement()
          : nullptr;
  // Keep highlights below units; the fallback batches each sprite layer.
  map_raster::begin();
  for (int n = 0; n < count; n++) {
    int t = tiles[n];
    if (!reveal && !(state.tiles[t].seen & 1))
      continue;
    int x = center_x(t) + ox, y = center_y(t) + oy;
    bool visible = reveal || (state.tiles[t].visible & 1);
    const auto &im = *terrains[state.tiles[t].terrain];
    int height = tw * im.h / im.w;
    sprite(im, x - tw / 2, y + th / 2 - height, tw, height,
           screen == CityTiles && index.claims[t] != selected_city ? 175
           : visible                                               ? 0
                                                                   : 130);
  }
  map_raster::flush();
  for (int n = 0; n < count; n++) {
    int t = tiles[n], x = center_x(t) + ox, y = center_y(t) + oy;
    if ((reveal || (state.tiles[t].visible & 1)) &&
        state.tiles[t].improvement == 3) {
      line(x - 5, y - 5, x + 5, y + 5, RED);
      line(x - 5, y + 5, x + 5, y - 5, RED);
    }
    if ((reveal || (state.tiles[t].visible & 1)) &&
        state.tiles[t].improvement == 1)
      for (int k = -8; k < 10; k += 4)
        line(x - 9, y + k / 2, x + 8, y + k / 2 + 6, GOLD);
  }
  if (!reveal)
    for (int n = 0; n < count; ++n) {
      int t = tiles[n], c = index.claims[t];
      if (c < 0 || !(state.tiles[t].visible & 1))
        continue;
      int x = center_x(t) + ox, y = center_y(t) + oy, r = tw / 2, h = r * 7 / 8;
      int xx[6] = {x - r, x - r / 2, x + r / 2, x + r, x + r / 2, x - r / 2};
      int yy[6] = {y, y - h, y - h, y, y + h, y + h};
      for (int d = 0; d < 6; ++d)
        if (index.borders[t] & (1u << d)) {
          int e = (d + 3) % 6, next = (e + 1) % 6;
          map_raster::stroke(xx[e], yy[e], xx[next], yy[next], 2,
                             colors[state.cities[c].owner], 40, bottom());
        }
      if (screen == CityTiles && c == selected_city &&
          t != state.cities[c].tile && state.tiles[t].terrain != Mountain)
        tile_overlay(art::map_highlight, t,
                     (index.work[t] & 2)   ? GOLD
                     : (index.work[t] & 1) ? MINT
                                           : PAPER,
                     (index.work[t] & 2)   ? 170
                     : (index.work[t] & 1) ? 115
                                           : 40);
    }
  if (range) {
    for (int n = 0; n < count; ++n) {
      int t = tiles[n];
      if (!(state.tiles[t].seen & 1))
        continue;
      if (range->cost[t] > 0)
        tile_overlay(art::map_highlight, t,
                     t == preview_tile ? colors[0] : PAPER,
                     t == preview_tile ? 210 : 180);
      else if (range->attacks[t])
        tile_overlay(art::map_highlight, t, RED, 150);
      else if (t == state.units[selected].tile)
        tile_overlay(art::map_highlight, t, GOLD, 85);
    }
    draw_route(*range);
  }
  for (int n = 0; n < count; n++) {
    int t = tiles[n];
    if (!reveal && !(state.tiles[t].visible & 1))
      continue;
    int x = center_x(t) + ox, y = center_y(t) + oy;
    if (state.tiles[t].improvement == 2)
      sprite(art::mine, x - 13, y - 10, 26, 26);
    int c = index.cities[t];
    if (c >= 0)
      sprite(art::city_center, x - tw * 5 / 12, y - tw * 9 / 16, tw * 5 / 6,
             tw * 11 / 12);
    int u = index.units[t];
    if (u >= 0 && screen != CityTiles) {
      auto &im = *units[state.units[u].kind];
      int h = tw * 2 / 3, w = std::max(12, im.w * h / im.h);
      sprite(im, x - w / 2, y - tw * 5 / 12, w, h);
    }
  }
  map_raster::end();
  for (int n = 0; n < count; n++) {
    int t = tiles[n], x = center_x(t) + ox, y = center_y(t) + oy;
    bool seen = reveal || (state.tiles[t].seen & 1),
         visible = reveal || (state.tiles[t].visible & 1);
    if (!seen) {
      hex(x, y, tw / 2 - 1, rgb(29, 51, 75));
      continue;
    }
    if (visible && state.tiles[t].resource && y + 14 >= 40 && y + 14 < bottom())
      studio::disc(x + tw / 4, y + th / 4, 2, GOLD);
    int c = visible ? index.cities[t] : -1;
    if (c >= 0)
      hex(x, y, tw / 2 - 1, colors[state.cities[c].owner]);
    if (range && range->attacks[t])
      tile_overlay(art::map_crosshair, t, RED, 256);
    if (range && t == state.units[selected].tile) {
      hex(x, y, tw / 2 - 1, DARK, 4);
      hex(x, y, tw / 2 - 1, GOLD, 2);
    } else if (t == cursor && screen == Map && !dragging && !pinching) {
      hex(x, y, tw / 2 - 1, DARK, 3);
      hex(x, y, tw / 2 - 1, colors[0], 1);
    }
    int u = visible ? index.units[t] : -1;
    if (screen == CityTiles && index.claims[t] == selected_city) {
      if (index.work[t] & 1)
        icon(art::ui_population, x - 5, y - 5, 10);
      if (index.work[t] & 2)
        hex(x, y, tw / 2 - 2, GOLD, 2);
      if (t == work_tile)
        hex(x, y, tw / 2 - 3, PAPER, 2);
    }
    if (screen != CityTiles && u >= 0 && y + 15 >= 40 && y + 18 < bottom()) {
      auto &v = state.units[u];
      rect(x - 12, y + 15, 24, 3, DARK);
      rect(x - 12, y + 15, 24 * v.hp / 100, 3, colors[v.owner]);
      if (v.owner == 0 && v.moves)
        studio::disc(x + 15, y - 12, 2, MINT);
    }
  }
  if (!dragging && !pinching)
    for (int c = 0; c < CITIES; c++)
      if (state.cities[c].tile >= 0) {
        int t = state.cities[c].tile, x = center_x(t) + ox,
            y = center_y(t) + oy;
        if ((reveal || (state.tiles[t].visible & 1)) && x >= 20 &&
            x < WIDTH - 20 && y >= 56 && y < bottom() - 28) {
          rect(x - 28, y - 28, 56, 12, DARK);
          label(x - 27, y - 28, 54, city_name(c), colors[state.cities[c].owner],
                1, 1);
        }
      }
}
const char *action_name() {
  if (selected_city >= 0 && state.cities[selected_city].tile == cursor)
    return tr("City", "城市");
  if (selected < 0 || state.units[selected].tile < 0)
    return tr("Action", "行动");
  int k = state.units[selected].kind;
  if (state.units[selected].order != Manual)
    return tr("Wake", "唤醒");
  if (state.units[selected].work_left)
    return tr("Stop work", "停止作业");
  if (state.units[selected].fortified)
    return tr("Wake", "唤醒");
  return k == Settler  ? tr("Found", "建城")
         : k == Worker ? tr("Improve", "改良")
                       : tr("Fortify", "驻防");
}
void bar(int x, int y, int w, int value, int maximum, uint16_t color) {
  rect(x, y, w, 3, rgb(33, 51, 73));
  rect(x, y,
       w * std::clamp(value, 0, std::max(1, maximum)) / std::max(1, maximum), 3,
       color);
}
Yield totals() {
  static uint32_t revision = ~0u;
  static Yield cached;
  if (revision == state_revision)
    return cached;
  revision = state_revision;
  Yield out;
  for (int i = 0; i < CITIES; i++)
    if (state.cities[i].tile >= 0 && state.cities[i].owner == 0) {
      auto y = yields(state, i);
      out.food += growth_surplus(state, i);
      out.production += y.production;
      out.gold += y.gold;
      out.science += y.science;
      out.culture += y.culture;
    }
  return cached = out;
}
void minimap() {
  int x = WIDTH - 65, y = bottom() - 43, w = 59, h = 36;
  if (!map_raster::minimap_restore(x - 2, y - 2, w + 4, h + 4,
                                   state_revision)) {
    rect(x - 2, y - 2, w + 4, h + 4, DARK);
    for (int t = 0; t < N; t++)
      if (inside(state, t)) {
        int xx = t % W, yy = t / W;
        int l = x + xx * w / state.width, r = x + (xx + 1) * w / state.width,
            top = y + yy * h / state.height,
            b = y + (yy + 1) * h / state.height;
        uint16_t c = !(state.tiles[t].seen & 1)        ? rgb(22, 36, 53)
                     : state.tiles[t].terrain >= Coast ? rgb(38, 83, 119)
                                                       : rgb(73, 108, 87);
        rect(l, top, std::max(1, r - l), std::max(1, b - top), c);
      }
    map_raster::minimap_capture();
  }
  int cx = x + (cursor % W) * w / state.width,
      cy = y + (cursor / W) * h / state.height;
  outline(cx - 2, cy - 2, 5, 5, PAPER);
}
void hud() {
  rect(0, 0, WIDTH, 39, DARK);
  auto yields = totals();
  char b[128], preview_status[96];
  int part = (WIDTH - 88) / 3;
  icon(art::ui_gold, 7, 4, 12);
  snprintf(b, sizeof(b), "%u +%d", state.civ[0].gold, yields.gold);
  label(22, 4, part - 17, b, GOLD);
  icon(art::ui_science, 7 + part, 4, 12);
  snprintf(b, sizeof(b), "+%d", yields.science);
  label(22 + part, 4, part - 17, b, colors[0]);
  icon(art::ui_culture, 7 + part * 2, 4, 12);
  snprintf(b, sizeof(b), "%u", state.civ[0].culture);
  label(22 + part * 2, 4, part - 17, b, rgb(195, 166, 231));
  box(WIDTH - 78, 3, 34, 18, tr("Civs", "概览"));
  int t = state.civ[0].research;
  snprintf(b, sizeof(b), tr("Turn %u   %s %u/%u", "回合%u  %s %u/%u"),
           state.turn, tech_name(t), state.civ[0].science, tech_info[t].cost);
  label(7, 24, WIDTH - 94, b, PAPER);
  bar(8, 37, WIDTH - 95, state.civ[0].science, tech_info[t].cost, colors[0]);
  snprintf(b, sizeof(b), "%d", turn_log.unread);
  box(WIDTH - 81, 24, 23, 15, turn_log.unread ? b : "!", turn_log.unread > 0,
      GOLD);
  box(WIDTH - 54, 24, 22, 15, "-");
  box(WIDTH - 28, 24, 22, 15, "+");
  rect(0, bottom(), WIDTH, 80, DARK);
  rect(0, bottom(), WIDTH, 1, rgb(64, 88, 115));
  const char *hint = tr("Select a unit or city.", "选择一个单位或城市。");
  if (selected >= 0 && state.units[selected].tile >= 0) {
    auto &u = state.units[selected];
    snprintf(b, sizeof(b), "%s  %s", kind_name(u.kind),
             terrain_name(state.tiles[u.tile].terrain));
    label(8, bottom() + 6, WIDTH - 64, b, PAPER);
    box(WIDTH - 43, bottom() + 4, 37, 16, tr("Details", "详情"));
    snprintf(b, sizeof(b),
             tr("HP %u/100   Movement %u/%u", "生命 %u/100   行动力 %u/%u"),
             u.hp, u.moves, u.kind == Scout ? 3 : 2);
    label(8, bottom() + 22, WIDTH - 16, b, u.moves ? MINT : GREY);
    bar(8, bottom() + 38, 70, u.hp, 100, MINT);
    hint = !u.moves
               ? tr("No movement points remaining.", "本回合行动力已用完。")
               : tr("White: move · hold: route", "白圈可移动，按住预览路线");
    if (u.order == Healing)
      hint = u.acted
                 ? tr("Healing begins after a full rest turn.",
                      "本回合已行动，下回合开始休整。")
                 : tr("Healing until full health.", "休整中，满血后恢复行动。");
    else if (earned_promotions(u) > promotion_count(u))
      hint = tr("Promotion ready: open Details.", "可晋升：打开单位详情。");
    if (u.work_left) {
      snprintf(preview_status, sizeof(preview_status),
               tr("Work: %u turns left", "作业：剩余%u回合"), u.work_left);
      hint = preview_status;
    }
    if (u.moves && inside(state, preview_tile)) {
      int cost = movement().cost[preview_tile];
      if (cost > 0) {
        snprintf(preview_status, sizeof(preview_status),
                 tr("Move %d · %d left", "移动消耗%d，剩余%d"), cost,
                 u.moves - cost);
        if (cost == u.moves) {
          int from = movement().previous[preview_tile];
          if (zone_of_control(state, u.owner, from, preview_tile))
            snprintf(preview_status, sizeof(preview_status), "%s",
                     tr("Enemy control zone: movement ends here",
                        "敌方控制区：到达后行动力耗尽"));
        }
        hint = preview_status;
      }
    }
  } else if (selected_city >= 0) {
    auto &c = state.cities[selected_city];
    snprintf(b, sizeof(b),
             tr("%s   Population %u   Defense %u", "%s   人口%u   城防%u"),
             city_name(selected_city), c.pop, c.hp);
    label(8, bottom() + 6, WIDTH - 16, b);
    if (c.job == NoJob)
      snprintf(
          b, sizeof(b),
          tr("Choose production · Overflow %u", "等待选择生产 · 结转产能%u"),
          c.production);
    else
      snprintf(b, sizeof(b), tr("Producing %s  %u/%u", "生产：%s  %u/%u"),
               kind_name(c.job), c.production, costs[c.job]);
    label(8, bottom() + 22, WIDTH - 16, b, colors[0]);
    hint = tr("Open the city to change production.", "进入城市管理生产。");
  } else {
    auto &t = state.tiles[cursor];
    label(8, bottom() + 6, WIDTH - 16,
          (t.seen & 1) ? terrain_name(t.terrain) : tr("Unexplored", "未探索"));
  }
  label(88, bottom() + 37, WIDTH - 96,
        preview_tile >= 0               ? hint
        : toast && millis() < toast_end ? toast
                                        : hint,
        preview_tile >= 0 || (toast && millis() < toast_end) ? GOLD : GREY);
  int waiting = 0;
  for (const auto &u : state.units)
    waiting += u.tile >= 0 && u.owner == 0 && u.moves && !u.fortified &&
               !u.work_left && u.order == Manual;
  char next[32];
  snprintf(next, sizeof(next), tr("Unit (%d)", "下一单位%d"), waiting);
  bool needs_production = false;
  for (const auto &c : state.cities)
    needs_production |= c.tile >= 0 && c.owner == 0 && c.job == NoJob;
  const char *names[] = {next, action_name(), tr("Technology", "科技"),
                         needs_production ? tr("Production", "选择生产")
                                          : tr("Next turn", "下一回合")};
  for (int i = 0; i < 4; i++)
    box(i * WIDTH / 4 + 2, HEIGHT - 28, WIDTH / 4 - 4, 26, names[i], i == 3,
        colors[0]);
}
void header(const char *title, const char *subtitle = nullptr,
            int title_width = 0) {
  clear(DARK);
  label(10, 8, title_width > 0 ? title_width : WIDTH - 58, title, PAPER, 2);
  if (subtitle)
    label(10, 30, WIDTH - 20, subtitle, GREY);
  box(8, HEIGHT - 27, WIDTH - 16, 25, tr("Back", "返回"));
}
void draw_title() {
  // A separate decorative world never exposes the campaign's unexplored tiles
  // or changes its camera, random generator or saved state.
  clear(DARK);
  const art::Sprite *terrain[] = {
      &art::grassland, &art::plains, &art::desert, &art::hill,
      &art::mountain, &art::forest, &art::coast, &art::ocean};
  static constexpr uint8_t preview[6][10] = {
      {7,7,6,0,0,5,3,4,3,0}, {7,6,0,5,5,0,3,4,1,1},
      {7,7,6,0,5,3,0,1,2,1}, {7,7,6,6,0,0,1,2,2,1},
      {7,6,0,0,5,0,3,1,2,0}, {6,0,0,5,5,3,4,3,0,0}};
  int tw = std::clamp(WIDTH / 6, 40, 64), th = tw * 7 / 8;
  map_raster::begin();
  for (int col = 0; col * tw * 3 / 4 < WIDTH + tw; ++col)
    for (int row = 0; row * th < HEIGHT + th; ++row) {
      const auto &im = *terrain[preview[row % 6][col % 10]];
      int h = tw * im.h / im.w;
      sprite(im, col * tw * 3 / 4 - tw / 2,
             row * th + (col & 1) * th / 2 + th - h - th / 2,
             tw, h, 170, false);
    }
  map_raster::end();
  const auto menu = title_layout();
  int title_x = (WIDTH - 196) / 2, title_y = menu.y - 70;
  icon(art::ui_logo, title_x, title_y, 48);
  label(title_x + 61, title_y + 8, 135, "Unciv", DARK, 4);
  label(title_x + 60, title_y + 7, 135, "Unciv", PAPER, 4);
  const char *labels[] = {started ? tr("Resume", "继续游戏")
                                  : tr("Start new game", "开始新游戏"),
                          started ? tr("Start new game", "开始新游戏")
                                  : tr("How to play", "游戏说明"),
                          tr("How to play", "游戏说明")};
  for (int row = 0; row < menu.count; ++row) {
    int y = menu.y + row * (menu.row_height + menu.gap);
    bool active = focus == row;
    uint16_t fill = active ? rgb(34, 72, 109) : rgb(20, 44, 69);
    studio::card(menu.x, y, menu.width, menu.row_height, fill,
                 active ? PAPER : rgb(80, 108, 134), 4);
    int size = menu.row_height - 12, left = menu.x + 12;
    if (row == menu.count - 1)
      label(left, y + (menu.row_height - 20) / 2, size, "?", PAPER, 2, 1);
    else
      icon(started && row == 0 ? art::ui_resume : art::ui_new,
           left, y + 6, size, PAPER);
    int scale = menu.width >= 220 ? 2 : 1;
    label(menu.x + 48, y + (menu.row_height - scale * 10) / 2,
          menu.width - 58, labels[row], PAPER, scale);
  }
#ifdef UNCIV_APP_VERSION
  label(0, HEIGHT - 16, WIDTH, UNCIV_APP_VERSION, GREY, 1, 1);
#endif
}
void draw_technology() {
  char b[128];
  int current = state.civ[0].research;
  snprintf(b, sizeof(b),
           tr("Current: %s   %u/%u science", "正在研究：%s   %u/%u 科研"),
           tech_name(current), state.civ[0].science, tech_info[current].cost);
  header(tr("Technology tree", "科技树"), b);
  int cw = (WIDTH - 16) / 3, step = grid_step();
  for (int n = 0; n < TECHS; n++) {
    int t = slot_tech(n), x = 8 + n % 3 * cw, y = grid_y() + n / 3 * step;
    bool known = state.civ[0].tech & (1 << t),
         ready = (state.civ[0].tech & tech_info[t].prerequisites) ==
                 tech_info[t].prerequisites;
    uint16_t edge = known ? MINT : current == t ? colors[0] : rgb(65, 84, 106);
    rect(x + 1, y + 1, cw - 4, step - 4,
         known          ? rgb(24, 57, 56)
         : current == t ? rgb(29, 64, 98)
                        : rgb(24, 40, 60));
    rect(x + 1, y + 1, 3, step - 4, edge);
    if (n == focus)
      outline(x, y, cw - 2, step - 2, PAPER);
    label(x + 7, y + 3, cw - 13, tech_name(t),
          known   ? MINT
          : ready ? PAPER
                  : GREY);
    if (known)
      snprintf(b, sizeof(b), "%s", tr("Researched", "已研究"));
    else if (current == t)
      snprintf(b, sizeof(b), "%s", tr("Researching", "研究中"));
    else if (ready)
      snprintf(b, sizeof(b), tr("%u science", "%u 科研"), tech_info[t].cost);
    else {
      int missing = 0;
      for (int k = 0; k < TECHS; k++)
        if ((tech_info[t].prerequisites & (1 << k)) &&
            !(state.civ[0].tech & (1 << k))) {
          missing = k;
          break;
        }
      snprintf(b, sizeof(b), tr("Requires %s", "需要%s"), tech_name(missing));
    }
    // Keep both lines inside compact landscape cards, including Canvas
    // fallback.
    label(x + 7, y + std::min(18, step - 13), cw - 13, b, GREY);
  }
  label(10, HEIGHT - 66, WIDTH - 20, tech_detail(slot_tech(focus)), GREY);
  int t = slot_tech(focus);
  bool known = state.civ[0].tech & (1 << t),
       ready = (state.civ[0].tech & tech_info[t].prerequisites) ==
               tech_info[t].prerequisites;
  snprintf(b, sizeof(b),
           known          ? tr("%s · researched", "%s · 已研究")
           : !ready       ? tr("Prerequisites required", "需要前置科技")
           : current == t ? tr("%s · researching", "%s · 研究中")
                          : tr("Research %s", "研究%s"),
           tech_name(t));
  box(8, HEIGHT - 53, WIDTH - 16, 22, b, !known && ready, colors[0]);
}
void draw_city() {
  if (selected_city < 0 || state.cities[selected_city].owner != 0) {
    screen = Map;
    draw_map();
    hud();
    return;
  }
  auto &c = state.cities[selected_city];
  auto v = yields(state, selected_city);
  char b[128];
  snprintf(b, sizeof(b),
           tr("Population %u  Food %u/%u  Defense %u",
              "人口%u   粮食%u/%u   城防%u"),
           c.pop, c.food, growth_threshold(c.pop), c.hp);
  header(city_name(selected_city), nullptr, WIDTH - 154);
  box(WIDTH - 140, 5, 46, 22, tr("Queue", "队列"));
  box(WIDTH - 88, 5, 46, 22, tr("Growth", "发展"));
  icon(art::ui_population, 10, 30, 11);
  label(25, 30, WIDTH - 35, b, GREY);
  const art::Sprite *stats[] = {&art::ui_food, &art::ui_production,
                                &art::ui_science, &art::ui_gold};
  int amounts[] = {growth_surplus(state, selected_city),
                   production_yield(state, selected_city), v.science, v.gold};
  int stat_width = (WIDTH - 20) / 4;
  for (int n = 0; n < 4; ++n) {
    int x = 10 + n * stat_width;
    icon(*stats[n], x, 44, 12);
    snprintf(b, sizeof(b), "%+d", amounts[n]);
    label(x + 16, 45, stat_width - 18, b, PAPER);
  }
  int remaining = std::max(0, int(costs[c.job]) - int(c.production)),
      turns = (remaining + std::max(1, production_yield(state, selected_city)) -
               1) /
              std::max(1, production_yield(state, selected_city));
  if (c.job == NoJob)
    snprintf(b, sizeof(b),
             tr("Choose production · Overflow %u", "选择生产项目 · 结转产能%u"),
             c.production);
  else
    snprintf(b, sizeof(b), tr("%s  %u/%u   %d turns", "%s  %u/%u   剩余%d回合"),
             kind_name(c.job), c.production, costs[c.job], turns);
  label(10, 62, WIDTH - 20, b, colors[0]);
  bar(10, 77, WIDTH - 20, c.production, costs[c.job], colors[0]);
  box(8, 83, (WIDTH - 20) / 2, 20, tr("Units", "单位"), !city_tab, colors[0]);
  box(WIDTH / 2 + 2, 83, (WIDTH - 20) / 2, 20, tr("Buildings", "建筑"),
      city_tab, colors[0]);
  int cw = (WIDTH - 16) / 2, step = grid_step(), count = city_tab ? 4 : 6;
  for (int n = 0; n < count; n++) {
    int j = city_job(n), x = 8 + n % 2 * cw, y = grid_y() + n / 2 * step;
    bool built = j >= 6 && (c.buildings & (1 << (j - 6))),
         ready = can_produce(state, selected_city, j);
    snprintf(b, sizeof(b),
             built   ? tr("%s · built", "%s · 已建")
             : ready ? "%s · %u"
                     : tr("%s · locked", "%s · 未解锁"),
             kind_name(j), costs[j]);
    box(x + 1, y + 1, cw - 3, step - 3, "", n == focus,
        ready ? colors[0] : GREY);
    uint16_t ink = n == focus ? DARK : ready ? PAPER : GREY;
    if (j < 6) {
      const art::Sprite *units[] = {&art::settler, &art::warrior,
                                    &art::scout,   &art::worker,
                                    &art::archer,  &art::swordsman};
      icon(*units[j], x + 7, y + (step - 20) / 2, 20);
    } else {
      const art::Sprite *buildings[] = {&art::ui_monument, &art::ui_granary,
                                        &art::ui_science, &art::ui_barracks};
      // Library uses the original science symbol; the others have building art.
      icon(*buildings[j - 6], x + 7, y + (step - 18) / 2, 18, j == 8 ? 0 : ink);
    }
    label(x + 32, y + (step - 10) / 2, cw - 37, b, ink);
    if (c.job == j)
      rect(x + 3, y + 3, 3, step - 7, MINT);
  }
  label(10, HEIGHT - 39, WIDTH - 20,
        tr("Select a project to inspect it.", "选择项目查看效果与生产条件。"),
        GREY);
  box(8, HEIGHT - 27, WIDTH / 2 - 12, 25, tr("Back", "返回"));
  if (c.job == NoJob)
    snprintf(b, sizeof(b), "%s", tr("Choose production", "选择生产"));
  else
    snprintf(b, sizeof(b), tr("Buy: %d gold", "购买：%d 金币"),
             purchase_cost(state, selected_city));
  box(WIDTH / 2 + 3, HEIGHT - 27, WIDTH / 2 - 11, 25, b, false, colors[0]);
}
void draw_production() {
  if (selected_city < 0 || state.cities[selected_city].owner != 0) {
    open(Map);
    return;
  }
  auto &c = state.cities[selected_city];
  int j = inspected_job, tech = job_prerequisite(j);
  bool built = j >= KINDS && (c.buildings & (1 << (j - KINDS))),
       ready = can_produce(state, selected_city, j) && !built;
  char b[128];
  snprintf(b, sizeof(b), tr("Current: %s  %u/%u", "当前：%s  %u/%u"),
           kind_name(c.job), c.production, costs[c.job]);
  header(kind_name(j), b);
  const art::Sprite *images[] = {
      &art::settler,    &art::warrior,    &art::scout,       &art::worker,
      &art::archer,     &art::swordsman,  &art::ui_monument, &art::ui_granary,
      &art::ui_science, &art::ui_barracks};
  icon(*images[j], 13, 53, 43, j >= KINDS && j != Library ? PAPER : 0);
  snprintf(b, sizeof(b), tr("Progress: %d/%u", "已有进度：%d/%u"),
           construction_progress(state, selected_city, j), costs[j]);
  label(70, 54, WIDTH - 80, b);
  int remaining = costs[j] - construction_progress(state, selected_city, j),
      output = std::max(1, production_yield(state, selected_city, j));
  snprintf(b, sizeof(b), tr("Estimated: %d turns", "预计回合：%d"),
           std::max(0, (remaining + output - 1) / output));
  label(70, 71, WIDTH - 80, b, colors[0]);
  snprintf(b, sizeof(b), "%s",
           built      ? tr("Already built", "已经建造")
           : tech < 0 ? tr("Available", "无需前置科技")
                      : tech_name(tech));
  label(70, 88, WIDTH - 80, b, ready ? MINT : GREY);
  const char *en[][2] = {
      {"Needs population 2; excess food adds production.",
       "Founds a city 4 hexes away; consumes the unit."},
      {"Melee strength 10; movement 2.",
       "Can capture cities; can fortify and heal."},
      {"Strength 6; movement 3; vision 3.",
       "Hills and forests cost 1 movement."},
      {"Builds farms and mines near own cities.",
       "Farms: 3 turns; mines / forests: 4."},
      {"Ranged strength 12; range 2.",
       "Requires Archery; cannot capture cities."},
      {"Melee strength 17; movement 2.",
       "Requires Iron Working; can capture cities."},
      {"Culture +2 per turn.", "One per city."},
      {"Food +2 per turn.", "Requires Pottery; one per city."},
      {"Science +2, plus population / 2.", "Requires Writing; one per city."},
      {"City strength +4; new military units gain 15 XP.",
       "Requires Bronze Working; one per city."}};
  const char *zh[][2] = {
      {"需要人口2；粮食盈余转为产能。", "建城需距其他城市四格；消耗开拓者。"},
      {"近战基础战力10，行动力2。", "可占领城市，可驻防恢复生命。"},
      {"基础战力6，行动力3，视野3格。", "经过丘陵和森林仅消耗1点行动力。"},
      {"在己方城市周围修建农田和矿山。", "农田3回合；矿山和森林作业4回合。"},
      {"远程基础战力12，射程2格。", "需要弓术；无法占领城市。"},
      {"近战基础战力17，行动力2。", "需要铁器；可占领城市。"},
      {"每回合文化+2。", "每座城市只能建造一次。"},
      {"每回合粮食+2。", "需要制陶；每座城市只能建造一次。"},
      {"每回合科研+2，额外增加人口数/2。", "需要文字；每座城市只能建造一次。"},
      {"城防战力+4；军事单位初始经验15。", "需要青铜冶炼；每城只能建造一次。"}};
  for (int n = 0; n < 2; ++n)
    label(10, 111 + n * 17, WIDTH - 20, tr(en[j][n], zh[j][n]), PAPER);
  if (HEIGHT >= 260)
    label(10, HEIGHT - 96, WIDTH - 20,
          tr("Switching projects preserves their progress.",
             "切换项目会保留各自的生产进度。"),
          GREY);
  box(8, HEIGHT - 79, WIDTH - 16, 22,
      tr("Add to production queue", "加入生产队列"), false,
      ready ? colors[0] : GREY);
  if (built)
    snprintf(b, sizeof(b), "%s", tr("Already built", "已经建造"));
  else if (!ready && j == Settler && c.pop < 2)
    snprintf(b, sizeof(b), "%s", tr("Requires population 2", "需要人口2"));
  else if (!ready)
    snprintf(b, sizeof(b), tr("Requires %s", "需要%s"), tech_name(tech));
  else
    snprintf(b, sizeof(b), "%s",
             j == c.job ? tr("Keep current production", "保持当前生产")
                        : tr("Start production", "开始生产"));
  box(8, HEIGHT - 53, WIDTH - 16, 22, b, ready, ready ? colors[0] : GREY);
}
void draw_queue() {
  if (selected_city < 0 || state.cities[selected_city].owner != 0) {
    open(Map);
    return;
  }
  const auto &c = state.cities[selected_city];
  char b[128];
  snprintf(b, sizeof(b), tr("Current: %s %u/%u", "当前：%s %u/%u"),
           kind_name(c.job), c.production, costs[c.job]);
  if (c.job == NoJob)
    snprintf(b, sizeof(b), "%s",
             tr("No current production", "当前没有生产项目"));
  header(tr("Production queue", "生产队列"), b);
  int rows = queue_rows(), pages = std::max(1, (c.queued + rows - 1) / rows);
  queue_page = std::clamp(queue_page, 0, pages - 1);
  if (!c.queued)
    label(10, 62, WIDTH - 20,
          tr("Add projects from the production list.",
             "从生产详情页添加后续项目。"),
          GREY);
  for (int n = 0; n < rows; ++n) {
    int i = queue_page * rows + n;
    if (i >= c.queued)
      break;
    int job = c.queue[i];
    int progress = job == c.job ? 0 : c.invested[job];
    for (int previous = 0; previous < i; ++previous)
      if (c.queue[previous] == job)
        progress = 0;
    snprintf(b, sizeof(b), "%d. %s  %d/%u", i + 1, kind_name(job), progress,
             costs[job]);
    box(8, 50 + n * 28, WIDTH - 16, 25, b, focus == i, colors[0]);
  }
  snprintf(b, sizeof(b), tr("Previous · %d/%d", "上一页 · %d/%d"),
           queue_page + 1, pages);
  box(8, HEIGHT - 107, WIDTH / 2 - 12, 22, b);
  box(WIDTH / 2 + 3, HEIGHT - 107, WIDTH / 2 - 11, 22,
      tr("Next page", "下一页"));
  const char *buttons[] = {tr("Move up", "上移"), tr("Move down", "下移"),
                           tr("Remove", "移除")};
  for (int n = 0; n < 3; ++n)
    box(n * WIDTH / 3 + 3, HEIGHT - 81, WIDTH / 3 - 6, 22, buttons[n]);
  snprintf(b, sizeof(b), tr("Repeat current unit: %s", "连续生产当前单位：%s"),
           c.repeat ? tr("On", "开") : tr("Off", "关"));
  box(8, HEIGHT - 53, WIDTH - 16, 22, b, c.repeat,
      c.job < KINDS && !c.queued ? colors[0] : GREY);
}
void draw_battle() {
  auto p = battle_preview(state, selected, battle_target);
  header(tr("Battle preview", "战斗预览"));
  if (!p.possible) {
    label(10, 54, WIDTH - 20, tr("Target unavailable", "目标已不可攻击"), GREY);
    return;
  }
  char b[112];
  int c = city_at(state, battle_target), v = unit_at(state, battle_target);
  int half = WIDTH / 2;
  label(10, 38, half - 16, kind_name(state.units[selected].kind), colors[0]);
  label(half + 6, 38, half - 16,
        p.city ? city_name(c) : kind_name(state.units[v].kind), RED);
  snprintf(b, sizeof(b), tr("Strength %d", "战斗力 %d"), p.attacker_strength);
  label(10, 58, half - 16, b);
  snprintf(b, sizeof(b), tr("Strength %d", "战斗力 %d"), p.defender_strength);
  label(half + 6, 58, half - 16, b);
  snprintf(b, sizeof(b), tr("HP %d -> %d", "生命 %d → %d"), p.attacker_hp,
           p.attacker_after);
  label(10, 82, half - 16, b, p.attacker_after ? MINT : RED);
  snprintf(b, sizeof(b), tr("HP %d -> %d", "生命 %d → %d"), p.defender_hp,
           p.defender_after);
  label(half + 6, 82, half - 16, b, RED);
  bar(10, 101, half - 20, p.attacker_hp, 100, GREY);
  bar(10, 101, half - 20, p.attacker_after, 100, MINT);
  bar(half + 6, 101, half - 20, p.defender_hp, 100, GREY);
  bar(half + 6, 101, half - 20, p.defender_after, 100, RED);
  label(10, 119, WIDTH - 20,
        p.capture           ? tr("City will be captured", "将占领城市")
        : !p.attacker_after ? tr("Your unit will be lost", "己方单位将战败")
        : !p.defender_after
            ? tr("Enemy unit will be defeated", "将击败敌方单位")
            : tr("Both sides survive", "双方单位仍将存活"),
        p.attacker_after ? GOLD : RED);
  label(10, 139, WIDTH - 20,
        p.ranged ? tr("Ranged: no retaliation; cannot capture",
                      "远程攻击不受反击，无法占领城市")
                 : tr("Melee: the defender retaliates", "近战攻击会受到反击"),
        GREY);
  label(10, 157, WIDTH - 20,
        tr("Wounds, terrain and fortification affect strength",
           "伤势、地形和驻防影响战斗力"),
        GREY);
  box(8, HEIGHT - 53, WIDTH - 16, 22, tr("Attack", "攻击"), true, RED);
}
void draw_economy() {
  if (selected_city < 0 || state.cities[selected_city].owner != 0) {
    open(Map);
    return;
  }
  const auto &c = state.cities[selected_city];
  auto y = yields(state, selected_city);
  header(city_name(selected_city), tr("Citizen focus", "市民工作重心"));
  const char *en[] = {"Balanced", "Food", "Production", "Gold"};
  const char *zh[] = {"均衡", "粮食", "生产", "金币"};
  for (int n = 0; n < FOCUSES; ++n)
    box(n * WIDTH / 4 + 3, 49, WIDTH / 4 - 6, 26, tr(en[n], zh[n]),
        c.focus == n, colors[0]);
  char b[128];
  int net = growth_surplus(state, selected_city),
      needed = growth_threshold(c.pop);
  snprintf(b, sizeof(b), tr("Population %u   Food %u/%d", "人口%u   粮食%u/%d"),
           c.pop, c.food, needed);
  label(10, 86, WIDTH - 20, b);
  bar(10, 102, WIDTH - 20, c.food, needed, MINT);
  snprintf(b, sizeof(b),
           tr("Food %d - consumption %u = %+d", "粮食%d - 人口消耗%u = %+d"),
           y.food, food_consumption(state, selected_city),
           y.food - food_consumption(state, selected_city));
  label(10, 112, WIDTH - 20, b, net < 0 ? RED : MINT);
  if (net > 0)
    snprintf(b, sizeof(b), tr("Growth in %d turns", "预计%d回合后人口增长"),
             std::max(1, (needed - int(c.food) + net - 1) / net));
  else
    snprintf(b, sizeof(b), "%s",
             net < 0 ? tr("Starvation reduces population when food runs out",
                          "粮食耗尽后将减少人口")
             : c.job == Settler && c.pop >= 2
                 ? tr("Growth paused; surplus food trains Settler",
                      "暂停增长，粮食盈余用于训练开拓者")
                 : tr("Population is stable", "人口暂不增长"));
  label(10, 130, WIDTH - 20, b, net < 0 ? RED : GREY);
  int16_t tiles[18];
  int count = worked_tiles(state, selected_city, tiles);
  snprintf(b, sizeof(b),
           tr("Working tiles %d/%u   Production %d   Gold %d",
              "工作地块%d/%u   产能%d   金币%d"),
           count, c.pop, production_yield(state, selected_city), y.gold);
  label(10, 151, WIDTH - 20, b, colors[0]);
  if (HEIGHT > 250)
    label(10, 169, WIDTH - 20,
          tr("Focus assigns citizens to nearby tiles.",
             "重心会自动调整市民工作的地块。"),
          GREY);
  if (HEIGHT > 250)
    label(
        10, 185, WIDTH - 20,
        tr("A tile can supply only one city.", "每块地仅为一座城市提供产出。"),
        GREY);
  box(8, HEIGHT - 53, WIDTH - 16, 22, tr("Manage worked tiles", "管理工作地块"),
      false, colors[0]);
}
const char *promotion_name(int p, const Unit &u) {
  if (p == OpenCombat)
    return u.kind == Archer ? tr("Accuracy", "精准") : tr("Shock", "冲击");
  if (p == RoughCombat)
    return u.kind == Archer ? tr("Barrage", "弹幕") : tr("Drill", "操练");
  return p == Medic ? tr("Medic", "医疗") : tr("Heal instantly", "即时恢复");
}
bool selected_unit() {
  return selected >= 0 && state.units[selected].tile >= 0 &&
         state.units[selected].owner == 0;
}
void draw_unit() {
  if (!selected_unit()) {
    open(Map);
    return;
  }
  const auto &u = state.units[selected];
  char b[128];
  snprintf(b, sizeof(b),
           tr("HP %u/100   Movement %u/%u", "生命%u/100   行动力%u/%u"), u.hp,
           u.moves, u.kind == Scout ? 3 : 2);
  header(kind_name(u.kind), b);
  snprintf(b, sizeof(b),
           tr("XP %u/%d   Promotions available %d", "经验%u/%d   可晋升%d"),
           u.xp, next_promotion_xp(u),
           earned_promotions(u) - promotion_count(u));
  if (promotion_count(u) >= 4)
    snprintf(b, sizeof(b),
             tr("XP %u · Promotion limit reached", "经验%u · 晋升次数已满"),
             u.xp);
  label(10, 50, WIDTH - 20, military(u) ? b : tr("Civilian unit", "平民单位"),
        colors[0]);
  snprintf(b, sizeof(b),
           tr("Open %u   Rough %u   Medic %s", "开阔%u   崎岖%u   医疗%s"),
           u.promotions & 3, (u.promotions >> 2) & 3,
           u.promotions & 16 ? "+8" : "0");
  label(10, 68, WIDTH - 20,
        military(u) ? b
        : u.kind == Worker
            ? tr("Build improvements near own cities", "在己方城市周围修建改良")
            : tr("Founds a city at least 4 hexes away",
                 "建城需距其他城市至少四格"),
        GREY);
  int half = WIDTH / 2;
  box(8, 88, half - 12, 26,
      u.fortified ? tr("Wake", "唤醒") : tr("Fortify", "驻防"), focus == 0);
  box(half + 3, 88, half - 11, 26,
      u.order == Healing ? tr("Stop healing", "停止休整")
                         : tr("Heal until full", "休整至满血"),
      focus == 1);
  box(8, 120, half - 12, 26, tr("Wait this turn", "本回合待命"), focus == 2);
  box(half + 3, 120, half - 11, 26, tr("Promote", "选择晋升"), focus == 3,
      earned_promotions(u) > promotion_count(u) ? GOLD : colors[0]);
  int kind = upgrade_kind(state, selected);
  if (kind >= 0)
    snprintf(b, sizeof(b), tr("%s · %d gold", "%s · %d金币"), kind_name(kind),
             upgrade_cost(state, selected));
  else
    snprintf(b, sizeof(b), "%s", tr("No upgrade", "暂无升级"));
  box(8, 155, half - 12, 26, b, focus == 4, kind >= 0 ? colors[0] : GREY);
  box(half + 3, 155, half - 11, 26, tr("Pillage", "掠夺改良"), focus == 5,
      can_pillage(state, selected) ? GOLD : GREY);
  snprintf(b, sizeof(b),
           tr("Rest +%d HP   Fortify +%d%%", "休整+%d生命  驻防+%d%%"),
           healing_rate(state, selected), fortification_bonus(u));
  label(10, 188, WIDTH - 20, b, GREY);
}
void draw_pillage() {
  header(tr("Pillage improvement", "掠夺改良"));
  if (!selected_unit() || !can_pillage(state, selected)) {
    label(10, 54, WIDTH - 20,
          tr("No enemy or neutral improvement here",
             "此处没有可掠夺的敌方或中立改良"),
          GREY);
    return;
  }
  const auto &u = state.units[selected];
  char b[100];
  snprintf(b, sizeof(b), tr("Gold %u -> %u", "金币%u → %u"), state.civ[0].gold,
           unsigned(std::min(60000, int(state.civ[0].gold) + 25)));
  label(10, 52, WIDTH - 20, b, GOLD);
  snprintf(b, sizeof(b),
           tr("HP %u -> %u   Movement %u -> %u", "生命%u → %u  行动力%u → %u"),
           u.hp, unsigned(std::min(100, int(u.hp) + 25)), u.moves, u.moves - 1);
  label(10, 77, WIDTH - 20, b, MINT);
  label(10, 108, WIDTH - 20,
        tr("The improvement stops providing its bonus.",
           "改良受损后不再提供额外产出。"),
        PAPER);
  label(10, 129, WIDTH - 20,
        tr("Workers can repair it in 2 turns.", "工人可用2回合修复。"), GREY);
  box(8, HEIGHT - 53, WIDTH - 16, 22, tr("Pillage", "确认掠夺"), true, RED);
}
void draw_promotions() {
  if (!selected_unit()) {
    open(Map);
    return;
  }
  const auto &u = state.units[selected];
  char b[112];
  snprintf(b, sizeof(b), tr("XP %u   Available %d", "经验%u   可选晋升%d"),
           u.xp, earned_promotions(u) - promotion_count(u));
  header(tr("Promotion", "单位晋升"), b);
  const char *en[] = {"Open: +15% strength", "Rough: +15% strength",
                      "Heal +8 HP/turn", "Heal 50 HP, once"};
  const char *zh[] = {"开阔战力+15%", "崎岖战力+15%", "休整恢复+8",
                      "50生命，仅一次"};
  int step = std::clamp((HEIGHT - 130) / 2, 46, 62), half = WIDTH / 2;
  for (int p = 0; p < PROMOTIONS; ++p) {
    int x = 8 + p % 2 * half, y = 54 + p / 2 * step;
    bool ready = can_promote(state, selected, p);
    box(x, y, half - 16, step - 3, "", focus == p, ready ? colors[0] : GREY);
    uint16_t ink = focus == p ? DARK : ready ? PAPER : GREY;
    int rank = p == OpenCombat    ? (u.promotions & 3)
               : p == RoughCombat ? ((u.promotions >> 2) & 3)
                                  : -1;
    if (rank >= 0)
      snprintf(b, sizeof(b), "%s %d/3", promotion_name(p, u), rank);
    else
      snprintf(b, sizeof(b), "%s", promotion_name(p, u));
    label(x + 6, y + 5, half - 28, b, ink);
    label(x + 6, y + 23, half - 28, tr(en[p], zh[p]), ink);
  }
  label(10, HEIGHT - 69, WIDTH - 20,
        tr("Each choice consumes one earned promotion.",
           "每次选择消耗一个晋升机会。"),
        GREY);
  bool ready = can_promote(state, selected, focus);
  box(8, HEIGHT - 53, WIDTH - 16, 22, tr("Confirm promotion", "确认晋升"),
      ready, ready ? colors[0] : GREY);
}
void draw_upgrade() {
  if (!selected_unit()) {
    open(Map);
    return;
  }
  const auto &u = state.units[selected];
  int kind = upgrade_kind(state, selected),
      cost = upgrade_cost(state, selected);
  char b[112];
  header(tr("Upgrade unit", "升级单位"));
  if (kind < 0) {
    label(10, 54, WIDTH - 20,
          tr("This unit has no upgrade route.", "该单位暂无升级路线。"), GREY);
    return;
  }
  snprintf(b, sizeof(b), "%s → %s", kind_name(u.kind), kind_name(kind));
  label(10, 42, WIDTH - 20, b, colors[0], 2);
  snprintf(b, sizeof(b),
           tr("Cost %d gold · Treasury %u", "花费%d金币 · 当前%u"), cost,
           state.civ[0].gold);
  label(10, 75, WIDTH - 20, b, GOLD);
  label(10, 97, WIDTH - 20,
        tr("Health, XP and promotions are retained.", "保留生命、经验和晋升。"),
        PAPER);
  int city = working_city(state, u.tile);
  const char *reason =
      !(state.civ[0].tech & (1 << Iron))
          ? tr("Requires Iron Working", "需要研究铁器")
      : city < 0 || state.cities[city].owner != 0
          ? tr("Requires own territory", "需要位于己方领土")
      : u.moves != 2 ? tr("Requires full movement", "需要完整行动力")
      : state.civ[0].gold < cost
          ? tr("Not enough gold", "金币不足")
          : tr("Consumes all movement this turn", "升级后本回合无法行动");
  label(10, 123, WIDTH - 20, reason,
        can_upgrade(state, selected) ? MINT : GREY);
  box(8, HEIGHT - 53, WIDTH - 16, 22, tr("Confirm upgrade", "确认升级"),
      can_upgrade(state, selected),
      can_upgrade(state, selected) ? colors[0] : GREY);
}
void draw_city_tiles() {
  if (selected_city < 0 || state.cities[selected_city].owner != 0) {
    open(Map);
    return;
  }
  draw_map();
  rect(0, 0, WIDTH, 40, DARK);
  label(8, 5, WIDTH - 52, city_name(selected_city), PAPER, 2);
  const auto &index = objects();
  int assigned = 0, locked = 0;
  for (int t = 0; t < N; ++t)
    if (index.claims[t] == selected_city) {
      assigned += bool(index.work[t] & 1);
      locked += bool(index.work[t] & 2);
    }
  char b[128];
  snprintf(
      b, sizeof(b),
      tr("Population %u · Working %d · Locked %d", "人口%u · 工作%d · 锁定%d"),
      state.cities[selected_city].pop, assigned, locked);
  label(8, 26, WIDTH - 16, b, MINT);
  rect(0, bottom(), WIDTH, 80, DARK);
  int tile =
      inside(state, work_tile) ? work_tile : state.cities[selected_city].tile;
  bool can_work = index.claims[tile] == selected_city &&
                  tile != state.cities[selected_city].tile &&
                  state.tiles[tile].terrain != Mountain;
  const char *status = !can_work ? tr("Cannot assign", "无法分配")
                       : tile_locked(state, selected_city, tile)
                           ? tr("Locked", "已锁定")
                       : index.work[tile] ? tr("Working", "工作中")
                                          : tr("Unworked", "未工作");
  snprintf(b, sizeof(b), "%s · %s", terrain_name(state.tiles[tile].terrain),
           status);
  label(8, bottom() + 6, WIDTH - 16, b, PAPER);
  auto y = tile == state.cities[selected_city].tile ? Yield{2, 2, 2, 0, 0}
                                                    : tile_yield(state, tile);
  snprintf(b, sizeof(b),
           tr("Food %d   Production %d   Gold %d", "粮食%d   产能%d   金币%d"),
           y.food, y.production, y.gold);
  label(8, bottom() + 23, WIDTH - 16, b, colors[0]);
  label(8, bottom() + 40, WIDTH - 16,
        toast && millis() < toast_end
            ? toast
            : tr("Green: working · Gold: locked", "绿色工作中，金色已锁定"),
        toast ? GOLD : GREY);
  box(2, HEIGHT - 28, WIDTH / 3 - 4, 26, tr("Automatic", "自动分配"));
  box(WIDTH / 3 + 2, HEIGHT - 28, WIDTH / 3 - 4, 26,
      tile_locked(state, selected_city, tile) ? tr("Unlock", "解除锁定")
                                              : tr("Lock tile", "锁定地块"),
      can_work, can_work ? colors[0] : GREY);
  box(WIDTH * 2 / 3 + 2, HEIGHT - 28, WIDTH / 3 - 4, 26, tr("Back", "返回"));
}
void draw_purchase() {
  if (selected_city < 0 || state.cities[selected_city].owner != 0) {
    open(Map);
    return;
  }
  const auto &c = state.cities[selected_city];
  int cost = purchase_cost(state, selected_city);
  header(tr("Purchase production", "购买生产"), city_name(selected_city));
  char b[112];
  label(10, 54, WIDTH - 20, kind_name(c.job), colors[0], 2);
  snprintf(b, sizeof(b), tr("Current production %u/%u", "当前产能 %u/%u"),
           c.production, costs[c.job]);
  label(10, 84, WIDTH - 20, b);
  snprintf(b, sizeof(b), tr("Cost %d gold", "花费 %d 金币"), cost);
  label(10, 106, WIDTH - 20, b, GOLD);
  if (state.civ[0].gold >= cost)
    snprintf(b, sizeof(b), tr("Treasury %u -> %d", "金币 %u → %d"),
             state.civ[0].gold, int(state.civ[0].gold) - cost);
  else
    snprintf(b, sizeof(b),
             tr("Treasury %u · need %d more", "当前金币%u，还缺%d"),
             state.civ[0].gold, cost - int(state.civ[0].gold));
  label(10, 126, WIDTH - 20, b, state.civ[0].gold >= cost ? MINT : RED);
  label(10, 148, WIDTH - 20,
        tr("Completes the current project immediately.",
           "立即完成当前生产项目。"),
        GREY);
  box(8, HEIGHT - 53, WIDTH - 16, 22,
      state.civ[0].gold >= cost ? tr("Confirm purchase", "确认购买")
                                : tr("Not enough gold", "金币不足"),
      state.civ[0].gold >= cost, colors[0]);
}
void draw_journal() {
  header(tr("Notifications", "回合通知"),
         tr("Tap an event to locate it", "点击事件查看对应位置"));
  char b[128];
  int rows = journal_rows(),
      pages = std::max(1, (turn_log.count + rows - 1) / rows);
  journal_page = std::clamp(journal_page, 0, pages - 1);
  if (!turn_log.count)
    label(10, 62, WIDTH - 20, tr("No events this session", "本次游戏暂无事件"),
          GREY);
  for (int n = 0; n < rows; ++n) {
    int i = journal_page * rows + n;
    if (i >= turn_log.count)
      break;
    auto e = turn_log.entries[i];
    int y = 44 + n * 34;
    box(8, y, WIDTH - 16, 31, "", focus == i);
    uint16_t ink = focus == i ? DARK : PAPER;
    int city = city_at(state, e.tile);
    const char *name =
        e.type == TurnLog::Research ? tech_name(e.value)
        : e.type == TurnLog::Produced || e.type == TurnLog::BuildingDone ||
                e.type == TurnLog::UnitLost || e.type == TurnLog::PromotionReady
            ? kind_name(e.value)
        : e.type == TurnLog::Improved || e.type == TurnLog::Pillaged
            ? (e.value == 2 ? tr("Mine", "矿山") : tr("Farm", "农田"))
        : city >= 0 ? city_name(city)
                    : terrain_name(state.tiles[e.tile].terrain);
    const char *en[] = {
        "Research completed",  "Unit trained",        "Building completed",
        "Population grew",     "Starvation",          "Improvement completed",
        "Unit lost",           "City lost",           "City under attack",
        "Promotion available", "Improvement pillaged"};
    const char *zh[] = {"研究完成",   "单位训练完成", "建筑完成",
                        "人口增长",   "饥荒",         "改良完成",
                        "单位战败",   "城市失守",     "城市遭到攻击",
                        "单位可晋升", "改良遭到掠夺"};
    snprintf(b, sizeof(b), tr("Turn %u · %s", "回合%u · %s"), e.turn,
             tr(en[e.type], zh[e.type]));
    label(14, y + 3, WIDTH - 28, b, ink);
    snprintf(b, sizeof(b), e.tile < 0 ? "%s" : "%s · %d,%d", name,
             e.tile % W + 1, e.tile / W + 1);
    label(14, y + 17, WIDTH - 28, b, ink);
  }
  box(8, HEIGHT - 53, WIDTH / 3 - 8, 22, tr("Previous", "上一页"));
  snprintf(b, sizeof(b), "%d / %d", journal_page + 1, pages);
  label(WIDTH / 3, HEIGHT - 47, WIDTH / 3, b, GREY, 1, 1);
  box(WIDTH * 2 / 3, HEIGHT - 53, WIDTH / 3 - 8, 22, tr("Next", "下一页"));
}
void draw_empire() {
  char b[100];
  snprintf(b, sizeof(b),
           tr("Score %d · treaties last ten turns",
              "文明得分 %d · 和平条约持续十回合"),
           score(state, 0));
  header(tr("Civilization overview", "文明概览"), b,
         empire_tab == 1 ? WIDTH - 106 : 0);
  const char *tabs[] = {tr("Cities", "城市"), tr("Units", "单位"),
                        tr("Diplomacy", "外交")};
  int tabw = (WIDTH - 16) / 3;
  for (int n = 0; n < 3; ++n)
    box(8 + n * tabw, 42, tabw - 2, 21, tabs[n], n == empire_tab);
  if (empire_tab == 1)
    box(WIDTH - 88, 5, 46, 22, tr("Details", "详情"));
  if (empire_tab < 2) {
    bool units = empire_tab == 1;
    int count = owned_count(units), rows = overview_rows(),
        pages = std::max(1, (count + rows - 1) / rows);
    empire_page = std::clamp(empire_page, 0, pages - 1);
    for (int n = 0; n < rows; ++n) {
      int index = empire_page * rows + n, id = owned_id(units, index),
          y = 68 + n * 32;
      if (id < 0)
        break;
      box(8, y, WIDTH - 16, 29, "", index == focus);
      uint16_t ink = index == focus ? DARK : PAPER;
      if (units) {
        auto &u = state.units[id];
        snprintf(b, sizeof(b), "%s  %d,%d", kind_name(u.kind), u.tile % W + 1,
                 u.tile / W + 1);
        label(15, y + 3, WIDTH - 30, b, ink);
        snprintf(b, sizeof(b),
                 tr("HP %u  Moves %u/%u%s", "生命%u  行动%u/%u%s"), u.hp,
                 u.moves, u.kind == Scout ? 3 : 2,
                 u.work_left   ? tr("  Working", "  作业中")
                 : u.fortified ? tr("  Fortified", "  驻防")
                               : "");
      } else {
        auto &c = state.cities[id];
        snprintf(b, sizeof(b), tr("%s  Population %u", "%s  人口%u"),
                 city_name(id), c.pop);
        label(15, y + 3, WIDTH - 30, b, ink);
        int output = std::max(1, production_yield(state, id));
        snprintf(b, sizeof(b),
                 tr("%s  %u/%u · %d turns", "%s  %u/%u · 剩余%d回合"),
                 kind_name(c.job), c.production, costs[c.job],
                 std::max(0, (int(costs[c.job]) - c.production + output - 1) /
                                 output));
      }
      label(15, y + 16, WIDTH - 30, b, ink);
    }
    if (!count)
      label(10, 78, WIDTH - 20,
            units ? tr("No units", "暂无单位")
                  : tr("No cities founded", "尚未建立城市"),
            GREY, 1, 1);
    int w = (WIDTH - 20) / 3;
    box(8, HEIGHT - 53, w, 22, tr("Previous", "上一页"));
    snprintf(b, sizeof(b), "%d / %d", empire_page + 1, pages);
    label(10 + w, HEIGHT - 47, w, b, GREY, 1, 1);
    box(WIDTH - 8 - w, HEIGHT - 53, w, 22, tr("Next", "下一页"));
    return;
  }
  for (int o = 1; o < 3; o++) {
    int y = 70 + (o - 1) * 44;
    rect(8, y, WIDTH - 16, 40, rgb(29, 48, 60));
    snprintf(b, sizeof(b), "%s · %s", nation(o),
             !alive(state, o)   ? tr("Eliminated", "已覆灭")
             : war(state, 0, o) ? tr("At war", "战争")
                                : tr("Peace", "和平"));
    label(14, y + 5, WIDTH - 28, b, colors[o]);
    snprintf(b, sizeof(b), tr("Score %d · tap to %s", "得分 %d · 点击%s"),
             score(state, o),
             war(state, 0, o) ? tr("make peace", "议和")
                              : tr("declare war", "宣战"));
    label(14, y + 22, WIDTH - 28, b, GREY);
    if (focus == o - 1)
      outline(8, y, WIDTH - 16, 40, GOLD);
  }
  box(8, 165, WIDTH - 16, 25, tr("Social policies", "社会政策"), focus == 2);
}
void draw_policies() {
  char b[90];
  snprintf(b, sizeof(b),
           tr("Culture %u · adoption costs %d", "文化 %u · 采用政策需要 %d"),
           state.civ[0].culture, policy_cost(state, 0));
  header(tr("SOCIAL POLICIES", "社会政策"), b);
  const char *en[] = {"Tradition · +1 food / city",
                      "Honor · +1 production, +2 strength",
                      "Rationalism · +2 science / city"};
  const char *zh[] = {"传统 · 每城粮食 +1", "荣誉 · 产能 +1，战力 +2",
                      "理性 · 每城科研 +2"};
  for (int i = 0; i < 3; i++) {
    bool adopted = state.civ[0].policies & (1 << i);
    box(8, 48 + i * 36, WIDTH - 16, 32, tr(en[i], zh[i]), i == focus,
        adopted ? MINT : colors[0]);
    if (adopted)
      label(WIDTH - 45, 58 + i * 36, 30, tr("YES", "已用"), GOLD);
  }
}
void draw_help() {
  header(tr("HOW TO PLAY", "玩法说明"));
  const char *en[] = {"1  Select your Settler, then tap Found.",
                      "2  Open a city to choose units / buildings.",
                      "3  White: move. Hold: route. Red: attack.",
                      "4  Workers build farms; Mining unlocks mines.",
                      "5  Choose Research. End turn to grow.",
                      "6  Civ: adopt policies, declare war or peace.",
                      "7  Capture all capitals or finish all techs.",
                      "8  Turn 150: highest score wins.",
                      "D-pad: cursor · A: select · X: action",
                      "Y: turn · L: research · R: diplomacy"};
  const char *zh[] = {"1  选中开拓者，点击「建城」。",
                      "2  点城市，选择生产单位或建筑。",
                      "3  白圈移动，按住看路线，红准星攻击。",
                      "4  工人修农田；研究采矿后修矿山。",
                      "5  科技页选研究；结束回合获得产出。",
                      "6  概览：城市、单位、外交与政策。",
                      "7  占领全部首都或研究全部科技获胜。",
                      "8  第150回合后比较得分决胜。",
                      "方向键移动光标，A选择，X行动。",
                      "Y结束回合，L科技，R外交。"};
  int step = std::min(18, (HEIGHT - 98) / 10);
  for (int i = 0; i < 10; i++)
    label(10, 38 + i * step, WIDTH - 20, tr(en[i], zh[i]), GREY);
  box(8, HEIGHT - 59, WIDTH / 2 - 12, 26,
      sound::enabled() ? tr("Effects: ON", "音效：开启")
                       : tr("Effects: OFF", "音效：关闭"));
  box(WIDTH / 2 + 3, HEIGHT - 59, WIDTH / 2 - 11, 26,
      sound::music_enabled() ? tr("Music: ON", "音乐：开启")
                             : tr("Music: OFF", "音乐：关闭"));
}
void draw_result() {
  header(state.winner == 0 ? tr("VICTORY", "胜利") : tr("DEFEAT", "游戏结束"));
  char b[90];
  const char *reason =
      state.result == Domination
          ? tr("All original capitals captured", "所有初始首都已被占领")
      : state.result == Science
          ? tr("The technology tree is complete", "全部科技研究完成")
      : state.result == Score
          ? tr("Turn limit reached: score victory", "达到回合上限，按得分决胜")
          : tr("Your cities and settlers are lost", "失去全部城市与开拓者");
  label(10, 45, WIDTH - 20, reason, GOLD);
  for (int o = 0; o < 3; o++) {
    snprintf(b, sizeof(b), "%s  %d", nation(o), score(state, o));
    label(16, 75 + o * 25, WIDTH - 32, b, colors[o]);
  }
  box(8, HEIGHT - 65, WIDTH - 16, 29, tr("Start new game", "开始新游戏"));
}
} // namespace
namespace game {
bool pinch_area(int x, int y) {
  return screen == Map && x >= 0 && x < WIDTH && y >= 40 && y < bottom() &&
         hit(x, y) < 0;
}
bool pinch_begin(int x, int y) {
  if (screen != Map || y < 40 || y >= bottom())
    return false;
  preview_tile = -1;
  touch = dragging = false;
  pressed = -1;
  pinching = true;
  zoom_reference = {tile_width(), pan_x, pan_y, x, y};
  redraw();
  return true;
}
void pinch_update(int x, int y, uint32_t scale) {
  if (!pinching || screen != Map)
    return;
  auto &r = zoom_reference;
  auto camera =
      pinch_tracker::zoom(r.size, r.pan_x, r.pan_y, r.x, r.y, WIDTH / 2,
                          (40 + bottom()) / 2, x, y, scale);
  if (camera.size != tile_width() || camera.pan_x != pan_x ||
      camera.pan_y != pan_y)
    apply_zoom(camera);
}
void pinch_end() {
  if (!pinching)
    return;
  pinching = false;
  touch = dragging = false;
  preview_tile = -1;
  pressed = -1;
  char text[96];
  snprintf(text, sizeof(text), "Unciv: pinch size=%d pan=%d,%d", tile_width(),
           pan_x, pan_y);
  log(text);
  redraw();
}
const char *title() { return "Unciv"; }
const char *app_id() { return "pxa-unciv"; }
void init() {
  unciv::start(state, 0x554e4349, map_size);
  ++state_revision;
  preview_tile = -1;
  center(int(state.height / 2) * W + 3);
  selected = unit_at(state, cursor);
  screen = Title;
  started = false;
#ifdef UNCIV_TEST_TACTICS
  selected = unciv::test::tactics_fixture(state);
  center(state.units[selected].tile);
  started = true;
#endif
}
void reset() { open(NewGame); }
void draw() {
  uint64_t draw_begin = map_raster::now();
  if (screen == Title)
    draw_title();
  else if (screen == Map) {
    draw_map();
    minimap();
    hud();
  } else if (screen == Technology)
    draw_technology();
  else if (screen == CityView)
    draw_city();
  else if (screen == Production)
    draw_production();
  else if (screen == ProductionQueue)
    draw_queue();
  else if (screen == UnitView)
    draw_unit();
  else if (screen == PromotionView)
    draw_promotions();
  else if (screen == UpgradeView)
    draw_upgrade();
  else if (screen == PillageView)
    draw_pillage();
  else if (screen == CityTiles)
    draw_city_tiles();
  else if (screen == PurchaseConfirm)
    draw_purchase();
  else if (screen == BattleView)
    draw_battle();
  else if (screen == CityEconomy)
    draw_economy();
  else if (screen == Journal)
    draw_journal();
  else if (screen == Empire)
    draw_empire();
  else if (screen == Policies)
    draw_policies();
  else if (screen == Help)
    draw_help();
  else if (screen == Result)
    draw_result();
  else if (screen == NewGame) {
    header(tr("New game", "新游戏"));
    int top = HEIGHT / 2 - 20;
    label(10, top - 22, WIDTH - 20, tr("Map size", "地图大小"), PAPER);
    const char *en[] = {"Small 20x14", "Standard 28x18", "Large 36x20"};
    const char *zh[] = {"小 20×14", "标准 28×18", "大 36×20"};
    for (int n = 0; n < 3; n++)
      box(n * WIDTH / 3 + 4, top, WIDTH / 3 - 8, 30, tr(en[n], zh[n]),
          map_size == n, colors[0]);
    label(10, top + 37, WIDTH - 20,
          started ? tr("The autosave will be replaced.", "自动存档将被覆盖。")
                  : tr("China · Rome · India", "中华 · 罗马 · 印度"),
          GREY);
    box(8, top + 55, WIDTH / 2 - 12, 30, tr("Cancel", "取消"), focus == 0,
        colors[0]);
    box(WIDTH / 2 + 4, top + 55, WIDTH / 2 - 12, 30,
        tr("Start game", "开始游戏"), focus == 1, colors[0]);
  }
  map_raster::record_draw(map_raster::now() - draw_begin);
}
void tick(uint32_t) {
  if (toast && millis() >= toast_end) {
    toast = nullptr;
    redraw();
  }
}
void key(Key k) {
  if (k == Key::B) {
    go_back();
    return;
  }
  if (screen == CityTiles) {
    if (k == Key::X || k == Key::A) {
      activate(111);
      return;
    }
    if (k == Key::Y) {
      activate(110);
      return;
    }
    int d = k == Key::RIGHT  ? 0
            : k == Key::DOWN ? 1
            : k == Key::LEFT ? 3
            : k == Key::UP   ? 4
                             : -1;
    int t = d >= 0 ? neighbor(work_tile, d) : -1;
    if (inside(state, t) &&
        distance(t, state.cities[selected_city].tile) <= 2) {
      work_tile = t;
      redraw();
    }
    return;
  }
  if (screen == Empire && empire_tab == 1 && k == Key::X) {
    activate(104);
    return;
  }
  if (screen == Production && k == Key::X) {
    activate(92);
    return;
  }
  if (screen == CityView && k == Key::X) {
    open(ProductionQueue);
    return;
  }
  if (screen == ProductionQueue) {
    if (k == Key::X) {
      activate(112);
      return;
    }
    if (k == Key::Y) {
      activate(113);
      return;
    }
    if (k == Key::L || k == Key::R) {
      activate(k == Key::L ? 110 : 111);
      return;
    }
  }
  if (screen == CityView && k == Key::Y) {
    activate(91);
    return;
  }
  if (screen == CityEconomy && k == Key::SELECT) {
    open(CityTiles);
    return;
  }
  if (screen == Map) {
    if (k == Key::Y) {
      finish_turn();
      return;
    }
    if (k == Key::X) {
      primary();
      return;
    }
    if (k == Key::L) {
      open(Technology);
      return;
    }
    if (k == Key::R) {
      open(Empire);
      return;
    }
    if (k == Key::SELECT) {
      next_unit();
      return;
    }
    if (k == Key::A) {
      choose(cursor);
      return;
    }
    int d = k == Key::RIGHT  ? 0
            : k == Key::DOWN ? 1
            : k == Key::LEFT ? 3
            : k == Key::UP   ? 4
                             : -1;
    if (d >= 0) {
      int t = neighbor(cursor, d);
      if (t >= 0) {
        center(t);
        redraw();
      }
    }
    return;
  }
  if (screen == NewGame && (k == Key::L || k == Key::R)) {
    map_size = (map_size + (k == Key::R ? 1 : 2)) % 3;
    redraw();
    return;
  }
  if (k == Key::SELECT && screen == CityView) {
    open(CityEconomy);
    focus = state.cities[selected_city].focus;
    return;
  }
  if (k == Key::SELECT && screen == Empire) {
    journal_page = 0;
    turn_log.unread = 0;
    open(Journal);
    return;
  }
  if (screen == CityView && (k == Key::L || k == Key::R)) {
    city_tab = 1 - city_tab;
    focus = 0;
    redraw();
    return;
  }
  if (screen == Empire && (k == Key::L || k == Key::R)) {
    empire_tab = (empire_tab + (k == Key::R ? 1 : 2)) % 3;
    empire_page = focus = 0;
    redraw();
    return;
  }
  if (k == Key::A) {
    activate(screen == Technology || screen == Production ||
                     screen == BattleView || screen == PurchaseConfirm ||
                     screen == PromotionView || screen == UpgradeView ||
                     screen == PillageView
                 ? 91
                 : focus);
    return;
  }
  int max =
          screen == Technology ? 12
          : screen == CityView ? (city_tab ? 4 : 6)
          : screen == UnitView ? 6
          : screen == ProductionQueue
              ? std::max(1, int(state.cities[selected_city].queued))
          : screen == PromotionView ? 4
          : screen == CityEconomy   ? 4
          : screen == Journal       ? std::max(1, turn_log.count)
          : screen == Policies      ? 3
          : screen == Empire
              ? empire_tab == 2 ? 3 : std::max(1, owned_count(empire_tab == 1))
          : screen == Title && started ? 3
                                       : 2,
      cols = screen == Technology ? 3
             : screen == CityView || screen == NewGame || screen == UnitView ||
                     screen == PromotionView
                 ? 2
                 : 1;
  int delta = k == Key::DOWN    ? cols
              : k == Key::UP    ? -cols
              : k == Key::RIGHT ? 1
              : k == Key::LEFT  ? -1
                                : 0;
  if (delta) {
    focus = (focus + delta + max) % max;
    if (screen == ProductionQueue)
      queue_page = focus / queue_rows();
    if (screen == Journal)
      journal_page = focus / journal_rows();
    if (screen == Empire && empire_tab < 2)
      empire_page = focus / overview_rows();
    redraw();
  }
}
void pointer(Phase phase, int x, int y) {
  if (screen == CityTiles) {
    if (phase == Phase::DOWN) {
      touch = true;
      dragging = false;
      down_x = x;
      down_y = y;
      pressed = hit(x, y);
    } else if (phase == Phase::CANCEL) {
      touch = dragging = false;
      pressed = -1;
    } else if (phase == Phase::MOVE && touch &&
               abs(x - down_x) + abs(y - down_y) > 6)
      dragging = true;
    else if (phase == Phase::UP && touch) {
      touch = false;
      if (!dragging) {
        int a = hit(x, y);
        if (a >= 0 && a == pressed)
          activate(a);
        else if (a < 0 && pressed < 0) {
          int t = tile_at(x, y);
          if (inside(state, t) &&
              distance(t, state.cities[selected_city].tile) <= 2) {
            work_tile = t;
            redraw();
          }
        }
      }
      dragging = false;
      pressed = -1;
    }
    return;
  }
  if (phase == Phase::DOWN) {
    touch = true;
    dragging = false;
    down_x = x;
    down_y = y;
    old_pan_x = pan_x;
    old_pan_y = pan_y;
    pressed = hit(x, y);
    preview_tile = -1;
    if (screen == Map && selected >= 0 && pressed < 0) {
      int t = tile_at(x, y);
      if (inside(state, t) && movement().cost[t] > 0) {
        preview_tile = t;
        redraw();
      }
    }
    return;
  }
  if (phase == Phase::CANCEL) {
    if (preview_tile >= 0)
      redraw();
    preview_tile = -1;
    touch = dragging = false;
    pressed = -1;
    return;
  }
  if (!touch)
    return;
  if (phase == Phase::MOVE && screen == Map && pressed < 0 && down_y >= 40 &&
      down_y < bottom()) {
    if (!dragging && abs(x - down_x) + abs(y - down_y) > 6) {
      dragging = true;
      preview_tile = -1;
      pan_started = map_raster::now();
      pan_frame_begin = map_raster::frames();
      pan_render_begin = map_raster::render_us();
    }
    if (dragging) {
      pan_x = std::clamp(old_pan_x + x - down_x, -state.width * tile_width(),
                         state.width * tile_width());
      pan_y = std::clamp(old_pan_y + y - down_y, -state.height * tile_width(),
                         state.height * tile_width());
      redraw();
    }
    return;
  }
  if (phase == Phase::UP) {
    touch = false;
    if (preview_tile >= 0)
      redraw();
    preview_tile = -1;
    if (dragging) {
      dragging = false;
      char note[160];
      unsigned frames = map_raster::frames() - pan_frame_begin;
      uint64_t elapsed = map_raster::now() - pan_started;
      snprintf(
          note, sizeof(note),
          "Unciv: pan frames=%u wall_us=%llu render_us=%llu", frames,
          (unsigned long long)elapsed,
          (unsigned long long)(map_raster::render_us() - pan_render_begin));
      log(note);
      redraw();
      return;
    }
    int a = hit(x, y);
    if (a >= 0 && a == pressed) {
      if (a < 90)
        focus = a;
      activate(a);
    } else if (screen == Map && pressed < 0) {
      int tile = tile_at(x, y);
      if (tile >= 0)
        choose(tile);
    }
    pressed = -1;
  }
}
size_t serialize(uint8_t *out, size_t cap) {
  return started ? unciv::serialize(state, out, cap) : 0;
}
void load(const uint8_t *data, size_t size) {
  State candidate;
  if (!unciv::restore(candidate, data, size))
    return;
  state = candidate;
  turn_log.clear();
  ++state_revision;
  preview_tile = -1;
  map_size = state.width == 20 ? 0 : state.width == 28 ? 1 : 2;
  started = true;
  selected = -1;
  selected_city = -1;
  for (int i = 0; i < UNITS; i++)
    if (state.units[i].tile >= 0 && state.units[i].owner == 0) {
      selected = i;
      center(state.units[i].tile);
      break;
    }
  if (selected < 0)
    for (auto &c : state.cities)
      if (c.tile >= 0 && c.owner == 0) {
        center(c.tile);
        break;
      }
  screen = Title;
  record("load");
  redraw();
}
bool menu_hit(int x, int y) {
  return x >= WIDTH - 40 && x < WIDTH - 4 && y >= 0 && y < 24;
}
void draw_menu(bool paused, int row, bool exiting) {
  // Keep edge pixels uniform before the shared renderer extends safe borders.
  rect(0, 0, WIDTH, 2, DARK);
  rect(0, HEIGHT - 2, WIDTH, 2, DARK);
  rect(0, 0, 2, HEIGHT, DARK);
  rect(WIDTH - 2, 0, 2, HEIGHT, DARK);
  if (toast && millis() < toast_end && screen != Map && screen != Title &&
      screen != NewGame && screen != CityTiles) {
    rect(8, 28, WIDTH - 16, 17, rgb(38, 67, 94));
    label(11, 31, WIDTH - 22, toast, PAPER);
  }
  box(WIDTH - 38, 3, 32, 18, "");
  icon(art::ui_options, WIDTH - 28, 6, 12, PAPER);
  if (!paused)
    return;
  int left = (WIDTH - 192) / 2, top = HEIGHT > 224 ? (HEIGHT - 180) / 2 : 32;
  rect(left, top, 192, 180, DARK);
  label(left + 8, top + 10, 176, tr("PAUSED", "游戏已暂停"), GOLD, 1, 1);
  const char *names[] = {
      tr("Resume", "继续游戏"), tr("Start new game", "开始新游戏"),
      tr("Save and exit", "保存并退出"), tr("Language: English", "语言：中文")};
  for (int i = 0; i < 4; i++)
    box(left + 8, top + 40 + i * 30, 176, 28, names[i], row == i);
  if (exiting)
    label(left + 8, top + 164, 176, tr("Saving...", "保存中…"), GOLD);
}
} // namespace game
