// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "raster.hpp"
#include <vector>
#include <cstring>
namespace iron {
struct MinimapCache {
 std::vector<uint16_t> pixels;std::array<uint8_t,N*N> terrain{},seen{},fog{},ore{};int width=0,height=0;bool valid=false;unsigned hits=0,misses=0;
 void draw(Raster& r,const World& w,Rect m){bool same=valid&&width==m.w&&height==m.h&&terrain==w.terrain&&seen==w.seen&&fog==w.fog;
  if(same)for(int c=0;c<N*N;++c)if(ore[c]!=bool(w.ore[c])){same=false;break;}
  if(!same){++misses;width=m.w;height=m.h;terrain=w.terrain;seen=w.seen;fog=w.fog;pixels.resize(width*height);Raster dest(pixels.data(),width,height,width);
   for(int y=0;y<N;++y)for(int x=0;x<N;++x){int c=y*N+x;ore[c]=bool(w.ore[c]);auto color=!w.seen[c]?navy:w.terrain[c]==2?rgb(38,72,80):w.ore[c]?gold:w.terrain[c]==ridge?rgb(97,108,101):w.terrain[c]==road||w.terrain[c]==bridge?rgb(120,133,117):w.fog[c]?rgb(81,106,82):rgb(45,62,56);dest.rect(x*width/N,y*height/N,(x+1)*width/N-x*width/N,(y+1)*height/N-y*height/N,color);}valid=true;
  }else ++hits;
  int left=std::max(m.x,r.clip.x),right=std::min(m.x+m.w,r.clip.x+r.clip.w);if(right<=left)return;
  for(int y=std::max(m.y,r.clip.y);y<std::min(m.y+m.h,r.clip.y+r.clip.h);++y)std::memcpy(r.pixels+y*r.stride+left,pixels.data()+(y-m.y)*width+left-m.x,size_t(right-left)*2);
 }
};
}
