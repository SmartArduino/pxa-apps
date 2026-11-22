// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "raster.hpp"
#include <cstring>
#include <vector>
namespace iron {
struct TileBounds {
 int left,right,first,last;
 TileBounds expanded(int by,int side)const{
  return {std::max(0,left-by),std::min(side-1,right+by),
          std::max(0,first-by),std::min(side-1,last+by)};
 }
};
// Keep damage in a few non-overlapping horizontal bands. A coastline can
// span the view without forcing the land between its segments to repaint.
struct GroundPatches {
 Rect clip;
 std::array<Rect,4> bands{};
 void add(Rect area){
  area=intersect(area,clip);
  if(!area.w||!area.h)return;
  for(int i=0;i<int(bands.size());++i){
   int y=clip.y+i*clip.h/int(bands.size());
   int end=clip.y+(i+1)*clip.h/int(bands.size());
   Rect part=intersect(area,{clip.x,y,clip.w,end-y});
   if(!part.h)continue;
   auto& band=bands[i];
   if(!band.w){band=part;continue;}
   int left=std::min(band.x,part.x),top=std::min(band.y,part.y);
   int right=std::max(band.x+band.w,part.x+part.w),bottom=std::max(band.y+band.h,part.y+part.h);
   band={left,top,right-left,bottom-top};
  }
 }
 int area()const{
  int total=0;
  for(auto band:bands)total+=band.w*band.h;
  return total;
 }
};
struct GroundCache {
 std::vector<uint16_t> pixels;std::array<uint8_t,N*N> terrain{},fog{},seen{},ore{};
 int width=0,top=0,bottom=0,chapter=-1,phase=0,side=0;
 GroundMaterial material=GroundMaterial::grass;
 uint32_t defense_mode=0,seed=0;
 uint8_t map_rules=0;
 bool animated_water=false;float camx=0,camy=0,zoom=0;unsigned hits=0,misses=0,scrolls=0,patches=0;bool valid=false;
 TileBounds previous_tiles{};
 void release(){std::vector<uint16_t>{}.swap(pixels);valid=false;}
 bool unchanged(const World& world,TileBounds bounds)const{
  // A road samples two neighboring tiles to choose its direction.
  auto neighbors=bounds.expanded(2,world.side);
  int count=neighbors.right-neighbors.left+1;
  for(int y=neighbors.first;y<=neighbors.last;++y){
   int first=y*N+neighbors.left;
   if(std::memcmp(terrain.data()+first,world.terrain.data()+first,count)||
      std::memcmp(seen.data()+first,world.seen.data()+first,count)||
      std::memcmp(fog.data()+first,world.fog.data()+first,count))return false;
  }
  for(int y=bounds.first;y<=bounds.last;++y)
   for(int x=bounds.left;x<=bounds.right;++x){
    int c=y*N+x;
    if(ore[c]!=bool(world.ore[c]))return false;
   }
  return true;
 }
 void scroll(Rect view,Rect overlap,int dx,int dy){
  for(int row=0;row<overlap.h;++row){
   int y=dy>0?overlap.y+overlap.h-1-row:overlap.y+row;
   std::memmove(pixels.data()+y*view.w+overlap.x,
                pixels.data()+(y-dy)*view.w+overlap.x-dx,
                size_t(overlap.w)*sizeof(uint16_t));
  }
 }
 template<class Add>
 void find_damage(const World& world,bool tiles_changed,bool water_changed,Add add)const{
  auto bounds=previous_tiles;
  auto scan=tiles_changed?bounds.expanded(2,world.side):bounds;
  for(int y=scan.first;y<=scan.last;++y)for(int x=scan.left;x<=scan.right;++x){
   int cell=y*N+x;
   bool visible=x>=bounds.left&&x<=bounds.right&&y>=bounds.first&&y<=bounds.last;
   bool changed_type=tiles_changed&&terrain[cell]!=world.terrain[cell];
   bool discovered=tiles_changed&&seen[cell]!=world.seen[cell];
   if(visible&&tiles_changed&&(changed_type||discovered||fog[cell]!=world.fog[cell]||
                               ore[cell]!=bool(world.ore[cell])))add(x,y,false);
   else if(visible&&water_changed&&seen[cell]&&terrain[cell]==water)add(x,y,true);
   if(!changed_type&&!discovered)continue;
   // Shore decorations read the next tile's discovery and terrain.
   // Bridge ends read adjacent bridge tiles and roads read up to two cells.
   for(int distance=1;distance<=(changed_type?2:1);++distance)
    for(auto d:{Vec{1,0},Vec{-1,0},Vec{0,1},Vec{0,-1}}){
     int xx=x+int(d.x)*distance,yy=y+int(d.y)*distance;
     if(xx<bounds.left||xx>bounds.right||yy<bounds.first||yy>bounds.last)continue;
     int neighbor=yy*N+xx;
     if(!seen[neighbor]&&!world.seen[neighbor])continue;
     auto type=world.terrain[neighbor];
     if((distance==1&&type==water)||(changed_type&&(type==road||(distance==1&&type==bridge))))
      add(xx,yy,false);
    }
  }
 }
 template<class ClipOverlap,class TileRect,class Draw> void draw(Raster& output,const World& world,int ww,int tt,int bb,
                                float xx,float yy,float zz,int water_phase,TileBounds bounds,
                                ClipOverlap clip_overlap,TileRect tile_rect,Draw render){
  bool same_layout=valid&&ww==width&&tt==top&&bb==bottom&&zz==zoom&&
            world.chapter==chapter&&world.side==side&&world.material()==material&&
            world.extra.mode==defense_mode&&world.extra.seed==seed&&world.extra.map_rules==map_rules;
  bool moved=xx!=camx||yy!=camy;
  bool tiles_changed=same_layout&&!unchanged(world,previous_tiles);
  bool water_changed=same_layout&&animated_water&&phase!=water_phase;
  if(same_layout&&!moved&&!tiles_changed&&!water_changed)++hits;else{
   Rect view{0,tt,ww,bb-tt};
   float shift_x=camx-xx,shift_y=camy-yy;
   int dx=int(shift_x),dy=int(shift_y);
   Rect overlap=intersect(view,{dx,tt+dy,ww,bb-tt});
   // Several ground details truncate coordinates at x=0. Redraw that edge
   // when its original and translated positions can round differently.
   if(dx){
    int left=std::max(0,dx)+42;
    overlap=intersect(overlap,{left,tt,ww-left,bb-tt});
   }
   // At this scale the projected tile centers have an exact half-pixel
   // phase. Other scales retain the full redraw to avoid rounding seams.
   bool aligned=same_layout&&zz==.75f&&tt>=32&&dx==shift_x&&dy==shift_y;
   if(aligned&&dy)overlap=clip_overlap(overlap);
   bool reuse_pixels=aligned&&overlap.w>=ww/2&&overlap.h>=view.h/2;
   GroundPatches damage{overlap};
   if(reuse_pixels&&(tiles_changed||water_changed)){
    find_damage(world,tiles_changed,water_changed,[&](int x,int y,bool wave){damage.add(tile_rect(x,y,wave));});
    // When changes cover most of the view, one painter pass is cheaper.
    reuse_pixels=damage.area()<overlap.w*overlap.h*3/5;
   }
   ++misses;width=ww;top=tt;bottom=bb;camx=xx;camy=yy;zoom=zz;chapter=world.chapter;side=world.side;
   material=world.material();defense_mode=world.extra.mode;seed=world.extra.seed;map_rules=world.extra.map_rules;phase=water_phase;
   terrain=world.terrain;fog=world.fog;seen=world.seen;previous_tiles=bounds;
   for(int y=bounds.first;y<=bounds.last;++y)
    for(int x=bounds.left;x<=bounds.right;++x)ore[y*N+x]=world.ore[y*N+x]>0;
   size_t needed=size_t(width)*bottom;if(needed>pixels.capacity())std::vector<uint16_t>{}.swap(pixels);
   pixels.resize(needed);Raster backing(pixels.data(),width,bottom,width);
   if(reuse_pixels){
    if(moved){++scrolls;scroll(view,overlap,dx,dy);}
    auto repaint=[&](Rect area){
     if(area.w>0&&area.h>0){backing.clip=area;animated_water=render(backing)||animated_water;}
    };
    for(auto strip:{Rect{0,tt,ww,overlap.y-tt},
                    Rect{0,overlap.y,overlap.x,overlap.h},
                    Rect{overlap.x+overlap.w,overlap.y,ww-overlap.x-overlap.w,overlap.h},
                    Rect{0,overlap.y+overlap.h,ww,bb-overlap.y-overlap.h}}){
     repaint(strip);
    }
    if(damage.area())++patches;
    for(auto band:damage.bands)repaint(band);
   }else{
    backing.clip=view;animated_water=render(backing);
   }
   valid=true;
  }
  int left=std::max(0,output.clip.x),right=std::min(width,output.clip.x+output.clip.w),first=std::max(top,output.clip.y),last=std::min(bottom,output.clip.y+output.clip.h);
  if(left<right)for(int y=first;y<last;++y)std::copy_n(pixels.data()+y*width+left,right-left,output.pixels+y*output.stride+left);
 }
};
}
