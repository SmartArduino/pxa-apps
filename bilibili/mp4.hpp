// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
namespace bili {
using Bytes=std::span<const uint8_t>;
inline uint32_t be32(const uint8_t *p) { return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]; }
inline uint64_t be64(const uint8_t *p) { return uint64_t(be32(p))<<32|be32(p+4); }
struct Box { uint32_t type=0; Bytes data{}; };
inline uint32_t tag(const char *p) { return be32(reinterpret_cast<const uint8_t*>(p)); }
template<class Fn> bool boxes(Bytes bytes,Fn fn) {
    while (!bytes.empty()) {
        if (bytes.size()<8) return false;
        uint64_t n=be32(bytes.data()); size_t head=8;
        if (n==1) { if (bytes.size()<16) return false; n=be64(bytes.data()+8); head=16; }
        if (!n) n=bytes.size();
        if (n<head || n>bytes.size()) return false;
        if (!fn(Box{be32(bytes.data()+4),bytes.subspan(head,size_t(n)-head)})) return false;
        bytes=bytes.subspan(size_t(n));
    } return true;
}
inline Bytes child(Bytes p,const char *name) {
    Bytes out{}; boxes(p,[&](Box b){if (b.type==tag(name)) out=b.data; return true;}); return out;
}
struct Sample { uint64_t offset=0,pts=0; uint32_t size=0,duration=0; };
struct Track {
    std::vector<Sample> samples;
    std::vector<uint8_t> parameters;
    std::vector<uint32_t> sync_samples;
    uint16_t width=0,height=0,channels=0;
    uint32_t timescale=0,rate=0;
    unsigned nal_bytes=4,aac_rate_index=0,aac_profile=1;
    bool video=false,audio=false;
};
inline bool table(Bytes b,size_t row,uint32_t& n) {
    if (b.size()<8) return false;
    n=be32(b.data()+4); return n<=100000 && n<=(b.size()-8)/row;
}
inline bool parse_track(Bytes trak,Track& t) {
    auto mdia=child(trak,"mdia"), mdhd=child(mdia,"mdhd"), handler=child(mdia,"hdlr");
    if (mdhd.size()<24 || handler.size()<12) return false;
    t.video=be32(handler.data()+8)==tag("vide"); t.audio=be32(handler.data()+8)==tag("soun");
    if (!t.video && !t.audio) return true;
    size_t scale_at=mdhd[0] ? 20 : 12;
    if (mdhd.size()<scale_at+4 || !(t.timescale=be32(mdhd.data()+scale_at))) return false;
    auto stbl=child(child(mdia,"minf"),"stbl"), stsd=child(stbl,"stsd");
    if (stsd.size()<16 || be32(stsd.data()+4)!=1) return false;
    uint32_t entry_size=be32(stsd.data()+8), codec=be32(stsd.data()+12);
    if (entry_size<8 || entry_size>stsd.size()-8) return false;
    Bytes entry=stsd.subspan(8,entry_size);
    if (t.video) {
        if (codec!=tag("avc1") || entry.size()<86) return false;
        t.width=uint16_t(entry[32]<<8|entry[33]); t.height=uint16_t(entry[34]<<8|entry[35]);
        auto avcc=child(entry.subspan(86),"avcC"); if (avcc.size()<7 || avcc[0]!=1) return false;
        t.nal_bytes=(avcc[4]&3)+1; size_t at=6;
        auto sets=[&](unsigned count) {
            while (count--) {
                if (at+2>avcc.size()) return false;
                size_t n=size_t(avcc[at]<<8|avcc[at+1]); at+=2;
                if (!n || n>avcc.size()-at || t.parameters.size()+n+4>4096) return false;
                t.parameters.insert(t.parameters.end(),{0,0,0,1});
                t.parameters.insert(t.parameters.end(),avcc.begin()+at,avcc.begin()+at+n); at+=n;
            } return true;
        };
        if (!sets(avcc[5]&31) || at==avcc.size()) return false;
        unsigned pps=avcc[at++]; if (!sets(pps) || t.parameters.empty()) return false;
    } else {
        if (codec!=tag("mp4a") || entry.size()<36) return false;
        t.channels=uint16_t(entry[24]<<8|entry[25]); t.rate=be32(entry.data()+32)>>16;
        constexpr unsigned rates[]={96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,7350};
        auto it=std::find(std::begin(rates),std::end(rates),t.rate);
        if (it==std::end(rates) || !t.channels || t.channels>2) return false;
        t.aac_rate_index=unsigned(it-std::begin(rates));
    }
    auto sz=child(stbl,"stsz"), sc=child(stbl,"stsc"), co=child(stbl,"stco");
    bool wide=co.empty(); if (wide) co=child(stbl,"co64");
    if (sz.size()<12) return false;
    uint32_t fixed=be32(sz.data()+4), count=be32(sz.data()+8), chunks=0, maps=0;
    if (!count || count>100000 || (!fixed && count>(sz.size()-12)/4) || !table(co,wide?8:4,chunks) ||
        !table(sc,12,maps) || !maps || be32(sc.data()+8)!=1) return false;
    t.samples.resize(count); uint32_t sample=0,map=0;
    for (uint32_t c=1;c<=chunks;++c) {
        while (map+1<maps && be32(sc.data()+8+(map+1)*12)<=c) ++map;
        const uint32_t per=be32(sc.data()+12+map*12);
        if (!per || be32(sc.data()+16+map*12)!=1) return false;
        uint64_t offset=wide ? be64(co.data()+8+(c-1)*8) : be32(co.data()+8+(c-1)*4);
        for (uint32_t j=0;j<per;++j) {
            if (sample>=count) return false;
            const uint32_t bytes=fixed ? fixed : be32(sz.data()+12+sample*4);
            if (!bytes || bytes>512*1024 || offset>UINT64_MAX-bytes) return false;
            t.samples[sample++]={offset,0,bytes,0}; offset+=bytes;
        }
    }
    if (sample!=count) return false;
    auto ts=child(stbl,"stts"); uint32_t runs=0; if (!table(ts,8,runs)) return false;
    uint64_t dts=0; sample=0;
    for (uint32_t i=0;i<runs;++i) {
        uint32_t n=be32(ts.data()+8+i*8), duration=be32(ts.data()+12+i*8);
        if (!duration || n>count-sample) return false;
        while (n--) { t.samples[sample].pts=dts*1000000/t.timescale;
            t.samples[sample++].duration=uint64_t(duration)*1000000/t.timescale; dts+=duration; }
    }
    if (sample!=count) return false;
    auto ctts=child(stbl,"ctts");
    if (!ctts.empty()) {
        if (!table(ctts,8,runs)) return false;
        sample=0;
        for (uint32_t i=0;i<runs;++i) {
            uint32_t n=be32(ctts.data()+8+i*8), raw=be32(ctts.data()+12+i*8);
            int64_t delta=(ctts[0]==1 ? int64_t(int32_t(raw)) : int64_t(raw))*1000000/t.timescale;
            if (n>count-sample) return false;
            while (n--) { auto& s=t.samples[sample++]; if (delta<0 && uint64_t(-delta)>s.pts) s.pts=0;
                else s.pts=uint64_t(int64_t(s.pts)+delta); }
        }
        if (sample!=count) return false;
    }
    auto sync=child(stbl,"stss");
    if (t.video && !sync.empty()) {
        uint32_t keys=0; if (!table(sync,4,keys)) return false;
        for (uint32_t i=0;i<keys;++i) {
            uint32_t index=be32(sync.data()+8+i*4);
            if (!index || index>count || (!t.sync_samples.empty() && index-1<=t.sync_samples.back())) return false;
            t.sync_samples.push_back(index-1);
        }
    }
    return true;
}
inline bool parse_moov(Bytes bytes,Track& video,Track& audio) {
    return boxes(bytes,[&](Box b){
        if (b.type!=tag("trak")) return true;
        Track t; if (!parse_track(b.data,t)) return false;
        if (t.video && video.samples.empty()) video=std::move(t);
        if (t.audio && audio.samples.empty()) audio=std::move(t);
        return true;
    }) && !video.samples.empty();
}
inline bool annexb(Bytes input,unsigned nal,std::vector<uint8_t>& out,size_t maximum,bool *non_reference=nullptr) {
    out.assign(16,0);bool vcl=false,disposable=true;
    if(non_reference)*non_reference=false;
    while (!input.empty()) {
        if (!nal || nal>4 || input.size()<nal) return false;
        uint32_t n=0; for (unsigned i=0;i<nal;++i) n=n<<8|input[i]; input=input.subspan(nal);
        if (!n || n>input.size() || out.size()+n+4>maximum+16) return false;
        if(input[0]&0x80)return false;
        unsigned type=input[0]&31;
        if(type==1){vcl=true;if(input[0]&0x60)disposable=false;}
        else if(type!=6 && type!=9 && type!=10 && type!=11 && type!=12)disposable=false;
        out.insert(out.end(),{0,0,0,1}); out.insert(out.end(),input.begin(),input.begin()+n); input=input.subspan(n);
    }
    if(non_reference)*non_reference=vcl && disposable;
    return out.size()>16;
}
}
