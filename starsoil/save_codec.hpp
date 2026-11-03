// SPDX-License-Identifier: MIT
#pragma once
#include "save_checksum.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace starsoil_save {
// PackBits envelope around the unchanged, checksummed SSR1 representation.
// Both directions relocate their input by the measured overlap distance.
// This needs only ~4 KiB of slack for a largest world, not another snapshot.
constexpr size_t buffer_capacity(size_t raw) { return raw + raw / 128 + 32; }
inline uint32_t load32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline void store32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(n >> (8 * i));
}
inline bool packed(const uint8_t *p, size_t size) {
  return size >= 4 && std::memcmp(p, "SSZ1", 4) == 0;
}
inline size_t run_length(const uint8_t *p, size_t size, size_t at) {
  size_t end = at + 1;
  while (end < size && end - at < 130 && p[end] == p[at]) ++end;
  return end - at;
}
template <typename Emit>
inline void tokens(const uint8_t *p, size_t size, Emit emit) {
  for (size_t at = 0; at < size;) {
    size_t count = run_length(p, size, at);
    bool run = count >= 3;
    if (!run) {
      count = 1;
      while (count < 128 && at + count < size &&
             run_length(p, size, at + count) < 3) ++count;
    }
    emit(at, count, run);
    at += count;
  }
}
inline size_t pack(uint8_t *p, size_t size, size_t capacity) {
  if (size < 128 || size > capacity || std::memcmp(p, "SSR1", 4)) return size;
  // Seal before packing. Uncompressed writes can still use incremental sealing.
  store32(p + size - 4, checksum(InitialHash, p, size - 4));
  size_t bytes = 8, overlap = 8;
  tokens(p, size, [&](size_t at, size_t count, bool run) {
    bytes += run ? 2 : count + 1;
    if (bytes > at + count) overlap = std::max(overlap, bytes - at - count);
  });
  if (bytes + 64 >= size || overlap > capacity - size) return size;
  std::memmove(p + overlap, p, size);
  size_t out = 8;
  tokens(p + overlap, size, [&](size_t at, size_t count, bool run) {
    // Read the run value before its output could overwrite the source.
    uint8_t value = p[overlap + at];
    p[out++] = run ? uint8_t(128 + count - 3) : uint8_t(count - 1);
    if (run) p[out++] = value;
    else { std::memmove(p + out, p + overlap + at, count); out += count; }
  });
  std::memcpy(p, "SSZ1", 4); store32(p + 4, uint32_t(size));
  return out;
}
inline bool unpack(uint8_t *p, size_t &size, size_t capacity, size_t raw_limit) {
  if (!packed(p, size)) return true; // Legacy SSR1 is validated by the world codec.
  if (size < 8) return false;
  size_t raw = load32(p + 4), out = 0, overlap = 0;
  if (raw < 128 || raw > raw_limit || raw > capacity || size >= raw) return false;
  // Validate every input/output bound before mutating the buffer.
  for (size_t at = 8; at < size;) {
    uint8_t token = p[at++];
    size_t count = token & 128 ? (token & 127) + 3 : token + 1;
    size_t consumed = token & 128 ? 1 : count;
    if (consumed > size - at || count > raw - out) return false;
    at += consumed; out += count;
    if (out > at) overlap = std::max(overlap, out - at);
  }
  if (out != raw || overlap > capacity - size) return false;
  std::memmove(p + overlap, p, size);
  out = 0;
  for (size_t at = 8; at < size;) {
    uint8_t token = p[overlap + at++];
    size_t count = token & 128 ? (token & 127) + 3 : token + 1;
    if (token & 128) {
      uint8_t value = p[overlap + at++]; std::memset(p + out, value, count);
    } else {
      std::memmove(p + out, p + overlap + at, count); at += count;
    }
    out += count;
  }
  size = raw;
  return true;
}
} // namespace starsoil_save
