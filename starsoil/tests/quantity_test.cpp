// SPDX-License-Identifier: MIT
#include "../quantity.hpp"
#include <array>
#include <cassert>
#include <cstdio>
int main() {
  for(unsigned count:{0u,1u,2u,9u,10u,24u,99u,100u,999u}) {
    std::array<uint16_t,32*20> pixels{};
    for(unsigned i=0;i<pixels.size();++i)pixels[i]=uint16_t(100+i);
    const auto before=pixels;
    int painted=0;
    starsoil_quantity::draw(23,16,count,1,2,[&](int x,int y,uint16_t c) {
      assert(x>=8 && x<23 && y>=8 && y<16);
      pixels[y*32+x]=c;++painted;
    });
    if(count<=1)assert(pixels==before && !painted);
    else {
      assert(painted>0);
      int retained=0;
      for(int y=8;y<16;++y)for(int x=8;x<23;++x)retained+=pixels[y*32+x]==before[y*32+x];
      assert(retained>35); // Icons/slot colour remain visible; no backing box.
      for(int y=0;y<20;++y)for(int x=0;x<32;++x)
        if(x<8 || x>=23 || y<8 || y>=16)assert(pixels[y*32+x]==before[y*32+x]);
    }
  }
  puts("PASS transparent quantity glyphs: 0/1 hidden, 2..999 fit a 15x8 area, preserve the background and never touch adjacent slots");
}
