// SPDX-License-Identifier: MIT
#include "font_cache.hpp"
#include "game_blit.hpp"
#include "map_gestures.hpp"
#include "overlay_raster.hpp"
#include "pinch_input.hpp"
#include "pinch_tracker.hpp"
#include "render_config.hpp"
#ifdef UNCIV_SCANLINE_BLIT
#include "scanline_cache.hpp"
#endif
#include "pxa.h"
#include "pxa_game_render_rgb565.h"
#include "pxa_storage.h"
#include "save_store.hpp"
#include "save_transport.hpp"
#include <stdio.h>
#include <string.h>
namespace {
uint8_t journal_packet[PXA_STORAGE_MAX_PACKET];
void journal_deliver(bool, uint64_t, int, const uint8_t *, size_t);
int journal_submit(bool write, const char *key, const uint8_t *value, size_t n,
                   uint64_t token) {
  return write ? pxa_storage_request_set(journal_packet, sizeof(journal_packet),
                                         token, key, strlen(key), value, n)
               : pxa_storage_request_get(token, key, strlen(key));
}
bool is_save(const char *key, size_t n) {
  return n == 17 && !memcmp(key, "pxa-unciv.save.v1", 17);
}
int32_t journal_get(uint64_t token, const char *key, size_t n) {
  save_store::journal.bind(journal_submit, journal_deliver);
  return is_save(key, n) ? save_store::journal.read(token)
                         : pxa_storage_request_get(token, key, n);
}
int32_t journal_set(uint8_t *p, size_t cap, uint64_t token, const char *key,
                    size_t n, const uint8_t *data, size_t bytes) {
  save_store::journal.bind(journal_submit, journal_deliver);
  return is_save(key, n)
             ? save_store::journal.write(token, data, bytes)
             : pxa_storage_request_set(p, cap, token, key, n, data, bytes);
}
} // namespace
#define pxa_storage_request_get journal_get
#define pxa_storage_request_set journal_set
#include "../common/pocket-play/runtime.cpp"
#undef pxa_storage_request_get
#undef pxa_storage_request_set
namespace pinch_input {
namespace {
pinch_tracker::Tracker tracker;
}
void reset() {
  if (tracker.active())
    game::pinch_end();
  tracker.reset();
}
bool event(const pxa_event_t &e, bool foreground) {
  uint8_t life;
  pxa_ui_environment_t environment;
  pxa_ui_controller_data_t controller;
  if (!foreground || pxa_lifecycle_parse(&e, &life) ||
      pxa_ui_parse_environment_event(&e, &environment) ||
      pxa_window_is_back_requested(&e) ||
      (pxa_ui_parse_controller(&e, &controller) && controller.buttons)) {
    reset();
    return false;
  }
  pxa_ui_pointer_data_t p;
  if (!pxa_ui_parse_pointer(&e, &p) || !surface_ready)
    return false;
  int x = content_viewport.map_x(pocket_ui::pointer_pixel(p.x, pointer_density),
                                 pocket::WIDTH),
      y = content_viewport.map_y(pocket_ui::pointer_pixel(p.y, pointer_density),
                                 pocket::HEIGHT);
  auto phase = p.phase == PXA_POINTER_DOWN   ? pinch_tracker::Phase::Down
               : p.phase == PXA_POINTER_MOVE ? pinch_tracker::Phase::Move
               : p.phase == PXA_POINTER_UP   ? pinch_tracker::Phase::Up
                                             : pinch_tracker::Phase::Cancel;
  auto result =
      tracker.event(phase, p.pointer_id, x, y, game::pinch_area(x, y));
  using pinch_tracker::Action;
  switch (result.action) {
  case Action::Begin:
    cancel_input();
    if (!game::pinch_begin(result.x, result.y))
      reset();
    return true;
  case Action::Update:
    game::pinch_update(result.x, result.y, result.scale);
    return true;
  case Action::End:
    game::pinch_end();
    return true;
  case Action::Consume:
    return true;
  case Action::Pass:
    return false;
  }
  return false;
}
} // namespace pinch_input
namespace {
void journal_deliver(bool writing, uint64_t token, int status,
                     const uint8_t *value, size_t size) {
  static uint8_t payload[2060], wire[2100];
  pxa_store_u32(payload, uint32_t(status));
  size_t n = 4, record = 0;
  uint32_t total = 0;
  if (!writing && !status) {
    if (!pxa_wire_record_encode(payload + 4, sizeof(payload) - 4, 2, value,
                                size, &record))
      __builtin_trap();
    n += record;
  }
  if (!pxa_build_message(wire, sizeof(wire), PXA_STORAGE_SERVICE,
                         writing ? PXA_STORAGE_SET : PXA_STORAGE_GET, token,
                         payload, n, &total))
    __builtin_trap();
  if (status && status != PXA_STATUS_NOT_FOUND) {
    char text[100];
    snprintf(text, sizeof(text), "Unciv: save journal status=%d", status);
    pxa_log_write(3, text);
  }
  handle_event(wire, total);
}
} // namespace
namespace save_transport {
bool event(const pxa_event_t &e) {
  if (e.service != PXA_STORAGE_SERVICE || e.token != save_store::TOKEN)
    return false;
  if (e.opcode == PXA_STORAGE_GET) {
    pxa_storage_get_result_t r{};
    if (!pxa_storage_parse_get(&e, save_store::TOKEN, &r))
      return save_store::journal.event(false, e.token, -15, nullptr, 0);
    return save_store::journal.event(false, e.token, r.status, r.value.data,
                                     r.value.size);
  }
  int32_t status = -15;
  pxa_storage_parse_status(&e, save_store::TOKEN, PXA_STORAGE_SET, &status);
  return save_store::journal.event(true, e.token, status, nullptr, 0);
}
} // namespace save_transport
namespace map_raster {
uint64_t now() { return now_us(); }
static uint64_t drawn_us = 0;
void record_draw(uint64_t n) { drawn_us += n; }
unsigned frames() { return unsigned(frame_id); }
uint64_t render_us() { return drawn_us; }
namespace {
struct Texture {
  const uint16_t *source = nullptr;
  int dark = 0;
  uint16_t rgb[1600];
};
struct Pending {
  int x, y, w, h;
  const uint16_t *rgb;
  const uint8_t *alpha;
  int sw, sh, dark, top, bottom;
};
Texture textures[32];
Pending pending[512];
uint8_t batch[32 + 512 * 32];
unsigned texture_count = 0, pending_count = 0;
bool batching = false, native_fallback = false;
#ifdef UNCIV_SCANLINE_BLIT
scanline_cache::Cache<> spans;
#endif
} // namespace
void begin() {
  flush();
  batching = true;
}
void end() {
  flush();
  batching = false;
}
void flush() {
  if (!pending_count)
    return;
  auto &v = pending[0];
  int first = std::clamp(studio_y(v.top), 0, viewport.height),
      last = std::clamp(studio_y(v.bottom), 0, viewport.height);
  pxa_game_render_rgb565_header_wire_t h{};
  h.magic = PXA_GAME_RENDER_RGB565_MAGIC;
  h.version = 1;
  h.count = pending_count;
  h.target_offset = uint32_t(uintptr_t(canvas + first * viewport.width));
  h.target_bytes = (last - first) * viewport.width * 2;
  h.target_width = viewport.width;
  h.target_height = last - first;
  h.target_pitch = viewport.width * 2;
  pxa_game_render_rgb565_header_encode(batch, 32, &h);
  unsigned n = pending_count;
  pending_count = 0;
  int bytes = 32 + n * 32;
  int result =
      pxa_game_render_execute_rgb565(game_blit::context(), batch, bytes);
  if (result != bytes) {
    native_fallback = true;
    for (unsigned i = 0; i < n; i++) {
      auto &p = pending[i];
      sprite(p.x, p.y, p.w, p.h, p.rgb, p.alpha, p.sw, p.sh, p.dark, p.top,
             p.bottom);
    }
    native_fallback = false;
  }
}
void sprite(int x, int y, int w, int h, const uint16_t *rgb,
            const uint8_t *alpha, int sw, int sh, int dark, int top,
            int bottom) {
  if (!canvas || w <= 0 || h <= 0)
    return;
#ifdef UNCIV_REFERENCE_BLIT
  for (int j = 0; j < h; j++)
    if (y + j >= top && y + j < bottom)
      for (int i = 0; i < w; i++) {
        int k = (j * sh / h) * sw + i * sw / w;
        if (alpha[k] >= 60)
          pocket::pixel(x + i, y + j,
                        dark
                            ? studio::mix(rgb[k], pocket::rgb(12, 25, 42), dark)
                            : rgb[k]);
      }
  return;
#endif
#ifdef UNCIV_SCANLINE_BLIT
  flush();
  if (spans.blit({canvas, viewport.width, viewport.height, studio_y(top),
                  studio_y(bottom)},
                 studio_x(x), studio_y(y), std::max(1, studio_dx(w)),
                 std::max(1, studio_dy(h)), rgb, alpha, sw, sh, dark)) {
    static bool announced = false;
    if (!announced) {
      pocket::log(
          "Unciv: renderer=native-size cached row copies; cache_limit=262144");
      announced = true;
    }
    return;
  }
#endif
  if (!native_fallback && game_blit::context() && sw * sh <= 1600) {
    Texture *im = nullptr;
    for (unsigned i = 0; i < texture_count; i++)
      if (textures[i].source == rgb && textures[i].dark == dark)
        im = &textures[i];
    if (!im && texture_count < 32) {
      im = &textures[texture_count++];
      im->source = rgb;
      im->dark = dark;
      for (int i = 0; i < sw * sh; i++) {
        uint16_t color =
            dark ? studio::mix(rgb[i], pocket::rgb(12, 25, 42), dark) : rgb[i];
        im->rgb[i] = alpha[i] < 60 ? 0 : color ? color : 1;
      }
    }
    if (im) {
      if (pending_count == 512)
        flush();
      auto &v = pending[pending_count++];
      v = {x, y, w, h, rgb, alpha, sw, sh, dark, top, bottom};
      int first = std::clamp(studio_y(top), 0, viewport.height);
      pxa_game_render_rgb565_command_wire_t cmd{};
      cmd.kind = 1;
      cmd.flags = 1;
      cmd.light = 255;
      cmd.x = studio_x(x);
      cmd.y = studio_y(y) - first;
      cmd.width = std::max(1, studio_dx(w));
      cmd.height = std::max(1, studio_dy(h));
      cmd.source_offset = uint32_t(uintptr_t(im->rgb));
      cmd.source_bytes = sw * sh * 2;
      cmd.source_width = sw;
      cmd.source_height = sh;
      cmd.source_pitch = sw * 2;
      pxa_game_render_rgb565_command_encode(
          batch + 32 + (pending_count - 1) * 32, 32, &cmd);
      if (!batching)
        flush();
      return;
    }
    flush();
  }

  int x0 = std::max(0, x), x1 = std::min(pocket::WIDTH, x + w),
      y0 = std::max(top, y), y1 = std::min(bottom, y + h);
  if (x0 >= x1 || y0 >= y1)
    return;
  int left = std::clamp(studio_x(x0), 0, viewport.width),
      right = std::clamp(studio_x(x1), 0, viewport.width);
  int first = std::clamp(studio_y(y0), 0, viewport.height),
      last = std::clamp(studio_y(y1), 0, viewport.height);
  int columns[480];
  int physical_left = studio_x(x), physical_top = studio_y(y),
      physical_w = std::max(1, studio_dx(w)),
      physical_h = std::max(1, studio_dy(h));
  for (int i = left; i < right; i++)
    columns[i] = std::clamp((i - physical_left) * sw / physical_w, 0, sw - 1);
  for (int j = first; j < last; j++) {
    int row = std::clamp((j - physical_top) * sh / physical_h, 0, sh - 1) * sw;
    uint16_t *out = canvas + j * viewport.width;
    for (int i = left; i < right; i++) {
      int k = row + columns[i];
      if (alpha[k] >= 60)
        out[i] =
            dark ? studio::mix(rgb[k], pocket::rgb(12, 25, 42), dark) : rgb[k];
    }
  }
}
} // namespace map_raster

