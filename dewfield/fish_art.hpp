// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
namespace art {
// Small original fish portraits; square pixels at integer scales, no images or rotation.
template<class Canvas> void fish_portrait(Canvas &r,int x,int y,int id,int scale=1,bool known=true) {
  constexpr uint16_t colors[]={rgb(154,181,159),rgb(121,160,148),rgb(188,151,73),rgb(120,143,87),
    rgb(109,157,169),rgb(117,119,112),rgb(179,198,188),rgb(197,132,74)};
  uint16_t body=known?colors[id&7]:farm_palette::Muted;
  uint16_t light=known?farm_palette::Highlight:body;
  auto box=[&](int xx,int yy,int w,int h,uint16_t c){r.rect(x+xx*scale,y+yy*scale,w*scale,h*scale,c);};
  box(0,2,2,7,Ink);box(2,3,2,5,Ink);box(4,2,11,7,Ink);box(6,1,6,9,Ink);
  box(7,0,4,2,Ink);box(14,3,2,4,Ink);
  box(1,3,1,5,body);box(2,4,2,3,body);box(5,3,9,5,body);box(6,2,6,7,body);
  box(7,1,3,1,body);box(7,2,5,1,light);box(5,3,2,1,light);
  box(7,6,4,2,known?farm_palette::Water:body);box(5,5,1,1,light);
  box(8,4,1,1,light);box(10,5,1,1,light);box(12,4,1,1,Ink);box(14,5,1,1,light);
  if(id==5)box(14,7,3,1,Ink);
  if(id==7){box(6,4,2,2,farm_palette::WoodDark);box(10,7,2,1,farm_palette::Brass);}
}
} // namespace art
