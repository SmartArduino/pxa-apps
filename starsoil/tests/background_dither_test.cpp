// SPDX-License-Identifier: MIT
#include "../background_dither.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

int main() {
  using namespace starsoil_dither;
  const Row palette={0x0123,0x4567,0x89ab,0xcdef};
  for (int offset=0;offset<4;++offset) for (unsigned phase=0;phase<4;++phase)
    for (int count=0;count<514;++count) {
      std::vector<uint16_t> pixels(size_t(count+offset+8),0x1357);
      fill(pixels.data()+offset+2,size_t(count),palette.data(),phase);
      for (size_t i=0;i<pixels.size();++i)
        assert(pixels[i]==(i>=size_t(offset+2) && i<size_t(offset+2+count)
            ? palette[(phase+i-size_t(offset+2))&3] : 0x1357));
    }
  puts("PASS repeated-pattern writes: all phases, odd spans, alignment and guard pixels");
  double largest=0;
  for (uint32_t a:{0u,0xffffffu,0x80afc4u,0x192940u,0x141d30u})
    for (uint32_t b:{0u,0xffffffu,0xeee0b5u,0x53657eu,0x284452u})
      for (unsigned k=0;k<=256;++k) {
        double sum[3]={};
        for (int y=0;y<4;++y) {
          auto row=gradient_row(a,b,k,y);
          for (auto c:row) {sum[0]+=c&31;sum[1]+=(c>>5)&63;sum[2]+=c>>11;}
        }
        for (int channel=0;channel<3;++channel) {
          const double ideal=(((a>>(channel*8))&255)*(256-k)+
                              ((b>>(channel*8))&255)*k)*(channel==1?63.0:31.0)/(256*255);
          const double error=std::abs(sum[channel]/16-ideal);
          largest=std::max(largest,error);assert(error<=1.0/32+1e-9);
        }
      }
  printf("PASS RGB888 gradients retain average color to %.5f RGB565 channel units; no saturated-channel wrap\n",largest);
  for (uint16_t a:{uint16_t(0),uint16_t(0xffff),uint16_t(0x2345),uint16_t(0xabcd)})
    for (uint16_t b:{uint16_t(0),uint16_t(0xffff),uint16_t(0x2345),uint16_t(0xabcd)})
      for (unsigned k=0;k<256;++k) {
        double sum[3]={};
        for (int y=0;y<4;++y) for (int x=0;x<4;++x) {
          auto c=mix565(a,b,k,x,y);
          if (!k) assert(c==a); if(k==255) assert(c==b);
          sum[0]+=c&31;sum[1]+=(c>>5)&63;sum[2]+=c>>11;
        }
        for (int channel=0;channel<3;++channel) {
          int shift=channel==2?11:channel*5,mask=channel==1?63:31;
          double ideal=(((a>>shift)&mask)*(255-k)+((b>>shift)&mask)*k)/255.0;
          assert(std::abs(sum[channel]/16-ideal)<=1.0/32+1e-9);
        }
      }
  puts("PASS cave alpha mixing preserves endpoints and unbiased neighboring colors");
}
