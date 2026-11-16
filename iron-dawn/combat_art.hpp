// SPDX-License-Identifier: MIT
#pragma once
#include "sprites.hpp"
#include "model.hpp"
namespace iron {
struct CombatArt {
 static unsigned facing(float angle){return unsigned(int(std::lround(angle*4/3.14159265f)))&7;}
 struct Leg {Pt hip,knee,ankle,toe;float travel,lift;};
 struct Gait {std::array<Leg,2> legs;Pt forward,side;};
 static Gait gait(int kind,unsigned direction,float phase,float weight){
  const auto& frame=art::frames[art::unit_first[kind]+direction];const auto& rig=art::infantry_rigs[kind][direction];
  float angle=direction*3.14159265f/4,c=std::cos(angle),s=std::sin(angle);
  Pt forward{c-s,(c+s)*.5f},side{-s-c,(c-s)*.5f};float fl=std::hypot(forward.x,forward.y),sl=std::hypot(side.x,side.y);forward={forward.x/fl,forward.y/fl};side={side.x/sl,side.y/sl};
  float sample=std::floor(phase*8)/8;weight=std::clamp(weight,0.f,1.f);Gait result{{},forward,side};float reach=.72f*20*fl/4;
  for(int j=0;j<2;++j){float sign=j?-1.f:1.f,p=std::fmod(sample+j*.5f,1.f),travel,lift;
   if(p<.5f){travel=reach*(1-4*p)*weight;lift=0;}else{float u=(p-.5f)*2;travel=reach*(2*u*u*(3-2*u)-1)*weight;lift=std::sin(u*3.14159265f)*2.2f*weight;}
   Pt hip{float(rig.hip_x-frame.ox)+side.x*2*sign,float(rig.hip_y-frame.oy)+side.y*2*sign};
   Pt ankle{side.x*2.5f*sign+forward.x*travel,side.y*2.5f*sign+forward.y*travel-1.5f-lift};
   Pt knee{(hip.x+ankle.x)*.5f+forward.x*(.5f+lift*.5f),(hip.y+ankle.y)*.5f+forward.y*(.5f+lift*.5f)};
   Pt toe{ankle.x+forward.x*1.8f,ankle.y+forward.y*1.8f};result.legs[j]={hip,knee,ankle,toe,travel,lift};
  }return result;
 }
 [[gnu::noinline]] static void infantry(Raster& r,int kind,unsigned direction,int x,int y,float zoom,float phase,float weight,unsigned variant){
  auto g=gait(kind,direction,phase,weight);auto color=[&](uint16_t c){return variant==2||variant==3?mix(c,cream,5):c;};
  auto point=[&](Pt p){return Pt{x+std::round(p.x*zoom),y+std::round(p.y*zoom)};};
  int near=g.legs[0].hip.y>=g.legs[1].hip.y?0:1;
  for(int j:{1-near,near}){auto leg=g.legs[j];auto h=point(leg.hip),k=point(leg.knee),a=point(leg.ankle),t=point(leg.toe);int thick=std::max(1,int(std::round(5*zoom))),inner=std::max(1,int(std::round(3*zoom)));
   uint16_t ink=color(rgb(25,30,26)),armor=color(j==near?rgb(91,102,63):rgb(65,77,49)),lit=color(j==near?rgb(143,150,99):rgb(100,114,71));
   r.line(h.x,h.y,k.x,k.y,ink,thick);r.line(k.x,k.y,a.x,a.y,ink,thick-std::max(0,int(zoom)));
   r.line(h.x,h.y,k.x,k.y,armor,inner);r.line(h.x-zoom,h.y+zoom,(h.x+k.x)*.5f-zoom,(h.y+k.y)*.5f,lit,std::max(1,int(zoom)));r.line(k.x,k.y,a.x,a.y,color(rgb(61,71,47)),std::max(1,inner-int(zoom)));
   r.rect(k.x-zoom,k.y-zoom,std::max(1,int(2*zoom)),std::max(1,int(2*zoom)),lit);
   r.line(a.x,a.y,t.x,t.y,ink,thick);r.line(a.x,a.y,t.x,t.y,color(rgb(62,60,47)),inner);r.line(a.x,a.y-zoom,t.x,t.y-zoom,color(rgb(102,103,80)),std::max(1,int(zoom)));
  }
  unsigned id=art::unit_first[kind]+direction;const auto& f=art::frames[id];const auto& rig=art::infantry_rigs[kind][direction];auto old=r.clip;int bottom=y-int(std::floor(f.oy*zoom))+int(std::floor(rig.cut*zoom));int end=std::min(old.y+old.h,bottom);r.clip.h=std::max(0,end-old.y);Sprites::anchor(r,id,x,y,zoom,variant);r.clip=old;
 }
 static Pt socket(unsigned kind,unsigned direction,int x,int y,float zoom){
  kind=std::min(kind,3u);const auto& f=art::frames[art::unit_first[kind]+direction];auto s=art::muzzles[kind][direction];
  return {float(x-int(std::floor(f.ox*zoom))+int(std::floor(s.x*zoom))),float(y-int(std::floor(f.oy*zoom))+int(std::floor(s.y*zoom)))};
 }
 static Pt shell_axis(unsigned direction){constexpr std::array<Pt,8> a={Pt{.88f,-.47f},Pt{0,-1},Pt{-.88f,-.47f},Pt{-.98f,-.15f},Pt{-.82f,-.58f},Pt{0,-1},Pt{.82f,-.58f},Pt{.98f,-.15f}};return a[direction&7];}
 static Pt trajectory(Pt start,Pt end,float t,bool shell,float zoom,unsigned direction){
  t=std::clamp(t,0.f,1.f);if(!shell)return {start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t};
  auto axis=shell_axis(direction);float length=std::min(30.f*zoom,std::hypot(end.x-start.x,end.y-start.y)*.3f);Pt c1{start.x+axis.x*length,start.y+axis.y*length},c2{end.x-(end.x-start.x)*.2f,end.y-55*zoom};float u=1-t;
  return {u*u*u*start.x+3*u*u*t*c1.x+3*u*t*t*c2.x+t*t*t*end.x,u*u*u*start.y+3*u*u*t*c1.y+3*u*t*t*c2.y+t*t*t*end.y};
 }
};
}
