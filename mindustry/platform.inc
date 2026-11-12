// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-game/runtime.hpp"
#include "../common/pocket-ui/preferences.hpp"
#include "../common/pocket-ui/viewport.hpp"
#include "game_input.hpp"
#include "pxa.h"
#include "pxa_audio.h"
#include "pxa_clock.h"
#include "pxa_event_port.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include "pxa_surface.h"
#include "pxa_system.h"
#include "pxa_ui.h"
#include "pxa_window.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace {
using namespace pocket;
enum : uint64_t {
  PORT = 1,
  CREATE,
  CONFIGURE,
  LOAD,
  SAVE,
  PREFERENCES_LOAD,
  PREFERENCES_SAVE
};

uint8_t packet[PXA_MAX_CONTROL_BYTES], events[PXA_MAX_CONTROL_BYTES],
    loaded[SAVE_CAPACITY], pending_save[SAVE_CAPACITY];
char save_key[80];
size_t loaded_size;
uint64_t port, surface, frame_id, epoch, paused_us, pause_at, save_due,
    save_retry, last_tick;
uint64_t repeats[4];
uint32_t buttons, pointer_id, rng = 0x6d2b79f5;
int display_w = 320, display_h = 240;
uint32_t pointer_density = 65536;
int last_x, last_y, menu_row;
bool running, finished, background = true, fresh_tick, surface_ready,
                        load_ready;
bool dirty = true, save_dirty, save_pending, pointer_active, pointer_consumed,
     paused;
uint64_t revision = 1, exit_deadline, last_present;
bool exit_requested;

uint64_t now_us() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
#include "../common/pocket-ui/preferences.inc"
#include "render_surface.inc"
uint64_t foreground_us() {
  return epoch ? (background ? pause_at : now_us()) - epoch - paused_us : 0;
}
// The proven Pocket Play backend uses this runtime's foreground/lifecycle
// state.
#include "audio_backend.inc"
void cancel_input() {
  clear_sound();
  buttons = 0;
  memset(repeats, 0, sizeof(repeats));
  if (running)
    game::cancel_touches();
  pointer_active = pointer_consumed = false;
}
void send_save(bool force = false) {
  uint64_t now = now_us();
  if (!running || !save_dirty || save_pending || now < save_retry ||
      (!force && now < save_due))
    return;
  size_t size = game::serialize(pending_save, sizeof(pending_save));
  if (!size) {
    save_dirty = false;
    return;
  }
  if (size > sizeof(pending_save))
    __builtin_trap();
  int32_t status = pxa_storage_request_set(packet, sizeof(packet), SAVE,
                                           save_key, uint32_t(strlen(save_key)),
                                           pending_save, uint32_t(size));
  if (!status) {
    save_pending = true;
    save_dirty = false;
    save_due = 0;
  } else {
    save_retry = now + 1000000;
    pxa_log_write(3, "Pocket: save submission failed; retrying");
  }
}
void request_exit() {
  if (exit_requested)
    return;
  exit_requested = true;
  exit_deadline = now_us() + 2000000;
  cancel_input();
  paused = true;
  pocket::redraw();
  send_save(true);
}
void toggle_pause() {
  clear_sound();
  paused = !paused;
  menu_row = 0;
  pocket::redraw();
  if (running)
    game::cancel_touches();
  if (pointer_active)
    pointer_consumed = true;
  memset(repeats, 0, sizeof(repeats));
  pxa_log_write(2, paused ? "Pocket: menu opened" : "Pocket: menu closed");
  if (paused)
    send_save(true);
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
  if (c.controller || background || !running)
    return;
  uint32_t next = c.connected ? c.buttons : 0, edges = next & ~buttons;
  buttons = next;
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
    if (edges & key_masks[i])
      dispatch_key(Key(i));
}
void pointer(const pxa_ui_pointer_data_t &p) {
  if (background || !running || exit_requested)
    return;
  int px = pocket_ui::pointer_pixel(p.x, pointer_density);
  int py = pocket_ui::pointer_pixel(p.y, pointer_density);
  int x = viewport.map_x(px, WIDTH), y = viewport.map_y(py, HEIGHT);
  Phase phase = p.phase == PXA_POINTER_DOWN   ? Phase::DOWN
                : p.phase == PXA_POINTER_MOVE ? Phase::MOVE
                : p.phase == PXA_POINTER_UP   ? Phase::UP
                                              : Phase::CANCEL;
  if (phase == Phase::DOWN && (!surface_ready || !viewport.contains(px, py)))
    return;
  if (!paused && !(pointer_active && pointer_consumed)) {
    if (phase == Phase::DOWN && !game::touch_active() &&
        milky::menu_button().contains(x, y)) {
      pointer_id = p.pointer_id;
      pointer_active = pointer_consumed = true;
      toggle_pause();
    } else
      game::touch(phase, p.pointer_id, x, y);
  } else {
    // System pause rows retain one captured finger, including the release after
    // a row closes the menu. It cannot become a map tap underneath the overlay.
    if (phase == Phase::DOWN && !pointer_active) {
      pointer_id = p.pointer_id;
      pointer_active = pointer_consumed = true;
    }
    if (!pointer_active || pointer_id != p.pointer_id)
      return;
    if (paused && phase == Phase::DOWN)
      for (unsigned row = 0; row < 4; ++row)
        if (milky::pause_row(row).contains(x, y)) {
          menu_row = int(row);
          menu_activate();
          break;
        }
    if (phase == Phase::UP || phase == Phase::CANCEL)
      pointer_active = pointer_consumed = false;
  }
  last_x = x;
  last_y = y;
  pocket::redraw();
}
void draw_menu() { milky::menu_draw(paused, menu_row, exit_requested); }

