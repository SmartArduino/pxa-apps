// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/game.hpp>
#include <string_view>
#include "font.hpp"
namespace gravity {
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
constexpr uint16_t cream=rgb(255,249,237),ink=rgb(42,60,67),muted=rgb(111,128,130),
  teal=rgb(42,147,130),mint=rgb(220,243,229),pink=rgb(255,225,218),coral=rgb(230,111,99),
  lilac=rgb(236,228,253),purple=rgb(129,103,180),gold=rgb(249,191,69),white=rgb(255,255,255);
struct Paint {
  pxa::game::Frame &frame;bool alpha;int width,height;
  void rect(int x,int y,int w,int h,uint16_t c){if(w<=0||h<=0)return;
    frame.quad({int16_t(x*16),int16_t(y*16),int16_t((x+w)*16),int16_t(y*16),
      int16_t((x+w)*16),int16_t((y+h)*16),int16_t(x*16),int16_t((y+h)*16)},{c});}
  void circle(int x,int y,int radius,uint16_t c){for(int yy=-radius;yy<radius;++yy){int dx=int(std::sqrt(float(radius*radius-yy*yy)));rect(x-dx,y+yy,2*dx,1,c);}}
  void round(int x,int y,int w,int h,uint16_t c,int r=8){r=std::min({r,w/2,h/2});
    rect(x+r,y,w-2*r,h,c);rect(x,y+r,r,h-2*r,c);rect(x+w-r,y+r,r,h-2*r,c);
    for(int yy=0;yy<r;++yy){int dx=int(std::sqrt(float(r*r-(r-yy)*(r-yy))));
      rect(x+r-dx,y+yy,w-2*r+2*dx,1,c);rect(x+r-dx,y+h-yy-1,w-2*r+2*dx,1,c);}}
  static uint32_t next(std::string_view &s){auto b=uint8_t(s[0]);unsigned n=b<128?1:b<224?2:3;uint32_t cp=b& (n==1?127:n==2?31:15);for(unsigned j=1;j<n;++j)cp=(cp<<6)|(uint8_t(s[j])&63);s.remove_prefix(n);return cp;}
  static unsigned glyph(uint32_t cp){auto i=std::lower_bound(font_codes.begin(),font_codes.end(),cp);return i!=font_codes.end()&&*i==cp?unsigned(i-font_codes.begin()):unsigned('?' -32);}
  static int text_width(std::string_view s,int scale=1){int w=0;while(!s.empty())w+=font_advance[glyph(next(s))]*scale;return w;}
  void text(int x,int y,std::string_view s,uint16_t color=ink,int align=0,int scale=1){
    if(align)x-=text_width(s,scale)/(align==1?2:1);
    while(!s.empty()){
      unsigned i=glyph(next(s)),page=i/font_per_page,cell=i%font_per_page;
      pxa::game::Sprite sprite{int16_t(x),int16_t(y),uint16_t(font_cell_w*scale),uint16_t(font_cell_h*scale),
        uint16_t(cell%font_columns*font_cell_w),uint16_t(cell/font_columns*font_cell_h),font_cell_w,font_cell_h};
      frame.sprites({uint8_t(page)},{&sprite,1},{.transparent_index0=true,.solid_color=true,.texel_alpha=alpha,.color={color}});
      x+=font_advance[i]*scale;
    }
  }
  void star(int x,int y,int r,uint16_t c){
    for(int i=0;i<10;++i){float a=-1.570796f+i*.6283185f,b=a+.6283185f;
      float ra=(i&1)?r*.45f:r,rb=(i&1)?r:r*.45f;
      int16_t x1=int16_t((x+std::cos(a)*ra)*16),y1=int16_t((y+std::sin(a)*ra)*16);
      int16_t x2=int16_t((x+std::cos(b)*rb)*16),y2=int16_t((y+std::sin(b)*rb)*16);
      frame.quad({int16_t(x*16),int16_t(y*16),x1,y1,x2,y2,int16_t(x*16),int16_t(y*16)},{c});}}
  void die(int x,int y,int size,unsigned value,uint16_t face=white){
    round(x,y+4,size,size,rgb(203,186,165),size/6);round(x,y,size,size,face,size/6);
    rect(x+size/6,y+size-5,size-size/3,2,rgb(239,227,212));
    int inset=size/4,mid=size/2,far=size-inset,r=std::max(2,size/15);
    auto dot=[&](int xx,int yy){circle(x+xx,y+yy,r,value==1?coral:ink);};
    if(value&1)dot(mid,mid);if(value>=2){dot(inset,inset);dot(far,far);}
    if(value>=4){dot(far,inset);dot(inset,far);}if(value==6){dot(inset,mid);dot(far,mid);}}
  void marble(int x,int y,int r){circle(x+1,y+3,r,rgb(181,185,158));circle(x,y,r,teal);circle(x-1,y-2,std::max(2,r-2),rgb(79,190,173));circle(x-r/3,y-r/3,std::max(1,r/3),white);}
};
}
