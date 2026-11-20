// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "raster.hpp"
#include <vector>
#include <cstring>
namespace iron {
struct MinimapCache {
 std::vector<uint16_t> pixels;std::array<uint8_t,N*N> terrain{},seen{},fog{},ore{};int width=0,height=0,side=0;GroundMaterial material=GroundMaterial::grass;bool valid=false,intelligence=false;unsigned hits=0,misses=0;
 void release(){std::vector<uint16_t>{}.swap(pixels);valid=false;}
 static uint16_t tile_color(const World& w,int c,bool intel){
  if(!intel&&!w.seen[c])return navy;
  if(w.ore[c])return gold;
  switch(w.terrain[c]){
   case water:return rgb(38,72,80);
   case ridge:return rgb(100,108,110);
   case road:case bridge:return rgb(126,133,117);
   case railbed:return rgb(125,105,88);
   case woodland:return rgb(44,81,60);
   case crater:case ruins:return rgb(72,64,61);
   default:break;
  }
  if(!intel&&!w.fog[c])return rgb(45,62,56);
  switch(w.material()){
   case GroundMaterial::red_gravel:return rgb(134,79,56);
   case GroundMaterial::marsh:return rgb(63,88,77);
   case GroundMaterial::scree:return rgb(71,82,91);
   case GroundMaterial::asphalt:return rgb(71,81,89);
   default:return rgb(81,106,82);
  }
 }
 void draw(Raster& r,const World& w,Rect m,bool intel=false){bool same=valid&&intelligence==intel&&width==m.w&&height==m.h&&side==w.side&&material==w.material()&&terrain==w.terrain&&seen==w.seen&&fog==w.fog;
  if(same)for(int c=0;c<N*N;++c)if(ore[c]!=bool(w.ore[c])){same=false;break;}
  if(!same){++misses;width=m.w;height=m.h;side=w.side;material=w.material();intelligence=intel;terrain=w.terrain;seen=w.seen;fog=w.fog;pixels.resize(width*height);Raster dest(pixels.data(),width,height,width);
   for(int y=0;y<w.side;++y)for(int x=0;x<w.side;++x){int c=y*N+x;ore[c]=bool(w.ore[c]);auto color=tile_color(w,c,intel);dest.rect(x*width/w.side,y*height/w.side,(x+1)*width/w.side-x*width/w.side,(y+1)*height/w.side-y*height/w.side,color);}valid=true;
  }else ++hits;
  int left=std::max(m.x,r.clip.x),right=std::min(m.x+m.w,r.clip.x+r.clip.w);if(right<=left)return;
  for(int y=std::max(m.y,r.clip.y);y<std::min(m.y+m.h,r.clip.y+r.clip.h);++y)std::memcpy(r.pixels+y*r.stride+left,pixels.data()+(y-m.y)*width+left-m.x,size_t(right-left)*2);
 }
};
}
