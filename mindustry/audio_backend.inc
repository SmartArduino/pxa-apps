// SPDX-License-Identifier: GPL-3.0-only
// App-local adaptation of the MIT Milky Frog and CC0 Pocket Play audio adapters.
// Original adapter notices are retained in assets/licenses/. No PCM pump: the
// Host mixes prepared effects and streams original Ogg music independently.
#include "audio.hpp"
#include "pxa_assets.h"
#include <cmath>
enum : uint64_t { SOUND_PERMISSION = 600, SOUND_LEGACY, SOUND_GRAPH,
                  RECORDING_LOAD = 800, RECORDING_QUERY };
constexpr unsigned SoundTracks = 6, RecordingSlots = 12;
constexpr unsigned RECORDING_BYTES_LIMIT = 480 * 1024;
struct Recording { uint64_t handle; unsigned id, age, bytes; };
struct RequestedSound { unsigned id, gain; bool pending, loop; uint64_t until; };
struct SoundTrack { unsigned id = ~0u, gain = 0; bool loop = false; uint64_t end = 0; };
Recording recordings[RecordingSlots]{};
RequestedSound requested_sounds[SoundTracks]{};
SoundTrack sound_tracks[SoundTracks]{};
const char *const *recording_paths;
unsigned recording_count, recording_age, recording_loading_id, recording_loading_slot;
unsigned recording_sizes[64]{}, recording_query_id, prewarm_index;
bool recording_loading, recording_querying;
uint64_t recording_retry, sound_handle, sound_pending_handle, sound_permission;
unsigned effects_gain = 65, music_gain = 55;
int16_t recording_gain(unsigned percentage) {
  if (!percentage) return -60 * 256;
  return int16_t(std::max(-60.f, -3.f + 20.f * std::log10(std::min(100u, percentage) / 100.f)) * 256);
}
const char *const music_paths[] = {
  "assets/music/menu.ogg", "assets/music/game1.ogg", "assets/music/game3.ogg",
  "assets/music/game6.ogg", "assets/music/game8.ogg", "assets/music/game9.ogg",
  "assets/music/fine.ogg", "assets/music/game2.ogg", "assets/music/game5.ogg",
  "assets/music/game7.ogg", "assets/music/game4.ogg"
};
md_audio::Scene music_scene = md_audio::Scene::Menu;
unsigned music_mode, music_id, music_last_random = ~0u, music_failures;
uint32_t music_random = 0x1b873593;
uint64_t music_instance, music_retry, music_interval, music_wave_due;
uint64_t music_last_service, music_accepted_at;
unsigned music_level;
bool music_ready, music_paused, music_changing;
unsigned audio_random(unsigned limit) {
  music_random ^= music_random << 13; music_random ^= music_random >> 17;
  music_random ^= music_random << 5;
  return limit ? music_random % limit : 0;
}
void stop_track(unsigned track) {
  if (track >= SoundTracks) return;
  requested_sounds[track] = {};
  if (sound_handle && sound_tracks[track].end)
    pxa_audio_control_sound(sound_handle, uint8_t(track), PXA_AUDIO_ASSET_STOP, 0);
  sound_tracks[track] = {};
}
void music_stop() {
  if (sound_handle && music_instance)
    pxa_audio_control_music(sound_handle, PXA_AUDIO_ASSET_STOP, 0);
  music_instance = 0; music_ready = music_paused = false; music_level = 0;
}
void clear_sound() {
  for (unsigned i = 0; i < SoundTracks; ++i) stop_track(i);
  if (exit_requested) music_stop();
  else if (sound_handle && music_instance && !music_paused) {
    if (pxa_audio_control_music(sound_handle, PXA_AUDIO_ASSET_PAUSE, 0) >= 0)
      music_paused = true;
  }
}
void add_tone(float, float, float) {} // Unused Pocket runtime compatibility.
void request_sound() {
  uint32_t n;
  if (pxa_permission_build(packet, sizeof(packet), PXA_PERMISSION_ACQUIRE, SOUND_PERMISSION,
      "audio.playback", 14, reinterpret_cast<const uint8_t *>("media"), 5, &n))
    pxa_control_packet(packet, n);
}
int32_t handle_sound(const pxa_event_t &e) {
  pxa_audio_playback_event_t playback{};
  if (pxa_audio_parse_playback(&e, &playback)) {
    if (playback.session != sound_handle || playback.instance != music_instance)
      return PXA_EVENT_HANDLED; // Replacement and delayed events are harmless.
    char line[120];
    snprintf(line, sizeof(line), "Mindustry: music event id=%u state=%u status=%ld",
             music_id, playback.state, (long)playback.status);
    pxa_log_write(2, line);
    if (playback.state == PXA_AUDIO_PLAYBACK_READY) {
      music_ready = true; music_failures = 0;
    } else {
      music_instance = 0; music_ready = music_paused = false; music_level = 0;
      if (playback.state == PXA_AUDIO_PLAYBACK_ERROR) {
        music_failures = std::min(6u, music_failures + 1);
        music_retry = foreground_us() + (uint64_t(1) << music_failures) * 1000000;
      }
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_PERMISSION_SERVICE && e.token == SOUND_PERMISSION) {
    pxa_permission_acquire_result_t r{};
    if (!pxa_permission_parse_acquire(&e, SOUND_PERMISSION, &r)) return PXA_STATUS_INTERNAL;
    if (!r.status) { sound_permission = r.handle; pxa_audio_open_media(SOUND_LEGACY, r.handle); }
    else pxa_log_write(2, "Mindustry: optional audio denied");
    return PXA_EVENT_HANDLED;
  }
  pxa_permission_revoked_t revoked{};
  if (pxa_permission_parse_revoked(&e, &revoked)) {
    if (revoked.name_size == 14 && !memcmp(revoked.name, "audio.playback", 14)) {
      sound_handle = sound_pending_handle = sound_permission = 0;
      music_instance = 0; music_ready = music_paused = false;
      for (unsigned t = 0; t < SoundTracks; ++t) { requested_sounds[t] = {}; sound_tracks[t] = {}; }
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_AUDIO_SERVICE && e.token == SOUND_LEGACY) {
    pxa_audio_open_result_t r{};
    if (!pxa_audio_parse_open(&e, SOUND_LEGACY, &r)) return PXA_STATUS_INTERNAL;
    if (!r.status) { sound_pending_handle = r.handle; pxa_audio_commit_gain(SOUND_GRAPH, r.handle, 0); }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_AUDIO_SERVICE && e.token == SOUND_GRAPH) {
    int32_t status;
    if (!pxa_audio_parse_status(&e, SOUND_GRAPH, PXA_AUDIO_COMMIT_GRAPH, &status)) return PXA_STATUS_INTERNAL;
    if (!status && sound_permission) {
      sound_handle = sound_pending_handle;
      music_random ^= uint32_t(now_us());
      pxa_log_write(2, "Pocket: audio ready 16000 Hz mono");
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_ASSETS_SERVICE && e.token == RECORDING_QUERY) {
    pxa_asset_result_t r{}; recording_querying = false;
    if (!pxa_assets_parse_result(&e, RECORDING_QUERY, PXA_ASSETS_QUERY, &r)) return PXA_STATUS_INTERNAL;
    if (!r.status && recording_query_id < 64) recording_sizes[recording_query_id] = r.info.decoded_bytes;
    else recording_retry = now_us() + 1000000;
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_ASSETS_SERVICE && e.token == RECORDING_LOAD) {
    pxa_asset_result_t r{}; recording_loading = false;
    if (!pxa_assets_parse_result(&e, RECORDING_LOAD, PXA_ASSETS_LOAD, &r)) return PXA_STATUS_INTERNAL;
    if (!r.status && r.info.kind == PXA_ASSET_AUDIO && r.info.encoding == PXA_ASSET_ENCODING_PCM_S16LE_16K_MONO) {
      recordings[recording_loading_slot] = {r.handle, recording_loading_id, ++recording_age, r.info.decoded_bytes};
      char line[96]; snprintf(line, sizeof(line), "Milky: recording ready id=%u bytes=%lu", recording_loading_id, (unsigned long)r.info.decoded_bytes); pxa_log_write(2, line);
    } else {
      if (r.handle) pxa_close_handle(r.handle);
      recording_retry = now_us() + 1000000;
      pxa_log_write(3, "Mindustry: recording preparation deferred");
    }
    return PXA_EVENT_HANDLED;
  }
  return PXA_EVENT_UNHANDLED;
}
void service_music() {
  const uint64_t now = foreground_us();
  unsigned elapsed = unsigned(std::min<uint64_t>(100000, now - music_last_service));
  music_last_service = now;
  if (!music_gain || music_mode == 2) { music_stop(); return; }
  if (music_paused && music_instance) {
    if (pxa_audio_control_music(sound_handle, PXA_AUDIO_ASSET_RESUME, 0) >= 0) music_paused = false;
  }
  if (music_changing && music_instance) {
    // Original 120-frame fade when crossing between menu and gameplay.
    music_level = music_level > elapsed ? music_level - elapsed : 0;
    if (!music_level) music_stop();
  } else if (music_instance && music_ready) music_level = std::min(2000000u, music_level + elapsed);
  if (music_instance && music_ready) {
    unsigned level = std::max(1u, music_gain * (music_level / 2000) / 1000);
    static int16_t previous = 1;
    int16_t gain = recording_gain(level);
    if (gain != previous && pxa_audio_control_music(sound_handle, PXA_AUDIO_ASSET_SET_GAIN, gain) >= 0) previous = gain;
  }
  if (music_instance && !music_ready && now - music_accepted_at > 15000000) {
    music_stop(); music_failures = std::min(6u, music_failures + 1);
    music_retry = now + (uint64_t(1) << music_failures) * 1000000;
    pxa_log_write(3, "Mindustry: music readiness timed out; retry deferred");
  }
  if (music_instance || now < music_retry) return;
  bool start = music_scene == md_audio::Scene::Menu || music_mode == 1;
  if (music_interval && now >= music_interval) {
    music_interval = now + 180000000;
    start |= audio_random(100) < 80;
  }
  if (music_wave_due && now >= music_wave_due) {
    music_wave_due = 0; start |= audio_random(100) < 46;
  }
  // Original intermittent playback: retries after an actual decoder error are
  // delayed, rather than turning every frame into another stream-open request.
  start |= music_failures != 0;
  music_changing = false;
  if (!start) return;
  unsigned id = 0;
  if (music_scene != md_audio::Scene::Menu) {
    unsigned first = music_scene == md_audio::Scene::Dark ? 7 : 1;
    unsigned count = music_scene == md_audio::Scene::Dark ? 4 : 6;
    id = first + audio_random(count);
    if (id == music_last_random) id = first + ((id - first + 1) % count);
  }
  uint64_t instance = 0;
  int result = pxa_audio_play_music(sound_handle, music_paths[id], id == 0, -60 * 256, &instance);
  if (result > 0 && instance) {
    music_id = id; if (id) music_last_random = id;
    music_instance = instance; music_accepted_at = now; music_ready = music_paused = false; music_level = 0;
    char line[128]; snprintf(line, sizeof(line), "Mindustry: music accepted id=%u instance=%llu path=%s", id, (unsigned long long)instance, music_paths[id]); pxa_log_write(2, line);
  } else {
    music_failures = std::min(6u, music_failures + 1);
    music_retry = now + (uint64_t(1) << music_failures) * 1000000;
  }
}
bool recording_active(unsigned id) {
  for (const auto &t : sound_tracks)
    if (t.id == id && (t.loop || foreground_us() < t.end)) return true;
  return false;
}
void service_sound() {
  if (!sound_handle || background || paused || exit_requested) return;
  service_music();
  unsigned wanted = recording_count;
  for (unsigned track = 0; track < SoundTracks; ++track) {
    auto &request = requested_sounds[track];
    auto &playing = sound_tracks[track];
    if (playing.loop && foreground_us() > playing.end) stop_track(track);
    if (!request.pending) continue;
    if (!effects_gain || request.id >= recording_count || foreground_us() > request.until) { request = {}; continue; }
    bool ready = false;
    for (auto &r : recordings) if (r.handle && r.id == request.id) {
      unsigned gain = effects_gain * request.gain / 100;
      auto result = pxa_audio_play_sound_track(sound_handle, r.handle, uint8_t(track), request.loop, recording_gain(gain));
      if (result >= 0) {
        r.age = ++recording_age;
        playing = {r.id, request.gain, request.loop, foreground_us() + (request.loop ? 500000 : uint64_t(r.bytes) * 1000000 / 32000)};
        request = {};
        char line[96]; snprintf(line, sizeof(line), "Milky: recording play id=%u track=%u", r.id, track); pxa_log_write(2, line);
      }
      ready = true; break;
    }
    if (!ready && wanted == recording_count) wanted = request.id;
  }
  if (recording_loading || recording_querying || now_us() < recording_retry) return;
  // Small, common effects are warmed without delaying the first frame.
  if (wanted == recording_count && effects_gain) {
    const unsigned warm[] = {0, 2, 8, 4, 5, 3, 1};
    while (prewarm_index < sizeof(warm)/sizeof(*warm)) {
      unsigned id = warm[prewarm_index++]; bool found = false;
      for (auto &r : recordings) found |= r.handle && r.id == id;
      if (!found && id < recording_count) { wanted = id; break; }
    }
  }
  if (wanted >= recording_count || wanted >= 64) return;
  if (!recording_sizes[wanted]) {
    recording_query_id = wanted;
    recording_querying = pxa_assets_query(RECORDING_QUERY, recording_paths[wanted]) == 0;
    if (!recording_querying) recording_retry = now_us() + 1000000;
    // Revisit a prewarm after its size query completes.
    for (unsigned i = 0; i < SoundTracks; ++i) if (requested_sounds[i].pending && requested_sounds[i].id == wanted) return;
    if (prewarm_index) --prewarm_index;
    return;
  }
  unsigned used = 0; int slot = -1;
  for (unsigned i = 0; i < RecordingSlots; ++i) {
    used += recordings[i].bytes;
    if (!recordings[i].handle) slot = int(i);
  }
  // Never evict any recording still in use by a Host voice, even if all slots
  // are occupied. The caller retries; simulation and input continue normally.
  while (used + recording_sizes[wanted] > RECORDING_BYTES_LIMIT || slot < 0) {
    int oldest = -1;
    for (unsigned i = 0; i < RecordingSlots; ++i)
      if (recordings[i].handle && !recording_active(recordings[i].id) &&
          (oldest < 0 || recordings[i].age < recordings[oldest].age)) oldest = int(i);
    if (oldest < 0) return;
    auto &r = recordings[oldest];
    if (pxa_close_handle(r.handle)) return;
    used -= r.bytes; r = {}; slot = oldest;
  }
  recording_loading_id = wanted; recording_loading_slot = unsigned(slot);
  recording_loading = pxa_assets_load_sound(RECORDING_LOAD, recording_paths[wanted]) == 0;
  if (!recording_loading) recording_retry = now_us() + 1000000;
}
