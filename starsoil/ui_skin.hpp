// SPDX-License-Identifier: MIT
#pragma once
#include "ui_art.hpp"
#include <algorithm>
#include <cstdint>
namespace starsoil_skin {
enum class Frame:uint8_t { Window,Button,Pressed,Slot,Selected,Tab,ActiveTab,Meter,Card };
// Nine-slice assembly uses fixed native-pixel corners and single sampled edge
// columns/rows. The center is a quiet flat field, so ornaments never stretch
// across labels. Identical adjacent runs are emitted as one rectangle.
template<class Fill>
void draw(int x,int y,int w,int h,Frame style,Fill fill,uint16_t interior=0) {
  if(w<=0 || h<=0)return;
  const auto *source=starsoil_skin_art::tiles[unsigned(style)];
  const auto *palette=starsoil_skin_art::palette;
  const int corner=style==Frame::Window?6:style==Frame::Card && w>80 && h>50?5:h<20?1:h<=24?2:4;
  int edge=std::min({corner,w/2,h/2});
  uint16_t center=interior?interior:palette[source[16*32+16]];
  if(!edge) {fill(x,y,w,h,center);return;}
  if(w>edge*2 && h>edge*2)fill(x+edge,y+edge,w-edge*2,h-edge*2,center);
  auto row=[&](int dy,int sy,int repeat) {
    int start=0;uint8_t prior=0;
    for(int dx=0;dx<=w;++dx) {
      int sx=dx<edge?dx:dx>=w-edge?32-w+dx:8;
      uint8_t next=dx==w?0:source[sy*32+sx];
      // The center fill is one rectangle, not a repeated patch.
      if(dx>=edge && dx<w-edge && dy>=edge && dy<h-edge)next=0;
      if(next!=prior) {
        if(prior)fill(x+start,y+dy,dx-start,repeat,palette[prior]);
        start=dx;prior=next;
      }
    }
  };
  for(int dy=0;dy<edge;++dy)row(dy,dy,1);
  if(h>edge*2)row(edge,8,h-edge*2);
  for(int dy=std::max(edge,h-edge);dy<h;++dy)row(dy,32-h+dy,1);
}
} // namespace starsoil_skin
