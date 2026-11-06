// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "media.hpp"
#include <bit>
#include <cassert>
#include <cstdio>
#include <string_view>
namespace camera_media {
constexpr uint32_t mkv_header_bytes = 512, mkv_block_header_bytes = 27;
inline void put_be(std::byte *out, uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i)
        out[size - 1 - i] = std::byte(value >> (i * 8));
}
inline uint64_t get_be(const std::byte *in, unsigned size) {
    uint64_t value = 0;
    for (unsigned i = 0; i < size; ++i)
        value = (value << 8) | std::to_integer<unsigned>(in[i]);
    return value;
}
struct EbmlBuffer {
    std::array<std::byte, 512> bytes{};
    size_t size = 0;
    std::span<const std::byte> data() const { return {bytes.data(), size}; }
    void raw(std::span<const std::byte> value) {
        assert(size + value.size() <= bytes.size());
        std::copy(value.begin(), value.end(), bytes.begin() + size);
        size += value.size();
    }
    void number(uint64_t n, unsigned count) {
        assert(size + count <= bytes.size());
        put_be(bytes.data() + size, n, count);
        size += count;
    }
    void element(uint32_t id, std::span<const std::byte> value) {
        unsigned count = id > 0xffffff ? 4 : id > 0xffff ? 3 : id > 0xff ? 2 : 1;
        number(id, count);
        number(0x10000000u + value.size(), 4);
        raw(value);
    }
    void integer(uint32_t id, uint64_t value, unsigned count = 4) {
        std::array<std::byte, 8> b{};
        put_be(b.data(), value, count);
        element(id, std::span(b).first(count));
    }
    void text(uint32_t id, std::string_view value) {
        element(id, {reinterpret_cast<const std::byte *>(value.data()), value.size()});
    }
};
// Finite clusters with absolute millisecond timestamps. The Segment has unknown
// size, so recording uses constant memory and remains sequential during capture.
inline std::array<std::byte, mkv_header_bytes> mkv_header(const Video &video) {
    EbmlBuffer out, ebml, info, tracks, vtrack, picture;
    ebml.integer(0x4286, 1, 1);
    ebml.integer(0x42f7, 1, 1);
    ebml.integer(0x42f2, 4, 1);
    ebml.integer(0x42f3, 8, 1);
    ebml.text(0x4282, "matroska");
    ebml.integer(0x4287, 4, 1);
    ebml.integer(0x4285, 2, 1);
    out.element(0x1a45dfa3, ebml.data());
    out.number(0x18538067, 4);
    out.number(0x01ffffffffffffffULL, 8);
    info.integer(0x2ad7b1, 1000000);
    info.text(0x4d80, "PXA Camera");
    info.text(0x5741, "PXA Camera");
    if (video.elapsed_us)
        info.integer(0x4489, std::bit_cast<uint64_t>(double(video.elapsed_us) / 1000), 8);
    out.element(0x1549a966, info.data());
    vtrack.integer(0xd7, 1, 1);
    vtrack.integer(0x73c5, 1, 1);
    vtrack.integer(0x83, 1, 1);
    vtrack.text(0x86, "V_MJPEG");
    picture.integer(0xb0, video.size.width);
    picture.integer(0xba, video.size.height);
    vtrack.element(0xe0, picture.data());
    tracks.element(0xae, vtrack.data());
    if (video.audio) {
        EbmlBuffer atrack, sound;
        atrack.integer(0xd7, 2, 1);
        atrack.integer(0x73c5, 2, 1);
        atrack.integer(0x83, 2, 1);
        atrack.text(0x86, "A_OPUS");
        std::array<std::byte, 19> opus{};
        std::memcpy(opus.data(), "OpusHead", 8);
        opus[8] = opus[9] = std::byte{1};
        put32(opus.data() + 12, 16000);
        atrack.element(0x63a2, opus);
        atrack.integer(0x56aa, 0);
        atrack.integer(0x56bb, 80000000);
        sound.integer(0xb5, std::bit_cast<uint64_t>(48000.0), 8);
        sound.integer(0x9f, 1, 1);
        atrack.element(0xe1, sound.data());
        tracks.element(0xae, atrack.data());
    }
    out.element(0x1654ae6b, tracks.data());
    assert(out.size + 5 <= 480);
    out.number(0xec, 1);
    out.number(0x10000000u + mkv_header_bytes - out.size - 4, 4);
    // App metadata lives inside standard Void padding. Other Matroska readers
    // use Info/Tracks; this fixed record lets the device open its albums cheaply.
    std::memcpy(out.bytes.data() + 480, "PXACAM1", 8);
    put16(out.bytes.data() + 488, video.size.width);
    put16(out.bytes.data() + 490, video.size.height);
    put_be(out.bytes.data() + 492, video.elapsed_us, 8);
    put32(out.bytes.data() + 500, video.frames);
    out.bytes[504] = std::byte(video.audio);
    return out.bytes;
}
inline std::array<std::byte, mkv_block_header_bytes> mkv_block_header(unsigned track, uint64_t ms,
                                                                      uint32_t size) {
    std::array<std::byte, mkv_block_header_bytes> out{};
    put_be(out.data(), 0x1f43b675, 4);
    put_be(out.data() + 4, 0x10000000u + 19 + size, 4);
    out[8] = std::byte{0xe7};
    out[9] = std::byte{0x88};
    put_be(out.data() + 10, ms, 8);
    out[18] = std::byte{0xa3};
    put_be(out.data() + 19, 0x10000000u + 4 + size, 4);
    out[23] = std::byte(0x80 + track);
    out[26] = std::byte{0x80};
    return out;
}
struct MkvBlock {
    uint32_t size = 0;
    uint64_t ms = 0;
    unsigned track = 0;
};
inline MkvBlock mkv_block(std::span<const std::byte> header, uint32_t offset, uint32_t end) {
    if (header.size() != 27 || offset > end || end - offset < 27 ||
        get_be(header.data(), 4) != 0x1f43b675 || header[8] != std::byte{0xe7} ||
        header[9] != std::byte{0x88} || header[18] != std::byte{0xa3})
        return {};
    const auto size = get_be(header.data() + 19, 4);
    if ((size & 0xf0000000u) != 0x10000000u || size < 0x10000005u)
        return {};
    const auto bytes = uint32_t(size - 0x10000004u);
    unsigned track = std::to_integer<unsigned>(header[23]) & 0x7f;
    if (bytes > 262144 || bytes > end - offset - 27 || (track != 1 && track != 2) ||
        get_be(header.data() + 4, 4) != 0x10000000u + 19 + bytes || header[24] != std::byte{} ||
        header[25] != std::byte{})
        return {};
    return {bytes, get_be(header.data() + 10, 8), track};
}

} // namespace camera_media

