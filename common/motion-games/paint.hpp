// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/game.hpp>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <font.hpp>
namespace motion {
constexpr uint16_t rgb(unsigned r,unsigned g,unsigned b){return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
constexpr uint16_t paper=rgb(255,249,237),ink=rgb(42,60,67),muted=rgb(111,128,130),teal=rgb(42,147,130),mint=rgb(220,243,229),pink=rgb(255,225,218),coral=rgb(230,111,99),lilac=rgb(236,228,253),purple=rgb(129,103,180),gold=rgb(249,191,69),white=rgb(255,255,255),disabled=rgb(233,232,221);
struct Rect {int x=0,y=0,w=0,h=0;bool contains(int xx,int yy)const{return xx>=x&&yy>=y&&xx<x+w&&yy<y+h;}};
struct Point {float x=0,y=0;};
// Keep repeated drawing helpers out of each screen's generated native code.
struct Paint {
 pxa::game::Frame& frame;bool alpha;int width,height;
 int base_font()const{return std::min(width,height)<200?12:16;}
 [[gnu::noinline]] void quad(std::array<Point,4> a,uint16_t c){std::array<int16_t,8> out;for(unsigned i=0;i<4;++i){out[2*i]=int16_t(std::lround(a[i].x*16));out[2*i+1]=int16_t(std::lround(a[i].y*16));}
   // An edge-on pip can collapse after Q4 quantization. Omit that invisible
   // polygon rather than let one zero-area quad reject the entire frame.
   int64_t area=0;for(unsigned i=0;i<4;++i){unsigned j=(i+1)%4;area+=int64_t(out[2*i])*out[2*j+1]-int64_t(out[2*j])*out[2*i+1];}if(area)frame.quad(out,{c});}
 [[gnu::noinline]] void rect(int x,int y,int w,int h,uint16_t c){if(w<=0||h<=0)return;quad({Point{float(x),float(y)},Point{float(x+w),float(y)},Point{float(x+w),float(y+h)},Point{float(x),float(y+h)}},c);}
 void ellipse(int x,int y,int rx,int ry,uint16_t c){if(rx<1||ry<1)return;for(int yy=-ry;yy<ry;++yy){int dx=int(rx*std::sqrt(std::max(0.f,1.f-float(yy*yy)/float(ry*ry))));rect(x-dx,y+yy,2*dx,1,c);}}
 void circle(int x,int y,int r,uint16_t c){ellipse(x,y,r,r,c);}
 [[gnu::noinline]] void round(Rect r,uint16_t c,int radius=8){int n=std::min({radius,r.w/2,r.h/2});rect(r.x+n,r.y,r.w-2*n,r.h,c);rect(r.x,r.y+n,n,r.h-2*n,c);rect(r.x+r.w-n,r.y+n,n,r.h-2*n,c);
   for(int yy=0;yy<n;++yy){int dx=int(std::sqrt(float(n*n-(n-yy)*(n-yy))));rect(r.x+n-dx,r.y+yy,r.w-2*n+2*dx,1,c);rect(r.x+n-dx,r.y+r.h-yy-1,r.w-2*n+2*dx,1,c);}}
 void line(Point a,Point b,float thick,uint16_t c){float dx=b.x-a.x,dy=b.y-a.y,n=std::sqrt(dx*dx+dy*dy);if(n<.001f)return;float x=-dy/n*thick*.5f,y=dx/n*thick*.5f;quad({Point{a.x+x,a.y+y},Point{b.x+x,b.y+y},Point{b.x-x,b.y-y},Point{a.x-x,a.y-y}},c);}
 static uint32_t next(std::string_view& s){if(s.empty())return 0;unsigned b=uint8_t(s[0]),n=b<128?1:b<224?2:b<240?3:4;uint32_t cp=b&(n==1?127:n==2?31:n==3?15:7);if(s.size()<n){s={};return '?';}for(unsigned j=1;j<n;++j)cp=(cp<<6)|(uint8_t(s[j])&63);s.remove_prefix(n);return cp;}
 static unsigned glyph(uint32_t cp){auto i=std::lower_bound(font_codes.begin(),font_codes.end(),cp);if(i==font_codes.end()||*i!=cp)i=std::lower_bound(font_codes.begin(),font_codes.end(),uint32_t('?'));return unsigned(i-font_codes.begin());}
 static int text_width(std::string_view s,int px=16){int n=0;while(!s.empty())n+=font_advance[glyph(next(s))];return (n*px+8)/16;}
 static int text_height(int px){return (int(font_cell_h)*px+15)/16;}
 [[gnu::noinline]] void text(int x,int y,std::string_view s,uint16_t c=ink,int align=0,int px=0){if(!px)px=base_font();if(align)x-=text_width(s,px)/(align==1?2:1);int origin=x,advance=0;
  while(!s.empty()){unsigned i=glyph(next(s)),page=i/font_per_page,cell=i%font_per_page;int xx=origin+(advance*px+8)/16;
   pxa::game::Sprite g{int16_t(xx),int16_t(y),uint16_t((font_cell_w*px+8)/16),uint16_t((font_cell_h*px+8)/16),uint16_t(cell%font_columns*font_cell_w),uint16_t(cell/font_columns*font_cell_h),font_cell_w,font_cell_h};
   frame.sprites({uint8_t(page)},{&g,1},{.transparent_index0=true,.solid_color=true,.texel_alpha=alpha,.color={c}});advance+=font_advance[i];}}
 [[gnu::noinline]] void label(Rect r,std::string_view s,uint16_t c=ink,int align=1,int px=0){if(!px)px=base_font();px=std::min(px,std::max(8,r.h*16/int(font_cell_h)));int natural=text_width(s,px);if(natural>r.w&&natural)px=std::max(8,px*r.w/natural);
   text(align==1?r.x+r.w/2:align==2?r.x+r.w:r.x,r.y+(r.h-text_height(px))/2,s,c,align,px);}
 void star(int x,int y,int r,uint16_t c){for(int i=0;i<10;++i){float a=-1.570796f+i*.6283185f,b=a+.6283185f,ra=(i&1)?r*.45f:r,rb=(i&1)?r:r*.45f;quad({Point{float(x),float(y)},Point{x+std::cos(a)*ra,y+std::sin(a)*ra},Point{x+std::cos(b)*rb,y+std::sin(b)*rb},Point{float(x),float(y)}},c);}}
 void marble(int x,int y,int r){circle(x+1,y+3,r,rgb(181,185,158));circle(x,y,r,teal);circle(x-1,y-2,std::max(2,r-2),rgb(79,190,173));circle(x-r/3,y-r/3,std::max(1,r/3),white);}
};
}
