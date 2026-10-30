// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-studio/studio.hpp"
#include "model.hpp"
#include <stdio.h>
#include <string.h>
namespace {
using namespace pocket;
using level::device;
constexpr uint16_t BG0 = rgb(7, 19, 27), BG1 = rgb(15, 38, 47),
                   PANEL = rgb(18, 43, 52), EDGE = rgb(43, 72, 79),
                   INK0 = rgb(233, 243, 231), DIM = rgb(141, 169, 168),
                   MINT = rgb(119, 234, 190), GOLD = rgb(236, 197, 126),
                   COOL = rgb(127, 185, 209);
enum Screen { Measure, Settings, Calibration, Guide, ClearConfirm };
Screen screen = Measure;
level::Calibration calibration;
unsigned focus = 0, guide_page = 0;
int focus_action = 0;
bool nav_visible = true;
int pressed = -1;
uint32_t animation = 0, paint_due = 0, toast_ms = 0;
float bubble_x = 0, bubble_y = 0;
#ifdef PXA_POCKET_STUDIO
studio::RasterCache measure_base;
bool base_restored = false, capture_base = false;
#endif
const char *notice_en = "", *notice_zh = "";
void invalidate() { pocket::redraw(); }
bool portrait() { return HEIGHT >= WIDTH; }
int button_y() { return HEIGHT - 26; }
void report_focus() {
  char b[80];
  snprintf(b, sizeof(b), "Level: focus=%d screen=%u", focus_action,
           unsigned(screen));
  log(b);
}
void page(Screen next, int action) {
  screen = next;
  focus_action = action;
  pressed = -1;
  report_focus();
}
void changed() {
  invalidate();
  pocket::save();
}
void notice(const char *en, const char *zh) {
  notice_en = en;
  notice_zh = zh;
  toast_ms = 2000;
  invalidate();
}
const char *status() {
  if (device.held)
    return tr("READING HELD", "读数已锁定");
  if (device.status == level::Status::Connecting)
    return tr("CONNECTING", "正在连接体感");
  if (device.status == level::Status::Permission)
    return tr("ALLOW SENSOR ACCESS", "请允许使用体感");
  if (device.status == level::Status::Denied)
    return tr("MOTION NOT ALLOWED", "体感未授权");
  if (device.status == level::Status::Streaming) {
    if (!device.ready())
      return tr("WAITING FOR SENSOR", "等待读数更新");
    return device.tilt.stable(device.now)
               ? tr("LIVE · STEADY", "实时 · 已稳定")
               : tr("LIVE · SETTLING", "实时 · 请放稳");
  }
  return tr("NO MOTION SENSOR", "未检测到体感");
}
void label(int x, int y, int w, const char *s, uint16_t color = INK0,
           int scale = 1, unsigned align = 1) {
  studio::label(x, y, w, s, color, scale, align);
}
void button(int id, int x, int y, int w, int h, const char *text,
            bool active = false) {
  uint16_t fill = active ? rgb(34, 90, 77) : PANEL;
  if (pressed == id)
    fill = studio::mix(fill, 0, 60);
  if (nav_visible && focus_action == id)
    studio::round(x - 2, y - 2, w + 4, h + 4, 7, GOLD);
  studio::button(x, y, w, h, text, fill, active ? MINT : INK0,
                 active || pressed == id);
}
void header(const char *title) {
#ifdef PXA_POCKET_STUDIO
  if (!base_restored) {
#endif
  studio::gradient(0, 0, WIDTH, HEIGHT, BG0, BG1);
  studio::round(12, 28, WIDTH - 24, 1, 0, EDGE);
#ifdef PXA_POCKET_STUDIO
  }
#endif
  label(12, 5, screen == Measure ? (portrait() ? 74 : 88) : WIDTH - 60, title,
        INK0, 2, 0);
}
void tick_line(int cx, int cy, int radius, float angle, int length,
               uint16_t color) {
  float x = sinf(angle), y = -cosf(angle);
  line(cx + int(x * (radius - length)), cy + int(y * (radius - length)),
       cx + int(x * radius), cy + int(y * radius), color);
}
void dial(bool valid, bool good, int x, int y) {
#ifdef PXA_POCKET_STUDIO
  if (!base_restored) {
#endif
  studio::glow(x - 11, y - 16, 78, COOL, 19);
  studio::disc(x, y + 3, 67, rgb(3, 12, 18));
  studio::disc(x, y, 67, EDGE);
  studio::disc(x, y, 65, rgb(39, 64, 70));
  studio::disc(x, y - 1, 63, rgb(16, 38, 46));
  studio::disc(x, y, 60, rgb(8, 28, 37));
  for (int i = 0; i < 60; ++i)
    tick_line(x, y, 63, i * level::PI / 30, i % 5 ? 3 : 7,
              i % 5 ? rgb(62, 88, 93) : rgb(164, 189, 181));
  studio::disc(x, y, 49, rgb(33, 69, 68));
  studio::disc(x, y, 48, rgb(11, 42, 46));
  // Inner etched scale, then an unobstructed center target.
  for (int a = 0; a < 64; ++a) {
    float t = a * level::PI / 32;
    pixel(x + int(cosf(t) * 25), y + int(sinf(t) * 25), EDGE);
  }
  line(x - 46, y, x + 46, y, rgb(51, 84, 85));
  line(x, y - 46, x, y + 46, rgb(51, 84, 85));
#ifdef PXA_POCKET_STUDIO
    if (capture_base) measure_base.capture();
  }
#endif
  for (int a = 0; a < 32; ++a) {
    float t = a * level::PI / 16;
    pixel(x + int(cosf(t) * 9), y + int(sinf(t) * 9), good ? MINT : COOL);
  }
  if (valid) {
    int bx = x + int(bubble_x), by = y + int(bubble_y);
    studio::glow(bx, by, 23, good ? MINT : COOL, good ? 65 : 38);
    studio::disc(bx, by + 2, 13, rgb(3, 23, 30));
    studio::disc(bx, by, 13, good ? rgb(142, 243, 193) : rgb(133, 204, 208));
    studio::disc(bx, by - 1, 11, good ? rgb(74, 171, 134) : rgb(57, 138, 153));
    studio::disc(bx - 2, by - 3, 8,
                 good ? rgb(167, 247, 204) : rgb(173, 230, 224));
    studio::round(bx - 6, by - 7, 7, 3, 1, rgb(230, 255, 240));
    line(x - 5, y, x + 5, y, INK0);
    line(x, y - 5, x, y + 5, INK0);
  } else {
    studio::round(x - 11, y - 10, 22, 20, 4, EDGE);
    studio::round(x - 7, y - 6, 14, 12, 2, BG1);
    for (int i = -1; i <= 1; ++i) {
      line(x - 15, y + i * 5, x - 11, y + i * 5, DIM);
      line(x + 11, y + i * 5, x + 15, y + i * 5, DIM);
    }
  }
}
void bottom() {
  int w = (WIDTH - 48) / 3, y = button_y();
  button(0, 12, y, w, 22, device.held ? tr("Live", "解锁") : tr("Hold", "锁定"),
         device.held);
  button(1, 24 + w, y, w, 22,
         device.relative ? tr("ABS", "绝对") : tr("Zero", "归零"),
         device.relative);
  button(2, 36 + 2 * w, y, w, 22,
         device.prefs.mode ? tr("Bubble", "双轴") : tr("Edge", "边尺"));
}
void bubble_view(level::Reading r) {
  bool good = r.valid && r.stable && r.total <= device.tolerance();
  bool port = portrait();
  int cx = port ? WIDTH / 2 : (WIDTH - 124) / 2,
      cy = port ? (HEIGHT - 52) / 2 : 116;
  dial(r.valid, good, cx, cy);
  int w = port ? (WIDTH - 32) / 2 : 118, x1 = port ? 12 : WIDTH - 130,
      x2 = port ? 20 + w : WIDTH - 130, y1 = port ? HEIGHT - 94 : 48,
      y2 = port ? HEIGHT - 94 : 106, h = port ? 46 : 51;
  studio::card(x1, y1, w, h, PANEL, EDGE, 7);
  studio::card(x2, y2, w, h, PANEL, EDGE, 7);
  if (!port)
    studio::round(WIDTH - 130, 164, 118, 19, 5,
                  good ? rgb(31, 77, 62) : rgb(38, 57, 62));
  char b[64];
  label(x1 + 8, y1 + 6, w - 16, tr("X · LEFT / RIGHT", "X · 左右"), DIM, 1, 0);
  level::format_value(b, sizeof(b), r.x, device.prefs.unit, r.valid);
  label(x1 + 4, y1 + (port ? 21 : 24), w - 8, b, r.valid ? INK0 : DIM, 2);
  label(x2 + 8, y2 + 6, w - 16, tr("Y · FRONT / BACK", "Y · 前后"), DIM, 1, 0);
  level::format_value(b, sizeof(b), r.y, device.prefs.unit, r.valid);
  label(x2 + 4, y2 + (port ? 21 : 24), w - 8, b, r.valid ? INK0 : DIM, 2);
  if (!r.valid)
    snprintf(b, sizeof(b), "%s", tr("Tap dial to retry", "点圆盘重试"));
  else if (good)
    snprintf(b, sizeof(b), "%s",
             device.relative ? tr("REFERENCE ALIGNED", "已对齐基准")
                             : tr("LEVEL", "已水平"));
  else
    snprintf(b, sizeof(b), tr("Tilt %.1f°", "倾斜 %.1f°"), double(r.total));
  if (port) {
    char line[96];
    snprintf(line, sizeof(line), "%s · ±%.1f°", b, double(device.tolerance()));
    label(12, HEIGHT - 41, WIDTH - 24, line, good ? MINT : DIM);
  } else {
    label(WIDTH - 126, 168, 110, b, good ? MINT : DIM);
    label(WIDTH - 130, 186, 118,
          device.relative ? tr("RELATIVE REFERENCE", "相对基准")
                          : tr("GRAVITY REFERENCE", "重力基准"),
          DIM);
  }
  label(cx - 35, cy + 55, 70,
        r.valid ? tr("SCALE ±5°", "刻度 ±5°") : tr("NO SENSOR", "等待传感器"),
        DIM);
  if (!r.valid)
    label(cx - 48, cy + 24, 96, tr("TAP TO RETRY", "轻点重试"), DIM);
  if (nav_visible && focus_action == 8)
    outline(cx - 69, cy - 69, 138, 138, GOLD);
}
void edge_view(level::Reading r) {
  float angle = device.prefs.axis ? r.y : r.x,
        error = level::edge_error(angle, device.relative);
  bool vertical = !device.relative && fabsf(angle) > 45,
       good = r.valid && r.stable && fabsf(error) <= device.tolerance();
  int cy = portrait() ? HEIGHT - 85 : 151, vw = WIDTH - 68, cx = WIDTH / 2;
  studio::card(12, 48, WIDTH - 24, HEIGHT - 87, PANEL, EDGE, 9);
  studio::round(34, cy, vw, 20, 9, rgb(6, 26, 33));
  studio::round(36, cy + 2, vw - 4, 16, 7, rgb(28, 66, 65));
  line(cx, cy - 1, cx, cy + 22, good ? MINT : COOL);
  line(cx - 10, cy + 1, cx - 10, cy + 20, EDGE);
  line(cx + 10, cy + 1, cx + 10, cy + 20, EDGE);
  for (int i = -4; i <= 4; ++i)
    if (i) {
      int x = cx + i * (vw / 10);
      line(x, cy + 3, x, cy + 7, EDGE);
      line(x, cy + 15, x, cy + 19, EDGE);
    }
  if (r.valid) {
    float limit = vw / 2.f - 12, x = error / 5 * limit;
    if (x > limit)
      x = limit;
    if (x < -limit)
      x = -limit;
    studio::round(cx - 12 + int(x), cy + 3, 24, 14, 7,
                  good ? MINT : rgb(129, 201, 203));
    studio::round(cx - 9 + int(x), cy + 5, 13, 3, 1, rgb(221, 255, 233));
  }
  label(24, 57, WIDTH - 48,
        device.prefs.axis ? tr("Y EDGE · FRONT / BACK", "竖边 Y · 前后方向")
                          : tr("X EDGE · LEFT / RIGHT", "横边 X · 左右方向"),
        DIM);
  char b[64];
  level::format_value(b, sizeof(b), angle, device.prefs.unit, r.valid);
  label(28, portrait() ? 100 : 82, WIDTH - 56, b, r.valid ? INK0 : DIM, 4);
  if (r.valid)
    snprintf(b, sizeof(b),
             vertical ? tr("%.1f° from vertical", "距垂直 %.1f°")
                      : tr("%.1f° from level", "距水平 %.1f°"),
             double(fabsf(error)));
  else
    snprintf(b, sizeof(b), "%s", tr("Waiting for live data", "等待有效读数"));
  if (r.valid)
    label(24, portrait() ? 163 : 132, WIDTH - 48, b, good ? MINT : DIM);
  else
    button(8, 38, portrait() ? 160 : 127, WIDTH - 76, 19,
           tr("Retry sensor", "重试连接传感器"));
  button(9, WIDTH - 104, 31, 92, 14,
         device.prefs.axis ? tr("Axis: Y ↔ X", "轴向：Y ↔ X")
                           : tr("Axis: X ↔ Y", "轴向：X ↔ Y"));
}
void measure() {
#ifdef PXA_POCKET_STUDIO
  capture_base = !device.prefs.mode;
  base_restored = capture_base && measure_base.restore_frame();
#endif
  header(tr("LEVEL", "水平仪"));
  auto r = device.read();
  if (device.prefs.mode)
    edge_view(r);
  else
    bubble_view(r);
  button(3, WIDTH - 100, 3, 44, 20, tr("Set", "设置"));
  bottom();
  char b[56];
  snprintf(b, sizeof(b), tr("Tolerance ±%.1f°", "容差 ±%.1f°"),
           double(device.tolerance()));
  label(12, 33,
        portrait() && device.prefs.mode ? 112
        : portrait()                    ? WIDTH - 24
                                        : 163,
        toast_ms ? tr(notice_en, notice_zh) : status(),
        device.held ? GOLD
        : r.stable  ? MINT
                    : DIM,
        1, 0);
  if (!device.prefs.mode && !portrait())
    label(WIDTH - 106, 33, 94, b, DIM);
  label(portrait() ? 86 : WIDTH - 183, 9, portrait() ? 48 : 74,
        device.relative ? tr(portrait() ? "REL" : "RELATIVE",
                             portrait() ? "相对" : "相对测量")
                        : tr(portrait() ? "ABS" : "ABSOLUTE",
                             portrait() ? "绝对" : "绝对测量"),
        device.relative ? GOLD : DIM);
}
void settings() {
  header(tr("SETTINGS", "仪表设置"));
  label(12, 34, WIDTH - 24, status(), DIM);
  char b[80];
  int top = portrait() ? 58 : 54,
      step = portrait() ? (HEIGHT - 110) / 6 : (HEIGHT - 80) / 6,
      h = step - (portrait() ? 4 : 3);
  for (unsigned i = 0; i < 6; ++i) {
    if (i == 0)
      snprintf(b, sizeof(b), tr("Unit: %s", "读数单位：%s"),
               device.prefs.unit ? tr("SLOPE %", "坡度 %")
                                 : tr("ANGLE °", "角度 °"));
    if (i == 1)
      snprintf(b, sizeof(b), tr("Tolerance: ±%.1f°", "水平容差：±%.1f°"),
               double(device.tolerance()));
    if (i == 2)
      snprintf(b, sizeof(b), tr("Display axes: %u°", "轴向旋转：%u°"),
               device.prefs.rotation * 90u);
    if (i == 3)
      snprintf(b, sizeof(b), "%s",
               tr("Level-surface alignment >", "在水平面上标定 >"));
    if (i == 4)
      snprintf(b, sizeof(b), "%s", tr("How to measure >", "使用与测量说明 >"));
    if (i == 5)
      snprintf(b, sizeof(b), "%s", tr("Back to measuring", "返回测量"));
    button(20 + int(i), 24, top + int(i) * step, WIDTH - 48, h, b,
           nav_visible && focus_action == 20 + int(i));
  }
  if (portrait())
    label(18, HEIGHT - 37, WIDTH - 36,
          tr("Up / Down select · Confirm to use", "上/下选择 · 确认执行"), DIM);
  label(
      18, HEIGHT - 17, WIDTH - 36,
      device.prefs.calibrated
          ? tr("SAVED ALIGNMENT · 0.1° DISPLAY", "已保存标定 · 显示分辨率0.1°")
          : tr("GRAVITY REFERENCE · 0.1° DISPLAY", "重力基准 · 显示分辨率0.1°"),
      DIM);
}
void calibration_view() {
  header(tr("ALIGNMENT", "仪器标定"));
  int cx = WIDTH / 2, table = portrait() ? 96 : 80, progress_y = HEIGHT - 81,
      by = HEIGHT - 51, w = (WIDTH - 48) / 2;
  studio::card(20, 47, WIDTH - 40, HEIGHT - 107, PANEL, EDGE, 9);
  studio::round(cx - 78, table, 156, 12, 5, rgb(40, 73, 75));
  line(cx - 79, table + 14, cx + 79, table + 14, DIM);
  studio::round(cx - 39, table - 14, 78, 15, 4, rgb(91, 129, 126));
  studio::round(cx - 33, table - 11, 66, 7, 2, MINT);
  studio::round(44, progress_y, WIDTH - 88, 5, 2, EDGE);
  int progress = int(calibration.progress() * (WIDTH - 90));
  if (progress)
    studio::round(45, progress_y + 1, progress, 3, 1, MINT);
  const char *title =
      calibration.stage == level::Calibration::Complete
          ? tr("READY TO SAVE", "标定采样完成")
      : calibration.stage == level::Calibration::Collecting
          ? tr("KEEP STILL FOR TWO SECONDS", "请保持静止约两秒")
          : tr("USE A KNOWN LEVEL SURFACE", "请使用已知水平的平面");
  label(28, portrait() ? 128 : 102, WIDTH - 56, title, MINT);
  label(28, portrait() ? 152 : 122, WIDTH - 56,
        tr("Screen up. Keep still.", "屏幕朝上放平，勿移动。"), INK0);
  label(20, 32, WIDTH - 40,
        toast_ms ? tr(notice_en, notice_zh)
                 : tr("Sets the absolute reference.",
                      "此项修正仪器基准，不是相对归零。"),
        DIM);
  button(30, 20, by, w, 25,
         calibration.stage == level::Calibration::Complete
             ? tr("Save", "保存标定")
         : calibration.stage == level::Calibration::Collecting
             ? tr("Restart", "重新采样")
             : tr("Start", "开始采样"),
         true);
  button(31, 28 + w, by, w, 25, tr("Cancel", "取消"));
  button(32, WIDTH / 2 - 68, HEIGHT - 22, 136, 18,
         tr("Clear saved alignment", "清除已保存的标定"));
}
void clear_confirm() {
  header(tr("CLEAR ALIGNMENT", "清除标定"));
  int w = (WIDTH - 104) / 2;
  studio::card(28, 57, WIDTH - 56, 115, PANEL, EDGE, 8);
  label(40, 75, WIDTH - 80,
        tr("Clear stored alignment?", "恢复传感器原始基准？"), INK0);
  label(40, 97, WIDTH - 80,
        tr("Display settings stay.", "单位、容差与轴向设置保留。"), DIM);
  button(40, 40, 131, w, 27, tr("Keep", "保留"));
  button(41, 64 + w, 131, w, 27, tr("Clear", "清除"), true);
}
void wrap_guide(int &y, const char *value) {
  while (*value) {
    char line[192];
    unsigned n = 0, space = 0;
    while (value[n]) {
      unsigned char c = value[n];
      unsigned bytes = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
      if (n + bytes >= sizeof(line))
        break;
      memcpy(line + n, value + n, bytes);
      line[n + bytes] = 0;
      if (text_width(line) > WIDTH - 36)
        break;
      if (value[n] == ' ')
        space = n;
      n += bytes;
    }
    if (!n)
      return;
    if (value[n] && space)
      n = space;
    memcpy(line, value, n);
    line[n] = 0;
    label(18, y, WIDTH - 36, line, DIM, 1, 0);
    y += 13;
    value += n;
    while (*value == ' ')
      ++value;
  }
}
void guide() {
  header(tr("A SMALL GUIDE", "测量说明"));
  const char *lines[] = {tr("ABS uses gravity as the level reference.",
                            "绝对测量以重力方向判断水平。"),
                         tr("ZERO compares with a chosen slope.",
                            "归零用于比较坡度，显示为相对。"),
                         tr("HOLD keeps a reading. Tap again for live.",
                            "锁定保留读数，再点一次恢复实时。"),
                         tr("EDGE checks level or vertical; choose X/Y.",
                            "边尺测水平与垂直，可切换X/Y。"),
                         tr("CALIBRATE only on a known level surface.",
                            "仪器标定须使用已知水平的平面。"),
                         tr("0.1° display does not guarantee accuracy.",
                            "显示0.1°不等于测量误差为0.1°。"),
                         tr("Keep still; movement affects readings.",
                            "测量时放稳，移动会影响读数。"),
                         tr("Up/Down choose. Confirm performs an action.",
                            "三键操作：上/下选择，确认执行。")};
  int y = 45;
  for (unsigned i = guide_page * 4; i < guide_page * 4 + 4; ++i) {
    wrap_guide(y, lines[i]);
    y += 6;
  }
  char b[16];
  snprintf(b, sizeof(b), "%u / 2", guide_page + 1);
  label(18, HEIGHT - 50, WIDTH - 36, b, DIM);
  int w = (WIDTH - 36) / 2;
  button(52, 12, HEIGHT - 34, w, 24,
         guide_page ? tr("First page", "第一页") : tr("Next page", "下一页"));
  button(50, 24 + w, HEIGHT - 34, w, 24, tr("Back", "返回"));
}
bool inside(int x, int y, int left, int top, int w, int h) {
  return x >= left && x < left + w && y >= top && y < top + h;
}
int hit(int x, int y) {
  if (x < 4 || x >= WIDTH - 4 || y < 0 || y >= HEIGHT)
    return -1;
  if (screen == Measure) {
    if (inside(x, y, WIDTH - 100, 3, 44, 20))
      return 3;
    int w = (WIDTH - 48) / 3;
    if (inside(x, y, 12, button_y(), w, 24))
      return 0;
    if (inside(x, y, 24 + w, button_y(), w, 24))
      return 1;
    if (inside(x, y, 36 + 2 * w, button_y(), w, 24))
      return 2;
    if (device.prefs.mode && inside(x, y, WIDTH - 104, 31, 92, 14))
      return 9;
    if (device.prefs.mode && !device.ready() &&
        inside(x, y, 38, portrait() ? 160 : 127, WIDTH - 76, 19))
      return 8;
    if (!device.prefs.mode && !device.ready() &&
        inside(x, y, (portrait() ? WIDTH / 2 : (WIDTH - 124) / 2) - 64,
               (portrait() ? (HEIGHT - 52) / 2 : 116) - 64, 128, 130))
      return 8;
  } else if (screen == Settings) {
    int top = portrait() ? 58 : 54,
        step = portrait() ? (HEIGHT - 110) / 6 : (HEIGHT - 80) / 6,
        h = step - (portrait() ? 4 : 3);
    if (inside(x, y, 24, top, WIDTH - 48, step * 5 + h) && (y - top) % step < h)
      return 20 + (y - top) / step;
  } else if (screen == Calibration) {
    int w = (WIDTH - 48) / 2;
    if (inside(x, y, 20, HEIGHT - 51, w, 25))
      return 30;
    if (inside(x, y, 28 + w, HEIGHT - 51, w, 25))
      return 31;
    if (inside(x, y, WIDTH / 2 - 68, HEIGHT - 22, 136, 20))
      return 32;
  } else if (screen == ClearConfirm) {
    int w = (WIDTH - 104) / 2;
    if (inside(x, y, 40, 131, w, 27))
      return 40;
    if (inside(x, y, 64 + w, 131, w, 27))
      return 41;
  } else if (screen == Guide) {
    int w = (WIDTH - 36) / 2;
    if (inside(x, y, 12, HEIGHT - 34, w, 24))
      return 52;
    if (inside(x, y, 24 + w, HEIGHT - 34, w, 24))
      return 50;
  }
  return -1;
}
int focus_items(int *items) {
  int n = 0;
  if (screen == Measure) {
    items[n++] = 0;
    items[n++] = 1;
    items[n++] = 2;
    if (device.prefs.mode)
      items[n++] = 9;
    if (!device.ready())
      items[n++] = 8;
    items[n++] = 3;
  }
  if (screen == Settings)
    for (int i = 20; i <= 25; ++i)
      items[n++] = i;
  if (screen == Calibration) {
    items[n++] = 30;
    items[n++] = 31;
    items[n++] = 32;
  }
  if (screen == ClearConfirm) {
    items[n++] = 40;
    items[n++] = 41;
  }
  if (screen == Guide) {
    items[n++] = 52;
    items[n++] = 50;
  }
  items[n++] = 6;
  return n;
}
void move_focus(int delta) {
  int items[10], n = focus_items(items), at = 0;
  for (int i = 0; i < n; ++i)
    if (items[i] == focus_action)
      at = i;
  focus_action = items[(at + n + delta) % n];
  nav_visible = true;
  report_focus();
  invalidate();
}

void act(int action) {
  if (action == 0) {
    if (!device.toggle_hold())
      notice("Wait for a reading.", "请等待有效读数。");
    else
      log(device.held ? "Level: hold" : "Level: live");
  } else if (action == 1) {
    if (!device.toggle_relative())
      notice("Wait for a reading.", "请等待有效读数。");
    else
      log(device.relative ? "Level: relative" : "Level: absolute");
  } else if (action == 2) {
    device.prefs.mode ^= 1;
    changed();
  } else if (action == 3) {
    page(Settings, 20);
    focus = 0;
  } else if (action == 6) {
    pocket::open_menu();
  } else if (action == 8) {
    ++device.retry;
    notice("Reconnecting...", "正在重新连接体感…");
  } else if (action == 9) {
    device.prefs.axis ^= 1;
    changed();
  } else if (action >= 20 && action <= 25) {
    focus = unsigned(action - 20);
    if (action == 20)
      device.prefs.unit ^= 1;
    if (action == 21)
      device.prefs.tolerance = (device.prefs.tolerance + 1) % 4;
    if (action == 22)
      device.prefs.rotation = (device.prefs.rotation + 1) % 4;
    if (action == 23) {
      calibration = {};
      page(Calibration, 30);
    }
    if (action == 24) {
      guide_page = 0;
      page(Guide, 52);
    }
    if (action == 25)
      page(Measure, 3);
    changed();
  } else if (action == 30) {
    if (calibration.stage == level::Calibration::Complete) {
      device.apply(calibration);
      page(Settings, 23);
      notice("Alignment saved.", "仪器标定已保存。");
      changed();
      log("Level: alignment saved");
    } else if (!device.ready())
      notice("Wait for a reading.", "请等待有效读数。");
    else if (device.tilt.previous.z < .9659f)
      notice("Place screen-up on a level surface.",
             "请屏幕朝上，放在水平面上。");
    else {
      calibration.begin();
      log("Level: alignment capture");
    }
  } else if (action == 31) {
    calibration = {};
    page(Settings, 23);
  } else if (action == 32)
    page(ClearConfirm, 40);
  else if (action == 40)
    page(Calibration, 32);
  else if (action == 41) {
    device.clear_calibration();
    page(Settings, 23);
    notice("Original reference restored.", "已恢复传感器原始基准。");
    changed();
  } else if (action == 52) {
    guide_page ^= 1;
  } else if (action == 50)
    page(Settings, 24);
  invalidate();
}
} // namespace
namespace game {
const char *title() { return pocket::tr("Spirit Level", "水平仪"); }
const char *app_id() { return "pxa-spirit-level"; }
void init() {
  device = level::Model{};
  screen = Measure;
  pressed = -1;
  focus = 0;
  focus_action = 0;
  nav_visible = true;
  guide_page = 0;
  animation = paint_due = toast_ms = 0;
  bubble_x = bubble_y = 0;
  calibration = {};
  pocket::save();
  report_focus();
}
void reset() {
  device.absolute();
  page(Measure, 0);
  pressed = -1;
  calibration = {};
  notice("Absolute mode.", "已恢复绝对重力基准。");
}
void key(pocket::Key key) {
  if (key == Key::UP || key == Key::DOWN) {
    move_focus(key == Key::UP ? -1 : 1);
    return;
  }
  if (key == Key::A) {
    nav_visible = true;
    act(focus_action);
    report_focus();
    return;
  }
  if (screen == Measure) {
    if (key == Key::B)
      act(1);
    else if (key == Key::X)
      act(2);
    else if (key == Key::Y || key == Key::SELECT)
      act(3);
    else if (key == Key::L || key == Key::R) {
      if (device.prefs.mode)
        act(9);
      else {
        device.prefs.unit ^= 1;
        changed();
      }
    }
  } else if (screen == Settings) {
    if (key == Key::B)
      act(25);
    else if (key == Key::LEFT || key == Key::RIGHT)
      act(focus_action);
  } else if (screen == Calibration && key == Key::B)
    act(31);
  else if (screen == ClearConfirm && key == Key::B)
    act(40);
  else if (screen == Guide && key == Key::B)
    act(50);
}
void pointer(pocket::Phase phase, int x, int y) {
  if (phase == Phase::DOWN) {
    nav_visible = false;
    pressed = hit(x, y);
  } else if (phase == Phase::CANCEL)
    pressed = -1;
  else if (phase == Phase::UP) {
    int target = pressed;
    pressed = -1;
    if (target >= 0 && target == hit(x, y)) {
      focus_action = target;
      act(target);
    }
  }
  invalidate();
}
void tick(uint32_t ms) {
  int items[10], count = focus_items(items);
  bool found = false;
  for (int i = 0; i < count; ++i)
    found |= items[i] == focus_action;
  if (!found) {
    focus_action = items[0];
    report_focus();
  }
  animation += ms;
#ifdef PXA_POCKET_STUDIO
  static uint32_t cache_report_at = 0;
  if (animation - cache_report_at >= 5000) {
    cache_report_at = animation;
    char report[192];
    snprintf(report, sizeof(report), "Level raster cache hits=%u misses=%u failures=%u bytes=%u copy_mean_us=%u",
             measure_base.hits(), measure_base.misses(), measure_base.allocation_failures(),
             unsigned(measure_base.bytes()), measure_base.mean_copy_us());
    pocket::log(report);
  }
#endif
  paint_due += ms;
  toast_ms = ms >= toast_ms ? 0 : toast_ms - ms;
  auto r = device.read();
  // Meter Y points up; raster Y points down. This affects the bubble's
  // presentation, not sensor axes, measured angles or calibration.
  float x = r.valid ? r.x / 5 * 42 : 0, y = r.valid ? -r.y / 5 * 42 : 0;
  float distance = hypotf(x, y);
  if (distance > 43) {
    x *= 43 / distance;
    y *= 43 / distance;
  }
  const float blend = level::bubble_blend(ms);
  bubble_x += (x - bubble_x) * blend;
  bubble_y += (y - bubble_y) * blend;
  if (screen == Calibration &&
      calibration.stage == level::Calibration::Collecting) {
    if (!device.ready())
      calibration.reset_window();
    else if (calibration.sample(device.tilt.previous, device.tilt.received,
                                device.tilt.stable(device.now)))
      log("Level: alignment captured");
  }
  if (paint_due >= 20) {
    paint_due = 0;
    invalidate();
  }
}
void draw() {
#ifdef PXA_POCKET_STUDIO
  base_restored = capture_base = false;
#endif
  if (screen == Measure)
    measure();
  else if (screen == Settings)
    settings();
  else if (screen == Calibration)
    calibration_view();
  else if (screen == Guide)
    guide();
  else
    clear_confirm();
  if (nav_visible && focus_action == 6)
    outline(WIDTH - 50, 1, 28, 22, GOLD);
}
size_t serialize(uint8_t *p, size_t n) {
  return level::encode(device.prefs, p, n);
}
void load(const uint8_t *p, size_t n) {
  level::Preferences candidate;
  if (level::decode(candidate, p, n)) {
    device.prefs = candidate;
    device.absolute();
    page(Measure, 0);
    calibration = {};
    invalidate();
    log("Level: preferences restored");
  } else
    log("Level: rejected invalid preferences");
}
} // namespace game