void present() {
  gfx::service_fonts();
  if (!surface_ready || !dirty || background)
    return;
  // Cold text needs reply delivery more than repeated identical raster jobs.
  // Keep input responsive, refill the queue, and coalesce font-only paints.
  if (gfx::fonts_pending() && now_us() - last_present < 100000)
    return;
  uint64_t start = now_us(), before = revision;
  if (!gfx::begin(frame_id + 1, WIDTH, HEIGHT))
    return;
  game::draw();
  draw_menu();
  uint64_t encoded_us = now_us() - start, submit_start = now_us();
  int32_t status = gfx::finish();
  if (status == PXA_STATUS_WOULD_BLOCK)
    return;
  if (status < 0) {
    pxa_log_write(3, "Mindustry: GameRender submit failed");
    __builtin_trap();
  }
  ++frame_id;
  last_present = now_us();
  dirty = revision != before;
  gfx::profile(encoded_us, now_us() - submit_start);
}
int32_t handle_event(const uint8_t *bytes, uint32_t length) {
  pxa_event_t e;
  if (!pxa_parse_event(bytes, length, &e))
    return PXA_STATUS_INTERNAL;
  if (gfx::font_event(e)) {
    gfx::service_fonts(1);
    pocket::redraw();
    return PXA_EVENT_HANDLED;
  }
  int32_t audio_status = handle_sound(e);
  if (audio_status != PXA_EVENT_UNHANDLED)
    return audio_status;
  int32_t preferences_status = handle_preferences(e);
  if (preferences_status != PXA_EVENT_UNHANDLED)
    return preferences_status;
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
    if (next)
      send_save(true);
    return PXA_EVENT_HANDLED;
  }
  int32_t surface_status = handle_surface(e);
  if (surface_status != PXA_EVENT_UNHANDLED)
    return surface_status;
  if (e.service == PXA_STORAGE_SERVICE && e.token == LOAD) {
    pxa_storage_get_result_t r;
    if (!pxa_storage_parse_get(&e, LOAD, &r))
      return PXA_STATUS_INTERNAL;
    if (!r.status && r.value.size <= sizeof(loaded)) {
      loaded_size = r.value.size;
      memcpy(loaded, r.value.data, loaded_size);
    } else if (r.status != PXA_STATUS_NOT_FOUND)
      pxa_log_write(3, "Pocket: invalid/unavailable save; using defaults");
    load_ready = true;
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_STORAGE_SERVICE && e.token == SAVE) {
    int32_t status;
    if (!pxa_storage_parse_status(&e, SAVE, PXA_STORAGE_SET, &status))
      return PXA_STATUS_INTERNAL;
    save_pending = false;
    if (status) {
      save_dirty = true;
      save_retry = now_us() + 1000000;
      pxa_log_write(3, "Pocket: save failed; retrying latest state");
    } else
      pxa_log_write(2, "Pocket: saved");
    return PXA_EVENT_HANDLED;
  }
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
    pointer_density = env.density_q16 ? env.density_q16 : 65536;
    cancel_input();
    milky::configure_screen(env.width, env.height, env.density_q16,
                            env.safe_insets, env.display_shape,
                            env.corner_radii);
    if (surface_ready)
      apply_render_layout(viewport.width, viewport.height);
    pocket::redraw();
    if (!env.width || !env.height ||
        (uint32_t(display_w) == env.width && uint32_t(display_h) == env.height))
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
  epoch = last_tick = now_us();
  rng ^= uint32_t(epoch) ^ uint32_t(epoch >> 32);
  game::init();
  if (loaded_size) {
    game::load(loaded, loaded_size);
    pxa_log_write(2, "Pocket: load completed");
  }
  char message[160];
  snprintf(message, sizeof(message),
           "Pocket: ready app=%s canvas=%dx%d origin=%d,%d scale=%d",
           game::app_id(), WIDTH, HEIGHT, viewport.x, viewport.y,
           viewport.host_scale);
  pxa_log_write(2, message);
  while (!finished) {
    uint64_t loop_start = now_us();
    // Notifications must never form a backlog in front of input.
    uint64_t drain_start = now_us();
    for (unsigned i = 0; i < 32 && wait_event(0); ++i)
      if (now_us() - drain_start >= 4000)
        break;
    send_save(background || exit_requested);
    service_preferences();
    if (exit_requested && ((!save_dirty && !save_pending &&
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
    if (!finished) {
      uint64_t elapsed = now_us() - loop_start;
      uint64_t delay = elapsed < 20000 ? 20000 - elapsed : 1000;
      // Font replies refill the queue immediately. Drawing time already counts
      // toward the loop period; adding a fixed 20 ms here capped ESP at ~24 Hz.
      if (gfx::fonts_pending())
        delay = 1000;
      wait_event(delay);
    }
  }
}

} // namespace

namespace pocket {
bool portrait_display() { return display_h > display_w; }
void rect(int x, int y, int w, int h, uint16_t color) {
  gfx::rect(x, y, w, h, color);
}
void clear(uint16_t color) { gfx::clear(color); }
void outline(int x, int y, int w, int h, uint16_t color) {
  rect(x, y, w, 1, color);
  rect(x, y + h - 1, w, 1, color);
  rect(x, y, 1, h, color);
  rect(x + w - 1, y, 1, h, color);
}
void line(int x, int y, int xx, int yy, uint16_t color) {
  gfx::line(x, y, xx, yy, color);
}
int text_width(const char *s, int scale) {
  return pocket_ui::text_width(s, scale);
}
void text(int x, int y, const char *s, uint16_t color, int scale) {
  gfx::label(x, y, WIDTH - x, s, color, 10 * scale);
}
void tone(float frequency, float duration_seconds, float when_seconds) {
  add_tone(frequency, duration_seconds, when_seconds);
}
void silence() { clear_sound(); }
void redraw() {
  dirty = true;
  ++revision;
}
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
  pointer_density = env.density_q16 ? env.density_q16 : 65536;
  milky::configure_screen(display_w, display_h, env.density_q16,
                          env.safe_insets, env.display_shape, env.corner_radii);

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

      !pxa_ui_transaction_commit(&tx))
    return PXA_STATUS_INTERNAL;
  int32_t status = pxa_window_fullscreen();
  if (status)
    return status;
  if ((status = pxa_event_port_open(PORT)))
    return status;
  if ((status = request_surface()))
    return status;
  if ((status =
           pxa_storage_request_get(LOAD, save_key, uint32_t(strlen(save_key)))))
    return status;
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
