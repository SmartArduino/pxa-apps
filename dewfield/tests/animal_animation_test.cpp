// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <set>
using Pixels=std::array<uint16_t,32*28>;
struct Canvas {
  Pixels &p;
  void rect(int x,int y,int w,int h,uint16_t c){assert(x>=0&&y>=0&&x+w<=32&&y+h<=28);for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)p[yy*32+xx]=c;}
};
int main(){
 using namespace animal_sprites;
 static_assert(StorageBytes<96*1024,"Keep all animal poses within the fixed guest budget");
 static uint16_t tint[65536];for(unsigned i=0;i<65536;++i)tint[i]=uint16_t(i^0xffff);
 for(auto &f:frames)for(auto id:f.rows){assert(id<std::size(rows));const auto &row=rows[id];assert(row.start+row.count<=std::size(runs));int end=0;for(unsigned i=0;i<row.count;++i){auto &r=runs[row.start+i];assert(r.count&&r.x>=end&&r.x+r.count<=32&&r.offset+r.count<=std::size(pixels));end=r.x+r.count;}}
 for(int kind=1;kind<=5;++kind)for(int age=0;age<3;++age){
  farm::Animal a{uint8_t(kind),0,uint8_t(age==0?0:age==1?farm::Breeds[kind-1].young:farm::Breeds[kind-1].adult),70};
  std::set<Pixels> walking,eating;
  Pixels idle{};
  for(int frame=0;frame<Poses;++frame){
   Pixels east{},west{},ui{},shadow{};art::Raster er{east.data(),32,28},wr{west.data(),32,28},sr{shadow.data(),32,28};
   auto &e=art::animal_sprite(a,1,frame);auto &w=art::animal_sprite(a,3,frame);
   art::animal(er,16,27,a,1,frame,false);art::animal(wr,16,27,a,3,frame,false);
   Canvas canvas{ui};art::animal_pixels(canvas,e,16,27);assert(east==ui);
   int contacts=0;
   for(int y=0;y<28;++y)for(int x=0;x<32;++x){assert(east[y*32+x]==west[y*32+31-x]);assert(art::animal_hit(e,x,y)==bool(east[y*32+x]));assert(art::animal_hit(w,x,y)==bool(west[y*32+x]));if(y==26)contacts+=bool(east[y*32+x]);}
   assert(contacts>=2);assert(!art::animal_hit(e,-1,0)&&!art::animal_hit(e,32,27)&&!art::animal_hit(e,1,28));
   for(int x=0;x<32;++x)assert(!east[27*32+x]);
   Pixels tinted{};art::Raster tr{tinted.data(),32,28,0,0,tint};art::animal_pixels(tr,e,16,27);
   for(int i=0;i<32*28;++i)assert(tinted[i]==(east[i]?tint[east[i]]:0));
   for(auto &v:shadow)v=0x7bef;art::animal_shadow(sr,16,27,a,1,frame);
   for(int y=0;y<28;++y)for(int x=0;x<32;++x)if(shadow[y*32+x]!=0x7bef){assert(y==27&&east[26*32+x]);}
   if(!frame)idle=east;else if(frame<=8)walking.insert(east);else {
    eating.insert(east);
    // Rear support stays put; the lowered muzzle can occlude a front foot.
    int width=kind<=2?(age==0?9:age==1?13:17):(age==0?15:age==1?22:28);
    int left=(32-width)/2-(kind<=2),foot=left+(kind<=2?width/3:5);
    for(int y=25;y<=26;++y)for(int x=foot;x<=foot+1;++x)assert(east[y*32+x]==idle[y*32+x]);
   }
   // Partial offscreen spans and tint must agree with a full unclipped draw.
   for(auto offset:{std::pair<int,int>{-10,-15},{27,20},{-25,26}}){
    std::array<uint16_t,12*12+2> clipped{};clipped.front()=clipped.back()=0x1234;art::Raster cr{clipped.data()+1,12,12};
    art::animal_pixels(cr,e,offset.first+16,offset.second+27);
    for(int y=0;y<12;++y)for(int x=0;x<12;++x){int sx=x-offset.first,sy=y-offset.second;assert(clipped[1+y*12+x]==(sx>=0&&sx<32&&sy>=0&&sy<28?east[sy*32+sx]:0));}
    assert(clipped.front()==0x1234&&clipped.back()==0x1234);
   }
  }
  assert(walking.size()>=6&&eating.size()>=3);
 }
 farm::State s;farm::fresh(s);s.minute=0;
 for(int id=0;id<farm::AnimalMax;++id){
  auto prev=farm::animal_pose(s,id,0);std::set<int> walk,eat;
  for(int ms=1;ms<=32000;++ms){auto p=farm::animal_pose(s,id,ms);assert(std::abs(p.x-prev.x)<=1&&p.y==prev.y&&p.x/16==10+id%6*2);if(p.frame>=9){eat.insert(p.frame);if(prev.frame>=9)assert(p.x==prev.x);}else if(p.frame)walk.insert(p.frame);prev=p;}
  assert(walk.size()==8&&eat.size()==6);
 }
 std::printf("PASS: 450 bank selections, mirrored pixel picking, fixed eating soles, alternating gait, continuous pacing, clipping and %u-byte shared bank\n",StorageBytes);
}
