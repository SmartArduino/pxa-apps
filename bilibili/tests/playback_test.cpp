// SPDX-License-Identifier: MIT
#include "../playback.hpp"
#include <cassert>
#include <cstdio>
int main() {
    assert(bili::media_position(100,1000000,1100,6)==2500000);
    for(unsigned rate:{2,3,4,5,6,8}){bili::PcmSpeed whole,chunks;int16_t source[18],a[36],b[36];for(int i=0;i<18;++i)source[i]=i;
        size_t n=whole.convert(source,18,1,rate,a,36),first=chunks.convert(source,7,1,rate,b,36),second=chunks.convert(source+7,11,1,rate,b+first,36-first);
        assert(n==first+second&&std::equal(a,a+n,b));}

    assert(bili::media_position(50000,85133000,50000)==85133000);
    assert(bili::media_position(50000,85133000,50250)==85383000);
    assert(bili::media_position(50200,85133000,50100)==85133000);
    for(auto [sw,sh]:{std::pair{640u,360u},{480u,360u},{360u,640u},{1u,7u}}) {
        auto f=bili::fit(sw,sh,854,480);
        assert(f.width && f.height && f.x*2+f.width<=854 && f.y*2+f.height<=480);
        assert(f.width==854 || f.height==480);
        assert(uint64_t(f.width)*sh<=uint64_t(f.height)*sw+std::max(sw,sh));
        assert(uint64_t(f.height)*sw<=uint64_t(f.width)*sh+std::max(sw,sh));
    }
    assert(!bili::fit(0,360,854,480).width);
    auto portrait=bili::fit(480,360,480,854);assert(portrait.width==480 && portrait.height==360 && portrait.y==247);
    bili::Track t;for(unsigned i=0;i<12;++i)t.samples.push_back({i*10,i*1000,10,1000});
    t.sync_samples={0,4,8};assert(bili::keyframe(t,7000)==4 && bili::keyframe(t,0)==0 && bili::keyframe(t,99000)==8);
    assert(bili::duration(t)==12000);t.sync_samples.clear();assert(bili::keyframe(t,7000)==7);
    const uint8_t data[32]{};bili::Sample s{116,0,8,0};
    assert(bili::window(s,100,data,32)==data+16);
    assert(!bili::window(s,100,data,23) && !bili::window(s,UINT64_MAX,data,32));
    assert(bili::requested(s,100,24) && !bili::requested(s,117,32));
    s.offset=UINT64_MAX-2;s.size=8;assert(!bili::requested(s,UINT64_MAX-4,8));
    std::puts("Aspect fit, keyframe seeking, duration and shared range bounds passed");
}
