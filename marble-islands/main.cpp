// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-studio/studio.hpp"
#include "core.hpp"
#include "motion.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
namespace {
using pocket::Key;
using pocket::Phase;
marble::State state;
enum Screen { INTRO, RACE, ISLANDS, CONTROLS };
Screen controls_return = INTRO;
int controls_focus = 0;
Screen screen = INTRO;
int selected;
int nav_focus = 0;
bool slow_brake = false;
bool three_keys() {
  const auto mode = marble_motion::device.options.navigation;
  return pocket::portrait_display() || mode == 1;
}

bool dragging, touch_brake;
int anchor_x, anchor_y, drag_x, drag_y;
int pressed_action = -1;
int menu_hit(int x, int y);
void nav_log() {
  char b[96];
  snprintf(b, sizeof(b), "Marble: focus=%d screen=%u",
           (screen == CONTROLS ? 20 + controls_focus : nav_focus),
           unsigned(screen));
  pocket::log(b);
}
uint32_t checkpoint_ms, toast_until, gem_flash_ms;
const char *toast;
static uint16_t view[marble::MAX_VIEW_PIXELS];
int X(int x) { return x + (pocket::WIDTH - 288) / 2; }
int C(int y) { return y + (pocket::HEIGHT - 224) / 2; }
int B(int y) { return y + pocket::HEIGHT - 224; }
int S(int x) { return x * pocket::WIDTH / 288; }
int control_step() { return (pocket::HEIGHT - 70) / 7; }
int island_step() { return (pocket::HEIGHT - 84) / 4; }
int island_width() { return (pocket::WIDTH - 24) / 2; }
int brake_x() { return pocket::WIDTH - 35; }
int brake_y() { return pocket::HEIGHT - 54; }
constexpr uint16_t DEEP = pocket::rgb(24, 58, 83),
                   CREAM = pocket::rgb(244, 246, 218),
                   GOLD = pocket::rgb(255, 208, 100),
                   MINT = pocket::rgb(145, 235, 190),
                   CORAL = pocket::rgb(247, 146, 158);
const char *level_name(unsigned level) {
  switch (level) {
  case 0:
    return pocket::tr("First Ripples", "初泛涟漪");
  case 1:
    return pocket::tr("Ribbon Bridge", "丝带窄桥");
  case 2:
    return pocket::tr("Blue Windows", "碧水天窗");
  case 3:
    return pocket::tr("Tidal Gate", "潮汐之门");
  case 4:
    return pocket::tr("Pebble Garden", "卵石花园");
  case 5:
    return pocket::tr("Windmill Walk", "风车小径");
  case 6:
    return pocket::tr("Three Tides", "三重潮汐");
  case 7:
    return pocket::tr("Sunset Crown", "落日之冠");
  default:
    return "";
  }
}
const char *level_hint(unsigned level) {
  switch (level) {
  case 0:
    return pocket::tr("Collect every gem, then find the flag.",
                      "收齐金色宝石，滚向终点小旗。");
  case 1:
    return pocket::tr("Small moves. Brake before each turn.",
                      "小幅移动，每次转弯前先刹车。");
  case 2:
    return pocket::tr("Water holes have no invisible rails.",
                      "水洞没有护栏，小心绕行。");
  case 3:
    return pocket::tr("Coral gate: wait for the mint opening.",
                      "珊瑚红的门关闭，变薄荷绿再通过。");
  case 4:
    return pocket::tr("Pink bumpers bounce. Take the clear lanes.",
                      "粉色弹柱会反弹，走空旷路线。");
  case 5:
    return pocket::tr("Dark blocks stop you; the outer loop is safe.",
                      "深色墙挡路，沿外环行走更安全。");
  case 6:
    return pocket::tr("Three timed gates. Slow down and read the tide.",
                      "三道定时潮门，放慢速度看准时机。");
  case 7:
    return pocket::tr("One last loop. Bring every gem home.",
                      "最后一圈，把每颗宝石带回家。");
  default:
    return "";
  }
}
int clamp(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
void release() {
  dragging = touch_brake = false;
  drag_x = drag_y = 0;
  pressed_action = -1;
}
void log_run(const char *action) {
  char b[96];
  snprintf(b, sizeof(b), "Marble: %s level=%u gems=%u time=%u", action,
           state.run.level + 1, marble::collected(state.run),
           state.run.elapsed_ms);
  pocket::log(b);
}
void changed(bool durable = true) {
  pocket::redraw();
  if (durable)
    pocket::save();
}
void sync_motion() {
  marble_motion::device.request(
      (screen == RACE && state.run.phase != marble::Phase::WON) ||
      screen == CONTROLS);
}
void open_controls() {
  controls_return = screen;
  screen = CONTROLS;
  controls_focus = 0;
  slow_brake = false;
  release();
  sync_motion();
  nav_log();
  pocket::redraw();
}
bool manual_keys() {
  if (three_keys())
    return false;
  return pocket::held(Key::LEFT) || pocket::held(Key::RIGHT) ||
         pocket::held(Key::UP) || pocket::held(Key::DOWN);
}
const char *motion_status() {
  auto &d = marble_motion::device;
  if (!d.options.enabled)
    return pocket::tr("Motion control off", "体感已关闭");
  if (d.status == marble_motion::Status::Connecting)
    return pocket::tr("Connecting sensors...", "正在连接体感…");
  if (d.status == marble_motion::Status::Permission)
    return pocket::tr("Allow motion access to continue",
                      "请允许使用体感传感器");
  if (d.status == marble_motion::Status::Streaming) {
    if (d.tilt.calibrating)
      return pocket::tr("Hold still to set neutral", "扶稳设备，正在校准…");
    if (!d.ready())
      return pocket::tr("Signal lost. Touch still works.",
                        "读数中断，可用触屏继续");
    return d.gyro ? pocket::tr("Gyroscope fusion ready", "陀螺仪融合已就绪")
                  : pocket::tr("Gravity tilt ready", "重力体感已就绪");
  }
  return d.status == marble_motion::Status::Denied
             ? pocket::tr("Motion denied. Touch still works.",
                          "未授权体感，仍可触屏玩")
             : pocket::tr("No motion sensor. Touch to play.",
                          "未检测到体感，可用触屏玩");
}
void begin(unsigned level) {
  if (!marble::start(state, level))
    return;
  screen = RACE;
  nav_focus = 7;
  slow_brake = false;
  release();
  sync_motion();
  checkpoint_ms = 0;
  log_run("start");
  nav_log();
  changed();
}
void show_islands() {
  release();
  selected = state.run.level;
  screen = ISLANDS;
  nav_focus = 10 + state.run.level;
  slow_brake = false;
  sync_motion();
  nav_log();
  pocket::redraw();
}
void choose() {
  if (selected < state.progress.unlocked)
    begin(unsigned(selected));
  else {
    toast = "locked";
    toast_until = pocket::millis() + 2300;
    pocket::tone(180, .06f);
    pocket::redraw();
  }
}
void advance() {
  if (state.run.level + 1 < state.progress.unlocked)
    begin(state.run.level + 1);
  else
    show_islands();
}
void circle(int cx, int cy, int r, uint16_t color, bool filled) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    if (filled) {
      pocket::line(cx - x, cy + y, cx + x, cy + y, color);
      pocket::line(cx - x, cy - y, cx + x, cy - y, color);
      pocket::line(cx - y, cy + x, cx + y, cy + x, color);
      pocket::line(cx - y, cy - x, cx + y, cy - x, color);
    } else {
      const int xs[8] = {x, y, -y, -x, -x, -y, y, x},
                ys[8] = {y, x, x, y, -y, -x, -x, -y};
      for (int i = 0; i < 8; ++i)
        pocket::pixel(cx + xs[i], cy + ys[i], color);
    }
    ++y;
    if (err < 0)
      err += 2 * y + 1;
    else {
      --x;
      err += 2 * (y - x) + 1;
    }
  }
}
void diamond(int x, int y, int r, uint16_t color) {
  for (int i = -r; i <= r; ++i)
    pocket::line(x - (r - abs(i)), y + i, x + (r - abs(i)), y + i, color);
}
void center(int y, const char *s, uint16_t color = CREAM, int scale = 1) {
  pocket::text((pocket::WIDTH - pocket::text_width(s, scale)) / 2, y, s, color,
               scale);
}
void wrap(int x, int y, int width, const char *value, uint16_t color) {
  // Advance whole UTF-8 glyphs and measure pixels; never split a Chinese byte
  // sequence.
  while (*value) {
    char b[192];
    unsigned n = 0, last_space = 0;
    while (value[n]) {
      unsigned char lead = static_cast<unsigned char>(value[n]);
      unsigned bytes = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
      if (n + bytes >= sizeof(b))
        break;
      memcpy(b + n, value + n, bytes);
      b[n + bytes] = 0;
      if (pocket::text_width(b) > width)
        break;
      if (value[n] == ' ')
        last_space = n;
      n += bytes;
    }
    if (!n)
      return;
    if (value[n] && last_space)
      n = last_space;
    memcpy(b, value, n);
    b[n] = 0;
    pocket::text(x, y, b, color);
    y += 12;
    value += n;
    while (*value == ' ')
      ++value;
  }
}
void button(int x, int y, int w, const char *text, uint16_t color = DEEP) {
  bool pressed =
      pressed_action >= 0 && pressed_action == menu_hit(x + w / 2, y + 9);
  if (three_keys() && nav_focus == menu_hit(x + w / 2, y + 9))
    studio::round(x - 2, y - 2, w + 4, 22, 6, GOLD);
  studio::button(x, y, w, 18, text, pressed ? studio::mix(color, 0, 45) : color,
                 CREAM, pressed);
}
void tutorial() {
  studio::card(X(32), C(40), 224, 151, DEEP, pocket::rgb(91, 170, 171), 10);
  studio::label(X(40), C(49), 208, pocket::tr("MARBLE ISLANDS", "滚珠群岛"),
                CREAM, 2);
  studio::label(X(40), C(76), 208,
                pocket::tr("A little island. A golden adventure.",
                           "越过碧海，收集一路金色星光。"),
                MINT);
  diamond(X(54), C(101), 5, GOLD);
  pocket::text(X(68), C(97),
               pocket::tr("Collect every golden gem", "收齐宝石，打开终点"),
               CREAM);
  circle(X(54), C(122), 5, MINT, false);
  pocket::text(
      X(68), C(118),
      pocket::tr("Tilt your device, or drag gently", "倾斜设备，也可轻轻拖动"),
      CREAM);
  circle(X(54), C(143), 5, CORAL, true);
  pocket::text(X(68), C(139),
               three_keys() ? pocket::tr("Confirm toggles slow braking",
                                         "确认切换慢速，转弯更从容")
                            : pocket::tr("Hold brake before a turn",
                                         "转弯前按住刹车，稳稳前进"),
               CREAM);
  if (three_keys() && nav_focus == 0)
    studio::round(X(47), C(164), 194, 24, 6, GOLD);
  studio::button(X(49), C(166), 190, 20,
                 three_keys() ? pocket::tr("Let's roll", "出发吧")
                              : pocket::tr("Let's roll · A", "出发吧 · A"),
                 pocket::rgb(46, 110, 118), CREAM, true);
  if (three_keys())
    button(X(72), B(196), 144, pocket::tr("TILT / CONTROLS", "体感与操作设置"));
}

