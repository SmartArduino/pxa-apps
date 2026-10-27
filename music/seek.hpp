// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace music {
struct SeekMap {
  uint64_t bytes = 0, first = 0;
  unsigned bitrate = 0, frames_seen = 0;
  bool variable = false, toc_valid = false;
  uint8_t toc[100]{};
  static uint32_t be(const uint8_t *p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 |
           p[3];
  }
  void frame(const uint8_t *p, unsigned size, uint64_t at, unsigned kbps) {
    if (!frames_seen) {
      first = at;
      bitrate = kbps * 1000;
      if (size >= 4) {
        bool mpeg1 = (p[1] & 24) == 24, mono = (p[3] & 192) == 192;
        unsigned x = 4 + ((p[1] & 1) ? 0 : 2) +
                     (mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17));
        if (x + 8 <= size && (!std::memcmp(p + x, "Xing", 4) ||
                              !std::memcmp(p + x, "Info", 4))) {
          variable = !std::memcmp(p + x, "Xing", 4);
          uint32_t flags = be(p + x + 4);
          unsigned pos = x + 8;
          if (flags & 1)
            pos += 4;
          if ((flags & 2) && pos + 4 <= size) {
            bytes = first + be(p + pos);
            pos += 4;
          }
          if ((flags & 4) && pos + 100 <= size) {
            std::memcpy(toc, p + pos, 100);
            toc_valid = bytes > first;
            for (unsigned i = 1; i < 100; ++i)
              if (toc[i] < toc[i - 1])
                toc_valid = false;
          }
        }
      }
    } else if (bitrate != kbps * 1000)
      variable = true;
    ++frames_seen;
  }
  uint64_t offset(uint64_t ms, uint64_t duration) const {
    if (!duration || !ms || !frames_seen)
      return 0;
    if (toc_valid) {
      double f = std::min(99.999, double(ms) * 100 / duration);
      unsigned i = unsigned(f);
      double v = toc[i] + ((i == 99 ? 256 : toc[i + 1]) - toc[i]) * (f - i);
      return first + uint64_t(v * (bytes - first) / 256);
    }
    if (variable || frames_seen < 8 || !bitrate)
      return 0;
    uint64_t value = first + ms * bitrate / 8000;
    return bytes ? std::min(bytes - 1, value) : value;
  }
};
inline bool content_range(const char *value, uint64_t requested,
                          uint64_t &total) {
  if (std::strncmp(value, "bytes ", 6))
    return false;
  const char *p = value + 6;
  auto number = [&](uint64_t &n) {
    n = 0;
    bool digit = false;
    while (*p >= '0' && *p <= '9') {
      if (n > UINT64_MAX / 10 ||
          (n == UINT64_MAX / 10 && unsigned(*p - '0') > UINT64_MAX % 10))
        return false;
      n = n * 10 + *p++ - '0';
      digit = true;
    }
    return digit;
  };
  uint64_t start = 0, end = 0;
  return number(start) && *p++ == '-' && number(end) && *p++ == '/' &&
         number(total) && !*p && start == requested && end >= start &&
         end < total;
}
} // namespace music
