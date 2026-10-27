// SPDX-License-Identifier: MIT
// Fixture: 12 s, stereo, 44.1 kHz MP3, CBR 128 kbps, no Xing header.
// Frequencies: 440 Hz for [0,2), 880 Hz for [2,4), 1320 Hz for [4,12).
#include "../main.cpp"
#include <cassert>
#include <fstream>
#include <vector>
static std::vector<uint8_t> source;
static std::vector<int16_t> output;
static size_t read_at = 0;
static bool ignore_range = false, corrupt_range = false;
static constexpr uint64_t Net = UINT64_C(0x100000007),
                          Audio = UINT64_C(0x10000000b);
extern "C" int32_t pxa_call(uint32_t, uint64_t, const uint8_t *, uint32_t) {
  return 0;
}
extern "C" int32_t pxa_io(uint64_t h, uint32_t op, uint8_t *p, uint32_t n) {
  if (h == Audio && op == PXA_AUDIO_IO_WRITE) {
    const auto *v = (const int16_t *)p;
    output.insert(output.end(), v, v + n / 2);
    return n;
  }
  if (h != Net)
    return PXA_STATUS_WOULD_BLOCK;
  if (op == PXA_HTTP_IO_RESPONSE) {
    uint16_t code = range_start && !ignore_range ? 206 : 200;
    char value[128];
    std::snprintf(value, sizeof(value), "bytes %llu-%zu/%zu",
                  (unsigned long long)(range_start + (corrupt_range ? 1 : 0)),
                  source.size() - 1, source.size());
    const char *key = "content-range";
    size_t size = code == 206 ? 4 + std::strlen(key) + std::strlen(value) : 0;
    assert(n >= 16 + size);
    std::memset(p, 0, 16 + size);
    pxa_store_u16(p, code);
    pxa_store_u16(p + 2, 1);
    pxa_store_u32(p + 4, size);
    pxa_store_u64(p + 8, source.size());
    if (size) {
      pxa_store_u16(p + 16, std::strlen(key));
      pxa_store_u16(p + 18, std::strlen(value));
      std::memcpy(p + 20, key, std::strlen(key));
      std::memcpy(p + 20 + std::strlen(key), value, std::strlen(value));
    }
    return 16 + size;
  }
  if (op == 1) {
    size_t count = std::min<size_t>(n, source.size() - read_at);
    std::memcpy(p, source.data() + read_at, count);
    read_at += count;
    return count;
  }
  return PXA_STATUS_WOULD_BLOCK;
}
static void open_result() {
  uint8_t payload[12]{}, wire[64]{};
  uint32_t n = 0;
  pxa_store_u64(payload + 4, Net);
  assert(pxa_build_message(wire, sizeof(wire), PXA_NET_SERVICE, PXA_HTTP_OPEN,
                           play_token, payload, sizeof(payload), &n));
  pxa_app_on_event(wire, n);
}
static void pump(unsigned loops = 180) {
  for (unsigned i = 0; i < loops; ++i) {
    if (audio_token) {
      audio_token = 0;
      audio = Audio;
      audio_ready = true;
    }
    pump_player();
  }
}
static void jump(uint64_t ms, bool paused_value = false) {
  output.clear();
  paused = paused_value;
  seek_to(ms);
  assert(want_play && seek_pending && position == seek_target);
  read_at = ignore_range ? 0 : range_start;
  open_result();
  pump();
}
static unsigned crossings() {
  unsigned count = 0;
  size_t n = std::min<size_t>(output.size(), 8192);
  for (size_t i = 2; i < n; i += 2)
    count += output[i] > 0 && output[i - 2] <= 0;
  return count;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ifstream f(argv[1], std::ios::binary);
  source.assign(std::istreambuf_iterator<char>(f), {});
  assert(source.size() > 100000);
  media::Decoder scan;
  scan.reset();
  size_t at = 0;
  while (at < source.size() && seek_map.frames_seen < 20) {
    mp3dec_frame_info_t info{};
    scan.decode(source.data() + at, source.size() - at, info);
    assert(info.frame_bytes > 0);
    if (info.hz)
      seek_map.frame(source.data() + at + info.frame_offset,
                     info.frame_bytes - info.frame_offset,
                     at + info.frame_offset, info.bitrate_kbps);
    at += info.frame_bytes;
  }
  seek_map.bytes = source.size();
  assert(!seek_map.variable && seek_map.bitrate == 128000);
  have_current = want_play = true;
  current.duration = 12000;
  std::strcpy(play_url, "http://localhost/test.mp3");
  net_permission = audio_permission = UINT64_C(0x100000003);
  scene.initialized = 1;
  dirty = false;
  jump(1000);
  assert(!seek_pending && crossings() > 30 && crossings() < 60);
  assert(position_base == 1000);
  jump(9000);
  assert(range_start > 100000 && !seek_pending && crossings() > 100);
  assert(position_base == 9000);
  jump(3000);
  assert(!seek_pending && crossings() > 65 && crossings() < 100);
  jump(9000, true);
  assert(paused && output.empty() && !seek_pending && position == 9000);
  paused = false;
  pump();
  assert(!output.empty() && crossings() > 100);
  ignore_range = true;
  jump(3000);
  assert(range_start == 0 && !seek_pending && crossings() > 65 &&
         crossings() < 100);
  ignore_range = false;
  corrupt_range = true;
  jump(9000);
  assert(!want_play && output.empty());
  uint64_t total = 0;
  assert(music::content_range("bytes 10-19/20", 10, total) && total == 20);
  assert(!music::content_range("bytes 11-19/20", 10, total));
  assert(!music::content_range("bytes 10-20/20", 10, total));
  assert(!music::content_range("bytes 18446744073709551616-19/20", 10, total));
  // Xing table mapping uses the actual byte distribution instead of constant
  // bitrate.
  music::SeekMap vbr;
  uint8_t frame[160]{};
  frame[1] = 0xfb;
  frame[3] = 0;
  std::memcpy(frame + 36, "Xing", 4);
  frame[43] = 6;
  frame[46] = 0x10; // 4096 byte MPEG stream
  for (unsigned i = 0; i < 100; ++i)
    frame[48 + i] = i * 2;
  vbr.frame(frame, sizeof(frame), 100, 128);
  assert(vbr.variable && vbr.toc_valid && vbr.offset(5000, 10000) == 1700);
  media::SmoothList list;
  list.layout(50, 150, 48, 20);
  assert(list.down(90, 100));
  list.move(77, 120);
  assert(list.offset == 13 && list.moved);
  assert(list.up(77, 125));
  list.tick(140);
  list.tick(160);
  assert(list.offset > 13);
  list.reset();
  list.ensure(19);
  for (unsigned i = 1; i < 40; ++i)
    list.tick(200 + i * 20);
  assert(std::abs(list.offset - list.maximum) < 1);
  Track other;
  other.id = 100;
  other.duration = 12000;
  choose(other, true);
  assert(!play_url[0]);
  seek_to(5000);
  assert(!play_token);
  std::puts("seek: real PCM frequencies, forward/backward, pause, "
            "ignored/malformed Range, Xing and smooth scrolling passed");
}
