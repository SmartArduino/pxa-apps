// SPDX-License-Identifier: CC0-1.0
#include "../../common/pocket-play/runtime.hpp"
#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
namespace test_runtime {
uint16_t guarded[480 * 320 + 2];
uint16_t *pixels = guarded + 1;
bool drawing = false;
int focus_id = 0, screen_id = 0, menu_requests = 0;
unsigned saves = 0, redraws = 0;
uint32_t clock = 0;
struct Label {
  int x, y, w, h;
  std::string value;
};
std::vector<Label> labels;
void begin() {
  std::fill(guarded, guarded + pocket::WIDTH * pocket::HEIGHT + 2,
            uint16_t(0x5aa5));
  labels.clear();
  drawing = true;
}
void finish() {
  drawing = false;
  assert(guarded[0] == 0x5aa5 &&
         guarded[pocket::WIDTH * pocket::HEIGHT + 1] == 0x5aa5);
}
bool visible(const char *s) {
  for (auto &l : labels)
    if (l.value == s)
      return true;
  return false;
}
} // namespace test_runtime
namespace pocket {
void pixel(int x, int y, uint16_t c) {
  assert(test_runtime::drawing);
  if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
    test_runtime::pixels[y * WIDTH + x] = c;
}
void rect(int x, int y, int w, int h, uint16_t c) {
  assert(w >= 0 && h >= 0);
  for (int j = y; j < y + h; ++j)
    for (int i = x; i < x + w; ++i)
      pixel(i, j, c);
}
void clear(uint16_t c) { rect(0, 0, WIDTH, HEIGHT, c); }
void outline(int x, int y, int w, int h, uint16_t c) {
  rect(x, y, w, 1, c);
  rect(x, y + h - 1, w, 1, c);
  rect(x, y, 1, h, c);
  rect(x + w - 1, y, 1, h, c);
}
void line(int x, int y, int xx, int yy, uint16_t c) {
  int dx = abs(xx - x), sx = x < xx ? 1 : -1, dy = -abs(yy - y),
      sy = y < yy ? 1 : -1, err = dx + dy;
  for (;;) {
    pixel(x, y, c);
    if (x == xx && y == yy)
      break;
    int e = 2 * err;
    if (e >= dy) {
      err += dy;
      x += sx;
    }
    if (e <= dx) {
      err += dx;
      y += sy;
    }
  }
}
int text_width(const char *s, int scale) {
  return pocket_ui::text_width(s, scale);
}
void text(int x, int y, const char *s, uint16_t c, int scale) {
  assert(test_runtime::drawing);
  int h = 7;
  for (const char *p = s; *p;)
    if (pocket_ui::next_utf8(p) >= 128)
      h = 10;
  h *= scale;
  int w = text_width(s, scale);
  if (x < 0 || y < 0 || x + w > WIDTH || y + h > HEIGHT) {
    fprintf(stderr, "TEXT BOUNDS x=%d y=%d w=%d h=%d: %s\n", x, y, w, h, s);
    assert(false);
  }
  for (auto &old : test_runtime::labels)
    if (!(x + w <= old.x || old.x + old.w <= x || y + h <= old.y ||
          old.y + old.h <= y)) {
      fprintf(stderr, "TEXT OVERLAP: %s / %s\n", s, old.value.c_str());
      assert(false);
    }
  test_runtime::labels.push_back({x, y, w, h, s});
  pocket_ui::draw_text(x, y, s, scale, [=](int i, int j) { pixel(i, j, c); });
}
void open_menu() { ++test_runtime::menu_requests; }
void redraw() { ++test_runtime::redraws; }
void save() { ++test_runtime::saves; }
uint32_t millis() { return test_runtime::clock; }
uint32_t random() { return 17; }
void log(const char *s) {
  sscanf(s, "Level: focus=%d screen=%d", &test_runtime::focus_id,
         &test_runtime::screen_id);
  puts(s);
}
bool held(Key) { return false; }
uint16_t *framebuffer() {
  return test_runtime::drawing ? test_runtime::pixels : nullptr;
}
void tone(float, float, float) {}
void silence() {}
} // namespace pocket
