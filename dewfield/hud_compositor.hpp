// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace dewfield_hud {
// One bounded scratch region, reused for each HUD group. Fully opaque HUDs
// bypass both capture and blending; no extra full-screen framebuffer.
class Backdrop {
  uint16_t *pixels;
  int capacity;
  int x=0,y=0,w=0,h=0,opacity=8;
public:
  Backdrop(uint16_t *scratch,int count):pixels(scratch),capacity(count){}
  static uint16_t blend(uint16_t bg,uint16_t fg,unsigned alpha) {
    unsigned keep=8-alpha;
    return uint16_t(((((fg&0xf81fu)*alpha+(bg&0xf81fu)*keep)>>3)&0xf81fu) |
                    ((((fg&0x07e0u)*alpha+(bg&0x07e0u)*keep)>>3)&0x07e0u));
  }
  void begin(uint16_t *frame,int width,int height,int left,int top,
             int right,int bottom,int alpha) {
    w=h=0;opacity=std::clamp(alpha,0,8);
    if(!frame || opacity==8)return;
    x=std::clamp(left,0,width);y=std::clamp(top,0,height);
    w=std::clamp(right,0,width)-x;h=std::clamp(bottom,0,height)-y;
    if(w<=0||h<=0){w=h=0;return;}
    if(!pixels||w*h>capacity)__builtin_trap();
    for(int row=0;row<h;++row)
      std::memcpy(pixels+row*w,frame+(y+row)*width+x,size_t(w)*2);
  }
  void end(uint16_t *frame,int pitch) {
    for(int row=0;row<h;++row)for(int col=0;col<w;++col) {
      auto &out=frame[(y+row)*pitch+x+col];
      auto bg=pixels[row*w+col];
      if(out!=bg)out=blend(bg,out,opacity);
    }
    w=h=0;
  }
};
}
