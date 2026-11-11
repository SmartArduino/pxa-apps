// SPDX-License-Identifier: MIT
#include "../asset_stream.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
static uint64_t requested;
static unsigned offset,length,reads,cancels;
static bool reject;
extern "C" int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t*p,uint32_t n) {
 if((route&65535)==PXA_CORE_SERVICE){++cancels;return 0;}
 assert(route==(PXA_ASSETS_SERVICE|(PXA_ASSETS_READ<<16))&&n>=12);
 requested=token;offset=pxa_load_u32(p+4);length=pxa_load_u32(p+8);++reads;return reject?-1:0;
}
static pxa_event_t reply(std::vector<uint8_t>&packet,unsigned total,uint64_t token) {
 packet.resize(12+length);pxa_store_u32(packet.data(),0);pxa_store_u32(packet.data()+4,offset);pxa_store_u32(packet.data()+8,total);
 for(unsigned i=0;i<length;++i)packet[12+i]=uint8_t((offset+i)%128);
 pxa_event_t e{};e.service=PXA_ASSETS_SERVICE;e.opcode=PXA_ASSETS_READ;e.token=token;e.payload=packet.data();e.payload_size=packet.size();return e;
}
int main() {
 dewfield_assets::Stream stream;std::vector<uint8_t>dst(38402,0xcd),packet;
 assert(!stream.load("assets/opening-0.idx",dst.data()+1,38400)&&reads==1);
 auto first=requested;assert(!stream.load("assets/opening-0.idx",dst.data()+1,38400)&&reads==1);
 while(offset+length<38400){auto e=reply(packet,38400,requested);assert(stream.event(e));}
 auto last=reply(packet,38400,requested);assert(stream.event(last)&&stream.load("assets/opening-0.idx",dst.data()+1,38400));
 auto count=reads;for(int i=0;i<100;++i)assert(stream.load("assets/opening-0.idx",dst.data()+1,38400));assert(reads==count);
 assert(dst.front()==0xcd&&dst.back()==0xcd);for(int i=0;i<38400;++i)assert(dst[i+1]==i%128);
 assert(!stream.event(last));assert(!stream.load("assets/opening-1.idx",dst.data()+1,38400));auto old=requested;
 stream.cancel();auto stale=reply(packet,38400,old);assert(!stream.event(stale)&&cancels==1);
 assert(!stream.load("assets/opening-2.idx",dst.data()+1,38400));auto bad=reply(packet,38399,requested);assert(stream.event(bad));
 count=reads;assert(!stream.load("assets/opening-2.idx",dst.data()+1,38400)&&reads==count); // fail closed, no polling loop
 reject=true;assert(!stream.load("assets/missing.idx",dst.data()+1,38400));stream.cancel();
 puts("PASS: bounded asset chunks, zero reads on cached frames, cancellation, stale events, malformed sizes and failed reads");
}
