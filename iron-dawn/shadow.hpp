// SPDX-License-Identifier: MIT
#pragma once
#include "combat_art.hpp"
namespace iron {
struct ShadowArt {
 static void ellipse(Raster& r,int x,int y,int rx,int ry,int opacity){
  if(rx<1||ry<1)return;for(int yy=-ry;yy<=ry;++yy){int span=int(std::sqrt(std::max(0.f,1.f-float(yy*yy)/(ry*ry)))*rx);r.blend_span(x-span,y+yy,span*2+1,rgb(9,18,16),opacity);}
 }
 static void diamond(Raster& r,int x,int y,int rx,int ry,int opacity){if(rx<1||ry<1)return;for(int yy=-ry;yy<=ry;++yy){int span=rx*(ry-std::abs(yy))/ry;r.blend_span(x-span,y+yy,span*2+1,rgb(9,18,16),opacity);}}
 static void base(Raster& r,unsigned id,int x,int y,float zoom,bool building=false){
  const auto foot=art::contacts[id];int cx=x+std::lround(foot.x*zoom),bottom=y+std::lround(foot.bottom*zoom),rx=std::max(2,int(std::lround(foot.rx*zoom)));
  if(building){int ry=std::max(2,int(std::lround(foot.ry*zoom)));diamond(r,cx+2*zoom,bottom-ry+2*zoom,rx+2*zoom,ry,3);diamond(r,cx,bottom-ry+1,rx,ry,7);}
  else {int ry=std::max(1,int(std::lround(foot.ry*zoom)));ellipse(r,cx+2*zoom,bottom,rx+2*zoom,ry+1,3);ellipse(r,cx,bottom-1,rx,ry,9);}
 }
 static void infantry(Raster& r,int kind,unsigned facing,int x,int y,float zoom,float phase,float weight){
  auto gait=CombatArt::gait(kind,facing,phase,weight);ellipse(r,x+2*zoom,y+2*zoom,std::max(2,int(6*zoom)),std::max(1,int(2*zoom)),3);
  for(auto leg:gait.legs){int xx=x+std::lround((leg.ankle.x+leg.toe.x)*.5f*zoom),yy=y+std::lround((std::max(leg.ankle.y,leg.toe.y)+2+leg.lift)*zoom);ellipse(r,xx,yy,std::max(1,int(3*zoom)),std::max(1,int(zoom)),leg.lift>.5f?4:10);}
 }
};
}
