// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"

namespace iron {
// Transient input state stays outside World, preserving the published v4 save
// layout. Only release commits selection; cancellation leaves the army intact.
struct SelectionTool {
 bool armed=false,tracking=false,dragged=false;
 int x0=0,y0=0,x1=0,y1=0;
 Rect viewport{},map{},toggle{};

 void cancel(){armed=false;tracking=false;dragged=false;}
 bool begin(int x,int y,Rect field,Rect minimap,Rect button,bool ui,bool placing){
  tracking=false;dragged=false;
  if(!armed||ui||placing||!field.has(x,y)||minimap.has(x,y)||button.has(x,y))return false;
  viewport=field;map=minimap;toggle=button;x0=x1=x;y0=y1=y;tracking=true;return true;
 }
 void move(int x,int y){
  if(!tracking)return;
  // A finger may leave the battlefield; the box never consumes HUD pixels.
  x1=std::clamp(x,viewport.x,viewport.x+viewport.w-1);
  y1=std::clamp(y,viewport.y,viewport.y+viewport.h-1);
  dragged|=std::abs(x1-x0)+std::abs(y1-y0)>7;
 }
 Rect bounds()const{return {std::min(x0,x1),std::min(y0,y1),std::abs(x1-x0)+1,std::abs(y1-y0)+1};}
 bool visible(const Entity& a,const Battlefield& f)const{
  if(a.hp<=0||a.team!=0||!combat_kind(a.kind))return false;
  // Use the visible body rather than the ground point beneath a tank's hull.
  auto p=f.project(a.p,a.kind==rotorcraft?craft_altitude+14:8);
  return viewport.has(int(p.x),int(p.y))&&!map.has(int(p.x),int(p.y))&&!toggle.has(int(p.x),int(p.y));
 }
 bool contains(const Entity& a,const Battlefield& f)const{
  if(!visible(a,f))return false;
  auto p=f.project(a.p,a.kind==rotorcraft?craft_altitude+14:8);return bounds().has(int(p.x),int(p.y));
 }
 int preview(const World& w,const Battlefield& f)const{
  int n=0;for(auto& a:w.e)n+=contains(a,f);return n;
 }
 int commit(World& w,const Battlefield& f){
  if(!tracking)return -1;
  if(dragged){for(auto& a:w.e)a.selected=contains(a,f);}
  else {
   // A tap in box mode selects the nearest combat unit, and never issues an
   // accidental move or attack command when the finger barely moved.
   int nearest=-1;float best=18.f*18.f;
   for(int i=0;i<MAX;++i)if(visible(w.e[i],f)){
    auto p=f.project(w.e[i].p,w.e[i].kind==rotorcraft?craft_altitude+14:8);float d=(p.x-x1)*(p.x-x1)+(p.y-y1)*(p.y-y1);
    if(d<best){nearest=i;best=d;}
   }
   for(int i=0;i<MAX;++i)w.e[i].selected=i==nearest;
  }
  int n=w.selected();cancel();return n;
 }
 void draw(Raster& r,const World& w,const Battlefield& f)const{
  if(!tracking||!dragged)return;
  auto old=r.clip;r.clip=viewport;auto box=bounds();
  r.shade(box,cyan,2);
  r.rect(box.x,box.y,box.w,1,cyan);r.rect(box.x,box.y+box.h-1,box.w,1,cyan);
  r.rect(box.x,box.y,1,box.h,cyan);r.rect(box.x+box.w-1,box.y,1,box.h,cyan);
  int arm=std::min({8,box.w,box.h});
  for(int side=0;side<2;++side)for(int bottom=0;bottom<2;++bottom){
   int x=side?box.x+box.w-arm:box.x,y=bottom?box.y+box.h-2:box.y;
   r.rect(x,y,arm,2,cream);x=side?box.x+box.w-2:box.x;y=bottom?box.y+box.h-arm:box.y;r.rect(x,y,2,arm,cream);
  }
  for(auto& a:w.e)if(contains(a,f)){auto p=f.project(a.p);r.ring(p.x,p.y+1,12*f.zoom,5*f.zoom,gold);}
  char count[48];std::snprintf(count,sizeof count,"%d 单位",preview(w,f));
  int label_x=std::clamp(box.x,viewport.x+4,viewport.x+viewport.w-110);
  int label_y=std::clamp(box.y-26,viewport.y+33,viewport.y+viewport.h-25);
  Rect label{label_x,label_y,104,23};if(label.x+label.w>map.x&&label.y<toggle.y+toggle.h)label.x=map.x-label.w-5;
  r.box(label,navy,cyan);r.label(label,count,cyan,13);
  r.clip=old;
 }
};
}
