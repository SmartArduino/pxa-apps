// SPDX-License-Identifier: MIT
#include "../ui_skin.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
int main() {
  using namespace starsoil_skin;
  for(int w:{1,2,4,7,8,16,23,58,144,288,384})
    for(int h:{1,3,7,8,16,20,23,32,112,288})
      for(int kind=0;kind<9;++kind) {
        constexpr uint16_t background=0x1234;
        std::vector<uint16_t> pixels((w+8)*(h+8),background);
        unsigned calls=0;
        draw(3,2,w,h,Frame(kind),[&](int x,int y,int ww,int hh,uint16_t color) {
          assert(x>=3 && y>=2 && x+ww<=w+3 && y+hh<=h+2 && ww>0 && hh>0);++calls;
          for(int yy=y;yy<y+hh;++yy)for(int xx=x;xx<x+ww;++xx)pixels[yy*(w+8)+xx]=color;
        });
        assert(calls<=169); // At most twelve corner rows and two six-pixel edges.
        auto *source=starsoil_skin_art::tiles[kind];
        int corner=kind==0?6:kind==8 && w>80 && h>50?5:h<20?1:h<=24?2:4;
        int edge=std::min({corner,w/2,h/2});
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
          uint16_t expected=background;
          if(!edge || (x>=edge && x<w-edge && y>=edge && y<h-edge))
            expected=starsoil_skin_art::palette[source[16*32+16]];
          else {
            int sx=x<edge?x:x>=w-edge?32-w+x:8;
            int sy=y<edge?y:y>=h-edge?32-h+y:8;
            auto index=source[sy*32+sx];if(index)expected=starsoil_skin_art::palette[index];
          }
          assert(pixels[(y+2)*(w+8)+x+3]==expected);
        }
      }
  puts("PASS generated nine-slice skins: native corners, transparent outside corners, unclipped extents, quiet centers, tiny/large controls and bounded draw calls");
}
