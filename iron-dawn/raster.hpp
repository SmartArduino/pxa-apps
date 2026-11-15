// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include "font.hpp"
namespace iron {
constexpr uint16_t rgb(int r,int g,int b){return ((r&248)<<8)|((g&252)<<3)|(b>>3);}
constexpr uint16_t navy=rgb(12,23,29),panel=rgb(22,37,43),edge=rgb(56,77,78),cream=rgb(224,229,207),muted=rgb(132,156,151),cyan=rgb(109,221,202),gold=rgb(236,182,87),red=rgb(235,102,86);
inline uint16_t mix(uint16_t a,uint16_t b,int f){int r=(((a>>11)&31)*(16-f)+((b>>11)&31)*f)/16,g=(((a>>5)&63)*(16-f)+((b>>5)&63)*f)/16,bb=((a&31)*(16-f)+(b&31)*f)/16;return (r<<11)|(g<<5)|bb;}
struct Rect {int x,y,w,h;bool has(int xx,int yy)const{return xx>=x&&yy>=y&&xx<x+w&&yy<y+h;}};
struct Pt {float x,y;};
struct Raster {
 uint16_t* pixels;int w,h,stride;Rect clip;
 Raster(uint16_t* p,int ww,int hh,int s):pixels(p),w(ww),h(hh),stride(s),clip{0,0,ww,hh}{}
 void dot(int x,int y,uint16_t c){if(clip.has(x,y))pixels[y*stride+x]=c;}
 void blend(int x,int y,uint16_t c,int opacity){if(clip.has(x,y)){auto& dest=pixels[y*stride+x];dest=mix(dest,c,opacity);}}
 [[gnu::noinline]] void rect(int x,int y,int width,int height,uint16_t c){int left=std::max(x,clip.x),right=std::min(x+width,clip.x+clip.w),top=std::max(y,clip.y),bottom=std::min(y+height,clip.y+clip.h);if(right<=left)return;for(int yy=top;yy<bottom;++yy)std::fill(pixels+yy*stride+left,pixels+yy*stride+right,c);}
 void box(Rect a,uint16_t fill,uint16_t border=edge){rect(a.x,a.y,a.w,a.h,fill);rect(a.x,a.y,a.w,1,border);rect(a.x,a.y+a.h-1,a.w,1,border);rect(a.x,a.y,1,a.h,border);rect(a.x+a.w-1,a.y,1,a.h,border);}
 [[gnu::noinline]] void line(int x,int y,int xx,int yy,uint16_t c,int thick=1){int dx=std::abs(xx-x),dy=-std::abs(yy-y),sx=x<xx?1:-1,sy=y<yy?1:-1,err=dx+dy;for(;;){rect(x-thick/2,y-thick/2,thick,thick,c);if(x==xx&&y==yy)break;int twice=2*err;if(twice>=dy){err+=dy;x+=sx;}if(twice<=dx){err+=dx;y+=sy;}}}
 [[gnu::noinline]] void poly(const std::array<Pt,4>& v,uint16_t c){int ymin=h,ymax=0;for(auto p:v){ymin=std::min(ymin,int(std::floor(p.y)));ymax=std::max(ymax,int(std::ceil(p.y)));}ymin=std::max(ymin,clip.y);ymax=std::min(ymax,clip.y+clip.h-1);for(int y=ymin;y<=ymax;++y){float xs[4];int n=0;for(int j=0;j<4;++j){Pt a=v[j],b=v[(j+1)%4];if((a.y<=y+.5f&&b.y>y+.5f)||(b.y<=y+.5f&&a.y>y+.5f))xs[n++]=a.x+(y+.5f-a.y)*(b.x-a.x)/(b.y-a.y);}if(n>=2){std::sort(xs,xs+n);rect(int(std::ceil(xs[0])),y,int(std::ceil(xs[n-1]))-int(std::ceil(xs[0])),1,c);}}}
 [[gnu::noinline]] void ellipse(int x,int y,int rx,int ry,uint16_t c){if(ry<=0||rx<=0)return;for(int yy=-ry;yy<=ry;++yy){int width=int(std::sqrt(std::max(0.f,1.f-float(yy*yy)/(ry*ry)))*rx);rect(x-width,y+yy,width*2+1,1,c);}}
 void ring(int x,int y,int rx,int ry,uint16_t c){for(int i=0;i<36;++i){float a=i*.174533f,b=(i+1)*.174533f;line(x+std::cos(a)*rx,y+std::sin(a)*ry,x+std::cos(b)*rx,y+std::sin(b)*ry,c);}}
 static uint32_t next(std::string_view& s){if(s.empty())return 0;uint8_t b=s[0];int n=b<128?1:b<224?2:b<240?3:4;if(int(s.size())<n){s={};return '?';}uint32_t c=b&(n==1?127:n==2?31:n==3?15:7);for(int j=1;j<n;++j)c=(c<<6)|(uint8_t(s[j])&63);s.remove_prefix(n);return c;}
 static int glyph(uint32_t c){auto i=std::lower_bound(glyph_codes.begin(),glyph_codes.end(),c);return i==glyph_codes.end()||*i!=c?int(std::lower_bound(glyph_codes.begin(),glyph_codes.end(),uint32_t('?'))-glyph_codes.begin()):int(i-glyph_codes.begin());}
 static int measure(std::string_view s,int size=16){int width=0;while(!s.empty())width+=glyph_width[glyph(next(s))]*size;return width/20;}
 [[gnu::noinline]] void text(int x,int y,std::string_view s,uint16_t c=cream,int size=16,int align=0){if(align)x-=measure(s,size)/(align==1?2:1);int start=x,adv=0;while(!s.empty()){int g=glyph(next(s)),gw=(24*size+19)/20,gh=(28*size+19)/20;for(int yy=0;yy<gh;++yy)for(int xx=0;xx<gw;++xx){int sx=xx*20/size,sy=yy*20/size,sx1=std::min(24,((xx+1)*20+size-1)/size),sy1=std::min(28,((yy+1)*20+size-1)/size);int total=0,samples=0;for(int fy=sy;fy<std::max(sy+1,sy1);++fy)for(int fx=sx;fx<std::max(sx+1,sx1);++fx){int idx=fy*24+fx;total+=((glyph_bits[g*168+idx/4]>>((idx%4)*2))&3)*16;++samples;}int alpha=(total+samples/2)/samples;if(alpha)blend(start+adv*size/20+xx,y+yy,c,alpha/3);}adv+=glyph_width[g];}}
 void label(Rect r,std::string_view s,uint16_t c=cream,int size=16,int align=1){int width=measure(s,size);if(width>r.w)size=std::max(9,size*r.w/std::max(1,width));text(align==1?r.x+r.w/2:align==2?r.x+r.w:r.x,r.y+(r.h-size*28/20)/2,s,c,size,align);}
 [[gnu::noinline]] void shade(Rect r,uint16_t color,int opacity){for(int y=std::max(clip.y,r.y);y<std::min(clip.y+clip.h,r.y+r.h);++y)for(int x=std::max(clip.x,r.x);x<std::min(clip.x+clip.w,r.x+r.w);++x)blend(x,y,color,opacity);}
};
}
