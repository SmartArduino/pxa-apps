// SPDX-License-Identifier: MIT
#pragma once
#include "mp4.hpp"
namespace bili {
// A seek position can exceed system uptime; never subtract media PTS from it.
inline uint64_t media_position(uint64_t anchor_ms,uint64_t offset_us,uint64_t now_ms,unsigned speed_quarters=4) {
    return offset_us+(now_ms>anchor_ms ? (now_ms-anchor_ms)*250*speed_quarters : 0);
}
// Bounded PCM rate conversion; changing speed also changes pitch.
struct PcmSpeed {
    unsigned phase=0;
    void reset(){phase=0;}
    size_t convert(const int16_t *input,size_t frames,unsigned channels,unsigned quarters,int16_t *out,size_t capacity) {
        if(!input||!out||(channels!=1&&channels!=2)||quarters<2||quarters>8)return 0;
        size_t n=0;uint64_t at=phase,end=uint64_t(frames)*4;
        while(at<end&&n<capacity){size_t index=at/4;for(unsigned c=0;c<channels;++c)out[n*channels+c]=input[index*channels+c];at+=quarters;++n;}
        if(at<end)return 0;phase=unsigned(at-end);return n;
    }
};
struct Fit {unsigned x=0,y=0,width=0,height=0;};
inline Fit fit(unsigned sw,unsigned sh,unsigned dw,unsigned dh) {
    if(!sw || !sh || !dw || !dh)return {};
    unsigned w=dw,h=std::max<uint64_t>(1,uint64_t(sh)*dw/sw);
    if(h>dh){h=dh;w=std::max<uint64_t>(1,uint64_t(sw)*dh/sh);}
    return {(dw-w)/2,(dh-h)/2,w,h};
}
inline uint64_t duration(const Track& t) {
    uint64_t end=0;for(const auto& s:t.samples)end=std::max(end,s.pts+s.duration);return end;
}
inline size_t keyframe(const Track& t,uint64_t pts) {
    size_t chosen=0;
    if(t.sync_samples.empty()) {
        for(size_t i=0;i<t.samples.size();++i)if(t.samples[i].pts<=pts)chosen=i;
    } else for(auto i:t.sync_samples) {
        if(i<t.samples.size() && t.samples[i].pts<=pts)chosen=i;
    }
    return chosen;
}
inline const uint8_t *window(const Sample& sample,uint64_t start,const uint8_t *data,size_t used) {
    if(start==UINT64_MAX || sample.offset<start)return nullptr;
    uint64_t delta=sample.offset-start;
    return delta<=used && sample.size<=used-size_t(delta)?data+size_t(delta):nullptr;
}
inline bool requested(const Sample& sample,uint64_t start,size_t limit) {
    return start!=UINT64_MAX && sample.offset>=start && sample.offset-start<=limit &&
        sample.size<=limit-size_t(sample.offset-start);
}
}