void islands() {
  pocket::clear(DEEP);
  center(8, pocket::tr("CHOOSE AN ISLAND", "选择岛屿"), CREAM, 2);
  center(30,
         pocket::tr("GOLD: ALL GEMS WITHIN THE PAR TIME",
                    "金章：目标时间内收齐宝石通关"),
         MINT);
  for (int i = 0; i < 8; ++i) {
    int x = 8 + (i % 2) * (island_width() + 8),
        y = 43 + (i / 2) * island_step();
    bool open = i < state.progress.unlocked,
         focus = three_keys() ? nav_focus == 10 + i : i == selected;
    studio::card(x, y, island_width(), island_step() - 4,
                 focus ? pocket::rgb(53, 106, 119) : pocket::rgb(33, 73, 96),
                 focus  ? GOLD
                 : open ? MINT
                        : pocket::MUTED,
                 5);
    char b[96];
    snprintf(b, sizeof(b), "%d %s", i + 1,
             open ? level_name(unsigned(i)) : pocket::tr("LOCKED", "未解锁"));
    pocket::text(x + 5, y + 5, b, open ? CREAM : pocket::MUTED);
    if (state.progress.best_ms[i]) {
      snprintf(b, sizeof(b), pocket::tr("BEST %u.%us", "最佳%u.%u秒"),
               state.progress.best_ms[i] / 1000,
               (state.progress.best_ms[i] / 100) % 10);
      pocket::text(x + 5, y + 18, b, MINT);
    } else
      pocket::text(x + 5, y + 18,
                   open ? pocket::tr("READY TO EXPLORE", "准备出发")
                        : pocket::tr("FINISH EARLIER ISLAND", "先完成前一岛"),
                   open ? MINT : pocket::MUTED);
    for (unsigned j = 0; j < state.progress.medals[i]; ++j)
      diamond(x + island_width() - 28 + j * 9, y + island_step() - 13, 3,
              state.progress.medals[i] == 3   ? GOLD
              : state.progress.medals[i] == 2 ? CREAM
                                              : CORAL);
  }
  if (toast && int32_t(toast_until - pocket::millis()) > 0)
    wrap(12, B(181), pocket::WIDTH - 24,
         pocket::tr("Finish earlier islands to unlock this one.",
                    "先完成前面的岛屿即可解锁。"),
         GOLD);
  else
    center(B(181),
           three_keys() ? pocket::tr("UP/DOWN SELECT · CONFIRM TO PLAY",
                                     "上/下选岛 · 确认出发")
                        : pocket::tr("ARROWS + A / TAP A CARD TO PLAY",
                                     "方向键+A / 轻触卡片开始"),
           MINT);
  if (three_keys()) {
    button(S(8), B(194), S(70), pocket::tr("BACK", "返回"));
    button(S(86), B(194), S(106), pocket::tr("HOW TO PLAY", "玩法说明"));
    button(S(200), B(194), S(80), pocket::tr("TILT", "体感设置"));
  } else {
    button(S(8), B(194), S(78), pocket::tr("BACK", "返回"));
    button(S(98), B(194), S(182), pocket::tr("HOW TO PLAY", "玩法说明"));
  }
}
void won() {
  pocket::rect(X(27), C(49), 234, 137, DEEP);
  pocket::outline(X(27), C(49), 234, 137, MINT);
  center(C(62),
         state.run.level == 7
             ? pocket::tr("ISLANDS COMPLETE!", "群岛全部完成！")
             : pocket::tr("ISLAND CLEAR!", "小岛通关！"),
         MINT, 2);
  uint32_t par = marble::levels[state.run.level].gold_ms;
  unsigned medal = state.run.elapsed_ms <= par           ? 3
                   : state.run.elapsed_ms <= par * 3 / 2 ? 2
                                                         : 1;
  for (unsigned i = 0; i < 3; ++i)
    diamond(X(112 + i * 32), C(94), 10,
            i < medal ? GOLD : pocket::rgb(73, 103, 123));
  char b[64];
  snprintf(b, sizeof(b),
           pocket::tr("TIME %u.%us   GOLD %us", "用时%u.%u秒  金章%u秒"),
           state.run.elapsed_ms / 1000, (state.run.elapsed_ms / 100) % 10,
           marble::levels[state.run.level].gold_ms / 1000);
  center(C(115), b);
  center(C(130),
         medal == 3 ? pocket::tr("A PERFECT LITTLE VOYAGE.", "完美的小小旅程。")
                    : pocket::tr("TRY AGAIN FOR A FASTER MEDAL.",
                                 "再试一次，争取更快的奖章。"),
         MINT);
  button(X(45), C(153), 103,
         state.run.level == 7
             ? pocket::tr("ISLANDS", "群岛")
             : (three_keys() ? pocket::tr("NEXT", "下一岛")
                             : pocket::tr("NEXT / A", "下一岛 / A")));
  button(X(155), C(153), 88,
         three_keys() ? pocket::tr("RETRY", "重试")
                      : pocket::tr("RETRY / X", "重试 / X"));
  if (three_keys()) {
    button(S(20), B(195), S(110), pocket::tr("ISLANDS", "选择岛屿"));
    button(S(158), B(195), S(110), pocket::tr("TILT CONTROLS", "体感设置"));
  }
}
void draw_race() {
  pocket::clear(DEEP);
  const int scene_h = pocket::HEIGHT - 52;
  int rw = pocket::WIDTH, rh = scene_h;
  const int longest = std::max(rw, rh);
  if (longest > marble::MAX_VIEW) {
    rw = rw * marble::MAX_VIEW / longest;
    rh = rh * marble::MAX_VIEW / longest;
  }
  marble::render(state.run, view, pocket::millis(), rw, rh);
  studio::raster(0, 20, pocket::WIDTH, scene_h, view, rw, rh);
  pocket::rect(0, 0, pocket::WIDTH - 24, 20, DEEP);
  char b[72];
  snprintf(b, sizeof(b), "%u %s", state.run.level + 1,
           level_name(state.run.level));
  pocket::text(30, 5, b, CREAM);
  if (gem_flash_ms)
    studio::glow(pocket::WIDTH - 98, 9, 15, GOLD, int(gem_flash_ms / 5));
  snprintf(b, sizeof(b), "%u/%u", marble::collected(state.run),
           marble::coin_count(state.run.level));
  diamond(pocket::WIDTH - 115, 9, 3, GOLD);
  pocket::text(pocket::WIDTH - 107, 5, b, GOLD);
  snprintf(b, sizeof(b), pocket::tr("%us", "%u秒"),
           state.run.elapsed_ms / 1000);
  pocket::text(pocket::WIDTH - 55 - pocket::text_width(b), 5, b, CREAM);
  if (screen == INTRO) {
    tutorial();
    return;
  }
  if (state.run.phase == marble::Phase::WON) {
    won();
    return;
  }
  studio::round(12, 26, pocket::WIDTH - 24, 19, 5, CREAM);
  studio::label(18, 30, pocket::WIDTH - 36, level_hint(state.run.level), DEEP);
  if (state.run.phase == marble::Phase::FALLING) {
    pocket::rect(X(58), C(97), 172, 35, DEEP);
    center(C(101), pocket::tr("SPLASH!", "扑通！"), CORAL, 2);
    center(C(123), pocket::tr("A FRESH START IS COMING.", "马上重新出发。"),
           CREAM);
  }
  auto &motion = marble_motion::device;
  if (motion.waiting() && !dragging && !manual_keys()) {
    studio::card(X(36), C(82), 216, 51, DEEP, MINT, 7);
    studio::label(X(42), C(91), 204, motion_status(), CREAM);
    studio::label(X(42), C(111), 204,
                  three_keys()
                      ? pocket::tr("Check tilt settings, or use touch.",
                                   "可检查体感设置，或用触屏。")
                      : pocket::tr("Touch or arrows can take over.",
                                   "触屏或方向键可随时接管。"),
                  MINT);
  }
  int ax = dragging ? anchor_x : 42, ay = dragging ? anchor_y : brake_y();
  studio::glow(ax, ay, 22, DEEP, 170);
  circle(ax, ay, 21, MINT, false);
  if (dragging) {
    circle(ax, ay, 16, pocket::rgb(70, 139, 154), false);
    circle(ax + drag_x / 2, ay + drag_y / 2, 5, CREAM, true);
  } else if (motion.ready()) {
    auto tilt = motion.tilt.input(motion.options);
    studio::disc(ax + int(tilt.x * 14), ay + int(tilt.y * 14), 4, MINT);
  } else {
    pocket::line(ax - 6, ay, ax + 6, ay, CREAM);
    pocket::line(ax, ay - 6, ax, ay + 6, CREAM);
    studio::disc(ax, ay, 2, MINT);
  }
  if (three_keys() && nav_focus == 7)
    circle(brake_x(), brake_y(), 25, GOLD, true);
  circle(brake_x(), brake_y(), 22,
         touch_brake || slow_brake || (!three_keys() && pocket::held(Key::A))
             ? CORAL
             : DEEP,
         true);
  circle(brake_x(), brake_y(), 22, CREAM, false);
  pocket::text(
      brake_x() - pocket::text_width(
                      (three_keys() ? (slow_brake ? pocket::tr("SLOW", "慢速中")
                                                  : pocket::tr("BRAKE", "慢速"))
                                    : pocket::tr("BRAKE", "刹车"))) /
                      2,
      brake_y() - 5,
      (three_keys() ? (slow_brake ? pocket::tr("SLOW", "慢速中")
                                  : pocket::tr("BRAKE", "慢速"))
                    : pocket::tr("BRAKE", "刹车")),
      CREAM);
  if (three_keys())
    pocket::text(X(96), B(180),
                 pocket::tr("SELECT · CONFIRM", "上/下选，确认执行"), CREAM);
  else if (dragging)
    pocket::text(X(103), B(180), pocket::tr("EASE INTO TURNS", "转弯时放慢"),
                 CREAM);
  else
    pocket::text(X(96), B(180),
                 motion.ready() ? pocket::tr("TILT TO ROLL", "倾斜设备来滚动")
                                : pocket::tr("DRAG TO TILT", "拖动来倾斜"),
                 CREAM);
  pocket::rect(0, B(192), pocket::WIDTH, 32, DEEP);
  button(S(4), B(194), S(78),
         three_keys() ? pocket::tr("ISLANDS", "选岛")
                      : pocket::tr("ISLANDS / B", "选岛 / B"));
  button(S(91), B(194), S(69),
         three_keys() ? pocket::tr("RETRY", "重试")
                      : pocket::tr("RETRY / X", "重试 / X"));
  button(S(166), B(194), S(118),
         three_keys() ? pocket::tr("TILT", "体感设置")
                      : pocket::tr("TILT / Y", "体感 / Y"));
}
bool inside(int x, int y, int left, int top, int w, int h) {
  return x >= left && x < left + w && y >= top && y < top + h;
}
void controls() {
  using namespace pocket;
  clear(DEEP);
  studio::label(24, 8, pocket::WIDTH - 76, tr("TILT CONTROLS", "体感设置"),
                CREAM, 2);
  studio::label(20, 34, pocket::WIDTH - 40, motion_status(), MINT);

  auto &o = marble_motion::device.options;
  const char *speeds[] = {tr("GENTLE", "舒缓"), tr("NORMAL", "标准"),
                          tr("QUICK", "灵敏")};
  char b[96];
  for (int i = 0; i < 7; ++i) {
    if (i == 0)
      snprintf(b, sizeof(b), tr("Motion: %s", "体感：%s"),
               o.enabled ? tr("ON", "开") : tr("OFF", "关"));
    if (i == 1)
      snprintf(b, sizeof(b), "%s",
               tr("Calibrate neutral position", "校准当前握持角度"));
    if (i == 2)
      snprintf(b, sizeof(b), tr("Sensitivity: %s", "灵敏度：%s"),
               speeds[o.sensitivity]);
    if (i == 3)
      snprintf(b, sizeof(b), tr("Direction: %u degrees", "方向旋转：%u度"),
               o.rotation * 90u);
    if (i == 4)
      snprintf(b, sizeof(b), tr("Mirror left/right: %s", "左右镜像：%s"),
               o.mirror ? tr("ON", "开") : tr("OFF", "关"));
    if (i == 5) {
      const char *modes[] = {tr("AUTO", "自动"), tr("ON", "开启"),
                             tr("GAMEPAD", "手柄")};
      snprintf(b, sizeof(b), tr("Three keys: %s", "三按键导航：%s"),
               modes[o.navigation]);
    }
    if (i == 6)
      snprintf(b, sizeof(b), "%s", tr("Back to game", "返回游戏"));
    studio::button(24, 51 + i * control_step(), pocket::WIDTH - 48,
                   control_step() - 2, b,
                   controls_focus == i ? rgb(48, 107, 115) : rgb(31, 72, 96),
                   controls_focus == i ? GOLD : CREAM, controls_focus == i);
  }
  studio::label(20, B(211), pocket::WIDTH - 40,
                tr("Up/Down select · Confirm to use", "上/下选择 · 确认执行"),
                MINT);
}
void control_action(unsigned row) {
  auto &d = marble_motion::device;
  controls_focus = int(row);
  if (row == 0) {
    d.options.enabled = !d.options.enabled;
    if (d.options.enabled)
      ++d.retry;
  }
  if (row == 1) {
    d.options.enabled = true;
    d.tilt.calibrate();
    ++d.retry;
    state.run.vx = state.run.vy = 0;
  }
  if (row == 2)
    d.options.sensitivity = (d.options.sensitivity + 1) % 3;
  if (row == 3)
    d.options.rotation = (d.options.rotation + 1) % 4;
  if (row == 4)
    d.options.mirror = !d.options.mirror;
  if (row == 5)
    d.options.navigation =
        (d.options.navigation + 1) % (pocket::portrait_display() ? 2 : 3);
  if (row == 6) {
    screen = controls_return;
    nav_focus = screen == INTRO                         ? 0
                : screen == ISLANDS                     ? 10 + selected
                : state.run.phase == marble::Phase::WON ? 5
                                                        : 7;
    release();
  }
  sync_motion();
  nav_log();
  changed();
}
int menu_hit(int x, int y) {
  if (screen == CONTROLS) {
    if (x >= 24 && x < pocket::WIDTH - 24 && y >= 51 &&
        y < 51 + 7 * control_step() &&
        (y - 51) % control_step() < control_step() - 2)
      return 20 + (y - 51) / control_step();
    return -1;
  }
  if (screen == INTRO) {
    if (three_keys() && inside(x, y, X(72), B(196), 144, 18))
      return 6;
    return inside(x, y, X(49), C(166), 190, 20) ? 0 : -1;
  }
  if (screen == ISLANDS) {
    if (three_keys()) {
      if (inside(x, y, S(8), B(194), S(70), 18))
        return 1;
      if (inside(x, y, S(86), B(194), S(106), 18))
        return 2;
      if (inside(x, y, S(200), B(194), S(80), 18))
        return 6;
    }
    if (!three_keys() && inside(x, y, S(8), B(194), S(78), 18))
      return 1;
    if (!three_keys() && inside(x, y, S(98), B(194), S(182), 18))
      return 2;
    for (int i = 0; i < 8; ++i)
      if (inside(x, y, 8 + (i % 2) * (island_width() + 8),
                 43 + (i / 2) * island_step(), island_width(),
                 island_step() - 4))
        return 10 + i;
    return -1;
  }
  if (state.run.phase == marble::Phase::WON) {
    if (inside(x, y, X(45), C(153), 103, 18))
      return 5;
    if (inside(x, y, X(155), C(153), 88, 18))
      return 4;
    if (three_keys() && inside(x, y, S(20), B(195), S(110), 18))
      return 3;
    if (three_keys() && inside(x, y, S(158), B(195), S(110), 18))
      return 6;
    return -1;
  }
  if (inside(x, y, S(4), B(194), S(78), 18))
    return 3;
  if (inside(x, y, S(91), B(194), S(69), 18))
    return 4;
  if (inside(x, y, S(166), B(194), S(118), 18))
    return 6;
  return -1;
}
void activate_menu(int action) {
  if (action == 0 || action == 1) {
    screen = RACE;
    nav_focus = state.run.phase == marble::Phase::WON ? 5 : 7;
    release();
    sync_motion();
  } else if (action == 2) {
    screen = INTRO;
    nav_focus = 0;
    release();
    sync_motion();
  } else if (action == 3)
    show_islands();
  else if (action == 4)
    begin(state.run.level);
  else if (action == 5)
    advance();
  else if (action == 6)
    open_controls();
  else if (action == 7) {
    slow_brake = !slow_brake;
    pocket::log(slow_brake ? "Marble: slow brake on"
                           : "Marble: slow brake off");
  } else if (action == 8)
    pocket::open_menu();
  else if (action >= 20 && action < 27)
    control_action(unsigned(action - 20));
  else if (action >= 10 && action < 18) {
    selected = action - 10;
    choose();
  }
  pocket::redraw();
}

