// SPDX-License-Identifier: CC0-1.0
#include "runtime.hpp"
#include "../common/pocket-ui/preferences.hpp"
#include "../common/pocket-ui/viewport.hpp"
#include "input_geometry.hpp"
#include "pxa.h"
#include "pxa_audio.h"
#include "pxa_clock.h"
#include "pxa_event_port.h"
#include "pxa_fs.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include "pxa_surface.h"
#include "pxa_system.h"
#include "pxa_ui.h"
#include "pxa_window.h"
#include "text.hpp"
#include "ui_canvas.hpp"
#include <algorithm>
#include <cstring>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef PXA_POCKET_STUDIO
#include "../common/pocket-studio/studio.hpp"
#include "pxa_canvas.h"
#include <algorithm>
#endif

namespace {
using namespace pocket;
constexpr unsigned LOGICAL_PIXELS = 480 * 480;
// File-backed saves have a separate bounded buffer from 2 KiB preferences.
enum : uint64_t {
  PORT = 1,
  CREATE,
  CONFIGURE,
  LOAD,
  SAVE,
  PREFERENCES_LOAD,
  PREFERENCES_SAVE,
  SAVE_COMMIT, WORLD_OPEN, WORLD_REMOVE, WORLD_REMOVE_TEMP
};
alignas(64) uint16_t frames[2 * pocket_ui::MAX_SURFACE_PIXELS];
#ifndef PXA_POCKET_STUDIO
uint16_t logical_frame[LOGICAL_PIXELS];
#endif
uint16_t *canvas;
starsoil_ui::Canvas ui;
bool ui_active;
uint8_t packet[PXA_MAX_CONTROL_BYTES], events[PXA_MAX_CONTROL_BYTES],
    loaded[SAVE_CAPACITY], pending_save[SAVE_CAPACITY];
char save_key[80];
size_t loaded_size;
uint64_t port, surface, frame_id, epoch, paused_us, pause_at, save_due,
    save_retry, last_tick;
uint64_t repeats[4];
uint32_t buttons, blocked_buttons, pointer_id, rng = 0x6d2b79f5;
uint64_t perf_at, perf_render_us;
uint32_t perf_frames;
int display_w = 320, display_h = 240;
uint32_t input_density = 65536, display_safe[4]{}, display_corners[4]{};
bool update_geometry(const pxa_ui_environment_t &env) {
  bool changed =
      std::memcmp(display_safe, env.safe_insets, sizeof(display_safe)) ||
      std::memcmp(display_corners, env.corner_radii, sizeof(display_corners));
  std::memcpy(display_safe, env.safe_insets, sizeof(display_safe));
  std::memcpy(display_corners, env.corner_radii, sizeof(display_corners));
  input_density = env.density_q16 ? env.density_q16 : 65536;
  return changed;
}
int last_x, last_y, menu_row;
bool running, finished, background = true, fresh_tick, surface_ready,
                        load_ready;
bool mapped = true, staged, prepared, dirty = true, save_dirty, save_pending,
     pointer_active, pointer_consumed, paused;
uint8_t prepared_index;
uint64_t revision = 1, prepared_revision, exit_deadline;
bool exit_requested;
bool controller_input;

void set_controller_input(bool active) {
  if (controller_input != active) {
    controller_input = active;
    pocket::redraw();
  }
}

uint64_t now_us() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
#include "../common/pocket-ui/preferences.inc"
#ifdef PXA_POCKET_STUDIO
uint32_t studio_safe[4], studio_corners[4];
bool studio_geometry(const pxa_ui_environment_t &env) {
  bool changed =
      memcmp(studio_safe, env.safe_insets, sizeof(studio_safe)) ||
      memcmp(studio_corners, env.corner_radii, sizeof(studio_corners));
  memcpy(studio_safe, env.safe_insets, sizeof(studio_safe));
  memcpy(studio_corners, env.corner_radii, sizeof(studio_corners));
  return changed;
}
#endif
#include "surface.inc"
#ifdef PXA_POCKET_STUDIO
#include "../common/pocket-studio/runtime.inc"
#endif
uint64_t foreground_us() {
  return epoch ? (background ? pause_at : now_us()) - epoch - paused_us : 0;
}
// Optional audio is isolated from the game and storage state machines.
#include "../common/pocket-play/audio.inc"
void cancel_input() {
  set_controller_input(false);
  blocked_buttons |= buttons;
  buttons = 0;
  clear_sound();
  memset(repeats, 0, sizeof(repeats));
  if (pointer_active && !pointer_consumed && running)
    game::pointer(Phase::CANCEL, last_x, last_y);
  pointer_active = false;
}
#include "world_storage.inc"

void request_exit() {
  if (exit_requested)
    return;
  exit_requested = true;
  if (!opening_world && !removing_world)
    world_operation = WorldOperation::None;
  exit_deadline = now_us() + 2000000;
  cancel_input();
  paused = true;
  pocket::redraw();
  pocket::save();
  send_save(true);
}
void toggle_pause() {
  paused = !paused;
  menu_row = 0;
  pocket::redraw();
  if (pointer_active && !pointer_consumed)
    game::pointer(Phase::CANCEL, last_x, last_y);
  if (pointer_active)
    pointer_consumed = true;
  memset(repeats, 0, sizeof(repeats));
  blocked_buttons |= buttons;
  buttons = 0;
  clear_sound();
  pxa_log_write(2, paused ? "Pocket: menu opened" : "Pocket: menu closed");
  if (paused) {
    pocket::save();
    send_save(true);
  }
}
void menu_activate() {
  if (menu_row == 3) {
    toggle_language();
    return;
  }
  if (menu_row == 2) {
    request_exit();
  } else {
    if (menu_row == 1) {
      game::reset();
      pocket::save();
    }
    blocked_buttons |= buttons;
    buttons = 0;
    memset(repeats, 0, sizeof(repeats));
    paused = false;
    pocket::redraw();
    pxa_log_write(2, "Pocket: menu closed");
  }
}
void dispatch_key(Key key) {
  if (exit_requested)
    return;
  if (key == Key::START) {
    toggle_pause();
    return;
  }
  if (paused) {
    if (key == Key::UP)
      menu_row = (menu_row + 3) % 4;
    else if (key == Key::DOWN)
      menu_row = (menu_row + 1) % 4;
    else if (key == Key::A)
      menu_activate();
    else if (key == Key::B) {
      blocked_buttons |= buttons;
      buttons = 0;
      memset(repeats, 0, sizeof(repeats));
      paused = false;
      pxa_log_write(2, "Pocket: menu closed");
    }
  } else
    game::key(key);
  pocket::redraw();
}
constexpr uint32_t key_masks[] = {
    PXA_CONTROLLER_UP,   PXA_CONTROLLER_RIGHT, PXA_CONTROLLER_DOWN,
    PXA_CONTROLLER_LEFT, PXA_CONTROLLER_A,     PXA_CONTROLLER_B,
    PXA_CONTROLLER_X,    PXA_CONTROLLER_Y,     PXA_CONTROLLER_L,
    PXA_CONTROLLER_R,    PXA_CONTROLLER_START, PXA_CONTROLLER_SELECT};
void controller(const pxa_ui_controller_data_t &c) {
  if (c.controller)
    return;
  if (!c.connected)
    set_controller_input(false);
  uint32_t next = c.connected ? c.buttons : 0;
  blocked_buttons &= next;
  if (background || !running)
    return;
  next &= ~blocked_buttons;
  uint32_t edges = next & ~buttons;
  buttons = next;
  if (edges)
    set_controller_input(true);
  uint64_t now = now_us();
  for (unsigned i = 0; i < 4; ++i) {
    if (edges & key_masks[i])
      repeats[i] = now + 330000;
    if (!(next & key_masks[i]))
      repeats[i] = 0;
  }
  if (edges & PXA_CONTROLLER_START) {
    dispatch_key(Key::START);
    return;
  }
  for (unsigned i = 0; i < sizeof(key_masks) / sizeof(key_masks[0]); ++i)
    if ((edges & key_masks[i]) && !(blocked_buttons & key_masks[i]))
      dispatch_key(Key(i));
}
void pointer(const pxa_ui_pointer_data_t &p) {
  if (background || !running || exit_requested)
    return;
  const int display_x = starsoil::display_coordinate(p.x, input_density);
  const int display_y = starsoil::display_coordinate(p.y, input_density);
  if (p.phase == PXA_POINTER_DOWN && !pointer_active) {
    if (!surface_ready || !viewport.contains(display_x, display_y))
      return;
    set_controller_input(false);
    pointer_id = p.pointer_id;
    pointer_active = true;
    pointer_consumed = false;
  }
  if (!pointer_active || p.pointer_id != pointer_id)
    return;
  last_x = viewport.map_x(display_x, WIDTH);
  last_y = viewport.map_y(display_y, HEIGHT);
  Phase phase;
  if (p.phase == PXA_POINTER_DOWN)
    phase = Phase::DOWN;
  else if (p.phase == PXA_POINTER_MOVE)
    phase = Phase::MOVE;
  else if (p.phase == PXA_POINTER_UP)
    phase = Phase::UP;
  else
    phase = Phase::CANCEL;
  if (phase == Phase::DOWN &&
      last_x >=
#ifdef PXA_POCKET_STUDIO
          240 &&
      last_x < 264 && last_y < 26
#else
          WIDTH - 24 &&
      last_y < 20
#endif
  ) {
    pointer_consumed = true;
    toggle_pause();
  } else if (paused) {
    pointer_consumed = true;
    int mx = (WIDTH - 288) / 2, my = (HEIGHT - 224) / 2;
    if (phase == Phase::DOWN && last_x >= 56 + mx && last_x < 232 + mx &&
        last_y >= 72 + my && last_y < 192 + my) {
      menu_row = (last_y - 72 - my) / 30;
      menu_activate();
    }
  } else if (!pointer_consumed)
    game::pointer(phase, last_x, last_y);
  if (phase == Phase::UP || phase == Phase::CANCEL)
    pointer_active = false;
  pocket::redraw();
}
void draw_menu() {
#ifdef PXA_POCKET_STUDIO
  studio_menu(paused, menu_row, exit_requested);
#else
  pocket::rect(WIDTH - 24, 0, 24, 20, PANEL);
  for (int y : {5, 9, 13})
    pocket::rect(WIDTH - 18, y, 12, 2, ACCENT);
  if (!paused)
    return;
  int mx = (WIDTH - 288) / 2, my = (HEIGHT - 224) / 2;
  pocket::rect(48 + mx, 32 + my, 192, 180, BG);
  pocket::outline(48 + mx, 32 + my, 192, 180, ACCENT);
  pocket::text((WIDTH - pocket::text_width(tr("PAUSED", "已暂停"), 2)) / 2,
               43 + my, tr("PAUSED", "已暂停"), INK, 2);
  const char *labels[] = {tr("CONTINUE", "继续"), tr("SAVE & WORLDS", "保存并返回首页"),
                          tr("EXIT", "退出"),
                          tr("LANGUAGE: ENGLISH", "语言：中文")};
  for (int i = 0; i < 4; ++i) {
    int y = 72 + my + i * 30;
    pocket::rect(56 + mx, y, 176, 28, i == menu_row ? ACCENT : PANEL);
    pocket::text(68 + mx, y + (28 - pocket::text_height()) / 2,
                 labels[i], i == menu_row ? BG : INK);
  }
  if (exit_requested || controller_input)
    pocket::text(68 + mx, 195 + my,
                 exit_requested ? tr("SAVING...", "保存中...")
                                : tr("A SELECT  B RESUME", "A 选择  B 返回"), MUTED);

#endif
}

void present() {
  if (!surface_ready || !dirty || background)
    return;
  if (!prepared) {
    prepared_index = 0;
    if (mapped) {
      int32_t status = pxa_surface_acquire_buffer(surface, &prepared_index);
      if (status == PXA_STATUS_WOULD_BLOCK)
        return;
      if (status != 4 || prepared_index > 1)
        __builtin_trap();
    }
    uint64_t began = now_us();
#ifdef PXA_POCKET_STUDIO
    canvas = frames + prepared_index * viewport.pixels();
    studio_begin();
#else
    canvas = logical_frame;
    ui_active = false;
    ui = {frames + prepared_index * viewport.pixels(), viewport.width,
           viewport.height, WIDTH, HEIGHT};
#endif
    game::draw();
    draw_menu();
    canvas = nullptr;
#ifndef PXA_POCKET_STUDIO
    if (!ui_active)
      pocket_ui::resample(logical_frame, WIDTH, HEIGHT, ui.pixels,
                          viewport.width, viewport.height);
#endif
    perf_render_us += now_us() - began;
    prepared = true;
    prepared_revision = revision;
  }
  // Retain an acquired/mapped or copied/staged frame across backpressure.
  if (mapped) {
    int32_t status =
        pxa_surface_present_buffer(surface, prepared_index, frame_id + 1);
    if (status == PXA_STATUS_WOULD_BLOCK)
      return;
    if (status != 16)
      __builtin_trap();
  } else {
    if (!staged) {
      int32_t status = pxa_surface_write_frame(
          surface, reinterpret_cast<uint8_t *>(frames), frame_bytes);
      if (status == PXA_STATUS_WOULD_BLOCK)
        return;
      if (status != int(frame_bytes))
        __builtin_trap();
      staged = true;
    }
    uint32_t n;
    if (!pxa_surface_build_queue_frame(packet, sizeof(packet), surface,
                                       frame_id + 1, nullptr, 0, &n))
      __builtin_trap();
    int32_t status = pxa_control_packet(packet, n);
    if (status == PXA_STATUS_WOULD_BLOCK)
      return;
    if (status)
      __builtin_trap();
    staged = false;
  }
  ++frame_id;
  ++perf_frames;
  prepared = false;
  dirty = revision != prepared_revision;
#ifdef PXA_POCKET_STUDIO
  if (!studio_present())
    pocket::redraw();
#endif
}
int32_t handle_event(const uint8_t *bytes, uint32_t length) {
  pxa_event_t e;
  if (!pxa_parse_event(bytes, length, &e))
    return PXA_STATUS_INTERNAL;
  int32_t preferences_status = handle_preferences(e);
  if (preferences_status != PXA_EVENT_UNHANDLED)
    return preferences_status;
  int32_t audio_status = handle_sound(e);
  if (audio_status != PXA_EVENT_UNHANDLED)
    return audio_status;
  pxa_event_port_result_t p;
  if (pxa_event_port_parse_open(&e, PORT, &p)) {
    if (p.status)
      return p.status;
    port = p.handle;
    return PXA_EVENT_HANDLED;
  }
  uint8_t life;
  if (pxa_lifecycle_parse(&e, &life)) {
    bool next = life == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
    uint64_t now = now_us();
    if (running && next != background) {
      if (next)
        pause_at = now;
      else {
        paused_us += now - pause_at;
        last_tick = now;
      }
    }
    background = next;
    if (next)
      fresh_tick = false;
    cancel_input();
    pocket::redraw();
    if (next && running) {
      pocket::save();
      send_save(true);
    }
    return PXA_EVENT_HANDLED;
  }
  int32_t surface_status = handle_surface(e);
  if (surface_status != PXA_EVENT_UNHANDLED)
    return surface_status;
  int32_t world_storage_status = handle_world_storage(e);
  if (world_storage_status != PXA_EVENT_UNHANDLED) return world_storage_status;
  pxa_ui_controller_data_t c;
  if (pxa_ui_parse_controller(&e, &c)) {
    controller(c);
    return PXA_EVENT_HANDLED;
  }
  pxa_ui_pointer_data_t touch;
  if (pxa_ui_parse_pointer(&e, &touch)) {
    pointer(touch);
    return PXA_EVENT_HANDLED;
  }
  if (pxa_window_is_back_requested(&e)) {
    if (exit_requested)
      return finished ? PXA_EVENT_UNHANDLED : PXA_EVENT_HANDLED;
    if (running && !paused)
      toggle_pause();
    else
      request_exit();
    return PXA_EVENT_HANDLED;
  }
  pxa_ui_environment_t env;
  if (pxa_ui_parse_environment_event(&e, &env)) {
    bool display_geometry_changed = update_geometry(env);
#ifdef PXA_POCKET_STUDIO
    bool geometry_changed = studio_geometry(env);
    studio_density = env.density_q16 ? env.density_q16 : 65536;
    studio_hash = 0;
#endif
    if (!env.width || !env.height ||
        (uint32_t(display_w) == env.width &&
         uint32_t(display_h) == env.height && !display_geometry_changed
#ifdef PXA_POCKET_STUDIO
         && !geometry_changed
#endif
         ))
      return PXA_EVENT_HANDLED;
    display_w = env.width;
    display_h = env.height;
    cancel_input();
    pocket::redraw();
    surface_ready = false;
    if (!surface_inflight) {
      int32_t status = request_surface();
      if (status)
        return status;
    }
    return PXA_EVENT_HANDLED;
  }
  uint64_t timestamp;
  if (pxa_clock_parse_tick(&e, &timestamp)) {
    if (!background)
      fresh_tick = true;
    return PXA_EVENT_HANDLED;
  }
  return PXA_EVENT_UNHANDLED;
}
bool wait_event(uint64_t timeout) {
  int32_t n = pxa_event_port_wait(port, events, sizeof(events), timeout);
  if (n > 0) {
    if (handle_event(events, uint32_t(n)) < 0) {
      pxa_log_write(3, "Pocket: event failed");
      __builtin_trap();
    }
    return true;
  }
  if (n != PXA_STATUS_WOULD_BLOCK && n != PXA_STATUS_TIMED_OUT)
    __builtin_trap();
  return false;
}
void run() {
  running = true;
  epoch = last_tick = perf_at = now_us();
  rng ^= uint32_t(epoch) ^ uint32_t(epoch >> 32);
  game::init();
  char message[160];
  snprintf(message, sizeof(message),
           "Pocket: ready app=%s canvas=%dx%d origin=%d,%d scale=%d",
           game::app_id(), WIDTH, HEIGHT, viewport.x, viewport.y,
           viewport.host_scale);
  pxa_log_write(2, message);
  while (!finished) {
    // Notifications must never form a backlog in front of input.
    for (unsigned i = 0; i < 32 && wait_event(0); ++i) {
    }
    service_worlds();
    send_save(background || exit_requested);
    service_preferences();
    if (exit_requested && ((!worlds_busy() && !save_dirty && !save_pending &&
                            !preferences_dirty && !preferences_pending) ||
                           now_us() >= exit_deadline)) {
      if (save_dirty || save_pending || preferences_dirty ||
          preferences_pending)
        pxa_log_write(3, "Pocket: exit flush timed out");
      finished = true;
      pxa_window_request_back();
      break;
    }
    if (background) {
      wait_event(exit_requested ? 20000 : 1000000);
      continue;
    }
    uint64_t now = now_us();
    if (now - last_tick >= 20000) {
      uint32_t elapsed = uint32_t((now - last_tick) / 1000);
      last_tick = now;
      if (!paused)
        game::tick(elapsed > 100 ? 100 : elapsed);
      for (unsigned i = 0; i < 4; ++i)
        if (repeats[i] && now >= repeats[i]) {
          repeats[i] = now + 110000;
          dispatch_key(Key(i));
        }
    }
    service_sound();
    present();
    uint64_t after = now_us();
    if (after - perf_at >= 10000000) {
      char perf[144];
      snprintf(perf, sizeof(perf),
               "Pocket: perf frames=%u period_ms=%u mean_draw_us=%u",
               perf_frames, uint32_t((after - perf_at) / 1000),
               perf_frames ? uint32_t(perf_render_us / perf_frames) : 0);
      pxa_log_write(2, perf);
      perf_at = after;
      perf_frames = 0;
      perf_render_us = 0;
    }
    if (!finished)
      wait_event(after < last_tick + 20000 ? last_tick + 20000 - after : 0);
  }
}

} // namespace

