// SPDX-License-Identifier: MIT
#include "../canvas.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
std::vector<uint8_t> transaction, presented;
unsigned commits = 0, frames = 0;
struct Record {
  uint8_t type;
  const uint8_t *value;
  uint16_t size;
};
std::vector<Record> records(const std::vector<uint8_t> &bytes) {
  std::vector<Record> result;
  for (size_t at = 0; at < bytes.size();) {
    assert(bytes.size() - at >= 4);
    const uint16_t size = pxa_read_u16(bytes.data() + at + 2);
    assert(size <= bytes.size() - at - 4);
    result.push_back({bytes[at], bytes.data() + at + 4, size});
    at += 4 + size;
  }
  return result;
}
int coordinate(const uint8_t *value, unsigned offset) {
  return int32_t(pxa_read_u32(value + offset));
}
int length(uint32_t node, uint16_t property) {
  for (const auto &record : records(transaction)) {
    if (record.type != PXA_UI_COMMAND_SET_PROPERTY || record.size < 6 ||
        pxa_read_u32(record.value) != node ||
        pxa_read_u16(record.value + 4) != property)
      continue;
    assert(record.size == 14 && record.value[6] == PXA_UI_LENGTH_LOGICAL_PX);
    const int raw = coordinate(record.value, 10);
    assert(raw % 64 == 0);
    return raw / 64;
  }
  assert(false && "Missing native input geometry property");
  return 0;
}
} // namespace

extern "C" int32_t pxa_call(uint32_t route, uint64_t, const uint8_t *payload,
                             uint32_t size) {
  assert((route & 0xffff) == PXA_UI_SERVICE);
  switch (route >> 16) {
  case PXA_UI_TX_BEGIN:
    assert(size == 20);
    transaction.clear();
    break;
  case PXA_UI_TX_WRITE:
    assert(size >= 4);
    transaction.insert(transaction.end(), payload + 4, payload + size);
    break;
  case PXA_UI_TX_COMMIT:
    ++commits;
    break;
  case PXA_UI_CANVAS_BEGIN:
    presented.clear();
    break;
  case PXA_UI_CANVAS_WRITE:
    assert(size >= 12);
    presented.insert(presented.end(), payload + 12, payload + size);
    break;
  case PXA_UI_CANVAS_PRESENT:
    ++frames;
    break;
  default:
    assert(false && "Unexpected UI command");
  }
  return PXA_STATUS_OK;
}
extern "C" int32_t pxa_io(uint64_t, uint32_t, uint8_t *, uint32_t) {
  assert(false && "Canvas layout test must not perform I/O");
  return PXA_STATUS_WOULD_BLOCK;
}

