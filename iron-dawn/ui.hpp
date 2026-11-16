// SPDX-License-Identifier: MIT
#pragma once
#include "sprites.hpp"
namespace iron {
enum class UiIcon {none,pause,stop,repair,air,zoom,cancel,units,buildings,all,base,box,lock,ore,power,sound,star};
struct Skin {
 static void panel_frame(Raster& r,Rect a,uint16_t color=panel,uint16_t rim=rgb(79,121,112)){
  r.box(a,color,rim);r.rect(a.x+2,a.y+2,a.w-4,2,mix(color,cream,4));r.rect(a.x+2,a.y+a.h-4,a.w-4,2,mix(color,navy,8));r.rect(a.x+2,a.y+1,a.w-4,1,mix(color,cream,2));r.rect(a.x+2,a.y+a.h-2,a.w-4,1,mix(color,navy,7));
  r.rect(a.x,a.y,3,3,navy);r.line(a.x+3,a.y,a.x,a.y+3,rim);r.rect(a.x+a.w-3,a.y+a.h-3,3,3,navy);r.line(a.x+a.w-4,a.y+a.h-1,a.x+a.w-1,a.y+a.h-4,rim);
 }
 static void progress(Raster& r,Rect a,float value,uint16_t color){
  r.rect(a.x,a.y,a.w,a.h,edge);int filled=std::clamp(int(a.w*value),0,a.w);for(int x=0;x<a.w;x+=8){int n=std::min({6,a.w-x,filled-x});if(n>0)r.rect(a.x+x,a.y,n,a.h,color);}
 }
 static void button(Raster& r,Rect a,bool primary,bool held,bool enabled){
  uint16_t color=!enabled?rgb(19,32,36):held?rgb(52,83,77):primary?rgb(36,69,65):rgb(28,44,48);
  panel_frame(r,a,color,primary&&enabled?cyan:edge);
  if(!held&&enabled)r.rect(a.x+2,a.y+2,a.w-4,std::max(1,a.h/3-1),mix(color,cream,1));
  if(primary){r.rect(a.x+1,a.y+2,2,a.h-4,enabled?cyan:edge);r.rect(a.x+a.w-7,a.y+a.h-3,4,1,enabled?cyan:edge);}
 }
 static void icon(Raster& r,UiIcon icon,int x,int y,uint16_t c,int size=12){if(icon==UiIcon::none)return;Sprites::fit(r,art::icons_first+unsigned(icon)-1,{x,y,size,size},c==muted||c==edge?4:0);}
 static uint16_t class_color(int kind){return kind==1||kind==3?gold:kind==4?rgb(205,172,98):cyan;}
};
}