namespace map_raster {
namespace {
struct Mini {
  uint16_t pixels[9000];
  int layout[12]{};
  uint32_t key = 0;
  bool valid = false, pending = false;
} mini;
} // namespace
bool minimap_restore(int x, int y, int w, int h, uint32_t key) {
  int left = studio_x(x), top = studio_y(y), width = studio_x(x + w) - left,
      height = studio_y(y + h) - top;
  mini.pending = false;
  if (!canvas || left < 0 || top < 0 || width <= 0 || height <= 0 ||
      left + width > viewport.width || top + height > viewport.height ||
      width * height > 9000)
    return false;
  // Restoring an opaque bitmap must also hide retained Canvas labels behind it.
  studio_occlude(x, y, w, h);
  int layout[] = {left,
                  top,
                  width,
                  height,
                  viewport.width,
                  viewport.height,
                  pocket::WIDTH,
                  pocket::HEIGHT,
                  content_viewport.x,
                  content_viewport.y,
                  content_viewport.width,
                  content_viewport.height};
  if (mini.valid && mini.key == key &&
      !memcmp(mini.layout, layout, sizeof(layout))) {
    for (int row = 0; row < height; ++row)
      memcpy(canvas + (top + row) * viewport.width + left,
             mini.pixels + row * width, width * 2);
    return true;
  }
  memcpy(mini.layout, layout, sizeof(layout));
  mini.key = key;
  mini.valid = false;
  mini.pending = true;
  return false;
}
void minimap_capture() {
  if (!canvas || !mini.pending)
    return;
  int left = mini.layout[0], top = mini.layout[1], width = mini.layout[2],
      height = mini.layout[3];
  for (int row = 0; row < height; ++row)
    memcpy(mini.pixels + row * width,
           canvas + (top + row) * viewport.width + left, width * 2);
  mini.pending = false;
  mini.valid = true;
}
void overlay(int x, int y, int w, int h, const uint8_t *alpha, int sw, int sh,
             uint16_t color, int opacity, int top, int bottom) {
  flush();
  overlay_raster::mask({canvas, viewport.width, viewport.height, studio_y(top),
                        studio_y(bottom)},
                       studio_x(x), studio_y(y), std::max(1, studio_dx(w)),
                       std::max(1, studio_dy(h)), alpha, sw, sh, color,
                       opacity);
}
void stroke(int x, int y, int x2, int y2, int width, uint16_t color, int top,
            int bottom) {
  flush();
  overlay_raster::stroke({canvas, viewport.width, viewport.height,
                          studio_y(top), studio_y(bottom)},
                         studio_x(x), studio_y(y), studio_x(x2), studio_y(y2),
                         std::max(1, studio_dx(width)), color);
}
void icon(int x, int y, int size, const uint16_t *rgb, const uint8_t *alpha,
          int sw, int sh, uint16_t tint) {
  if (!canvas || size <= 0 || sw <= 0 || sh <= 0)
    return;
  flush();
  int longest = std::max(sw, sh), w = std::max(1, size * sw / longest),
      h = std::max(1, size * sh / longest);
  x += (size - w) / 2;
  y += (size - h) / 2;
  int left = studio_x(x), top = studio_y(y), width = std::max(1, studio_dx(w)),
      height = std::max(1, studio_dy(h));
  for (int yy = std::max(0, top); yy < std::min(viewport.height, top + height);
       ++yy) {
    int sy = (yy - top) * sh / height;
    uint16_t *row = canvas + yy * viewport.width;
    for (int xx = std::max(0, left);
         xx < std::min(viewport.width, left + width); ++xx) {
      int k = sy * sw + (xx - left) * sw / width;
      unsigned a = alpha[k];
      uint16_t color = tint ? tint : rgb[k];
      if (a)
        row[xx] = a == 255 ? color : studio::mix(row[xx], color, a);
    }
  }
}
bool text(int x, int y, int w, const char *text, uint16_t color, int scale,
          unsigned align) {
#ifdef UNCIV_CANVAS_TEXT
  return false;
#else
  if (!canvas || !text || !*text || w <= 0)
    return true;
  // A scaled Surface cannot retain native-resolution typography. Keep Canvas
  // text on those displays, rather than enlarging or blurring cached glyphs.
  if (viewport.host_scale != 1)
    return false;
  int desired = std::clamp(
      10 * scale * content_viewport.physical_height() / pocket::HEIGHT, 9, 96);
  int pixels = std::clamp(desired, 8, 48), width = 0;
  bool complete = true;
  const char *p = text;
  while (*p) {
    uint32_t cp = pocket_ui::next_utf8(p);
    const auto *g = font_cache::get(cp, pixels);
    if (!g)
      complete = false;
    else
      width += int(g->advance) * desired / pixels;
  }
  if (!complete)
    return false;
  int left = std::clamp(studio_x(x), 0, viewport.width),
      right = std::clamp(studio_x(x + w), 0, viewport.width), pen = studio_x(x),
      top = studio_y(y) - desired / 4;
  if (align == 1)
    pen += (right - left - width) / 2;
  else if (align == 2)
    pen += right - left - width;
  p = text;
  while (*p) {
    const auto *g = font_cache::get(pocket_ui::next_utf8(p), pixels);
    int gw = int(g->width) * desired / pixels,
        gh = int(g->height) * desired / pixels;
    int gx = pen + int(g->offset_x) * desired / pixels,
        gy = top + (int(g->line_height) - int(g->baseline) - int(g->height) -
                    int(g->offset_y)) *
                       desired / pixels;
    for (int yy = std::max(0, gy); yy < std::min(viewport.height, gy + gh);
         yy++) {
      uint16_t *row = canvas + yy * viewport.width;
      int sy = (yy - gy) * g->height / std::max(1, gh);
      for (int xx = std::max(left, gx); xx < std::min(right, gx + gw); xx++) {
        int sx = (xx - gx) * g->width / std::max(1, gw);
        unsigned a = g->coverage[sy * g->width + sx];
        if (a)
          row[xx] = a == 255 ? color : studio::mix(row[xx], color, a);
      }
    }
    pen += int(g->advance) * desired / pixels;
  }
  return true;
#endif
}
} // namespace map_raster
