// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fish::voice {
inline bool audio_json(const uint8_t *audio, size_t size, char *out, size_t capacity) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (!audio || !size || size > 128 * 1024 || capacity < 32 + (size + 2) / 3 * 4) return false;
    constexpr char prefix[] = "{\"audio\":\"";
    size_t at = sizeof(prefix) - 1;
    std::memcpy(out, prefix, at);
    for (size_t i = 0; i < size; i += 3) {
        unsigned value = unsigned(audio[i]) << 16;
        if (i + 1 < size) value |= unsigned(audio[i + 1]) << 8;
        if (i + 2 < size) value |= audio[i + 2];
        out[at++] = alphabet[value >> 18];
        out[at++] = alphabet[(value >> 12) & 63];
        out[at++] = i + 1 < size ? alphabet[(value >> 6) & 63] : '=';
        out[at++] = i + 2 < size ? alphabet[value & 63] : '=';
    }
    constexpr char suffix[] = "\",\"language\":\"zh\"}";
    std::memcpy(out + at, suffix, sizeof(suffix));
    return true;
}
} // namespace fish::voice