namespace camera_media {
inline EbmlBuffer mkv_footer(const Video &video) {
    EbmlBuffer out, tags;
    char duration[48];
    const auto ms = video.elapsed_us / 1000;
    std::snprintf(duration, sizeof(duration), "%02llu:%02llu:%02llu.%09llu",
                  (unsigned long long)(ms / 3600000), (unsigned long long)(ms / 60000 % 60),
                  (unsigned long long)(ms / 1000 % 60), (unsigned long long)(ms % 1000 * 1000000));
    for (unsigned track = 1; track <= (video.audio ? 2u : 1u); ++track) {
        EbmlBuffer tag, target, value;
        target.integer(0x63c5, track);
        tag.element(0x63c0, target.data());
        value.text(0x45a3, "DURATION");
        value.text(0x4487, duration);
        tag.element(0x67c8, value.data());
        tags.element(0x7373, tag.data());
    }
    out.element(0x1254c367, tags.data());
    std::array<std::byte, 64> footer{};
    footer[0] = std::byte{0xec};
    footer[1] = std::byte{0xbe};
    std::memcpy(footer.data() + 32, "PXACAM1", 8);
    put16(footer.data() + 40, video.size.width);
    put16(footer.data() + 42, video.size.height);
    put_be(footer.data() + 44, video.elapsed_us, 8);
    put32(footer.data() + 52, video.frames);
    footer[56] = std::byte(video.audio);
    put32(footer.data() + 60, mkv_header_bytes + video.movie_bytes);
    out.raw(footer);
    return out;
}
inline MovieInfo mkv_footer_info(std::span<const std::byte> footer, uint64_t bytes) {
    if (footer.size() != 64 || footer[0] != std::byte{0xec} || footer[1] != std::byte{0xbe} ||
        std::memcmp(footer.data() + 32, "PXACAM1", 8))
        return {};
    const auto duration = get_be(footer.data() + 44, 8);
    const auto frames = get32(footer.data() + 52), end = get32(footer.data() + 60);
    const auto word = [](const std::byte *p) {
        return uint16_t(std::to_integer<unsigned>(p[0]) | (std::to_integer<unsigned>(p[1]) << 8));
    };
    Size size{word(footer.data() + 40), word(footer.data() + 42)};
    if (!frames || !duration || !size.width || !size.height || end < 512 || bytes < end ||
        bytes - end < 64)
        return {};
    MovieInfo info{end,  frames, uint32_t(std::clamp<uint64_t>(duration / frames, 1, UINT32_MAX)),
                   size, 512,    footer[56] == std::byte{1},
                   true};
    return info;
}
} // namespace camera_media