#ifdef PXA_POCKET_STUDIO
#include "../common/pocket-studio/drawing.inc"
#endif
namespace pocket {
int WIDTH = BASE_WIDTH, HEIGHT = BASE_HEIGHT;
const WorldInfo &world_info(int slot) {
  static const WorldInfo empty;
  return slot >= 0 && slot < WorldSlots ? world_catalog[slot] : empty;
}
int active_world_slot() { return active_world; }
bool worlds_busy() { return world_operation != WorldOperation::None; }
int world_error() { return worlds_error; }
void clear_world_error() { worlds_error = 0; redraw(); }
bool load_world(int slot) {
  if (worlds_busy() || slot < 0 || slot >= WorldSlots) return false;
  if (!world_catalog[slot].valid) { worlds_error = 1; redraw(); return false; }
  if (active_world >= 0) save();
  worlds_error = 0;
  requested_world = slot;
  world_operation = WorldOperation::Open;
  redraw();
  return true;
}
bool delete_world(int slot) {
  if (worlds_busy() || slot < 0 || slot >= WorldSlots || !world_catalog[slot].occupied) return false;
  if (active_world >= 0) save();
  worlds_error = 0;
  requested_world = slot;
  world_operation = WorldOperation::Remove;
  redraw();
  return true;
}
bool create_world(uint32_t seed, uint8_t size) {
  if (worlds_busy() || size > 2) return false;
  int slot = 0;
  while (slot < WorldSlots && world_catalog[slot].occupied) ++slot;
  if (slot == WorldSlots) { worlds_error = 3; redraw(); return false; }
  if (active_world >= 0) save();
  worlds_error = 0;
  requested_world = slot;
  requested_seed = seed;
  requested_size = size;
  world_operation = WorldOperation::Create;
  redraw();
  return true;
}
void begin_ui(bool preserve_scene) {
#ifndef PXA_POCKET_STUDIO
  if (ui_active || !canvas) return;
  if (preserve_scene)
    pocket_ui::resample(logical_frame, WIDTH, HEIGHT, ui.pixels,
                        ui.width, ui.height);
  ui_active = true;
#endif
}
void rect(int x, int y, int w, int h, uint16_t color) {
#ifdef PXA_POCKET_STUDIO
  studio_rect(x, y, w, h, color);
  return;
#else
  if (ui_active) {
    ui.rect(x, y, w, h, color);
    return;
  }
  if (!canvas || w <= 0 || h <= 0)
    return;
  int x1 = x + w, y1 = y + h;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x1 > WIDTH)
    x1 = WIDTH;
  if (y1 > HEIGHT)
    y1 = HEIGHT;
  for (int row = y; row < y1; ++row)
    for (int col = x; col < x1; ++col)
      canvas[row * WIDTH + col] = color;
#endif
}
void clear(uint16_t color) { rect(0, 0, WIDTH, HEIGHT, color); }
void outline(int x, int y, int w, int h, uint16_t color) {
  rect(x, y, w, 1, color);
  rect(x, y + h - 1, w, 1, color);
  rect(x, y, 1, h, color);
  rect(x + w - 1, y, 1, h, color);
}
void line(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, error = dx + dy;
  for (;;) {
    rect(x0, y0, 1, 1, color);
    if (x0 == x1 && y0 == y1)
      break;
    int twice = error * 2;
    if (twice >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twice <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}
int text_width(const char *s, int scale) {
  return ui.text_width(s, scale);
}
int text_height(int scale) { return ui.text_height(scale); }
int text_line_height() { return ui.line_height(); }
void text(int x, int y, const char *s, uint16_t color, int scale) {
#ifdef PXA_POCKET_STUDIO
  studio_add(x, y, std::min(WIDTH - x, pocket_ui::text_width(s, scale) + 3), s,
             color, scale, 0);
#else
  ui.text(x, y, s, color, scale);
#endif
}
bool held(Key key) {
  unsigned i = unsigned(key);
  return i < sizeof(key_masks) / sizeof(key_masks[0]) && !background &&
         !paused && !exit_requested && (buttons & key_masks[i]);
}
bool using_controller() { return controller_input; }
uint16_t *framebuffer() { return canvas; }
void pixel(int x, int y, uint16_t color) {
#ifdef PXA_POCKET_STUDIO
  rect(x, y, 1, 1, color);
  return;
#endif
  if (ui_active) {
    ui.rect(x, y, 1, 1, color);
    return;
  }
  if (canvas && unsigned(x) < unsigned(WIDTH) && unsigned(y) < unsigned(HEIGHT))
    canvas[y * WIDTH + x] = color;
}
void tone(float frequency, float duration_seconds, float when_seconds) {
  add_tone(frequency, duration_seconds, when_seconds);
}
void silence() { clear_sound(); }
void redraw() {
  dirty = true;
  ++revision;
}
int save_status() { return persist_status; }
void save() {
  save_dirty = true;
  if (!save_due)
    save_due = now_us() + 200000;
}
uint32_t random() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}
uint32_t millis() { return uint32_t(foreground_us() / 1000); }
void log(const char *message) { pxa_log_write(2, message); }
} // namespace pocket
extern "C" int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
  pxa_ui_environment_t env{};
  if (pxa_ui_parse_start_environment(config, length, &env)) {
    display_w = env.width;
    display_h = env.height;
  }
  update_geometry(env);

