// SPDX-License-Identifier: MIT
#include "../main.cpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
static bool block_write = true, check_retry = false;
static uint64_t delivered = 0;
static std::vector<uint8_t> held;
static std::vector<unsigned> packet_sizes;
extern "C" int32_t pxa_call(uint32_t, uint64_t, const uint8_t *, uint32_t) {
  return 0;
}
extern "C" int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data,
                          uint32_t length) {
  if (handle == UINT64_C(0x10000000b) && operation == PXA_AUDIO_IO_WRITE) {
    if (block_write) {
      block_write = false;
      held.assign(data, data + length);
      check_retry = true;
      return PXA_STATUS_WOULD_BLOCK;
    }
    if (check_retry) {
      assert(held.size() == length && !std::memcmp(data, held.data(), length));
      check_retry = false;
    }
    packet_sizes.push_back(length);
    delivered += length / 4;
    return length;
  }
  return PXA_STATUS_WOULD_BLOCK;
}
static void consumption(uint64_t consumed) {
  uint8_t payload[44]{}, event[80]{};
  uint32_t n = 0;
  pxa_store_u16(payload + 4, 2);
  pxa_store_u16(payload + 6, 8);
  pxa_store_u64(payload + 8, sent_frames);
  pxa_store_u16(payload + 16, 3);
  pxa_store_u16(payload + 18, 8);
  pxa_store_u64(payload + 20, consumed);
  pxa_store_u16(payload + 28, 4);
  pxa_store_u16(payload + 30, 4);
  pxa_store_u32(payload + 32, sent_frames - consumed);
  pxa_store_u16(payload + 36, 5);
  pxa_store_u16(payload + 38, 4);
  query_token = 12345;
  assert(pxa_build_message(event, sizeof(event), PXA_AUDIO_SERVICE,
                           PXA_AUDIO_QUERY_STATE, query_token, payload,
                           sizeof(payload), &n));
  pxa_app_on_event(event, n);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<uint8_t> mp3{std::istreambuf_iterator<char>(f), {}};
  assert(!mp3.empty() && mp3.size() < sizeof(compressed));
  media::Decoder baseline;
  baseline.reset();
  uint64_t expected = 0;
  size_t at = 0;
  while (at < mp3.size()) {
    mp3dec_frame_info_t info{};
    int n = baseline.decode(mp3.data() + at, mp3.size() - at, info);
    if (!info.frame_bytes)
      break;
    at += info.frame_bytes;
    expected += n;
  }
  assert(expected > 4410);
  decoder.reset();
  stream = 7;
  audio = UINT64_C(0x10000000b);
  want_play = true;
  play_headers = true;
  audio_ready = true;
  rate = 44100;
  channels = 2;
  pcm_packet.configure(rate, channels);
  buffering = false;
  eof = true;
  stream_started = last_data = now();
  std::memcpy(compressed, mp3.data(), mp3.size());
  compressed_size = mp3.size();
  pump_player();
  assert(check_retry && !sent_frames && !delivered &&
         pcm_packet.frames == rate / 50);
  for (unsigned i = 0; i < 100 && !exhausted; ++i)
    pump_player();
  assert(exhausted && !compressed_size && delivered == expected &&
         sent_frames == expected);
  assert(packet_sizes.size() > 2);
  for (size_t i = 0; i + 1 < packet_sizes.size(); ++i)
    assert(packet_sizes[i] == rate / 50 * channels * 2);
  dirty = false;
  consumption(0);
  assert(!position && !advance);
  consumption(4410);
  assert(position == 100);
  lyrics.parse("[00:00.05]first\n[00:00.20]second");
  assert(lyrics.current(position) == 0);
  paused = true;
  uint64_t before = sent_frames;
  pump_player();
  assert(sent_frames == before);
  consumption(expected);
  assert(!advance);
  paused = false;
  consumption(expected);
  assert(advance && position == expected * 1000 / 44100);
  assert(lyrics.current(position) == 1);
  sent_frames = 0;
  pcm_frames = pcm_offset = 0;
  compressed_size = 0;
  eof = exhausted = true;
  stream = 7;
  want_play = true;
  advance = false;
  pump_player();
  assert(!want_play && !stream);
}
