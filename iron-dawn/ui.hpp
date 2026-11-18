// SPDX-License-Identifier: MIT
#pragma once
#include "sprites.hpp"
#include "skin_tiles.hpp"
#include "mission_skin.hpp"
#include "turret_art.hpp"
namespace iron {
enum class UiIcon {none,pause,stop,repair,air,zoom,cancel,units,buildings,all,base,box,lock,ore,power,sound,star};
struct Skin {
 static void picture(Raster& r,int kind,Rect area,unsigned variant=0){if(kind==10)TurretArt::fit(r,area,variant);else Sprites::fit(r,kind>=5?art::buildings_first+kind-5:Sprites::unit_frame(kind,1.5707963f,kind==4),area,variant);}
 static void panel_frame(Raster& r,Rect a,uint16_t color=panel,uint16_t rim=rgb(79,121,112)){
  (void)color;SkinTiles::draw(r,a,0);if(rim==cyan){r.rect(a.x+2,a.y+2,2,std::max(0,a.h-4),cyan);r.rect(a.x+3,a.y+2,std::min(16,std::max(0,a.w-6)),1,cyan);}
 }
 static void card_frame(Raster& r,Rect a,bool active){SkinTiles::draw(r,a,5);if(active){r.rect(a.x+2,a.y+2,2,std::max(0,a.h-4),cyan);r.rect(a.x+4,a.y+2,std::max(0,a.w-8),1,cyan);}}
 static void well(Raster& r,Rect a){SkinTiles::draw(r,a,4);}
 static void progress(Raster& r,Rect a,float value,uint16_t color){
  r.rect(a.x,a.y,a.w,a.h,edge);int filled=std::clamp(int(a.w*value),0,a.w);for(int x=0;x<a.w;x+=8){int n=std::min({6,a.w-x,filled-x});if(n>0)r.rect(a.x+x,a.y,n,a.h,color);}
 }
 static void button(Raster& r,Rect a,bool primary,bool held,bool enabled){
  SkinTiles::draw(r,a,!enabled?6:held?3:primary?2:1);
 }
 static void icon(Raster& r,UiIcon icon,int x,int y,uint16_t c,int size=12){if(icon==UiIcon::none)return;Sprites::fit(r,art::icons_first+unsigned(icon)-1,{x,y,size,size},c==muted||c==edge?4:0);}
 static uint16_t class_color(int kind){return kind==1||kind==3?gold:kind==4?rgb(205,172,98):cyan;}
};
}