void nav_move(int delta) {
  int ids[16], n = 0;
  if (screen == INTRO) {
    ids[n++] = 0;
    ids[n++] = 6;
  } else if (screen == ISLANDS) {
    for (int i = 10; i < 18; ++i)
      ids[n++] = i;
    ids[n++] = 2;
    ids[n++] = 6;
  } else if (state.run.phase == marble::Phase::WON) {
    ids[n++] = 5;
    ids[n++] = 4;
    ids[n++] = 3;
    ids[n++] = 6;
  } else {
    ids[n++] = 7;
    ids[n++] = 3;
    ids[n++] = 4;
    ids[n++] = 6;
  }
  ids[n++] = 8;
  if (screen == ISLANDS)
    ids[n++] = 1;
  int at = 0;
  for (int i = 0; i < n; ++i)
    if (ids[i] == nav_focus)
      at = i;
  nav_focus = ids[(at + n + delta) % n];
  if (nav_focus >= 10 && nav_focus < 18)
    selected = nav_focus - 10;
  nav_log();
  pocket::redraw();
}

} // namespace
namespace game {
const char *title() { return pocket::tr("Marble Islands", "滚珠群岛"); }
const char *app_id() { return "pxa-marble-islands"; }
void init() {
  nav_focus = 0;
  slow_brake = false;
  marble_motion::device = marble_motion::Device{};
  controls_focus = 0;
  controls_return = INTRO;
  marble::init(state);
  screen = INTRO;
  selected = 0;
  release();
  checkpoint_ms = gem_flash_ms = 0;
  toast = nullptr;
  log_run("start");
  nav_log();
  changed();
}
void reset() { begin(state.run.level); }
void key(Key key) {
  if (screen == CONTROLS) {
    if (key == Key::B || key == Key::Y || key == Key::SELECT)
      control_action(6);
    else if (key == Key::A)
      control_action(unsigned(controls_focus));
    else if (key == Key::UP || key == Key::DOWN) {
      controls_focus = (controls_focus + (key == Key::UP ? 6 : 1)) % 7;
      nav_log();
      pocket::redraw();
    } else if (key == Key::LEFT || key == Key::RIGHT)
      control_action(unsigned(controls_focus));
    return;
  }
  if (three_keys() && (key == Key::UP || key == Key::DOWN)) {
    nav_move(key == Key::UP ? -1 : 1);
    return;
  }
  if (three_keys() && key == Key::A) {
    activate_menu(nav_focus);
    nav_log();
    return;
  }
  if (key == Key::Y || key == Key::SELECT) {
    open_controls();
    return;
  }
  if (key == Key::X) {
    reset();
    return;
  }
  if (screen == INTRO) {
    if (key == Key::A || key == Key::B) {
      screen = RACE;
      sync_motion();
      pocket::redraw();
    }
    return;
  }
  if (screen == ISLANDS) {
    if (key == Key::B) {
      screen = RACE;
      release();
      sync_motion();
      pocket::redraw();
    } else if (key == Key::A)
      choose();
    else {
      selected = clamp(selected + (key == Key::UP      ? -2
                                   : key == Key::DOWN  ? 2
                                   : key == Key::LEFT  ? -1
                                   : key == Key::RIGHT ? 1
                                                       : 0),
                       0, 7);
      pocket::redraw();
    }
    return;
  }
  if (key == Key::B)
    show_islands();
  else if (state.run.phase == marble::Phase::WON && key == Key::A)
    advance();
}
void pointer(Phase phase, int x, int y) {
  if (phase == Phase::CANCEL) {
    release();
    pocket::redraw();
    return;
  }
  if (phase == Phase::UP) {
    int action = pressed_action;
    bool activate = action >= 0 && action == menu_hit(x, y);
    release();
    if (activate)
      activate_menu(action);
    pocket::redraw();
    return;
  }
  if (phase == Phase::DOWN) {
    release();
    int action = menu_hit(x, y);
    if (action >= 0) {
      pressed_action = action;
      pocket::redraw();
      return;
    }
    if (screen != RACE || state.run.phase != marble::Phase::ROLLING || y < 54 ||
        y >= B(192))
      return;
    int dx = x - brake_x(), dy = y - brake_y();
    if (dx * dx + dy * dy <= 25 * 25)
      touch_brake = true;
    else {
      dragging = true;
      anchor_x = x;
      anchor_y = y;
    }
  } else if (dragging) {
    drag_x = clamp(x - anchor_x, -30, 30);
    drag_y = clamp(y - anchor_y, -30, 30);
  }
  pocket::redraw();
}

void tick(uint32_t ms) {
  sync_motion();
  gem_flash_ms = ms >= gem_flash_ms ? 0 : gem_flash_ms - ms;
  if (screen != RACE) {
    if (screen == INTRO || screen == CONTROLS)
      pocket::redraw();
    return;
  }
  if (state.run.phase == marble::Phase::FALLING && state.run.fall_ms >= 650) {
    begin(state.run.level);
    return;
  }
  auto &motion = marble_motion::device;
  bool manual = dragging || manual_keys();
  if (motion.waiting() && !manual) {
    state.run.vx = state.run.vy = 0;
    pocket::redraw();
    return;
  }
  marble_motion::Vec tilt = motion.ready() && !manual
                                ? motion.tilt.input(motion.options)
                                : marble_motion::Vec{};
  marble::Input in = marble::tilt_from_screen(
      float(drag_x) / 30 + tilt.x, float(drag_y) / 30 + tilt.y,
      touch_brake || slow_brake || (!three_keys() && pocket::held(Key::A)));
  if (!three_keys() && pocket::held(Key::LEFT))
    in.x -= 1;
  if (!three_keys() && pocket::held(Key::RIGHT))
    in.x += 1;
  if (!three_keys() && pocket::held(Key::UP))
    in.y -= 1;
  if (!three_keys() && pocket::held(Key::DOWN))
    in.y += 1;
  unsigned e = marble::step(state, in, ms);
  if (e & marble::COLLECT) {
    gem_flash_ms = 600;
    log_run("collect");
    pocket::tone(760, .055f);
    pocket::tone(1120, .07f, pocket::millis() / 1000.f + .065f);
    pocket::save();
  }
  if (e & marble::BOUNCE)
    pocket::tone(220, .025f);
  if (e & marble::FALL) {
    release();
    log_run("fall");
    pocket::tone(150, .16f);
    pocket::save();
  }
  if (e & marble::WIN) {
    nav_focus = 5;
    slow_brake = false;
    release();
    log_run("win");
    nav_log();
    float now = pocket::millis() / 1000.f;
    pocket::tone(660, .09f);
    pocket::tone(880, .09f, now + .11f);
    pocket::tone(1320, .2f, now + .23f);
    pocket::save();
  }
  checkpoint_ms += ms;
  if (checkpoint_ms >= 2000 && state.run.phase != marble::Phase::WON) {
    checkpoint_ms = 0;
    pocket::save();
  }
  pocket::redraw();
}
void draw() {
  pocket::clear(DEEP);
  if (screen == ISLANDS)
    islands();
  else if (screen == CONTROLS)
    controls();
  else
    draw_race();
  if (three_keys() && screen != CONTROLS && nav_focus == 8)
    pocket::outline(pocket::WIDTH - 50, 1, 28, 22, GOLD);
}
size_t serialize(uint8_t *data, size_t capacity) {
  if (capacity < 104)
    return 0;
  if (marble::serialize(state, data, 88) != 88)
    return 0;
  memcpy(data + 88, "MCTL", 4);
  data[92] = 2;
  auto &o = marble_motion::device.options;
  data[93] = o.enabled;
  data[94] = o.sensitivity;
  data[95] = o.rotation;
  data[96] = o.mirror;
  data[97] = o.navigation;
  memset(data + 98, 0, 2);
  uint32_t hash = 2166136261u;
  for (unsigned i = 0; i < 100; ++i)
    hash = (hash ^ data[i]) * 16777619u;
  for (unsigned i = 0; i < 4; ++i)
    data[100 + i] = uint8_t(hash >> (8 * i));
  log_run("serialize");
  return 104;
}
void load(const uint8_t *data, size_t size) {
  marble_motion::Options options;
  bool valid = data && (size == 88 || size == 104);
  if (valid && size == 104) {
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 100; ++i)
      hash = (hash ^ data[i]) * 16777619u;
    uint32_t stored = 0;
    for (unsigned i = 0; i < 4; ++i)
      stored |= uint32_t(data[100 + i]) << (8 * i);
    valid = !memcmp(data + 88, "MCTL", 4) && (data[92] == 1 || data[92] == 2) &&
            data[93] <= 1 && data[94] < 3 && data[95] < 4 && data[96] <= 1 &&
            data[97] < 3 && (data[92] == 2 || !data[97]) && !data[98] &&
            !data[99] && hash == stored;
    if (valid) {
      options.enabled = data[93];
      options.sensitivity = data[94];
      options.rotation = data[95];
      options.mirror = data[96];
      options.navigation = data[97];
    }
  }
  marble::State candidate;
  if (valid && marble::load(candidate, data, 88)) {
    state = candidate;
    marble_motion::device.options = options;
    marble_motion::device.tilt.suspend();
    screen = RACE;
    nav_focus = state.run.phase == marble::Phase::WON ? 5 : 7;
    slow_brake = false;
    release();
    sync_motion();
    log_run("load");
    nav_log();
    pocket::redraw();
  } else
    pocket::log("Marble: rejected invalid save");
}
} // namespace game
