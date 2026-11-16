// SPDX-License-Identifier: MIT
#include "sprites.hpp"
#include "hud_layout.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
int main(){
 constexpr int w=320,h=220;std::vector<uint16_t> actual(w*h+2),expected(w*h+2);unsigned checks=0;std::vector<uint32_t> hashes;
 for(unsigned id=0;id<art::frames.size();++id){auto& f=art::frames[id];unsigned step=f.w>255?4:3;std::vector<uint32_t> native(f.w*f.h,0);auto data=art::bytes.data()+f.offset;
  for(unsigned i=0;i<f.count;++i){assert(data+step<=art::bytes.data()+f.end);unsigned x=data[0]+(step==4?(unsigned(data[1])<<8):0),y=data[step-2],length=data[step-1];data+=step;assert(length&&x+length<=f.w&&y<f.h&&data+length<=art::bytes.data()+f.end);for(unsigned n=0;n<length;++n){assert(data[n]<128);native[y*f.w+x+n]=256+data[n];}data+=length;}assert(data==art::bytes.data()+f.end);uint32_t digest=2166136261u;for(auto pixel:native)digest=(digest^pixel)*16777619u;hashes.push_back(digest);
  for(float scale:{.45f,.72f,1.f,1.3f})for(int edge=0;edge<2;++edge){std::fill(actual.begin(),actual.end(),navy);std::fill(expected.begin(),expected.end(),navy);actual.front()=actual.back()=expected.front()=expected.back()=0xa55a;Raster a(actual.data()+1,w,h,w),b(expected.data()+1,w,h,w);int x=edge?-7:13,y=edge?-11:19;unsigned variant=(id+edge)%5;Sprites::blit(a,id,x,y,scale,scale,variant);
   for(unsigned yy=0;yy<f.h;++yy)for(unsigned xx=0;xx<f.w;++xx)if(native[yy*f.w+xx]){int left=x+int(std::floor(xx*scale)),right=x+int(std::floor((xx+1)*scale)),top=y+int(std::floor(yy*scale)),bottom=y+int(std::floor((yy+1)*scale));b.rect(left,top,right-left,bottom-top,art::palettes[f.palette][variant][native[yy*f.w+xx]-256]);}
   assert(actual==expected);++checks;
  }
 }
 for(int height:{320,360,389,480,854,1060}){HudLayout layout(height);assert(layout.top>50&&layout.commands+24<=height-20);for(int i=0;i<5;++i){auto card=layout.card(i);auto name=layout.name(i),price=layout.price(i),picture=layout.image(i);assert(card.x>=0&&card.x+card.w<404&&card.y>=layout.top&&card.y+card.h<layout.commands);assert(picture.y+picture.h<=name.y&&name.y+name.h<=price.y&&price.y+price.h<=card.y+card.h);}}
 for(unsigned kind=0;kind<5;++kind)for(unsigned facing=0;facing<8;++facing){auto first=art::frames[art::unit_first[kind]+facing],second=art::frames[art::unit_first[kind]+facing+8];assert(first.count&&second.count&&hashes[art::unit_first[kind]+facing]!=hashes[art::unit_first[kind]+facing+8]);}
 std::printf("%u sprite scale/clipping/palette checks, all frame records and six HUD sizes passed\n",checks);
}
