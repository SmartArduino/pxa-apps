// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace translate {
// Wrap the Host's 16 kHz mono raw Opus packets without decoding or re-encoding.
// Ogg Opus granules always use the 48 kHz clock, independently of input rate.
struct OggOpus {
  static constexpr size_t Capacity = 128 * 1024;
  uint8_t data[Capacity]{};
  size_t size = 0, last = 0;
  uint32_t sequence = 0, frames = 0;
  uint64_t granule = 0;
  bool sealed = false;
  static uint32_t crc(const uint8_t *p, size_t n) {
    uint32_t result = 0;
    for (size_t i = 0; i < n; ++i) {
      result ^= uint32_t(p[i]) << 24;
      for (int bit = 0; bit < 8; ++bit)
        result = (result << 1) ^ ((result & 0x80000000u) ? 0x04c11db7u : 0);
    }
    return result;
  }
  static void put(uint8_t *p, uint64_t v, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
      p[i] = uint8_t(v >> (i * 8));
  }
  bool page(const uint8_t *p, size_t n, uint8_t flags) {
    size_t laces = n / 255 + 1, total = 27 + laces + n;
    if (sealed || n > 1275 || size + total > Capacity)
      return false;
    last = size;
    auto *out = data + size;
    std::memset(out, 0, 27 + laces);
    std::memcpy(out, "OggS", 4);
    out[5] = flags;
    put(out + 6, granule, 8);
    put(out + 14, 0x5452414e, 4);
    put(out + 18, sequence++, 4);
    out[26] = uint8_t(laces);
    for (size_t i = 0; i < laces; ++i)
      out[27 + i] = uint8_t(i + 1 < laces ? 255 : n % 255);
    std::memcpy(out + 27 + laces, p, n);
    put(out + 22, crc(out, total), 4);
    size += total;
    return true;
  }
  void reset() {
    size = last = 0;
    sequence = frames = 0;
    granule = 0;
    sealed = false;
    uint8_t head[19]{};
    std::memcpy(head, "OpusHead", 8);
    head[8] = 1;
    head[9] = 1;
    put(head + 12, 16000, 4);
    page(head, sizeof(head), 2);
    const uint8_t tags[] = {'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 3, 0,
                            0,   0,   'P', 'X', 'A', 0,   0,   0,   0};
    page(tags, sizeof(tags), 0);
  }
  bool append(const uint8_t *p, size_t n, unsigned milliseconds) {
    if (!p || !n ||
        (milliseconds != 20 && milliseconds != 40 && milliseconds != 60))
      return false;
    uint64_t next = granule + milliseconds * 48;
    auto old = granule;
    granule = next;
    if (!page(p, n, 0)) {
      granule = old;
      return false;
    }
    ++frames;
    return true;
  }
  bool finish() {
    if (sealed)
      return frames != 0;
    if (!frames)
      return false;
    data[last + 5] |= 4;
    std::memset(data + last + 22, 0, 4);
    put(data + last + 22, crc(data + last, size - last), 4);
    sealed = true;
    return true;
  }
};
} // namespace translate
