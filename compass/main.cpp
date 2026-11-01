// SPDX-License-Identifier: MIT
#include "../common/pocket-play/runtime.hpp"
#include "../common/pocket-studio/studio.hpp"
#include "model.hpp"
#include <cstdio>
namespace {
using namespace pocket;
using compass::device;
using compass::Quality;
constexpr auto BG = rgb(8, 17, 25), PANEL = rgb(17, 30, 40),
               EDGE = rgb(43, 60, 71), INK = rgb(235, 239, 229),
               DIM = rgb(131, 151, 160), GOLD = rgb(222, 184, 120),
               RED = rgb(242, 104, 95), GREEN = rgb(125, 212, 178);
enum Page { Measure, Calibrate, Settings, Guide, Clear };
Page page = Measure;
int pressed = -1, focus = 0;
bool keyboard = false;
uint32_t paint = 0, diagnostic = 0, elapsed = 0;
float needle = 0;
bool visual_valid = false;
void label(int x, int y, int w, const char *s, uint16_t c = INK, int scale = 1,
           unsigned align = 1) {
  studio::label(x, y, w, s, c, scale, align);
}
void button(int id, int x, int y, int w, int h, const char *s,
            bool active = false) {
  if (keyboard && focus == id)
    studio::round(x - 2, y - 2, w + 4, h + 4, 7, GOLD);
  studio::round(x, y, w, h, 5,
                pressed == id ? EDGE
                : active      ? rgb(39, 65, 68)
                              : PANEL);
  label(x + 3, y + (h - 12) / 2, w - 6, s, active ? GOLD : INK);
}
const char *state() {
  if (device.status == compass::Status::Denied)
    return tr("SENSOR ACCESS DENIED", "传感器未授权");
  if (device.status == compass::Status::Permission)
    return tr("ALLOW SENSOR ACCESS", "请允许传感器访问");
  if (device.status == compass::Status::Unavailable)
    return tr("NO MAGNETOMETER", "未检测到磁力计");
  if (device.status == compass::Status::NoGravity)
    return tr("NO ACCELEROMETER", "未检测到加速度计");
  if (device.status == compass::Status::Error)
    return tr("SENSOR UNAVAILABLE", "传感器连接中断");
  switch (device.quality) {
  case Quality::Held:
    return tr("READING HELD", "读数已锁定");
  case Quality::Calibrate:
    return tr("CALIBRATE TO BEGIN", "请先完成校准");
  case Quality::Moving:
    return tr("KEEP STILL", "请放稳设备");
  case Quality::Vertical:
    return tr("LOWER THE TOP EDGE", "请降低设备倾角");
  case Quality::Interference:
    return tr("MAGNETIC INTERFERENCE", "磁场异常 · 远离金属");
  case Quality::Good:
    return tr("TILT COMPENSATED", "实时 · 倾斜补偿");
  default:
    return tr("WAITING FOR SENSORS", "等待传感器读数");
  }
}
void header(const char *s) {
  clear(BG);
  label(14, 10, WIDTH - 72, s, INK, 2, 0);
  label(WIDTH - 62, 13, 48, "N / 01", GOLD);
  line(14, 34, WIDTH - 14, 34, EDGE);
}
void segment(int cx, int cy, float a, float r0, float r1, uint16_t c) {
  float x = std::sin(a), y = -std::cos(a);
  line(cx + int(x * r0), cy + int(y * r0), cx + int(x * r1), cy + int(y * r1),
       c);
}
void triangle(float x0, float y0, float x1, float y1, float x2, float y2,
              uint16_t c) {
  int lo = std::max(0, int(std::floor(std::min({y0, y1, y2})))),
      hi = std::min(HEIGHT - 1, int(std::ceil(std::max({y0, y1, y2}))));
  for (int y = lo; y <= hi; ++y) {
    float xs[3];
    int n = 0;
    float px[3] = {x0, x1, x2}, py[3] = {y0, y1, y2};
    for (int j = 0; j < 3; ++j) {
      int k = (j + 1) % 3;
      if ((py[j] <= y && py[k] > y) || (py[k] <= y && py[j] > y))
        xs[n++] = px[j] + (y - py[j]) * (px[k] - px[j]) / (py[k] - py[j]);
    }
    if (n == 2)
      line(int(std::ceil(std::min(xs[0], xs[1]))), y,
           int(std::floor(std::max(xs[0], xs[1]))), y, c);
  }
}
void dial(int cx, int cy, int r) {
  studio::disc(cx, cy, r + 2, EDGE);
  studio::disc(cx, cy, r, BG);
  studio::disc(cx, cy, r - 16, PANEL);
  studio::disc(cx, cy, r - 18, BG);
  bool valid = device.has_heading || device.held;
  float offset = valid ? needle : 0;
  for (int i = 0; i < 72; ++i) {
    float a = (i * 5 - offset) / compass::DEG;
    segment(cx, cy, a, r - (i % 6 ? 5 : 10), r - 2, i % 6 ? EDGE : DIM);
  }
  static const char *marks[] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; ++i) {
    float a = (i * 90 - offset) / compass::DEG;
    int x = cx + int(std::sin(a) * (r - 23)),
        y = cy - int(std::cos(a) * (r - 23));
    label(x - 12, y - 8, 24, marks[i], i ? INK : RED, 2);
  }
  triangle(cx, cy - r - 5, cx - 4, cy - r - 12, cx + 4, cy - r - 12, GOLD);
  if (valid) {
    float a = -offset / compass::DEG, ux = std::sin(a), uy = -std::cos(a),
          vx = -uy, vy = ux;
    float length = r * .48f;
    triangle(cx + ux * length, cy + uy * length, cx + vx * 5, cy + vy * 5,
             cx - vx * 5, cy - vy * 5, RED);
    triangle(cx - ux * length, cy - uy * length, cx + vx * 5, cy + vy * 5,
             cx - vx * 5, cy - vy * 5, INK);
  } else {
    label(cx - 24, cy - 5, 48, "--", DIM, 2);
  }
  studio::disc(cx, cy, 4, GOLD);
  studio::disc(cx, cy, 2, BG);
}
void measure() {
  header(tr("NORTHSTAR", "北辰指南针"));
  bool wide = WIDTH > HEIGHT * 1.25f;
  int r = wide ? std::min((HEIGHT - 94) / 2, (WIDTH - 150) / 2)
               : std::min((WIDTH - 44) / 2, (HEIGHT - 151) / 2);
  r = std::max(30, r);
  int cx = wide ? (WIDTH - 132) / 2 : WIDTH / 2,
      cy = wide ? (HEIGHT + 8) / 2 : 48 + r;
  dial(cx, cy, r);
  int x = wide ? WIDTH - 143 : 12, y = wide ? 67 : cy + r + 6,
      w = wide ? 135 : WIDTH - 24;
  char b[48];
  bool valid = device.has_heading || device.held;
  if (valid)
    std::snprintf(b, sizeof(b), "%03d", int(std::round(device.heading)) % 360);
  else
    std::snprintf(b, sizeof(b), "---");
  label(x, y, w, b, INK, wide ? 4 : 3);
  if (device.declination)
    std::snprintf(b, sizeof(b), tr("DEG / DECL. %+d", "度 / 磁偏角 %+d"),
                  device.declination);
  else
    std::snprintf(b, sizeof(b), "%s", tr("DEGREES / MAGNETIC", "度 / 磁北"));
  label(x, y + (wide ? 36 : 27), w, b, GOLD);
  int sy = HEIGHT - 55;
  label(wide ? x : 10, sy, wide ? w : WIDTH - 20, state(),
        device.quality == Quality::Good           ? GREEN
        : device.quality == Quality::Interference ? RED
                                                  : DIM);
  if (wide) {
    std::snprintf(b, sizeof(b), "%.1f uT", device.strength);
    label(x, HEIGHT - 82, w, b, DIM);
  }
  int bw = (WIDTH - 40) / 3, by = HEIGHT - 34;
  button(0, 10, by, bw, 25,
         device.held       ? tr("Live", "解锁")
         : !device.fresh() ? tr("Retry", "重试")
                           : tr("Hold", "锁定"),
         device.held);
  button(1, 20 + bw, by, bw, 25, tr("Calibrate", "校准"));
  button(2, 30 + bw * 2, by, bw, 25, tr("Settings", "设置"));
}
void calibration() {
  header(tr("CALIBRATE", "转动校准"));
  bool collecting = device.capture.collecting,
       ready = compass::valid_cal(device.capture.candidate);
  int y = 48;
  label(14, y, WIDTH - 28,
        tr("Turn slowly through all six faces", "请缓慢转动，依次朝向六个面"),
        GOLD);
  y += 18;
  label(14, y, WIDTH - 28,
        tr("Keep away from metal and magnets", "远离金属、磁铁和扬声器"), DIM);
  y += 18;
  label(14, y, WIDTH - 28,
        tr("Top / bottom / sides / front / back", "上、下、左、右、正面和背面"),
        DIM);
  int cy = std::max(y + 26, (HEIGHT + 10) / 2);
  int spacing = std::min(35, (WIDTH - 40) / 6),
      start = WIDTH / 2 - spacing * 5 / 2;
  unsigned faces = device.capture.faces;
  for (int i = 0; i < 6; ++i) {
    studio::disc(start + i * spacing, cy, 8,
                 unsigned(i) < faces ? GREEN : EDGE);
    char b[2] = {char('1' + i), 0};
    label(start + i * spacing - 6, cy - 5, 12, b,
          unsigned(i) < faces ? BG : DIM);
  }
  char b[72];
  std::snprintf(b, sizeof(b), "%u / 6   ·   %u %s", faces, device.capture.count,
                tr("samples", "个采样"));
  label(12, cy + 20, WIDTH - 24, b, DIM);
  label(12, HEIGHT - 65, WIDTH - 24,
        ready ? tr("READY · SAVE CALIBRATION", "采样合格 · 可以保存")
        : !device.fresh() ? state()
        : collecting
            ? tr("Keep turning slowly", "继续缓慢转动设备")
            : tr("Save to replace calibration", "保存前保留原校准"),
        ready ? GREEN : GOLD);
  int bw = (WIDTH - 30) / 2;
  button(3, 10, HEIGHT - 37, bw, 27, tr("Cancel", "取消"));
  button(4, 20 + bw, HEIGHT - 37, bw, 27,
         ready             ? tr("Save", "保存")
         : !device.fresh() ? tr("Retry", "重试")
         : collecting      ? tr("Restart", "重新开始")
                           : tr("Start", "开始采样"),
         ready);
}
void settings() {
  header(tr("SETTINGS", "仪表设置"));
  int w = WIDTH - 28;
  char b[64];
  std::snprintf(b, sizeof(b), "%s %u", tr("Magnetometer", "磁力计"),
                device.channel + 1);
  button(5, 14, 48, w, 28, b);
  std::snprintf(b, sizeof(b), "%s %+d", tr("Declination", "磁偏角"),
                device.declination);
  label(48, 89, WIDTH - 96, b, GOLD);
  button(6, 14, 82, 30, 28, "-");
  button(7, WIDTH - 44, 82, 30, 28, "+");
  if (HEIGHT >= 250)
    label(12, 118, WIDTH - 24,
          tr("East + / west -  ·  default 0", "东偏为正 / 西偏为负 · 默认 0"),
          DIM);
  int y = std::min(146, HEIGHT - 104);
  button(8, 14, y, w, 25, tr("Clear calibration", "清除当前校准"));
  button(9, 14, y + 32, w, 25, tr("How to use", "使用说明"));
  button(10, 14, HEIGHT - 37, w, 27, tr("Back", "返回"));
}
void guide() {
  header(tr("FIELD NOTES", "使用说明"));
  const char *lines[] = {
      tr("Travel direction: top edge.", "以屏幕上沿作为前进方向。"),
      tr("Slowly turn all six faces.",
         "缓慢转动六个面，采样合格后保存。"),
      tr("Magnetic north differs from true.",
         "默认指向磁北，并非地理真北。"),
      tr("Set declination only if known.", "已知当地磁偏角时，可手动修正。"),
      tr("Move away from magnets and steel.", "远离磁铁、钢铁及通电大电流线。"),
      tr("Too steep? Lower the top edge.", "倾角过大时，请降低设备上沿。"),
      tr("No sensor: no simulated heading.",
         "无传感器或信号中断时，不显示方位。")};
  for (int i = 0; i < 7; ++i)
    label(14, 48 + i * 20, WIDTH - 28, lines[i], i ? DIM : GOLD, 1, 0);
  button(10, 14, HEIGHT - 37, WIDTH - 28, 27, tr("Back", "返回"));
}
void navigate(Page next) {
  page = next;
  pressed = -1;
  focus = next == Measure     ? 0
          : next == Calibrate ? 3
          : next == Settings  ? 5
          : next == Clear     ? 11
                              : 10;
  redraw();
}
void act(int id) {
  if (id == 0) {
    if (!device.held && !device.fresh())
      ++device.retry;
    else
      device.toggle_hold();
    redraw();
  } else if (id == 1) {
    device.capture = {};
    device.held = false;
    navigate(Calibrate);
  } else if (id == 2)
    navigate(Settings);
  else if (id == 3) {
    device.capture.collecting = false;
    navigate(Measure);
  } else if (id == 4) {
    if (compass::valid_cal(device.capture.candidate)) {
      if (device.apply()) {
        save();
        log("Compass: calibration saved");
        navigate(Measure);
      }
    } else if (!device.fresh()) {
      ++device.retry;
    } else {
      device.capture.begin();
      log("Compass: calibration started");
    }
  } else if (id == 5) {
    device.select(1 - device.channel);
    save();
  } else if (id == 6 || id == 7) {
    device.declination =
        std::max(-30, std::min(30, device.declination + (id == 6 ? -1 : 1)));
    device.held = false;
    device.has_heading = false;
    device.last_compute = 0;
    save();
  } else if (id == 8)
    navigate(Clear);
  else if (id == 9)
    navigate(Guide);
  else if (id == 10)
    navigate(page == Guide ? Settings : Measure);
  else if (id == 11)
    navigate(Settings);
  else if (id == 12) {
    device.saved[device.channel] = {};
    device.held = false;
    device.has_heading = false;
    save();
    navigate(Settings);
  }
  redraw();
}
int hit(int x, int y) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
    return -1;
  if (page == Measure && y >= HEIGHT - 34 && y < HEIGHT - 9) {
    int bw = (WIDTH - 40) / 3;
    for (int i = 0; i < 3; ++i)
      if (x >= 10 + i * (bw + 10) && x < 10 + i * (bw + 10) + bw)
        return i;
  }
  if ((page == Calibrate || page == Clear) && y >= HEIGHT - 37 &&
      y < HEIGHT - 10) {
    if (x >= 10 && x < (WIDTH / 2 - 5))
      return page == Calibrate ? 3 : 11;
    if (x > WIDTH / 2 + 5 && x < WIDTH - 10)
      return page == Calibrate ? 4 : 12;
  }
  if (page == Settings) {
    int sy = std::min(146, HEIGHT - 104);
    if (y >= 48 && y < 76 && x >= 14 && x < WIDTH - 14)
      return 5;
    if (y >= 82 && y < 110) {
      if (x >= 14 && x < 44)
        return 6;
      if (x >= WIDTH - 44 && x < WIDTH - 14)
        return 7;
    }
    if (x >= 14 && x < WIDTH - 14) {
      if (y >= sy && y < sy + 25)
        return 8;
      if (y >= sy + 32 && y < sy + 57)
        return 9;
    }
  }
  if ((page == Settings || page == Guide) && y >= HEIGHT - 37 &&
      y < HEIGHT - 10 && x >= 14 && x < WIDTH - 14)
    return 10;
  return -1;
}
} // namespace
namespace game {
const char *title() { return pocket::tr("Northstar Compass", "北辰指南针"); }
const char *app_id() { return "pxa-compass"; }
void init() {
  device = compass::Model{};
  navigate(Measure);
}
void reset() {
  device.held = false;
  device.capture.collecting = false;
  ++device.retry;
  navigate(Measure);
}
void key(Key k) {
  keyboard = true;
  int first = page == Measure     ? 0
              : page == Calibrate ? 3
              : page == Settings  ? 5
              : page == Clear     ? 11
                                  : 10,
      last = page == Measure     ? 2
             : page == Calibrate ? 4
             : page == Settings  ? 10
             : page == Clear     ? 12
                                 : 10;
  if (k == Key::UP || k == Key::LEFT)
    focus = focus <= first ? last : focus - 1;
  else if (k == Key::DOWN || k == Key::RIGHT)
    focus = focus >= last ? first : focus + 1;
  else if (k == Key::A)
    act(focus);
  else if (k == Key::B) {
    device.capture.collecting = false;
    navigate(Measure);
  }
  redraw();
}
void pointer(Phase phase, int x, int y) {
  if (phase == Phase::DOWN) {
    keyboard = false;
    pressed = hit(x, y);
  } else if (phase == Phase::CANCEL)
    pressed = -1;
  else if (phase == Phase::UP) {
    int old = pressed;
    pressed = -1;
    if (old >= 0 && hit(x, y) == old)
      act(old);
  }
  redraw();
}
void tick(uint32_t ms) {
  device.update();
  elapsed += ms;
  paint += ms;
  diagnostic += ms;
  bool valid = device.has_heading || device.held;
  if (valid) {
    if (!visual_valid)
      needle = device.heading;
    else
      needle = compass::wrap(needle + compass::delta(device.heading, needle) *
                                          compass::needle_blend(ms));
  }
  visual_valid = valid;
  if (paint >= 50) {
    paint = 0;
    redraw();
  }
  if (diagnostic >= 2000) {
    diagnostic = 0;
    char b[240];
    std::snprintf(b, sizeof(b),
                  "Compass: state=%u quality=%u heading=%.1f field=%.1f "
                  "raw=%.2f,%.2f,%.2f gravity=%.3f,%.3f,%.3f calibrated=%u "
                  "samples=%u faces=%u",
                  unsigned(device.status), unsigned(device.quality),
                  device.heading, device.strength, device.tilt.raw.x,
                  device.tilt.raw.y, device.tilt.raw.z, device.tilt.gravity.x,
                  device.tilt.gravity.y, device.tilt.gravity.z,
                  device.saved[device.channel].valid, device.capture.count,
                  device.capture.faces);
    log(b);
  }
}
void draw() {
  if (page == Measure)
    measure();
  else if (page == Calibrate)
    calibration();
  else if (page == Settings)
    settings();
  else if (page == Guide)
    guide();
  else {
    header(tr("CLEAR CALIBRATION", "清除校准"));
    label(12, HEIGHT / 2 - 20, WIDTH - 24,
          tr("Clear this sensor's calibration?",
             "确定清除当前磁力计的校准？"),
          GOLD);
    int bw = (WIDTH - 30) / 2;
    button(11, 10, HEIGHT - 37, bw, 27, tr("Cancel", "取消"));
    button(12, 20 + bw, HEIGHT - 37, bw, 27, tr("Clear", "清除"));
  }
}
size_t serialize(uint8_t *p, size_t n) { return compass::encode(device, p, n); }
void load(const uint8_t *p, size_t n) {
  if (compass::decode(device, p, n))
    log("Compass: preferences restored");
  else
    log("Compass: rejected invalid preferences");
}
} // namespace game
