// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace starsoil_save {
constexpr uint32_t InitialHash = 2166136261u;
inline uint32_t checksum(uint32_t hash, const uint8_t *bytes, size_t size) {
  for (size_t i = 0; i < size; ++i)
    hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}
} // namespace starsoil_save