#ifdef PXA_POCKET_STUDIO
  studio_geometry(env);
  studio_density = env.density_q16 ? env.density_q16 : 65536;
#endif
  pocket::set_english(false);
  snprintf(save_key, sizeof(save_key), "%s.save.v1", game::app_id());
  snprintf(preferences_key, sizeof(preferences_key), "%s.prefs.v1",
           game::app_id());
  pxa_ui_transaction_t tx{};
  if (!pxa_ui_transaction_begin(&tx, 1, PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                packet, sizeof(packet)) ||
      !pxa_ui_create(&tx, 1, 0, 0, PXA_UI_NODE_ROOT) ||
      !pxa_ui_set_rgba(&tx, 1, PXA_UI_PROPERTY_BACKGROUND, 0) ||
      !pxa_ui_set_event_mask(&tx, 1,
                             PXA_UI_EVENT_MASK_CONTROLLER_STATE |
                                 PXA_UI_EVENT_MASK_POINTER) ||

#ifdef PXA_POCKET_STUDIO
      !studio_create(&tx) ||
#endif
      !pxa_ui_transaction_commit(&tx))
    return PXA_STATUS_INTERNAL;
  int32_t status = pxa_window_fullscreen();
  if (status)
    return status;
  if ((status = pxa_event_port_open(PORT)))
    return status;
  if ((status = request_surface()))
    return status;
  scan_next_world();
  request_sound();
  service_preferences();
  return pxa_clock_set_period(20);
}
extern "C" int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t length) {
  int32_t status = handle_event(bytes, length);
  if (status < 0)
    return status;
  service_preferences();
  if (!running && !finished && !background && fresh_tick && port &&
      surface_ready && load_ready)
    run();
  // After the main loop drains storage, the queued BACK must reach the Host.
  return status;
}
extern "C" void pxa_app_stop(uint32_t reason) {
  (void)reason;
  finished = true;
  buttons = 0;
  pointer_active = false;
  // Host resources have already been revoked: do not call imports or hooks.
}
