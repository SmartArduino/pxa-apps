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
// Native strikes are baked once; glyphs are always placed on integer pixels.
struct FontStyle {int face=0,scale=1;};
inline bool ascii_text(std::string_view s){for(unsigned char c:s)if(c>=128)return false;return true;}
inline FontStyle font_style(int desired,bool){return {0,std::clamp((desired+6)/12,1,8)};}
inline int pixel_scale(int value,int numerator,int denominator){int64_t p=int64_t(value)*numerator;return int(p>=0?(p+denominator/2)/denominator:-((-p+denominator/2)/denominator));}
struct TextCommand {int x,y,size,align;uint16_t color;Rect clip;uint16_t offset,length;};
struct NativeText {
 int width=480,height=320,canvas_width=480,canvas_height=320,minimum_scale=1;float text_scale=1;unsigned count=0,bytes=0;bool overflow=false;
 std::array<TextCommand,128> commands{};std::array<char,8192> strings{};
 void reset(int w,int h,int cw,int ch,float scale=1,int minimum=1){width=w;height=h;canvas_width=cw;canvas_height=ch;text_scale=scale;minimum_scale=minimum;count=bytes=0;overflow=false;}
 FontStyle style(std::string_view s,int size)const{auto f=font_style(std::lround(pixel_scale(size,width,canvas_width)*text_scale),ascii_text(s));f.scale=std::max(f.scale,minimum_scale);return f;}
 bool push(int x,int y,std::string_view s,uint16_t c,int size,int align,Rect clip){if(count==commands.size()||bytes+s.size()>strings.size()){overflow=true;return false;}std::copy(s.begin(),s.end(),strings.begin()+bytes);commands[count++]={x,y,size,align,c,clip,uint16_t(bytes),uint16_t(s.size())};bytes+=s.size();return true;}
};
struct Raster {
 uint16_t* pixels;int w,h,stride;Rect clip;NativeText* native_text=nullptr;
 Raster(uint16_t* p,int ww,int hh,int s):pixels(p),w(ww),h(hh),stride(s),clip{0,0,ww,hh}{}
 void dot(int x,int y,uint16_t c){if(clip.has(x,y))pixels[y*stride+x]=c;}
 void blend(int x,int y,uint16_t c,int opacity){if(clip.has(x,y)){auto& dest=pixels[y*stride+x];dest=mix(dest,c,opacity);}}
 [[gnu::noinline]] void rect(int x,int y,int width,int height,uint16_t c){int left=std::max(x,clip.x),right=std::min(x+width,clip.x+clip.w),top=std::max(y,clip.y),bottom=std::min(y+height,clip.y+clip.h);if(right<=left)return;for(int yy=top;yy<bottom;++yy)std::fill(pixels+yy*stride+left,pixels+yy*stride+right,c);}
 void box(Rect a,uint16_t fill,uint16_t border=edge){rect(a.x,a.y,a.w,a.h,fill);rect(a.x,a.y,a.w,1,border);rect(a.x,a.y+a.h-1,a.w,1,border);rect(a.x,a.y,1,a.h,border);rect(a.x+a.w-1,a.y,1,a.h,border);}
 void blend_span(int x,int y,int count,uint16_t color,int opacity){
  if(opacity<=0||y<clip.y||y>=clip.y+clip.h)return;int left=std::max(x,clip.x),right=std::min(x+count,clip.x+clip.w);if(left>=right)return;
  auto p=pixels+y*stride+left,end=pixels+y*stride+right;if(opacity>=16)std::fill(p,end,color);else for(;p<end;++p)*p=mix(*p,color,opacity);
 }
 [[gnu::noinline]] void line(int x,int y,int xx,int yy,uint16_t c,int thick=1){if(thick<=0)return;if(y==yy){rect(std::min(x,xx)-thick/2,y-thick/2,std::abs(xx-x)+thick,thick,c);return;}if(x==xx){rect(x-thick/2,std::min(y,yy)-thick/2,thick,std::abs(yy-y)+thick,c);return;}int dx=std::abs(xx-x),dy=-std::abs(yy-y),sx=x<xx?1:-1,sy=y<yy?1:-1,err=dx+dy;for(;;){if(thick==1)dot(x,y,c);else rect(x-thick/2,y-thick/2,thick,thick,c);if(x==xx&&y==yy)break;int twice=2*err;if(twice>=dy){err+=dy;x+=sx;}if(twice<=dx){err+=dx;y+=sy;}}}
 [[gnu::noinline]] void poly(const std::array<Pt,4>& v,uint16_t c){int ymin=h,ymax=0;for(auto p:v){ymin=std::min(ymin,int(std::floor(p.y)));ymax=std::max(ymax,int(std::ceil(p.y)));}ymin=std::max(ymin,clip.y);ymax=std::min(ymax,clip.y+clip.h-1);for(int y=ymin;y<=ymax;++y){float xs[4];int n=0;for(int j=0;j<4;++j){Pt a=v[j],b=v[(j+1)%4];if((a.y<=y+.5f&&b.y>y+.5f)||(b.y<=y+.5f&&a.y>y+.5f))xs[n++]=a.x+(y+.5f-a.y)*(b.x-a.x)/(b.y-a.y);}if(n>=2){std::sort(xs,xs+n);rect(int(std::ceil(xs[0])),y,int(std::ceil(xs[n-1]))-int(std::ceil(xs[0])),1,c);}}}
 [[gnu::noinline]] void ellipse(int x,int y,int rx,int ry,uint16_t c){if(ry<=0||rx<=0)return;for(int yy=-ry;yy<=ry;++yy){int width=int(std::sqrt(std::max(0.f,1.f-float(yy*yy)/(ry*ry)))*rx);rect(x-width,y+yy,width*2+1,1,c);}}
 void ring(int x,int y,int rx,int ry,uint16_t c){static const auto points=[](){std::array<Pt,37> p{};for(int i=0;i<=36;++i){float a=i*.174533f;p[i]={std::cos(a),std::sin(a)};}return p;}();for(int i=0;i<36;++i)line(x+points[i].x*rx,y+points[i].y*ry,x+points[i+1].x*rx,y+points[i+1].y*ry,c);}
 static uint32_t next(std::string_view& s){if(s.empty())return 0;uint8_t b=s[0];int n=b<128?1:b<224?2:b<240?3:4;if(int(s.size())<n){s={};return '?';}uint32_t c=b&(n==1?127:n==2?31:n==3?15:7);for(int j=1;j<n;++j)c=(c<<6)|(uint8_t(s[j])&63);s.remove_prefix(n);return c;}
 static int glyph(uint32_t c){auto i=std::lower_bound(glyph_codes.begin(),glyph_codes.end(),c);return i==glyph_codes.end()||*i!=c?int(std::lower_bound(glyph_codes.begin(),glyph_codes.end(),uint32_t('?'))-glyph_codes.begin()):int(i-glyph_codes.begin());}
 static int text_width(std::string_view s,FontStyle style){int width=0;while(!s.empty())width+=glyph_advances[style.face][glyph(next(s))]*style.scale;return width;}
 int measure(std::string_view s,int size=16)const{auto style=native_text?native_text->style(s,size):font_style(size,ascii_text(s));int width=text_width(s,style);return native_text?(width*native_text->canvas_width+native_text->width-1)/native_text->width:width;}
 int text_height(std::string_view s,int size)const{auto style=native_text?native_text->style(s,size):font_style(size,ascii_text(s));int height=font_strikes[style.face].height*style.scale;return native_text?(height*native_text->canvas_height+native_text->height-1)/native_text->height:height;}
 void bitmap_text(int x,int y,std::string_view s,uint16_t color,FontStyle style,int align=0){
  if(align)x-=text_width(s,style)/(align==1?2:1);auto face=font_strikes[style.face];
  while(!s.empty()){int g=glyph(next(s));const uint8_t* data=glyph_bits.data()+face.offset+g*face.stride;
   if(x+face.width*style.scale>clip.x&&x<clip.x+clip.w)for(int row=0;row<face.height;++row){int yy=y+row*style.scale;if(yy+style.scale<=clip.y||yy>=clip.y+clip.h)continue;int col=0;while(col<face.width){auto lit=[&](int c){int bit=row*face.width+c;return (data[bit/8]>>(bit%8))&1;};if(!lit(col)){++col;continue;}int left=col++;while(col<face.width&&lit(col))++col;rect(x+left*style.scale,yy,(col-left)*style.scale,style.scale,color);}}
   x+=glyph_advances[style.face][g]*style.scale;
  }
 }
 [[gnu::noinline]] void text(int x,int y,std::string_view s,uint16_t color=cream,int size=16,int align=0){if(native_text&&native_text->push(x,y,s,color,size,align,clip))return;bitmap_text(x,y,s,color,font_style(size,ascii_text(s)),align);}
 void label(Rect area,std::string_view s,uint16_t color=cream,int size=16,int align=1){
  if(area.w<=0||area.h<=0)return;int width=measure(s,size),height=text_height(s,size);if(width>area.w||height>area.h)size=std::max(6,std::min(size*area.w/std::max(1,width),size*area.h/std::max(1,height)));std::array<char,512> fitted{};
  if(measure(s,size)>area.w){auto prefix=s;int dots=measure("...",size);while(!prefix.empty()&&(prefix.size()+3>fitted.size()||measure(prefix,size)+dots>area.w)){size_t n=prefix.size()-1;while(n&&(uint8_t(prefix[n])&0xc0)==0x80)--n;prefix=prefix.substr(0,n);}if(dots>area.w)return;std::copy(prefix.begin(),prefix.end(),fitted.begin());std::copy_n("...",3,fitted.begin()+prefix.size());s={fitted.data(),prefix.size()+3};}
  text(align==1?area.x+area.w/2:align==2?area.x+area.w:area.x,area.y+(area.h-text_height(s,size))/2,s,color,size,align);
 }
 [[gnu::noinline]] void shade(Rect r,uint16_t color,int opacity){for(int y=std::max(clip.y,r.y);y<std::min(clip.y+clip.h,r.y+r.h);++y)blend_span(r.x,y,r.w,color,opacity);}
};
inline void draw_native_text(const NativeText& queue,Raster& output){
 for(unsigned i=0;i<queue.count;++i){auto& c=queue.commands[i];auto sx=[&](int v){return pixel_scale(v,queue.width,queue.canvas_width);};auto sy=[&](int v){return pixel_scale(v,queue.height,queue.canvas_height);};int left=std::clamp(sx(c.clip.x),0,output.w),right=std::clamp(sx(c.clip.x+c.clip.w),0,output.w),top=std::clamp(sy(c.clip.y),0,output.h),bottom=std::clamp(sy(c.clip.y+c.clip.h),0,output.h);output.clip={left,top,std::max(0,right-left),std::max(0,bottom-top)};std::string_view text(queue.strings.data()+c.offset,c.length);output.bitmap_text(sx(c.x),sy(c.y),text,c.color,queue.style(text,c.size),c.align);
 }
 output.clip={0,0,output.w,output.h};
}
}
