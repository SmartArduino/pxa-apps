// SPDX-License-Identifier: MIT
#pragma once
#include "items.hpp"
#include "color_math.hpp"
namespace starsoil_wall {
// Backing is darker than foreground blocks; seams follow world coordinates.
// A full base coat prevents transparent pinholes at corners or mixed materials.
template<class Rect> void draw(int wall, int mask, int column, int row,
                               int x, int y, int light, Rect rect) {
  using namespace starsoil;
  auto color=[](uint32_t c) {return uint16_t(((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31));};
  auto box=[&](int xx,int yy,int w,int h,uint32_t c) {
    rect(x+xx,y+yy,w,h,starsoil_color::dim(color(c),unsigned(light)));
  };
  const bool glass=wall==Glass,brick=wall==Brick;
  box(0,0,16,16,glass?0x52727f:brick?0x515864:0x675345);
  if(glass) {
    box(1,1,14,1,0xa6c7ca);box(1,1,1,14,0x779da6);
    box(15,0,1,16,0x344c5c);box(0,15,16,1,0x344c5c);
    box(3,3,4,1,0x8fb6c0);box(3,4,2,1,0x8fb6c0);
  } else if(brick) {
    for(int yy=0;yy<16;yy+=8) {
      int seam=((row*2+yy/8)&1)?4:12;
      box(0,yy,16,1,0x727b86);box(0,yy+7,16,1,0x363e4c);
      box(seam,yy,1,8,0x363e4c);box(seam+1,yy+1,1,5,0x69727e);
    }
    if((column+row)%3==0)box(2,4,3,1,0x626b77);
  } else {
    box(0,15,16,1,0x493e3d);box((row&1)?3:11,0,1,15,0x493e3d);
    box(2,3,1,8,0x8c6d51);box(6,7,3,1,0x796049);
  }
  if(!(mask&1))box(0,0,16,1,glass?0xa6c7ca:brick?0x727b86:0x8c6d51);
  if(!(mask&2))box(15,0,1,16,0x34313b);
  if(!(mask&4))box(0,15,16,1,0x34313b);
  if(!(mask&8))box(0,0,1,16,glass?0x779da6:brick?0x727b86:0x8c6d51);
}
} // namespace starsoil_wall
