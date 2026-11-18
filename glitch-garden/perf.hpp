// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace garden {
struct FramePerf {
 std::array<uint32_t,256> intervals{};unsigned count=0;uint64_t began=0,last=0,update=0,background=0,copy=0,draw=0,present=0;uint32_t maximum=0;
 void frame(uint64_t timestamp,uint32_t bg,uint32_t cp,uint32_t paint,uint32_t submit){if(!began)began=timestamp;
  if(last){auto delta=uint32_t(std::min(uint64_t(UINT32_MAX),timestamp-last));intervals[(count-1)%intervals.size()]=delta;maximum=std::max(maximum,delta);}last=timestamp;++count;background+=bg;copy+=cp;draw+=paint;present+=submit;}
 uint32_t percentile(unsigned p)const{auto sorted=intervals;unsigned n=std::min<unsigned>(count>0?count-1:0,sorted.size());if(!n)return 0;std::sort(sorted.begin(),sorted.begin()+n);return sorted[(n-1)*p/100];}
};
}
