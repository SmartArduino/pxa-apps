// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "filtered_art.hpp"
#include <cmath>
#include <optional>
#include <utility>
namespace garden {
inline int logical_inset(uint32_t surface_pixels,float zoom){return int(std::ceil(surface_pixels/zoom));}
struct Layout {
 int x=0,y=0,w=296,h=240,cell=28,bx=22,by=50,seed_offset=0;
 Rect field()const{return {bx,by,cell*columns,cell*rows};}
 Rect tile(int row,int col)const{return {bx+col*cell,by+row*cell,cell,cell};}
 std::optional<std::pair<int,int>> hit(int px,int py,float zoom)const{
  for(int row=0;row<rows;++row)for(int col=0;col<columns;++col){auto r=tile(row,col);int left=std::lround(r.x*zoom),top=std::lround(r.y*zoom),right=std::lround((r.x+r.w)*zoom),bottom=std::lround((r.y+r.h)*zoom);if(px>=left&&px<right&&py>=top&&py<bottom)return std::pair{row,col};}return std::nullopt;
 }
 int seed_left()const{return x+(w<260?40:48)+3;}
 int seed_width()const{return x+w-32-seed_left();}
 bool scroll_seeds()const{return plant_count*43+(plant_count-1)*3>seed_width();}
 int seed_visible()const{return scroll_seeds()?std::clamp((seed_width()-56+3)/46,1,plant_count):plant_count;}
 Rect seed_arrow(bool right)const{return {right?seed_left()+seed_width()-27:seed_left(),y+13,26,28};}
 Rect tray()const{int width=scroll_seeds()?seed_width():plant_count*46-3;return {seed_left()-5,y,width+10,53};}
 Rect card(int i)const{if(i==plant_count)return {x+3,y+h-31,39,29};int visible=seed_visible();if(i<seed_offset||i>=seed_offset+visible)return {};
  int used=visible*43+(visible-1)*3;int start=seed_left()+(scroll_seeds()?28+(seed_width()-56-used)/2:0);return {start+(i-seed_offset)*46,y+2,43,48};}
 // Include the unused gutter beside the edge control. Pointer canvas units
 // may cover several Surface pixels on a dense display.
 Rect card_hit(int i)const{auto r=card(i);if(i==plant_count){r.w+=r.x-x;r.x=x;}return r;}
 Rect start()const{return {x+w/2-77,y+h-61,154,43};}
 Rect wave()const{return {x+w-90,y+h-34,86,32};}
};
}
