// SPDX-License-Identifier: MIT
#pragma once
#include "raster.hpp"
namespace iron {
struct HudLayout {
 int top,card_height,icon_height,commands;
 explicit HudLayout(int height):top(height-(height<400?138:166)),card_height(height<400?80:108),icon_height(height<400?31:59),commands(top+card_height+14){}
 Rect card(int index)const{return {72+index*65,top+8,62,card_height};}
 Rect image(int index)const{auto a=card(index);return {a.x+3,a.y+4,a.w-6,icon_height};}
 Rect name(int index)const{auto a=card(index);return {a.x+2,a.y+icon_height+7,a.w-4,20};}
 Rect price(int index)const{auto a=card(index);return {a.x+2,a.y+icon_height+28,a.w-4,14};}
 Rect progress(int index)const{auto a=card(index);return {a.x+3,a.y+a.h-5,a.w-6,3};}
};
enum class DockPanel {closed,units,buildings,tactics};
struct GameViewport {
 int width,height;
 static GameViewport fit(int pixels_w,int pixels_h){int width=std::clamp(int(int64_t(pixels_w)*320/std::max(1,pixels_h)),480,1060);return {width,std::clamp(int(int64_t(pixels_h)*width/std::max(1,pixels_w)),320,1060)};}
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
 int height,width,offset,row,line,picture,card_height,header,top;
 static bool applies(int physical_width,int physical_height){return physical_width<400||physical_height<=480||physical_width>=physical_height;}
 explicit CompactHudLayout(int canvas_height,int physical_height,int canvas_width=480):height(canvas_height),width(canvas_width),offset((width-480)/2),row(std::max(28,(30*canvas_height+physical_height-1)/physical_height)),line(std::max(20,(14*canvas_height+physical_height-1)/physical_height)),picture(std::max(30,(20*canvas_height+physical_height-1)/physical_height)),card_height(12+picture+line*2),header(line+8),top(height-row-10){}
 Rect command(int index)const{return {offset+8+94*index,top+5,88,row};}
 Rect overlay(DockPanel panel)const{int size=panel==DockPanel::closed?0:header+12+(panel==DockPanel::tactics?row*2+6:card_height);return {0,top-size,width,size};}
 Rect card(int index,DockPanel panel)const{return {offset+8+94*index,overlay(panel).y+header+4,88,card_height};}
 Rect image(int index,DockPanel panel)const{auto a=card(index,panel);return {a.x+4,a.y+4,a.w-8,picture};}
 Rect name(int index,DockPanel panel)const{auto a=card(index,panel);return {a.x+2,a.y+picture+6,a.w-4,line};}
 Rect price(int index,DockPanel panel)const{auto a=name(index,panel);return {a.x,a.y+line,a.w,line};}
 Rect progress(int index,DockPanel panel)const{auto a=card(index,panel);return {a.x+4,a.y+a.h-4,a.w-8,2};}
 Rect tactic(int index)const{return {offset+8+118*(index%4),overlay(DockPanel::tactics).y+header+4+(index/4)*(row+6),110,row};}
};
}
