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
  for (unsigned alpha : {0u, 1u, 23u, 90u, 254u, 255u, 300u})
    for (int x : {-40, -2, 0, 9, 31, 40})
      for (int y : {-40, -2, 0, 9, 31, 40}) {
        std::vector<uint16_t> actual(32 * 32 + 2, 0x1357), expected = actual;
        for (int yy = std::max(0, y); yy < std::min(32, y + 19); ++yy)
          for (int xx = std::max(0, x); xx < std::min(32, x + 17); ++xx)
            expected[1 + yy * 32 + xx] = starsoil_color::mix(0x1357, 0xfd35, std::min(255u, alpha));
        starsoil_pixels::blend_rectangle(actual.data() + 1, 32, 32, x, y, 17, 19, 0xfd35, alpha);
        assert(actual == expected);
      }
  puts("PASS logical RGB565 glow alpha, scene edge clipping, zero/opaque alpha and buffer guards");
}
