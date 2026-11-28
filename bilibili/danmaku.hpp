// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <span>

namespace bili {
constexpr uint32_t danmaku_segment_ms=360000;
constexpr uint32_t danmaku_lifetime_ms=4600;
constexpr unsigned danmaku_lanes=3;
constexpr unsigned danmaku_line_dp=17;
struct Danmaku {
    uint32_t time_ms=0, color=0xffffff;
    uint8_t mode=1, chars=0;
    char text[81]{};
};

inline int danmaku_left(uint8_t mode,int width,int text_width,uint32_t age_ms) {
    if (mode==5) return (width-text_width)/2;
    const int shift=int(uint64_t(width+text_width)*std::min(age_ms,danmaku_lifetime_ms)/danmaku_lifetime_ms);
    return mode==6 ? shift-text_width : width-shift;
}

class ProtoReader {
public:
    explicit ProtoReader(std::span<const uint8_t> bytes): remaining(bytes) {}
    bool empty() const { return remaining.empty(); }
    bool number(uint64_t& n) {
        n=0;
        for (unsigned shift=0;shift<64 && !empty();shift+=7) {
            const uint8_t b=remaining.front();
            remaining=remaining.subspan(1);
            if (shift==63 && b>1) return false;
            n|=uint64_t(b&127)<<shift;
            if (!(b&128)) return true;
        }
        return false;
    }
    bool bytes(std::span<const uint8_t>& out) {
        uint64_t size;
        if (!number(size) || size>remaining.size()) return false;
        out=remaining.first(size);
        remaining=remaining.subspan(size);
        return true;
    }
    bool skip(uint64_t wire) {
        uint64_t ignored;
        std::span<const uint8_t> content;
        if (!wire) return number(ignored);
        if (wire==2) return bytes(content);
        const size_t count=wire==1 ? 8 : wire==5 ? 4 : 0;
        if (!count || count>remaining.size()) return false;
        remaining=remaining.subspan(count);
        return true;
    }
private:
    std::span<const uint8_t> remaining;
};

inline void danmaku_text(Danmaku& out,std::span<const uint8_t> text) {
    size_t used=0;
    out.chars=0;
    out.text[0]=0;
    for (size_t i=0;i<text.size() && out.chars<26;) {
        const uint8_t c=text[i];
        if (c<0x20 || c==0x7f) {++i;continue;}
        const unsigned length=c<0x80 ? 1 : c>=0xc2 && c<=0xdf ? 2 :
            c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
        if (!length || length>text.size()-i || length>sizeof(out.text)-used-1) break;
        uint32_t point=c&((1u<<(length==1 ? 7 : 7-length))-1);
        for (unsigned j=1;j<length;++j) {
            if ((text[i+j]&0xc0)!=0x80) return;
            point=(point<<6)|(text[i+j]&0x3f);
        }
        if ((length==3 && point<0x800) || (length==4 && point<0x10000) ||
            (point>=0xd800 && point<=0xdfff) || point>0x10ffff) return;
        std::memcpy(out.text+used,text.data()+i,length);
        used+=length;
        out.text[used]=0;
        i+=length;
        ++out.chars;
    }
}

inline bool parse_danmaku_elem(std::span<const uint8_t> bytes,Danmaku& out) {
    ProtoReader reader(bytes);
    while (!reader.empty()) {
        uint64_t key;
        if (!reader.number(key) || key<8) return false;
        if (key==16 || key==24 || key==40) {
            uint64_t n;
            if (!reader.number(n)) return false;
            if (key==16) out.time_ms=uint32_t(std::min(n,uint64_t(UINT32_MAX)));
            else if (key==24) out.mode=uint8_t(std::min(n,uint64_t(UINT8_MAX)));
            else out.color=uint32_t(n)&0xffffff;
        } else if (key==58) {
            std::span<const uint8_t> content;
            if (!reader.bytes(content)) return false;
            danmaku_text(out,content);
        } else if (!reader.skip(key&7)) return false;
    }
    return true;
}

// Keep two comments per second across the whole segment, even for dense videos.
struct DanmakuSegment {
    std::unique_ptr<Danmaku[]> entries;
    size_t size=0;
    bool parse(std::span<const uint8_t> bytes,uint32_t segment) {
        size=0;
        std::array<uint8_t,danmaku_segment_ms/1000> counts{};
        ProtoReader reader(bytes);
        const uint64_t base=uint64_t(segment-1)*danmaku_segment_ms;
        while (!reader.empty()) {
            uint64_t key;
            if (!reader.number(key) || key<8) return false;
            if (key!=10) {
                if (!reader.skip(key&7)) return false;
                continue;
            }
            std::span<const uint8_t> elem;
            Danmaku item;
            if (!reader.bytes(elem) || !parse_danmaku_elem(elem,item)) return false;
            if (!item.chars || item.mode<1 || item.mode==4 || item.mode>6 ||
                item.time_ms<base || item.time_ms-base>=danmaku_segment_ms) continue;
            auto& count=counts[(item.time_ms-base)/1000];
            if (count==2) continue;
            if (!entries) {
                entries.reset(new (std::nothrow) Danmaku[danmaku_segment_ms/1000*2]);
                if (!entries) return false;
            }
            ++count;
            entries[size++]=item;
        }
        if (size>1) {
            std::sort(entries.get(),entries.get()+size,
                      [](const auto& a,const auto& b){return a.time_ms<b.time_ms;});
        }
        return true;
    }
};
} // namespace bili
