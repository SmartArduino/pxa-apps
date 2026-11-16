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
}
