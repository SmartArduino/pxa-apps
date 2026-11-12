// SPDX-License-Identifier: MIT
// Exercise the actual Guest protocol. Large time gaps between service updates
// must not split effects into frame-paced WRITE calls or replay stale clicks.
#include "../audio.hpp"
#include "../font_cache.hpp"
#include "../game_blit.hpp"
#include "../pinch_input.hpp"
#include "../save_transport.hpp"
#include "pxa_assets.h"
#include "pxa_audio.h"
#include "pxa_permission.h"
#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
namespace game {
bool service_event(const pxa_event_t &, bool, uint64_t);
void service_update(bool, uint64_t);
} // namespace game
namespace pocket {
void log(const char *) {}
} // namespace pocket
namespace pinch_input {
bool event(const pxa_event_t &, bool) { return false; }
void reset() {}
} // namespace pinch_input
namespace font_cache {
bool event(const pxa_event_t &) { return false; }
void update(bool) {}
} // namespace font_cache
namespace game_blit {
bool event(const pxa_event_t &) { return false; }
void update(bool) {}
} // namespace game_blit
namespace save_transport {
bool event(const pxa_event_t &) { return false; }
} // namespace save_transport
struct Request {
  uint16_t service, opcode;
  uint64_t token;
};
std::vector<Request> requests;
unsigned played = 0, stopped = 0, pcm_writes = 0;
unsigned music_started = 0, music_pauses = 0, music_resumes = 0;
int result = 16;
uint8_t last_track = 255;
extern "C" int32_t pxa_call(uint32_t route, uint64_t token, const uint8_t *,
                            uint32_t) {
  requests.push_back({uint16_t(route), uint16_t(route >> 16), token});
  return 0;
}
extern "C" int32_t pxa_io(uint64_t, uint32_t op, uint8_t *data, uint32_t size) {
  if (op == PXA_AUDIO_IO_PLAY_SOUND) {
    assert(size == 16 && data[10] < 2 && data[11] == 0);
    last_track = data[10];
    ++played;
    return result;
  }
  if (op == PXA_AUDIO_IO_CONTROL_SOUND) {
    assert(size == 8 && data[0] < 2 && data[1] == PXA_AUDIO_ASSET_STOP);
    ++stopped;
    return size;
  }
  if (op == PXA_AUDIO_IO_PLAY_MUSIC) {
    assert(data[12] == PXA_AUDIO_ASSET_LOOP);
    assert(pxa_load_u16(data + 8) ==
           strlen("assets/audio/thatched-villagers.ogg"));
    assert(
        !memcmp(data + 16, "assets/audio/thatched-villagers.ogg", size - 16));
    pxa_store_u64(data, 100 + ++music_started);
    return size;
  }
  if (op == PXA_AUDIO_IO_CONTROL_MUSIC) {
    assert(size == 4);
    music_pauses += data[0] == PXA_AUDIO_ASSET_PAUSE;
    music_resumes += data[0] == PXA_AUDIO_ASSET_RESUME;
    return size;
  }
  if (op == PXA_AUDIO_IO_WRITE)
    ++pcm_writes;
  assert(false);
  return -1;
}
void event(uint16_t service, uint16_t opcode, uint64_t token,
           const uint8_t *data, unsigned size, bool fg = true) {
  assert(game::service_event({service, opcode, token, data, size}, fg, 1));
}
void open_session() {
  uint8_t permission[12]{};
  pxa_store_u64(permission + 4, UINT64_C(0x100000001));
  event(PXA_PERMISSION_SERVICE, PXA_PERMISSION_ACQUIRE, 8100, permission, 12);
  assert(requests.back().opcode == PXA_AUDIO_OPEN_SESSION);
  uint8_t data[35]{}, value[8]{};
  size_t off = 4;
  pxa_store_u64(value, UINT64_C(0x100000002));
  assert(pxa_audio_append(data, sizeof(data), &off, 3, value, 8));
  pxa_store_u32(value, 16000);
  assert(pxa_audio_append(data, sizeof(data), &off, 4, value, 4));
  value[0] = 1;
  assert(pxa_audio_append(data, sizeof(data), &off, 5, value, 1));
  pxa_store_u16(value, 20);
  assert(pxa_audio_append(data, sizeof(data), &off, 6, value, 2));
  event(PXA_AUDIO_SERVICE, PXA_AUDIO_OPEN_SESSION, 8101, data, off);
  uint8_t status[4]{};
  event(PXA_AUDIO_SERVICE, PXA_AUDIO_COMMIT_GRAPH, 8102, status, 4);
}
int main() {
  game::service_update(true, 1);
  open_session();
  game::service_update(true, 2);
  for (unsigned i = 0; i < 6; ++i) {
    assert(std::any_of(requests.begin(), requests.end(), [i](auto r) {
      return r.service == PXA_ASSETS_SERVICE && r.token == 8120 + i;
    }));
    auto before = requests.size();
    game::service_update(true, 1000000); // No duplicate while result delayed.
    assert(requests.size() == before);
    uint8_t data[32]{};
    pxa_store_u64(data + 4, UINT64_C(0x100000010) + i);
    data[12] = PXA_ASSET_AUDIO;
    pxa_store_u32(data + 28, 16000);
    if (i == 5) { // One unavailable sound must not disable the other effects.
      pxa_store_u32(data, uint32_t(PXA_STATUS_NOT_FOUND));
      event(PXA_ASSETS_SERVICE, PXA_ASSETS_LOAD, 8120 + i, data, 4);
    } else
      event(PXA_ASSETS_SERVICE, PXA_ASSETS_LOAD, 8120 + i, data, 32);
    game::service_update(true, 2000000 + i * 500000);
  }
  assert(music_started == 1); // A failed sound effect does not disable music.
  sound::play(sound::City);
  assert(played == 1 && last_track == 1);
  sound::play(sound::Click);
  assert(played == 2 && last_track == 0);
  for (unsigned i = 0; i < 100; ++i)
    game::service_update(true, 10000000 + i * 80000);
  assert(played == 2 && pcm_writes == 0 &&
         music_started == 1); // Host advances the complete clip.
  result = PXA_STATUS_WOULD_BLOCK;
  sound::play(sound::Battle);
  game::service_update(true, 20000000);
  assert(played == 3); // No late retry storm.
  sound::play(sound::Arrow);
  assert(played == 3);
  result = 16;
  unsigned before = stopped;
  // A background service event can arrive before service_update.
  uint8_t dummy[4]{};
  game::service_event({99, 1, 1, dummy, 4}, false, 30000000);
  assert(stopped == before + 2 && music_pauses == 1);
  sound::play(sound::City);
  assert(played == 3);
  game::service_update(true, 31000000);
  sound::toggle();
  assert(!sound::enabled() && stopped == before + 4);
  sound::play(sound::Click);
  assert(played == 3);
  sound::toggle();
  sound::play(sound::Click);
  assert(played == 4 && !pcm_writes);
  assert(music_resumes == 1 && music_pauses == 1);
  sound::toggle_music();
  assert(!sound::music_enabled() && sound::enabled() && music_pauses == 2);
  sound::toggle_music();
  assert(sound::music_enabled() && music_resumes == 2 && music_started == 1);
  uint8_t playback[24]{};
  pxa_store_u64(playback, UINT64_C(0x100000002));
  pxa_store_u64(playback + 8, 999); // Stale playback instance must be ignored.
  playback[16] = PXA_AUDIO_PLAYBACK_ERROR;
  pxa_store_u32(playback + 20, uint32_t(PXA_STATUS_NOT_FOUND));
  event(PXA_AUDIO_SERVICE, PXA_AUDIO_PLAYBACK_EVENT, 0, playback, 24);
  game::service_update(true, 32000000);
  assert(music_started == 1);
  pxa_store_u64(playback + 8, 101);
  event(PXA_AUDIO_SERVICE, PXA_AUDIO_PLAYBACK_EVENT, 0, playback, 24);
  for (unsigned i = 0; i < 100; ++i)
    game::service_update(true, 33000000 + i * 100000);
  assert(music_started == 1); // Decode failure never causes a restart loop.
  sound::toggle_music();
  sound::toggle_music();
  game::service_update(true, 44000000);
  assert(music_started == 2 && !pcm_writes);
  puts("Audio: async preload, whole-clip Host playback, delayed ticks, two "
       "tracks, backpressure, missing assets, background/mute, streamed music "
       "lifecycle and stale events passed");
}
