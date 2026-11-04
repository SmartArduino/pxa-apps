// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

// The recorder writes one complete Opus packet per page. This keeps memory
// bounded and lets a failed write leave earlier pages available for recovery.
namespace recorder::ogg {
constexpr size_t max_packet = 1275;
constexpr size_t max_page = 27 + 6 + max_packet;
inline void put32(std::byte* p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = std::byte(value >> (i * 8));
}
inline uint32_t get32(const std::byte* p) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t(p[i]) << (i * 8);
    return value;
}
inline uint64_t granule(const std::byte* p) {
    return get32(p + 6) | (uint64_t(get32(p + 10)) << 32);
}
inline uint32_t crc(std::span<const std::byte> bytes) {
    uint32_t value = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        // The checksum field is zero during checksum calculation.
        const auto byte = i >= 22 && i < 26 ? 0u : unsigned(bytes[i]);
        value ^= byte << 24;
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value << 1) ^ ((value & 0x80000000u) ? 0x04c11db7u : 0u);
    }
    return value;
}
inline size_t page(std::span<std::byte> out, std::span<const std::byte> packet,
                   uint32_t serial, uint32_t sequence, uint64_t samples48,
                   uint8_t flags = 0) {
    if (packet.size() > max_packet) return 0;
    const size_t segments = packet.empty() ? 0 : packet.size() / 255 + 1;
    const size_t size = 27 + segments + packet.size();
    if (out.size() < size) return 0;
    std::memset(out.data(), 0, size);
    std::memcpy(out.data(), "OggS", 4);
    out[5] = std::byte(flags);
    put32(out.data() + 6, uint32_t(samples48));
    put32(out.data() + 10, uint32_t(samples48 >> 32));
    put32(out.data() + 14, serial);
    put32(out.data() + 18, sequence);
    out[26] = std::byte(segments);
    for (size_t i = 0; i < segments; ++i)
        out[27 + i] = std::byte(i + 1 == segments ? packet.size() % 255 : 255);
    if (!packet.empty()) std::memcpy(out.data() + 27 + segments, packet.data(), packet.size());
    put32(out.data() + 22, crc(out.first(size)));
    return size;
}
inline bool header(std::span<const std::byte> bytes) {
    return bytes.size() >= 27 && !std::memcmp(bytes.data(), "OggS", 4) &&
           bytes[4] == std::byte{} && (unsigned(bytes[5]) & ~6u) == 0 &&
           unsigned(bytes[26]) <= 6;
}
inline constexpr std::array<std::byte, 19> head = {
    std::byte{'O'}, std::byte{'p'}, std::byte{'u'}, std::byte{'s'},
    std::byte{'H'}, std::byte{'e'}, std::byte{'a'}, std::byte{'d'},
    std::byte{1}, std::byte{1}, std::byte{0}, std::byte{0},
    std::byte{0x80}, std::byte{0x3e}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{0}, std::byte{0}};
inline constexpr std::array<std::byte, 20> tags = {
    std::byte{'O'}, std::byte{'p'}, std::byte{'u'}, std::byte{'s'},
    std::byte{'T'}, std::byte{'a'}, std::byte{'g'}, std::byte{'s'},
    std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{'P'}, std::byte{'X'}, std::byte{'A'}, std::byte{' '},
    std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
} // namespace recorder::ogg
