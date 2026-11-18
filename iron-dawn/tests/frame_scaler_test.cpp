// SPDX-License-Identifier: MIT
#include "frame_scaler.hpp"
#include "hud_layout.hpp"
#include <cassert>
#include <cstdio>
using namespace iron;
int main(){
 struct Case{int sw,sh,dw,dh;};
 for(auto c:{Case{1,1,1,1},{1,2,8,9},{7,5,1,1},{11,9,17,21},{533,320,800,480},{533,320,400,240},{480,389,296,240},{960,540,1920,1080},{480,320,480,480},{79,63,79,137}}){
  std::vector<uint16_t> input(c.sw*c.sh);for(unsigned i=0;i<input.size();++i)input[i]=uint16_t(i*2767+(i>>7));FrameScaler scaler;scaler.reset(c.sw,c.sh,c.dw,c.dh);
  for(int padding:{0,1,7})for(int offset:{0,1}){int stride=c.dw+padding;std::vector<uint16_t> result(stride*c.dh+4,0xcafe);auto out=result.data()+offset;scaler.draw(out,stride,input.data(),c.sw);for(int y=0;y<c.dh;++y){for(int x=0;x<c.dw;++x)assert(out[y*stride+x]==input[int64_t(y)*c.sh/c.dh*c.sw+int64_t(x)*c.sw/c.dw]);for(int x=c.dw;x<stride;++x)assert(out[y*stride+x]==0xcafe);}if(offset)assert(result[0]==0xcafe);assert(result.back()==0xcafe);}
 }
 auto high=DisplayPlan::fit(800,480,88474,65536);assert(high.scale==2&&high.width==400&&high.height==240);auto standard=DisplayPlan::fit(800,480,65536,65536);assert(standard.scale==1);auto small=DisplayPlan::fit(296,240,65536,65536);assert(small.scale==1);
 std::puts("Nearest-neighbor pixels, odd widths, padded/misaligned rows, row reuse, clipped memory bounds and integer high-DPI surfaces passed");
}
