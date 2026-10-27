// SPDX-License-Identifier: MIT
#include "../main.cpp"
#include <cassert>
#include <fstream>
#include <vector>
static std::vector<uint8_t> source, held;
static std::vector<int16_t> delivered;
static size_t at = 0;
static unsigned writes = 0;
static bool retrying = false;
extern "C" int32_t pxa_call(uint32_t, uint64_t, const uint8_t *, uint32_t) {
  return 0;
}
extern "C" int32_t pxa_io(uint64_t h, uint32_t operation, uint8_t *data,
                          uint32_t size) {
  if (h == UINT64_C(0x10000000b) && operation == PXA_AUDIO_IO_WRITE) {
    if (!retrying && writes % 7 == 0) {
      held.assign(data, data + size);
      retrying = true;
      return PXA_STATUS_WOULD_BLOCK;
    }
    if (retrying) {
      assert(held.size() == size && !std::memcmp(held.data(), data, size));
      retrying = false;
    }
    assert(size == audio_rate / 50 * audio_channels * 2);
    ++writes;
    const int16_t *p = (const int16_t *)data;
    delivered.insert(delivered.end(), p, p + size / 2);
    return size;
  }
  if (h == UINT64_C(0x100000007) && operation == 1) {
    size_t n = std::min<size_t>(size, source.size() - at);
    if (!n)
      return PXA_STATUS_WOULD_BLOCK;
    std::memcpy(data, source.data() + at, n);
    at += n;
    return n;
  }
  return PXA_STATUS_WOULD_BLOCK;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  source.assign(std::istreambuf_iterator<char>(f), {});
  assert(source.size() > sizeof(compressed) * 2);
  media::Decoder baseline;
  baseline.reset();
  std::vector<int16_t> expected;
  size_t offset = 0;
  while (offset < source.size()) {
    mp3dec_frame_info_t info{};
    int n =
        baseline.decode(source.data() + offset, source.size() - offset, info);
    if (!info.frame_bytes)
      break;
    offset += info.frame_bytes;
    for (int i = 0; i < n * info.channels; ++i)
      expected.push_back(baseline.pcm[i]);
  }
  decoder.reset();
  icy.reset(0);
  stream = UINT64_C(0x100000007);
  audio_permission = UINT64_C(0x100000001);
  want_play = play_headers = true;
  stream_started = last_received = last_pcm = now_ms();
  for (unsigned i = 0; i < 1500; ++i) {
    if (audio_open_token) {
      audio_open_token = 0;
      audio = UINT64_C(0x10000000b);
      audio_ready = true;
      pcm_packet.configure(audio_rate, audio_channels);
    }
    playback_pump();
  }
  assert(delivered.size() > 44100 * 2 * 10 &&
         delivered.size() <= expected.size());
  assert(!std::memcmp(delivered.data(), expected.data(),
                      delivered.size() * sizeof(int16_t)));
  assert(at == source.size() &&
         compressed_head + compressed_size <= sizeof(compressed) &&
         writes > 400);
  assert(blocked_writes > 20 && want_play);
  std::puts("radio: continuous PCM matches baseline across compressed-window "
            "compaction, 20 ms staging and atomic backpressure retries");
}