int main() {
  for (unsigned dpi : {120u, 125u, 160u, 205u}) {
    media::Canvas canvas;
    pxa_ui_environment_t env{};
    env.width = 296;
    env.height = 240;
    env.density_q16 = dpi * 65536 / 160;
    env.display_shape = PXA_UI_DISPLAY_SHAPE_ROUNDED_RECTANGLE;
    for (auto &radius : env.corner_radii) radius = 58;
    env.safe_insets[0] = env.safe_insets[2] = 8;
    env.safe_insets[1] = env.safe_insets[3] = 10;
    assert(canvas.environment(env));
    pxa_window_snapshot_t window{};
    window.revision = 1;
    window.pixel_width = env.width;
    window.pixel_height = env.height;
    window.system_bar_insets.left = 26;
    window.system_bar_insets.bottom = 20;
    assert(canvas.window(window));

    const auto area = canvas.viewport.content;
    const double scale = env.density_q16 / 65536.;
    // Visual content need not leave an entire blank column for Back gestures.
    assert(area.x * scale >= 10 && area.x * scale < 26);
    const int gesture_start = area.x + canvas.viewport.touch_left;
    assert(gesture_start * scale >= 26);
    assert(gesture_start * scale - 26 < scale);
    assert((area.x + canvas.margin(12)) * scale >= 26);
    if (dpi == 160) {
      assert(area.x == 10 && area.y == 17);
      assert(area.x + canvas.margin(12) == 26); // was 38 with extra margin
    }

    canvas.begin(area.w, area.h, 0x101020);
    canvas.pill(71, 0, 45, 80, "Back", true, 0x9988bb, 0xffffff, 0x333333);
    canvas.hit(72, -20, 80, area.w + 40, 30);
    assert(canvas.present() && canvas.initialized);
    const auto draw = records(presented);
    assert(draw.size() >= 7);
    assert(draw[0].type == PXA_CANVAS_DRAW_RECT);
    const int background_x = coordinate(draw[0].value, 0);
    const int background_y = coordinate(draw[0].value, 4);
    assert(background_x < 0 && background_y < 0);
    assert((background_x + coordinate(draw[0].value, 8)) * scale >= env.width);
    assert((background_y + coordinate(draw[0].value, 12)) * scale >= env.height);
    assert(draw[1].type == PXA_CANVAS_CLIP_PUSH);
    assert(coordinate(draw[1].value, 0) == area.x);
    assert(coordinate(draw[1].value, 4) == area.y);
    assert(draw[2].type == PXA_CANVAS_DRAW_RECT);
    const int button_x = coordinate(draw[2].value, 0);
    const int button_y = coordinate(draw[2].value, 4);
    const int button_width = coordinate(draw[2].value, 8);
    const int button_height = coordinate(draw[2].value, 12);
    assert(button_x == area.x && button_y == area.y + 45);
    // Reproduce panel -> Canvas DP conversion used by the Host: a rendered
    // button's physical center must resolve to that button's local hit box.
    const int panel_x = int((button_x + button_width / 2) * scale);
    const int panel_y = int((button_y + button_height / 2) * scale);
    const int pointer_x = int(int64_t(panel_x) * 65536 / env.density_q16);
    const int pointer_y = int(int64_t(panel_y) * 65536 / env.density_q16);
    assert(canvas.find(pointer_x - area.x, pointer_y - area.y) == 71);
    for (int panel = 0; panel < 26; ++panel) {
      const int logical = int(int64_t(panel) * 65536 / env.density_q16);
      assert(canvas.find(logical - area.x, 55) == 0);
      assert(canvas.find(logical - area.x, 90) == 0);
    }
    assert(canvas.find(canvas.viewport.touch_left, 90) == 72);
    assert(canvas.find(area.w, 90) == 0);
    int clip_depth = 0;
    for (const auto &record : draw) {
      if (record.type == PXA_CANVAS_CLIP_PUSH) ++clip_depth;
      if (record.type == PXA_CANVAS_CLIP_POP) --clip_depth;
      assert(clip_depth >= 0);
    }
    assert(clip_depth == 0);

    // A native text input shares the Canvas origin but never occupies the
    // gesture-only strip; trimming its left edge preserves its right edge.
    assert(canvas.input(20, 0, 80, 100, "search"));
    assert(length(20, PXA_UI_PROPERTY_X) == gesture_start);
    assert(length(20, PXA_UI_PROPERTY_Y) == area.y + 80);
    assert(length(20, PXA_UI_PROPERTY_X) + length(20, PXA_UI_PROPERTY_WIDTH)
           == area.x + 100);
    assert(length(20, PXA_UI_PROPERTY_HEIGHT) == 32);
    assert(canvas.input(21, 0, 80, area.w + 100, "url"));
    assert(length(21, PXA_UI_PROPERTY_X) + length(21, PXA_UI_PROPERTY_WIDTH)
           == area.x + area.w);
    const unsigned before_invalid = commits;
    assert(!canvas.input(22, 0, area.h - 20, 100, "outside"));
    assert(!canvas.input(23, 0, 80, canvas.viewport.touch_left, "gesture"));
    assert(commits == before_invalid);

    // A changed gesture strip alone must rebuild hit/native geometry, while
    // identical or older Window notifications must not churn the surface.
    canvas.initialized = 1;
    assert(!canvas.window(window) && canvas.initialized);
    ++window.revision;
    window.system_bar_insets.left = 42;
    assert(canvas.window(window) && !canvas.initialized);
    assert(canvas.viewport.content.x == area.x);
    canvas.initialized = 1;
    auto stale = window;
    stale.revision = 1;
    stale.system_bar_insets.left = 0;
    assert(!canvas.window(stale) && canvas.initialized);
    assert(!canvas.environment(env) && canvas.initialized);
    env.density_q16 += 512;
    assert(canvas.environment(env) && !canvas.initialized);
  }
  assert(frames == 4);
  std::puts("media Canvas: full background, presented/hit coordinate agreement, "
            "left gesture exclusion, native input placement and metric updates "
            "at 120/125/160/205 DPI passed");
}
