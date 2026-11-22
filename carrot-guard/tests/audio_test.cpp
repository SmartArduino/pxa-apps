// SPDX-License-Identifier: MIT
#include "../game_audio.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

struct Io {
  uint32_t op;
  std::vector<uint8_t> bytes;
};
static std::vector<Io> commands;
static unsigned opens = 0;
extern "C" int32_t pxa_call(uint32_t route, uint64_t, const uint8_t *,
                            uint32_t) {
  if (uint16_t(route) == PXA_AUDIO_SERVICE &&
      route >> 16 == PXA_AUDIO_OPEN_SESSION)
    ++opens;
  return 0;
}
extern "C" int32_t pxa_io(uint64_t, uint32_t op, uint8_t *data, uint32_t size) {
  commands.push_back({op, {data, data + size}});
  if (op == PXA_AUDIO_IO_PLAY_MUSIC)
    pxa_store_u64(data, 123);
  return size;
}
static void event(carrot::AudioPlayer &audio, uint16_t service, uint16_t op,
                  uint64_t token, std::vector<uint8_t> data) {
  pxa_event_t e{service, op, token, data.data(), uint32_t(data.size())};
  assert(audio.handle(e));
}
static void ready(carrot::AudioPlayer &audio) {
  audio.start();
  std::vector<uint8_t> grant(12);
  pxa_store_u64(grant.data() + 4, UINT64_C(1) << 32);
  event(audio, PXA_PERMISSION_SERVICE, PXA_PERMISSION_ACQUIRE, 701, grant);
  std::vector<uint8_t> opened(35);
  size_t offset = 4;
  auto record = [&](uint16_t tag, uint64_t value, size_t size) {
    uint8_t bytes[8];
    pxa_store_u64(bytes, value);
    size_t written;
    assert(pxa_wire_record_encode(opened.data() + offset,
                                  opened.size() - offset, tag, bytes, size,
                                  &written));
    offset += written;
  };
  record(3, UINT64_C(2) << 32, 8);
  record(4, 16000, 4);
  record(5, 1, 1);
  record(6, 20, 2);
  event(audio, PXA_AUDIO_SERVICE, PXA_AUDIO_OPEN_SESSION, 702, opened);
  event(audio, PXA_AUDIO_SERVICE, PXA_AUDIO_COMMIT_GRAPH, 703, {0, 0, 0, 0});
  for (unsigned i = 0; i < carrot::music::Count; ++i) {
    std::vector<uint8_t> asset(32);
    pxa_store_u64(asset.data() + 4, (UINT64_C(3) << 32) + i);
    asset[12] = PXA_ASSET_AUDIO;
    asset[13] = PXA_ASSET_ENCODING_PCM_S16LE_16K_MONO;
    event(audio, PXA_ASSETS_SERVICE, PXA_ASSETS_LOAD, 800 + i, asset);
  }
}
int main() {
  using carrot::music::Cue;
  carrot::AudioPlayer audio;
  ready(audio);
  assert(commands.front().op == PXA_AUDIO_IO_PLAY_MUSIC);
  const auto &request = commands.front().bytes;
  assert(std::string(reinterpret_cast<const char *>(request.data() + 16),
                     request.size() - 16) == "assets/patrol.ogg");
  commands.clear();
  audio.tick(1000000);
  for (unsigned i = 0; i < carrot::music::Count; ++i) {
    audio.tick(1000000 + i * 100000);
    audio.cue(Cue(i));
  }
  assert(commands.size() == carrot::music::Count);
  for (auto &command : commands)
    assert(command.op == PXA_AUDIO_IO_PLAY_SOUND);
  // Muting stops existing voices immediately and prevents new playback.
  audio.setEnabled(false);
  assert(commands.back().op == PXA_AUDIO_IO_CONTROL_SOUND);
  size_t count = commands.size();
  audio.cue(Cue::Build);
  audio.tick(5000000);
  assert(commands.size() == count);
  audio.setEnabled(true);
  assert(commands.back().op == PXA_AUDIO_IO_CONTROL_MUSIC &&
         commands.back().bytes[0] == PXA_AUDIO_ASSET_RESUME);
  // Backgrounding also stops effects, without restarting or opening music.
  audio.setForeground(false);
  count = commands.size();
  audio.cue(Cue::Win);
  audio.tick(9000000);
  assert(commands.size() == count);
  audio.setForeground(true);
  audio.cue(Cue::Build);
  assert(commands.back().op == PXA_AUDIO_IO_PLAY_SOUND);
  audio.stop();
  count = commands.size();
  audio.cue(Cue::Coin);
  assert(commands.size() == count);
  // Permission denial is a silent, playable degradation.
  carrot::AudioPlayer denied;
  denied.start();
  unsigned before = opens;
  std::vector<uint8_t> failure(4);
  pxa_store_u32(failure.data(), uint32_t(PXA_STATUS_DENIED));
  event(denied, PXA_PERMISSION_SERVICE, PXA_PERMISSION_ACQUIRE, 701, failure);
  denied.tick(100000);
  denied.cue(Cue::Begin);
  assert(opens == before && commands.size() == count);
  std::puts("generated audio: 14 sounds, music path, mute, background, stop "
            "and permission denial passed");
}
