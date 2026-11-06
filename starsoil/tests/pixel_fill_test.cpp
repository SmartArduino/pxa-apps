// SPDX-License-Identifier: MIT
#include "../pixel_fill.hpp"
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
int main() {
  std::mt19937 rng(123456);
  for(int i=0;i<500;++i) {
    int w=1+rng()%480,h=1+rng()%480;
    std::vector<uint16_t> actual(w*h+2,0x1234),expected=actual;
    for(int j=0;j<30;++j) {
      int left=rng()%w,top=rng()%h,right=left+rng()%(w-left+1),bottom=top+rng()%(h-top+1);
      if(j%3==0){left=0;right=w;}
      uint16_t c=uint16_t(rng());
      for(int y=top;y<bottom;++y)for(int x=left;x<right;++x)expected[1+y*w+x]=c;
      starsoil_pixels::rectangle(actual.data()+1,w,left,top,right,bottom,c);
      assert(actual==expected);
    }
  }
  puts("PASS exact flat fills, empty/single/odd spans, full and pitched rows, frame edges and buffer guards");
}
