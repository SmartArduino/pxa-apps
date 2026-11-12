// SPDX-License-Identifier: GPL-3.0-only
// Real SDK command encoding + delayed playback events, without a physical sink.
#include "../audio.hpp"
#include "pxa.h"
#include "pxa_audio.h"
#include "pxa_permission.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
uint64_t test_now = 1000000;
uint64_t now_us() { return test_now; }
uint64_t foreground_us() { return test_now; }
bool background, paused, exit_requested;
uint8_t packet[PXA_MAX_CONTROL_BYTES];
struct Op { uint32_t operation; std::vector<uint8_t> data; };
std::vector<Op> ops;
std::vector<uint64_t> closed;
unsigned music_opens, sound_plays, checks;
uint64_t next_instance = 1;
int fail_play;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
extern "C" int32_t pxa_call(uint32_t route, uint64_t, const uint8_t *p, uint32_t) {
  if ((route & 65535) == PXA_CORE_SERVICE && (route >> 16) == PXA_CORE_CLOSE_HANDLE) closed.push_back(pxa_load_u64(p));
  return 0;
}
extern "C" int32_t pxa_io(uint64_t, uint32_t operation, uint8_t *p, uint32_t n) {
  CHECK(operation != PXA_AUDIO_IO_WRITE); // No silent 16 kHz Guest PCM pump.
  ops.push_back({operation, {p, p+n}});
  if (operation == PXA_AUDIO_IO_PLAY_MUSIC) {
    ++music_opens;
    if (fail_play) return fail_play;
    pxa_store_u64(p, next_instance++);
  }
  if (operation == PXA_AUDIO_IO_PLAY_SOUND) ++sound_plays;
  return int32_t(n);
}
#include "../audio_backend.inc"
#include "../audio_api.inc"
void event(uint64_t instance, unsigned state, int status = 0, uint64_t session = 0) {
  uint8_t payload[24]{};
  pxa_store_u64(payload, session ? session : sound_handle);
  pxa_store_u64(payload+8, instance); payload[16] = uint8_t(state);
  pxa_store_u32(payload+20, uint32_t(status));
  pxa_event_t e{}; e.service = PXA_AUDIO_SERVICE; e.opcode = PXA_AUDIO_PLAYBACK_EVENT;
  e.payload = payload; e.payload_size = sizeof(payload);
  CHECK(handle_sound(e) == PXA_EVENT_HANDLED);
}
void advance(unsigned ms) { test_now += uint64_t(ms)*1000; service_sound(); }
int main() {
  static const char *const paths[] = {"a.s16","b.s16","c.s16"};
  milky::sounds(paths, 3);
  sound_handle = sound_permission = uint64_t(1)<<32;
  prewarm_index = 99;
  service_sound();
  CHECK(music_opens == 1 && music_instance && !music_ready);
  auto menu = music_instance;
  advance(20); CHECK(music_opens == 1); // Admission is not READY.
  event(menu+100, PXA_AUDIO_PLAYBACK_READY); CHECK(!music_ready);
  event(menu, PXA_AUDIO_PLAYBACK_READY, 0, uint64_t(2)<<32); CHECK(!music_ready);
  event(menu, PXA_AUDIO_PLAYBACK_READY); CHECK(music_ready);
  for (int i=0;i<110;++i) advance(20);
  CHECK(music_level == 2000000 && music_opens == 1);
  md_audio::scene(md_audio::Scene::Calm, 1);
  for (int i=0;i<110;++i) advance(20);
  CHECK(music_opens == 2 && music_id >= 1 && music_id <= 6);
  auto game = music_instance;
  event(menu, PXA_AUDIO_PLAYBACK_REPLACED); CHECK(music_instance == game);
  event(game, PXA_AUDIO_PLAYBACK_READY);
  auto previous_id = music_id;
  event(game, PXA_AUDIO_PLAYBACK_ENDED); advance(20);
  CHECK(music_opens == 3 && music_instance != game);
  CHECK(music_id != previous_id);
  auto second = music_instance;
  event(second, PXA_AUDIO_PLAYBACK_ERROR, -11);
  auto before = music_opens;
  advance(1000); CHECK(music_opens == before);
  advance(1100); CHECK(music_opens == before+1);
  event(music_instance, PXA_AUDIO_PLAYBACK_READY);
  md_audio::scene(md_audio::Scene::Dark, 1);
  event(music_instance, PXA_AUDIO_PLAYBACK_ENDED); advance(20);
  CHECK(music_id >= 7 && music_id <= 10);
  event(music_instance, PXA_AUDIO_PLAYBACK_READY);
  md_audio::scene(md_audio::Scene::Dark, 0);
  auto ongoing = music_instance;
  md_audio::wave(); advance(16000);
  CHECK(music_instance == ongoing); // Wave must not interrupt an existing song.
  background = true; clear_sound(); CHECK(music_paused);
  advance(1000); CHECK(music_instance == ongoing && music_paused);
  background = false; advance(20); CHECK(music_instance == ongoing && !music_paused);
  paused = true; clear_sound(); advance(20); CHECK(music_paused);
  paused = false; advance(20); CHECK(!music_paused);
  milky::volume(0,55); CHECK(music_instance == ongoing);
  md_audio::effect(md::Sound::Click, 0); CHECK(!requested_sounds[0].pending);
  milky::volume(65,55);
  recordings[0] = {uint64_t(3)<<32, 0, 1, 32000};
  md_audio::effect(md::Sound::Click, 0); advance(20);
  CHECK(sound_plays == 1 && sound_tracks[0].id == 0);
  md_audio::scene(md_audio::Scene::Dark, 2);
  CHECK(!music_instance && sound_tracks[0].id == 0); // Music mute leaves effects.
  md_audio::loop(md::Sound::Click, 3, 60); advance(20);
  before = sound_plays;
  for (int i=0;i<100;++i) { md_audio::loop(md::Sound::Click,3,60); advance(20); }
  CHECK(sound_plays == before && sound_tracks[3].loop); // Loop never restarts each frame.
  advance(600); CHECK(!sound_tracks[3].loop);
  // Fill cache; active audio assets are never closed to admit another voice.
  stop_track(0);
  for (unsigned i=0;i<RecordingSlots;++i) recordings[i] = {(uint64_t(i+3)<<32), i, i+1, 40000};
  sound_tracks[1] = {0,100,true,test_now+500000};
  recording_sizes[2] = 90000;
  recordings[2].id = 40; // Requested id is not resident.
  md_audio::effect(md::Sound::DuoShot,2); advance(20);
  CHECK(recording_loading && !closed.empty());
  CHECK(std::find(closed.begin(),closed.end(),uint64_t(3)<<32) == closed.end());
  unsigned bytes=0;for(auto &r:recordings)bytes+=r.bytes;
  CHECK(bytes+recording_sizes[2] <= RECORDING_BYTES_LIMIT);
  recording_loading = false;
  // Decode timeout is bounded; the game does not wait for audio readiness.
  md_audio::scene(md_audio::Scene::Menu,0); advance(20);
  CHECK(music_instance && !music_ready);
  advance(16000); CHECK(!music_instance && music_retry > test_now);
  before = music_opens; advance(1000); CHECK(music_opens == before);
  std::printf("%u audio checks passed: streams, readiness, stale events, retries, loop/cache bounds and independent mute\n",checks);
}
