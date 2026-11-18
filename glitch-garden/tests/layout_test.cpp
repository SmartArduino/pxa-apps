// SPDX-License-Identifier: MIT
#include "../layout.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace garden;
 assert(logical_inset(12,480.f/296)==8);assert(logical_inset(24,480.f/296)==15);
 for(float scale:{1.f,480.f/296,2.f,2.88378f}){Layout l;l.bx=20;l.by=86;l.cell=29;
  for(int row=0;row<rows;++row)for(int col=0;col<columns;++col){auto tile=l.tile(row,col);
   int left=std::lround(tile.x*scale),right=std::lround((tile.x+tile.w)*scale),top=std::lround(tile.y*scale),bottom=std::lround((tile.y+tile.h)*scale);
   for(int y=top;y<bottom;++y)for(int x=left;x<right;++x){auto hit=l.hit(x,y,scale);assert(hit&&hit->first==row&&hit->second==col);}
  }
  auto field=l.field();assert(!l.hit(std::lround(field.x*scale)-1,std::lround(field.y*scale),scale));assert(!l.hit(std::lround((field.x+field.w)*scale),std::lround(field.y*scale),scale));
 }
 Layout l;assert(l.scroll_seeds());assert(l.seed_visible()==3);l.w=600;assert(!l.scroll_seeds());assert(l.seed_visible()==plant_count);l.w=240;assert(l.scroll_seeds());assert(l.seed_visible()==2);
 for(int width:{200,224,240,280,296,600}){l.w=width;auto wave=l.wave();auto art=garden_art::frames[garden_art::ui_wave];assert(wave.w*art.h==wave.h*art.w);assert(wave.x>=l.x&&wave.x+wave.w<=l.x+l.w);
  if(l.scroll_seeds()){auto left=l.seed_arrow(false),right=l.seed_arrow(true);assert(left.w>=26&&right.w>=26);auto first=l.card(0),last=l.card(l.seed_visible()-1);assert(left.x+left.w<=first.x);assert(last.x+last.w<=right.x);}
 }
 std::cout<<"layout: every pixel maps to its drawn tile; fractional scales, boundaries and seed overflow passed\n";
}
