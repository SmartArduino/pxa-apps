// SPDX-License-Identifier: MIT
#pragma once
#include "animation.hpp"
#include "lawn.hpp"
#include <cmath>
#include <cstdio>
namespace garden {
struct Paint {
 uint16_t* pixels;int width,height,stride;FilteredArt& art;float zoom=1;bool cached=true,prepare_only=false;
 int pixel(int value)const{return int(std::lround(value*zoom));}
 Rect physical(Rect r)const{int x=pixel(r.x),y=pixel(r.y);return {x,y,pixel(r.x+r.w)-x,pixel(r.y+r.h)-y};}
 void rect(int x,int y,int w,int h,uint16_t color){if(prepare_only)return;auto r=physical({x,y,w,h});for(int yy=std::max(0,r.y);yy<std::min(height,r.y+r.h);++yy)std::fill(pixels+yy*stride+std::clamp(r.x,0,width),pixels+yy*stride+std::clamp(r.x+r.w,0,width),color);}
 void image(unsigned index,Rect r,bool fit=true){if(r.w<=0||r.h<=0)return;auto f=garden_art::frames[index];
  if(fit){int ww=std::min(r.w,r.h*int(f.w)/int(f.h)),hh=std::min(r.h,r.w*int(f.h)/int(f.w));r={r.x+(r.w-ww)/2,r.y+(r.h-hh)/2,ww,hh};}
  if(r.w<=0||r.h<=0)return;auto target=physical(r);if(prepare_only){art.prepare(index,target.w,target.h,false);return;}art.blit(pixels,width,height,stride,index,target,false,cached);}
 void shadow(float cx,float ground,int unit,bool moving=false){float left=(cx-unit*.5f)*zoom;Rect target{int(std::lround(left)),int(std::lround((ground-3)*zoom)),pixel(unit),pixel(6)};if(moving)art.blit_subpixel(pixels,width,height,stride,garden_art::ui_shadow,target,left,true);else art.blit(pixels,width,height,stride,garden_art::ui_shadow,target,true);}
 void quad(const std::array<int,8>& points,uint16_t color){
  if(prepare_only)return;
  std::array<int,8> v;int l=width,r=0,t=height,b=0;
  for(int i=0;i<4;++i){v[2*i]=pixel(points[2*i])*4;v[2*i+1]=pixel(points[2*i+1])*4;l=std::min(l,v[2*i]/4);r=std::max(r,v[2*i]/4);t=std::min(t,v[2*i+1]/4);b=std::max(b,v[2*i+1]/4);}
  for(int y=std::max(0,t);y<std::min(height,b);++y)for(int x=std::max(0,l);x<std::min(width,r);++x){unsigned covered=0;
   for(int sy:{1,3})for(int sx:{1,3}){bool positive=false,negative=false;for(int i=0;i<4;++i){int j=(i+1)%4;int cross=(v[2*j]-v[2*i])*(y*4+sy-v[2*i+1])-(v[2*j+1]-v[2*i+1])*(x*4+sx-v[2*i]);positive|=cross>0;negative|=cross<0;}covered+=!(positive&&negative);}
   auto& p=pixels[y*stride+x];p=over(premultiply(color,covered*255/4),p);}
 }
 void arrow(Rect r,bool right,bool enabled){image(garden_art::ui_page_button,r);int cx=r.x+r.w/2,cy=r.y+r.h/2;uint16_t color=enabled?rgb(255,230,142):rgb(151,127,80);auto part=[&](std::array<int,8> points,uint16_t ink,int dy){for(unsigned i=0;i<4;++i){points[2*i]=cx+(right?-points[2*i]:points[2*i]);points[2*i+1]+=cy+dy;}quad(points,ink);};
  for(int dy:{1,0}){auto ink=dy?rgb(76,43,21):color;part({2,-7,5,-4,-2,3,-5,0},ink,dy);part({-5,0,-2,-3,5,4,2,7},ink,dy);}}
 void actor(garden_art::Clip clip,float time,float cx,float ground,int unit,bool moving=false){unsigned index=animation_frame(clip,time);auto target=actor_bounds(garden_art::frames[index],cx,ground,unit,zoom);
  if(prepare_only){art.prepare(index,target.w,target.h,false);return;}
  if(!moving){art.blit(pixels,width,height,stride,index,target,false,cached);return;}
  float left=cx*zoom+std::lround((float(garden_art::frames[index].ox)-garden_art::frames[index].cw*.5f)*unit*zoom/32.f);
  art.blit_subpixel(pixels,width,height,stride,index,target,left);}
 void moving_image(unsigned index,float x,float y,int w,int h){auto f=garden_art::frames[index];int ww=std::min(w,h*int(f.w)/int(f.h)),hh=std::min(h,w*int(f.h)/int(f.w));x+=(w-ww)*.5f;y+=(h-hh)*.5f;Rect r{int(std::lround(x*zoom)),int(std::lround(y*zoom)),pixel(ww),pixel(hh)};art.blit_subpixel(pixels,width,height,stride,index,r,x*zoom);}
 void number(Rect r,int value){char text[16];std::snprintf(text,sizeof text,"%d",value);int count=0;while(text[count]&&count<12)++count;int step=std::min(r.h*15/23,r.w/std::max(1,count));int x=r.x+(r.w-step*count)/2;
  for(int i=0;i<count;++i){unsigned id=text[i]=='-'?garden_art::glyph_minus:garden_art::glyph_0+unsigned(text[i]-'0');image(id,{x+i*step,r.y,step,r.h});}}
};
}
