// SPDX-License-Identifier: MIT
#pragma once
#include "raster.hpp"
#include "model.hpp"
#include "art_data.hpp"
#include "span_cache.hpp"
#include <cstring>
namespace iron {
struct Sprites {
 static SpriteSpanCache& cache(){static SpriteSpanCache value;return value;}
 static_assert([](){for(auto f:art::frames)if(f.w>432||f.h>240||f.offset>f.end||f.end>art::bytes.size()||f.palette>=art::palettes.size())return false;return true;}());
 static void blit(Raster& r,unsigned id,int x,int y,float sx=1,float sy=1,unsigned variant=0){
  if(id>=art::frames.size()||sx<=0||sy<=0)return;const auto& f=art::frames[id];if(x>=r.clip.x+r.clip.w||y>=r.clip.y+r.clip.h||x+int(f.w*sx)+1<=r.clip.x||y+int(f.h*sy)+1<=r.clip.y)return;
  const auto& palette=art::palettes[f.palette][std::min(4u,variant)];auto bytes=art::bytes.data()+f.offset;unsigned step=f.w>255?4:3;
  const auto* cached=cache().get(id,variant,sx,sy);
  if(sx==1&&sy==1){for(unsigned i=0;i<f.count;++i){int xx=bytes[0]+(step==4?(int(bytes[1])<<8):0),yy=y+bytes[step-2],n=bytes[step-1];bytes+=step;if(yy>=r.clip.y&&yy<r.clip.y+r.clip.h){int left=std::max(x+xx,r.clip.x),right=std::min(x+xx+n,r.clip.x+r.clip.w);auto row=r.pixels+yy*r.stride;if(left<right){if(cached){auto source=cache().pixels.data()+cached->offset+(yy-y-cached->top)*cached->stride+left-x-cached->left;std::memcpy(row+left,source,size_t(right-left)*2);}else for(int col=left;col<right;++col)row[col]=palette[bytes[col-x-xx]];}}bytes+=n;}return;}
  std::array<int,433> xs;std::array<int,241> ys;for(unsigned i=0;i<=f.w;++i)xs[i]=x+int(std::floor(i*sx));for(unsigned i=0;i<=f.h;++i)ys[i]=y+int(std::floor(i*sy));
  for(unsigned i=0;i<f.count;++i){unsigned xx=bytes[0]+(step==4?(unsigned(bytes[1])<<8):0),yy=bytes[step-2],n=bytes[step-1];bytes+=step;int left=std::max(xs[xx],r.clip.x),right=std::min(xs[xx+n],r.clip.x+r.clip.w);for(int row=std::max(ys[yy],r.clip.y);row<std::min(ys[yy+1],r.clip.y+r.clip.h);++row){auto output=r.pixels+row*r.stride;if(cached&&left<right){auto source=cache().pixels.data()+cached->offset+(row-y-cached->top)*cached->stride+left-x-cached->left;std::memcpy(output+left,source,size_t(right-left)*2);}else {unsigned source=0;for(int col=left;col<right;++col){while(source+1<n&&xs[xx+source+1]<=col)++source;output[col]=palette[bytes[source]];}}}bytes+=n;}
 }
 static void anchor(Raster& r,unsigned id,int x,int y,float scale=1,unsigned variant=0){if(id>=art::frames.size())return;auto& f=art::frames[id];blit(r,id,x-int(std::floor(f.ox*scale)),y-int(std::floor(f.oy*scale)),scale,scale,variant);}
 static void fit(Raster& r,unsigned id,Rect area,unsigned variant=0){
  if(id>=art::frames.size()||area.w<=0||area.h<=0)return;auto& f=art::frames[id];float scale=std::min(float(area.w)/std::max(1u,unsigned(f.bw)),float(area.h)/std::max(1u,unsigned(f.bh)));
  int x=area.x+(area.w-int(f.bw*scale))/2-int(f.bx*scale),y=area.y+(area.h-int(f.bh*scale))/2-int(f.by*scale);auto old=r.clip;int left=std::max(old.x,area.x),top=std::max(old.y,area.y),right=std::min(old.x+old.w,area.x+area.w),bottom=std::min(old.y+old.h,area.y+area.h);r.clip={left,top,std::max(0,right-left),std::max(0,bottom-top)};blit(r,id,x,y,scale,scale,variant);r.clip=old;
 }
 static unsigned building_frame(int kind,Faction faction=Faction::northern,int landmark=-1){
  if(landmark>=2)return art::white_island_landmarks_first+unsigned(landmark-2);
  if(landmark>=0)return art::harbor_landmarks_first+unsigned(landmark);
  if(faction==Faction::gray_tower){
   if(kind>=hq&&kind<=turret)return art::gray_tower_buildings_first+kind-hq;
   if(kind>=shipyard&&kind<=aa_tower)return art::gray_tower_buildings_first+6+kind-shipyard;
  }
  if(kind>=shipyard&&kind<=aa_tower)return art::combined_structures_first+kind-shipyard;
  return art::buildings_first+std::clamp(kind-int(hq),0,7);
 }
 static unsigned unit_frame(int kind,float angle,unsigned pose=0,Faction faction=Faction::northern){
  unsigned facing=int(std::lround(angle*4/3.14159265f))&7;
  if(faction==Faction::gray_tower){
   switch(kind){
    case rifle:return (pose?art::gray_tower_walk_first:art::gray_tower_infantry_first)+facing;
    case rocket:return art::gray_tower_rocket_first+facing;
    case tank:return art::gray_tower_tank_first+facing;
    case artillery:return art::gray_tower_artillery_first+facing;
    case rotorcraft:return art::gray_tower_air_first+facing;
    case commander:return art::gray_tower_infantry_first+facing;
   }
  }
  if(kind==patrol_boat)return art::patrol_boats_first+facing;
  if(kind==landing_craft)return art::landing_craft_first+facing;
  if(kind==rotorcraft)return art::rotorcraft_first+facing;
  if(kind==commander)return art::commander_first+facing;
  return art::unit_first[std::clamp(kind,0,4)]+facing+(pose&1)*8;
 }
};
}
