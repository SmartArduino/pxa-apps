// SPDX-License-Identifier: MIT
#pragma once
#include "raster.hpp"
namespace iron {
// Wrap at complete UTF-8 glyphs using the physical font grid. A small screen
// gets additional pages instead of clipping or shrinking story dialogue.
struct StoryText {
 std::array<std::string_view,96> lines{};unsigned count=0;
 StoryText(const Raster& r,const std::array<const char*,3>& paragraphs,int width){for(auto text:paragraphs){std::string_view rest=text;while(!rest.empty()&&count<lines.size()){size_t fit=0;auto scan=rest;while(!scan.empty()){Raster::next(scan);size_t end=rest.size()-scan.size();if(fit&&r.measure(rest.substr(0,end),12)>width)break;fit=end;}lines[count++]=rest.substr(0,fit);rest.remove_prefix(fit);}}}
 int row(const Raster& r)const{return r.text_height("中",12)+4;}
 unsigned capacity(const Raster& r,Rect area)const{return std::max(1,area.h/row(r));}
 unsigned pages(const Raster& r,Rect area)const{return std::max(1u,(count+capacity(r,area)-1)/capacity(r,area));}
 void draw(Raster& r,Rect area,unsigned page)const{unsigned rows=capacity(r,area),first=std::min(page,pages(r,area)-1)*rows;for(unsigned i=first;i<std::min(count,first+rows);++i)r.text(area.x,area.y+(i-first)*row(r),lines[i],cream,12);}
};
}
