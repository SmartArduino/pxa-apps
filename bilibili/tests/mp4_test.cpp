// SPDX-License-Identifier: MIT
#include "../mp4.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
int main(int argc,char **argv) {
    assert(argc==2);
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};assert(!bytes.empty());
    bili::Track video,audio;
    auto moov=bili::child(bytes,"moov"); if (moov.empty()) moov=bytes;
    assert(bili::parse_moov(moov,video,audio));
    assert(video.width && video.height && video.nal_bytes<=4 && !video.parameters.empty());
    assert(!video.samples.empty() && !audio.samples.empty() && audio.channels<=2);
    for (const auto *track : {&video,&audio}) {
        uint64_t previous=0;
        for (const auto& s:track->samples) {assert(s.offset>=previous && s.size && s.duration);previous=s.offset;}
    }
    const uint8_t good[]={0,0,0,3,0x65,0x01,0x02},bad[]={0,0,0,9,0x65};std::vector<uint8_t> out;
    assert(bili::annexb(good,4,out,100) && out.size()==23 && out[19]==1 && out[20]==0x65);
    bool disposable=false;const uint8_t nonref[]={0,0,0,2,0x01,0x55},reference[]={0,0,0,2,0x41,0x55};
    assert(bili::annexb(nonref,4,out,100,&disposable) && disposable);
    assert(bili::annexb(reference,4,out,100,&disposable) && !disposable);
    assert(bili::annexb(good,4,out,100,&disposable) && !disposable);
    assert(!bili::annexb({},4,out,100));
    assert(!bili::annexb(bad,4,out,100));assert(!bili::annexb(good,4,out,2));
    bili::Track v2,a2;assert(!bili::parse_moov(moov.first(std::min<size_t>(30,moov.size())),v2,a2));
    std::printf("MP4 %ux%u: video=%zu audio=%zu, audio=%uHz/%uch\n",video.width,video.height,
        video.samples.size(),audio.samples.size(),audio.rate,audio.channels);
}
