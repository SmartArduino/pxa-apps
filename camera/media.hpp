// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace camera_media {
struct Size { uint16_t width = 0, height = 0; };
inline Size jpeg_size(std::span<const std::byte> bytes) {
    auto at = [&](size_t i) { return std::to_integer<unsigned>(bytes[i]); };
    if (bytes.size() < 4 || at(0) != 255 || at(1) != 216) return {};
    size_t pos = 2;
    while (pos < bytes.size()) {
        if (at(pos++) != 255) return {};
        while (pos < bytes.size() && at(pos) == 255) ++pos;
        if (pos == bytes.size()) return {};
        const auto marker = at(pos++);
        if (marker == 0xda || marker == 0xd9) return {};
        if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd8)) continue;
        if (bytes.size() - pos < 2) return {};
        const size_t length = at(pos) * 256 + at(pos + 1);
        if (length < 2 || length > bytes.size() - pos) return {};
        if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
            if (length < 8) return {};
            return {uint16_t(at(pos + 5) * 256 + at(pos + 6)), uint16_t(at(pos + 3) * 256 + at(pos + 4))};
        }
        pos += length;
    }
    return {};
}
inline uint32_t get32(const std::byte* input) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t(std::to_integer<unsigned>(input[i])) << (8 * i);
    return value;
}
struct MovieInfo {
    uint32_t end = 0, frames = 0, period_us = 0;
    Size size;
    uint32_t data = 224;
    bool audio = false;
    bool matroska = false;
};
constexpr uint32_t avi_header_bytes = 324;
inline MovieInfo movie_info(std::span<const std::byte> header, uint64_t file_bytes) {
    if (header.size() < 224 || std::memcmp(header.data(), "RIFF", 4) ||
        std::memcmp(header.data()+8, "AVI ", 4) || std::memcmp(header.data()+108, "vidsMJPG", 8) ||
        std::memcmp(header.data()+188, "MJPG", 4)) return {};
    const bool legacy = !std::memcmp(header.data()+220, "movi", 4);
    const uint32_t data = legacy ? 224 : avi_header_bytes;
    if (header.size() < data || std::memcmp(header.data()+data-4, "movi", 4)) return {};
    const bool audio = get32(header.data()+56) == 2;
    if (audio && (legacy || std::memcmp(header.data()+232, "auds", 4) ||
        get32(header.data()+252) != 2 || get32(header.data()+256) != 32000 ||
        get32(header.data()+296) != 0x00010001 || get32(header.data()+300) != 16000 ||
        get32(header.data()+304) != 32000 || get32(header.data()+308) != 0x00100002)) return {};
    const uint64_t end = uint64_t(get32(header.data()+4)) + 8;
    const auto frames = get32(header.data()+48), period = get32(header.data()+32);
    const auto width = get32(header.data()+64), height = get32(header.data()+68);
    if (end < data || end > file_bytes || end > UINT32_MAX || !frames || !period ||
        !width || !height || width > 65535 || height > 65535 ||
        uint64_t(get32(header.data()+data-8)) + data-4 != end) return {};
    return {uint32_t(end), frames, period, {uint16_t(width), uint16_t(height)}, data, audio};
}
inline uint32_t chunk_bytes(std::span<const std::byte> header, uint32_t offset, uint32_t end) {
    if (header.size()!=8 || offset>end || end-offset<8) return 0;
    const auto bytes=get32(header.data()+4);
    if (!bytes || bytes>1048576 || uint64_t(bytes)+(bytes&1)+8>end-offset) return 0;
    return bytes;
}
inline uint32_t frame_bytes(std::span<const std::byte> header, uint32_t offset, uint32_t end) {
    return header.size()==8 && !std::memcmp(header.data(), "00dc", 4) ? chunk_bytes(header,offset,end) : 0;
}
inline void put32(std::byte* output, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) output[i] = std::byte(value >> (i * 8));
}
inline void put16(std::byte* output, uint16_t value) {
    output[0] = std::byte(value); output[1] = std::byte(value >> 8);
}
struct Video {
    Size size;
    uint32_t frames = 0, movie_bytes = 0, max_frame = 0;
    uint64_t elapsed_us = 0;
    uint32_t audio_samples = 0;
    bool audio = false;
};
// Sequential MJPEG and optional 16 kHz mono S16LE. A fixed header also leaves
// room for audio when no microphone is available. No growing index allocation.
inline std::array<std::byte, avi_header_bytes> avi_header(const Video& video) {
    std::array<std::byte, avi_header_bytes> out{};
    auto fourcc = [&](size_t offset, const char* text) { std::memcpy(out.data() + offset, text, 4); };
    auto value = [&](size_t offset, uint32_t n) { put32(out.data() + offset, n); };
    const auto period = uint32_t(std::clamp<uint64_t>(video.frames ? video.elapsed_us / video.frames : 100000, 1, UINT32_MAX));
    fourcc(0, "RIFF"); value(4, avi_header_bytes-8 + video.movie_bytes); fourcc(8, "AVI ");
    fourcc(12, "LIST"); value(16, 292); fourcc(20, "hdrl");
    fourcc(24, "avih"); value(28, 56); value(32, period);
    value(48, video.frames); value(56, video.audio ? 2 : 1); value(60, video.max_frame);
    value(64, video.size.width); value(68, video.size.height);
    fourcc(88, "LIST"); value(92, 116); fourcc(96, "strl");
    fourcc(100, "strh"); value(104, 56); fourcc(108, "vids"); fourcc(112, "MJPG");
    value(128, period); value(132, 1000000); value(140, video.frames);
    value(144, video.max_frame); value(148, UINT32_MAX);
    put16(out.data() + 160, video.size.width); put16(out.data() + 162, video.size.height);
    fourcc(164, "strf"); value(168, 40); value(172, 40);
    value(176, video.size.width); value(180, video.size.height);
    put16(out.data() + 184, 1); put16(out.data() + 186, 24);
    fourcc(188, "MJPG"); value(192, uint32_t(video.size.width) * video.size.height * 3);
    if (video.audio) {
        fourcc(212,"LIST"); value(216,92); fourcc(220,"strl");
        fourcc(224,"strh"); value(228,56); fourcc(232,"auds");
        value(252,2); value(256,32000); value(264,video.audio_samples);
        value(268,3200); value(272,UINT32_MAX); value(276,2);
        fourcc(288,"strf"); value(292,16);
        put16(out.data()+296,1); put16(out.data()+298,1);
        value(300,16000); value(304,32000);
        put16(out.data()+308,2); put16(out.data()+310,16);
    } else { fourcc(212,"JUNK"); value(216,92); }
    fourcc(312, "LIST"); value(316, 4 + video.movie_bytes); fourcc(320, "movi");
    return out;
}
inline std::array<std::byte, 8> frame_header(uint32_t bytes) {
    std::array<std::byte, 8> out{};
    std::memcpy(out.data(), "00dc", 4); put32(out.data() + 4, bytes); return out;
}
}
