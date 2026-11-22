// SPDX-License-Identifier: MIT
#pragma once
#include "raster.hpp"
namespace iron {
inline Rect page_column(int width,int height,int gutter=24,int maximum=760){
 int breadth=std::min(maximum,width-2*gutter);
 return {(width-breadth)/2,0,breadth,height};
}
struct HudLayout {
 int top,card_height,icon_height,commands;
 explicit HudLayout(int height):top(height-(height<400?138:166)),card_height(height<400?80:108),icon_height(height<400?31:59),commands(top+card_height+14){}
 Rect card(int index)const{return {72+index*65,top+8,62,card_height};}
 Rect image(int index)const{auto a=card(index);return {a.x+3,a.y+4,a.w-6,icon_height};}
 Rect name(int index)const{auto a=card(index);return {a.x+2,a.y+icon_height+7,a.w-4,20};}
 Rect price(int index)const{auto a=card(index);return {a.x+2,a.y+icon_height+28,a.w-4,14};}
 Rect progress(int index)const{auto a=card(index);return {a.x+3,a.y+a.h-5,a.w-6,3};}
};
enum class DockPanel {closed,units,buildings,tactics,squads};
struct GameViewport {
 int width,height;
 static GameViewport fit(int pixels_w,int pixels_h){int width=std::clamp(int(int64_t(pixels_w)*320/std::max(1,pixels_h)),480,1060);return {width,std::clamp(int(int64_t(pixels_h)*width/std::max(1,pixels_w)),320,1060)};}
};
// The Host presents smaller mapped frames at exact 2x/4x nearest-neighbor
// scales. Bound both Guest buffers to 2 MiB even on desktop-sized windows.
struct DisplayPlan {
 int width,height,scale,offset_x,offset_y;bool wide;float units_per_dp,font_scale;
 static DisplayPlan fit(int pw,int ph,uint32_t density=65536,uint32_t font=65536){
  float dpi=std::clamp(density/65536.f,.5f,4.f);int scale=dpi>=1.25f&&pw>=768&&ph>=480?2:1;while(scale<4&&int64_t(((pw/scale)*2+63)/64*64)*(ph/scale)*2>2097152)scale*=2;
  int sw=std::max(1,pw/scale),sh=std::max(1,ph/scale);bool wide=pw>=ph&&pw/dpi>=800&&ph/dpi>=440&&sw>=512&&sh>=300;
  return {sw,sh,scale,(pw-sw*scale)/2,(ph-sh*scale)/2,wide,dpi/scale,std::clamp(font/65536.f,.75f,2.f)};
 }
 // Compensate for the game's virtual viewport before choosing an integer
 // glyph strike. The resulting physical text size follows display density.
 float text_scale(int canvas_width)const{return (wide?units_per_dp:units_per_dp*scale>=1.25f?std::max(1.f,units_per_dp*canvas_width/width):1.f)*font_scale;}
 int minimum_text_scale()const{return font_style(std::lround(12*units_per_dp),false).scale;}
};
struct LargeHudLayout {
 int width,height,row,line,side,world_width,top,bottom,map_size,tabs,grid,card_height,footer;
 LargeHudLayout(int w,int h,float dp,float font=1):width(w),height(h){
  line=12*font_style(std::lround(12*dp*font),false).scale+4;
  row=std::max({24,int(std::ceil(40*dp)),line+8});
  side=std::clamp(std::max(w/4,int(std::lround(224*dp))),168,std::min(336,w/3+12));
  world_width=w-side;top=std::max(36,row+8);bottom=h-row-10;footer=h-row-8;
  int map_budget=h-top-2*row-56-3*(2*line+4);
  int map_width=side-std::max(68,3*(line-4)+12)-32;
  int preferred=std::max(h/5,int(std::lround(96*dp)));
  map_size=std::clamp(std::min({preferred,map_budget,map_width}),32,160);
  tabs=top+std::max(map_size,row)+20;grid=tabs+row+8;
  int preferred_card=2*line+std::clamp(h/14,24,56)+12;
  card_height=std::min((footer-8-grid-12)/3,preferred_card);
 }
 Rect sidebar()const{return {world_width,top,side,height-top};}
 Rect map()const{return {world_width+12,top+12,map_size,map_size};}
 Rect selection_info()const{int x=map().x+map_size+10;return {x,map().y,width-x-10,tabs-map().y-8};}
 Rect squad_button()const{auto info=selection_info();return {info.x,info.y+info.h-row,info.w,row};}
 Rect tab(int n)const{return {world_width+8+n*((side-20)/2+4),tabs,(side-20)/2,row};}
 Rect card(int n)const{return {world_width+8+(n%2)*((side-22)/2+6),grid+(n/2)*(card_height+6),(side-22)/2,card_height};}
 Rect command(int n)const{int gap=6,cw=(world_width-16-gap*5)/6;return {8+n*(cw+gap),bottom+5,cw,row};}
 Rect foot(int n)const{return {world_width+8+n*((side-20)/2+4),footer,(side-20)/2,row};}
 Rect status()const{int y=grid+3*(card_height+6)+2;return {world_width+8,y,side-16,std::max(0,footer-y-8)};}
};
struct DockState {
 DockPanel panel=DockPanel::closed;
 void close(){panel=DockPanel::closed;}
 void toggle(DockPanel next){panel=panel==next?DockPanel::closed:next;}
 bool open()const{return panel!=DockPanel::closed;}
};
// Small displays keep one touchable command row. Drawers overlay the world;
// they never change its projection or enter the save format.
struct CompactHudLayout {
 int height,width,row,tactic_row,line,picture,card_height,header,top,status_height;
 static bool applies(int physical_width,int physical_height){return physical_width<400||physical_height<=480||physical_width>=physical_height;}
 explicit CompactHudLayout(int canvas_height,int physical_height,int canvas_width=480,float density=1,float font=1):height(canvas_height),width(canvas_width){
  bool readable=density>=1.25f;int glyph=12*font_style(std::lround(12*density*font),false).scale;
  auto logical=[&](float pixels){return int(std::ceil(pixels*height/std::max(1,physical_height)));};
  int line_min=std::min(20,height/16);
  line=std::clamp(logical(readable?glyph+4:14*font),line_min,std::max(line_min,height/11));
  row=std::min(std::max(line+8,logical(std::max(36.f,30*density))),height/5);
  header=line+8;top=height-row-10;
  int status_min=std::min(50,(height*50+319)/320);
  status_height=readable?std::clamp(2*line+8,status_min,height/4):50;
  int drawer_budget=top-status_height-18;
  tactic_row=std::clamp(logical(std::max(32.f,24*density)),24,std::max(24,std::min(row,(drawer_budget-header-18)/2)));
  picture=std::clamp(logical(readable?26*density:22.f),24,std::max(24,drawer_budget-header-24-line*2));
  card_height=12+picture+line*2;
 }
 Rect column(int index,int count,int y,int h)const{
  int space=width-16+6,left=8+space*index/count,right=8+space*(index+1)/count-6;
  return {left,y,right-left,h};
 }
 Rect command(int index)const{return column(index,5,top+5,row);}
 Rect overlay(DockPanel panel)const{int size=panel==DockPanel::closed?0:header+12+((panel==DockPanel::tactics||panel==DockPanel::squads)?tactic_row*2+6:card_height);return {0,top-size,width,size};}
 Rect card(int index,DockPanel panel)const{return column(index,5,overlay(panel).y+header+4,card_height);}
 Rect image(int index,DockPanel panel)const{auto a=card(index,panel);return {a.x+4,a.y+4,a.w-8,picture};}
 Rect name(int index,DockPanel panel)const{auto a=card(index,panel);return {a.x+2,a.y+picture+6,a.w-4,line};}
 Rect price(int index,DockPanel panel)const{auto a=name(index,panel);return {a.x,a.y+line,a.w,line};}
 Rect progress(int index,DockPanel panel)const{auto a=card(index,panel);return {a.x+4,a.y+a.h-4,a.w-8,2};}
 Rect tactic(int index,DockPanel panel=DockPanel::tactics)const{return column(index%4,4,overlay(panel).y+header+4+(index/4)*(tactic_row+6),tactic_row);}
 Rect map()const{int size=std::clamp(std::min(width,height)/5,std::min(80,height/4),112);return {width-size-10,status_height+8,size,size};}
 Rect selection()const{auto m=map();return {m.x,m.y+m.h+8,m.w,std::min(row,40)};}
};
}
