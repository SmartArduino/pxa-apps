// SPDX-License-Identifier: MIT
#include "../danmaku.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc,char** argv) {
    bili::DanmakuSegment segment;
    // Two scroll comments with different timestamps, followed by one advanced
    // comment that is ignored. The first color is the non-default value 0x80.
    const uint8_t response[]{
        10,10,16,172,2,24,1,40,128,1,58,0,
        10,9,16,172,2,24,1,58,2,'O','K',
        10,8,16,42,24,5,58,2,'H','i',
        10,9,16,233,7,24,6,58,2,'G','o',
        10,8,16,19,24,7,58,2,'N','o',
    };
    assert(segment.parse({},1) && !segment.entries);
    assert(segment.parse(response,2) && !segment.entries);
    assert(segment.parse(response,1));
    assert(segment.size==3);
    const auto* storage=segment.entries.get();
    assert(segment.entries[0].time_ms==42 && segment.entries[0].mode==5);
    assert(segment.entries[1].time_ms==300 && std::strcmp(segment.entries[1].text,"OK")==0);
    assert(segment.entries[2].time_ms==1001 && segment.entries[2].mode==6);
    constexpr int lane=320, text_width=80;
    assert(bili::danmaku_left(1,lane,text_width,0)==320);
    assert(bili::danmaku_left(6,lane,text_width,0)==-80);
    assert(bili::danmaku_left(1,lane,text_width,4600)==-80);
    assert(bili::danmaku_left(6,lane,text_width,4600)==320);
    assert(bili::danmaku_left(5,lane,text_width,1000)==120);
    assert(!segment.parse(std::span(response).first(sizeof(response)-1),1));
    assert(segment.parse(response,2) && !segment.size && segment.entries.get()==storage);
    assert(segment.parse(response,1) && segment.size==3 && segment.entries.get()==storage);
    const uint8_t text[]{'A',10,0xe4,0xb8,0xad,0xf0,0x9f};
    bili::Danmaku message{};
    bili::danmaku_text(message,text);
    assert(message.chars==2 && std::strcmp(message.text,"A中")==0);
    if (argc==2) {
        std::ifstream input(argv[1],std::ios::binary);
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
        assert(!bytes.empty() && segment.parse(bytes,1) && segment.size>0);
        for (size_t i=1;i<segment.size;++i)
            assert(segment.entries[i-1].time_ms<=segment.entries[i].time_ms);
    }
}
