// SPDX-License-Identifier: MIT
#include "audio.hpp"
#include "../common/pocket-play/runtime.hpp"
#include "font_cache.hpp"
#include "game_blit.hpp"
#include "pinch_input.hpp"
#include "pxa.h"
#include "pxa_assets.h"
#include "pxa_audio.h"
#include "pxa_permission.h"
#include "save_transport.hpp"
#include <stdio.h>
#include <string.h>
namespace {
constexpr uint64_t PERM = 8100, OPEN = 8101, GRAPH = 8102, LOAD = 8120;
constexpr unsigned COUNT = 6;
const char *paths[COUNT] = {
    "assets/audio/click.pcm",    "assets/audio/whoosh.pcm",
    "assets/audio/chimes.pcm",   "assets/audio/paper.pcm",
    "assets/audio/metalhit.pcm", "assets/audio/arrow.pcm"};
uint64_t session = 0, permission = 0, pending = 0, handles[COUNT]{};
uint8_t loading[COUNT]{};
unsigned ready = 0, bytes = 0, accepted = 0, blocked = 0, errors = 0;
bool requested = false, active = false, on = true;
constexpr const char *MUSIC = "assets/audio/thatched-villagers.ogg";
uint64_t music_instance = 0, music_retry = 0, music_now = 0, control_retry = 0;
bool music_on = true, music_paused = false, music_failed = false;
unsigned music_attempts = 0;
void sync_music() {
  if (!session || !music_instance)
    return;
  bool pause = !active || !music_on;
  if (pause == music_paused || music_now < control_retry)
    return;
  int status = pxa_audio_control_music(
      session, pause ? PXA_AUDIO_ASSET_PAUSE : PXA_AUDIO_ASSET_RESUME, 0);
  if (status == 4) {
    music_paused = pause;
    control_retry = 0;
    pocket::log(pause ? "Unciv: music paused" : "Unciv: music resumed");
  } else if (status == PXA_STATUS_WOULD_BLOCK || status == PXA_STATUS_NOT_FOUND)
    control_retry = music_now + 100000;
  else {
    music_failed = true;
    music_instance = 0;
    pocket::log("Unciv: music control unavailable");
  }
}
void update_music(uint64_t now) {
  sync_music();
  if (!active || !music_on || !session || music_instance || music_failed ||
      now < music_retry)
    return;
  for (unsigned i = 0; i < COUNT; ++i)
    if (loading[i] != 2)
      return;
  uint64_t instance = 0;
  int n = pxa_audio_play_music(session, MUSIC, 1, -18 * 256, &instance);
  if (n == int(strlen(MUSIC) + 16)) {
    music_instance = instance;
    music_paused = false;
    music_attempts = 0;
    pocket::log("Unciv: music requested: Thatched Villagers / Kevin MacLeod");
  } else if (n == PXA_STATUS_WOULD_BLOCK && ++music_attempts < 5)
    music_retry = now + 1000000;
  else {
    music_failed = true;
    pocket::log("Unciv: music unavailable; effects remain enabled");
  }
}
void stop() {
  if (session)
    for (uint8_t track = 0; track < 2; ++track)
      pxa_audio_control_sound(session, track, PXA_AUDIO_ASSET_STOP, 0);
}
void report() {
  char text[160];
  snprintf(text, sizeof(text),
           "Unciv: sfx host-clock ready=%u bytes=%u accepted=%u blocked=%u "
           "errors=%u",
           ready, bytes, accepted, blocked, errors);
  pocket::log(text);
}
void foreground(bool fg) {
  if (active != fg)
    control_retry = 0;
  if (active && !fg)
    stop();
  active = fg;
  sync_music();
}
} // namespace
namespace sound {
void play(Cue c) {
  if (!on || !active || !session || unsigned(c) >= COUNT || !handles[c])
    return;
  // UI sounds replace UI sounds; movement/battle/city cues share another
  // track. No delayed retry queue, file reads, allocations or Guest PCM writes.
  uint8_t track = c == Click || c == Paper ? 0 : 1;
  int32_t n =
      pxa_audio_play_sound_track(session, handles[c], track, 0, -4 * 256);
  if (n == 16)
    ++accepted;
  else if (n == PXA_STATUS_WOULD_BLOCK)
    ++blocked;
  else
    ++errors;
  if (accepted <= 3 || accepted % 16 == 0 || n != 16)
    report();
}
void toggle() {
  on = !on;
  if (!on)
    stop();
}
bool enabled() { return on; }
void toggle_music() {
  music_on = !music_on;
  control_retry = 0;
  if (music_on) {
    music_failed = false;
    music_attempts = 0;
    music_retry = 0;
  }
  sync_music();
}
bool music_enabled() { return music_on; }
} // namespace sound
namespace game {
bool service_event(const pxa_event_t &e, bool fg, uint64_t now) {
  music_now = now;
  foreground(fg);
  if (pinch_input::event(e, fg))
    return true;
  if (save_transport::event(e) || font_cache::event(e) || game_blit::event(e))
    return true;
  if (e.service == PXA_ASSETS_SERVICE && e.token >= LOAD &&
      e.token < LOAD + COUNT) {
    unsigned i = unsigned(e.token - LOAD);
    if (loading[i] != 1)
      return true;
    loading[i] = 2;
    pxa_asset_result_t r{};
    if (pxa_assets_parse_result(&e, e.token, PXA_ASSETS_LOAD, &r) &&
        !r.status && r.info.kind == PXA_ASSET_AUDIO) {
      handles[i] = r.handle;
      ++ready;
      bytes += r.info.resident_bytes;
    } else {
      if (r.handle)
        pxa_close_handle(r.handle);
      ++errors;
    }
    report();
    return true;
  }
  if (e.service == PXA_PERMISSION_SERVICE && e.token == PERM) {
    pxa_permission_acquire_result_t r{};
    if (pxa_permission_parse_acquire(&e, PERM, &r) && !r.status) {
      permission = r.handle;
      if (pxa_audio_open_media(OPEN, permission))
        ++errors;
    }
    return true;
  }
  pxa_permission_revoked_t rev{};
  if (pxa_permission_parse_revoked(&e, &rev) && rev.name_size == 14 &&
      !memcmp(rev.name, "audio.playback", 14)) {
    // The Host has already stopped the revoked session. Prepared assets have
    // their own lifetime and remain reusable after permission is acquired
    // again.
    session = pending = permission = 0;
    music_instance = music_retry = 0;
    music_paused = music_failed = false;
    music_attempts = 0;
    requested = false;
    return true;
  }
  if (e.service != PXA_AUDIO_SERVICE)
    return false;
  pxa_audio_playback_event_t playback{};
  if (pxa_audio_parse_playback(&e, &playback)) {
    if (playback.session != session || playback.instance != music_instance)
      return true;
    if (playback.state == PXA_AUDIO_PLAYBACK_READY) {
      pocket::log("Unciv: music ready: Thatched Villagers");
      control_retry = 0;
      sync_music();
    } else if (playback.state >= PXA_AUDIO_PLAYBACK_ENDED) {
      music_instance = 0;
      music_failed = true;
      pocket::log(playback.state == PXA_AUDIO_PLAYBACK_ERROR
                      ? "Unciv: music decode/playback error"
                      : "Unciv: music ended or replaced");
    }
    return true;
  }
  if (e.token == OPEN) {
    pxa_audio_open_result_t r{};
    if (pxa_audio_parse_open(&e, OPEN, &r) && !r.status) {
      pending = r.handle;
      if (pxa_audio_commit_gain(GRAPH, pending, 0)) {
        pxa_close_handle(pending);
        pending = 0;
        ++errors;
      }
    } else
      ++errors;
    return true;
  }
  if (e.token == GRAPH) {
    int32_t status = 0;
    if (pxa_audio_parse_status(&e, GRAPH, PXA_AUDIO_COMMIT_GRAPH, &status) &&
        !status)
      session = pending;
    else {
      if (pending)
        pxa_close_handle(pending);
      ++errors;
    }
    pending = 0;
    return true;
  }
  return false;
}
void service_update(bool fg, uint64_t now) {
  music_now = now;
  foreground(fg);
  if (!fg)
    pinch_input::reset();
  font_cache::update(fg);
  game_blit::update(fg);
  if (!fg)
    return;
  if (!requested) {
    uint8_t p[256];
    uint32_t n;
    if (pxa_permission_build(p, sizeof(p), PXA_PERMISSION_ACQUIRE, PERM,
                             "audio.playback", 14, (const uint8_t *)"media", 5,
                             &n) &&
        !pxa_control_packet(p, n))
      requested = true;
  }
  // One asynchronous preparation at a time bounds startup work. A failed
  // effect stays silent; it never falls back to frame-paced audio streaming.
  for (unsigned i = 0; i < COUNT; ++i) {
    if (loading[i] == 1)
      break;
    if (loading[i] != 0)
      continue;
    int32_t n = pxa_assets_load_sound(LOAD + i, paths[i]);
    if (!n)
      loading[i] = 1;
    else if (n != PXA_STATUS_WOULD_BLOCK) {
      loading[i] = 2;
      ++errors;
    }
    break;
  }
  update_music(now);
}
} // namespace game
