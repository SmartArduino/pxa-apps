// SPDX-License-Identifier: Apache-2.0
#include "../matroska.hpp"
#include "../preview_geometry.hpp"
#include <cassert>
#include <cstdio>
using namespace camera_media;
int main() {
    for (unsigned dpi : {160u,216u,240u,320u}) {
        const uint32_t density = dpi*65536/160;
        const int width = camera_geometry::native_preview_extent(320,density);
        const int height = camera_geometry::native_preview_extent(240,density);
        const auto physical_width = uint64_t(width)*density/65536;
        const auto physical_height = uint64_t(height)*density/65536;
        assert(physical_width >= 320 && physical_height >= 240);
        assert(std::min(physical_width*256/320,physical_height*256/240) == 256);
    }
    const std::array source{std::byte{255},std::byte{216},std::byte{255},std::byte{192},
        std::byte{0},std::byte{11},std::byte{8},std::byte{0},std::byte{240},
        std::byte{1},std::byte{64},std::byte{1},std::byte{1},std::byte{17},std::byte{0},
        std::byte{255},std::byte{217}};
    for (size_t n = 0; n < 15; ++n) assert(!jpeg_size(std::span(source).first(n)).width);
    const auto size = jpeg_size(source); assert(size.width == 320 && size.height == 240);
    auto invalid = source; invalid[5] = std::byte{1}; assert(!jpeg_size(invalid).width);
    invalid = source; invalid[4] = std::byte{1}; assert(!jpeg_size(invalid).width);
    invalid = source; invalid[3] = std::byte{218}; assert(!jpeg_size(invalid).width);
    Video video{{320,240},3,1010,400,1500000};
    const auto header = avi_header(video);
    assert(!std::memcmp(header.data(), "RIFF", 4) && get32(header.data()+4) == 1326);
    assert(!std::memcmp(header.data()+8, "AVI ", 4));
    assert(get32(header.data()+32) == 500000 && get32(header.data()+48) == 3);
    assert(get32(header.data()+64) == 320 && get32(header.data()+68) == 240);
    assert(get32(header.data()+128) == 500000 && get32(header.data()+132) == 1000000);
    assert(get32(header.data()+140) == 3 && !std::memcmp(header.data()+188, "MJPG", 4));
    assert(get32(header.data()+316) == 1014 && !std::memcmp(header.data()+320, "movi", 4));
    const auto chunk = frame_header(201);
    assert(!std::memcmp(chunk.data(), "00dc", 4) && get32(chunk.data()+4) == 201);
    const auto parsed = movie_info(header, 1334);
    assert(parsed.end == 1334 && parsed.frames == 3 && parsed.period_us == 500000);
    assert(!movie_info(header, 1333).end && !movie_info(std::span(header).first(223), 1234).end);
    assert(frame_bytes(chunk, 224, 434) == 201);
    assert(!frame_bytes(chunk, 224, 433) && !frame_bytes(chunk, 440, 434));
    auto corrupt = header; corrupt[188] = std::byte{0}; assert(!movie_info(corrupt, 1334).end);
    assert(parsed.data == 324 && !parsed.audio);
    video.audio=true; video.audio_samples=24000;
    const auto sound=avi_header(video);
    const auto with_sound=movie_info(sound,1334);
    assert(with_sound.audio && with_sound.data==324);
    assert(get32(sound.data()+264)==24000 && get32(sound.data()+256)==32000);
    auto bad_sound=sound; bad_sound[296]=std::byte{}; assert(!movie_info(bad_sound,1334).end);
    std::array<std::byte,224> legacy{};
    std::copy_n(header.begin(),212,legacy.begin());
    std::copy_n(header.begin()+312,12,legacy.begin()+212);
    put32(legacy.data()+4,1226); put32(legacy.data()+16,192);
    const auto old=movie_info(legacy,1234);
    assert(old.data==224 && old.end==1234 && !old.audio);
    const auto empty = avi_header({}); assert(get32(empty.data()+32) == 100000);
    video.frames=480;video.elapsed_us=60000000;video.movie_bytes=1000000;video.audio=true;
    const auto tail=mkv_footer(video);
    const auto mkv=mkv_footer_info(tail.data().last(64),512+video.movie_bytes+tail.size);
    assert(mkv.end==1000512 && mkv.frames==480 && mkv.audio && mkv.matroska);
    assert(!mkv_footer_info(tail.data().last(63),1001000).end);
    const auto block=mkv_block_header(1,60000,2000);
    assert(mkv_block(block,512,2539).size==2000 && mkv_block(block,512,2539).ms==60000);
    assert(!mkv_block(block,512,2538).size);
    auto bad_block=block;bad_block[23]=std::byte{0x83};assert(!mkv_block(bad_block,512,2539).size);
    puts("Camera media: JPEG segment bounds, truncation and AVI timing/chunk/header layouts passed");
}
