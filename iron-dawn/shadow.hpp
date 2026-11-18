// SPDX-License-Identifier: MIT
#pragma once
#include "combat_art.hpp"
namespace iron {
struct ShadowArt {
 static void ellipse(Raster& r,int x,int y,int rx,int ry,int opacity){
  if(rx<1||ry<1)return;for(int yy=-ry;yy<=ry;++yy){int span=int(std::sqrt(std::max(0.f,1.f-float(yy*yy)/(ry*ry)))*rx);r.blend_span(x-span,y+yy,span*2+1,rgb(9,18,16),opacity);}
 }
 static void base(Raster& r,unsigned id,int x,int y,float zoom,bool building=false){
  if(id>=art::frames.size()||zoom<=0)return;auto f=art::frames[id];auto p=art::shadow_profiles[id];int ox=x-int(std::floor(f.ox*zoom)),oy=y-int(std::floor(f.oy*zoom));int spread=std::max(1,int(std::floor(zoom))),soft=std::max(1,int(std::lround(zoom)));
  // Use the sprite's own floor-based scale transform, including its next row
  // boundary. A contact pixel always touches the track, wheel or foundation.
  // The faint outer row stays close; there is no displaced oval under the body.
  int previous=-32768;for(unsigned i=0;i<p.count;++i){unsigned yy=art::shadow_columns[p.offset+i];if(yy==255)continue;int left=ox+int(std::floor((p.left+i)*zoom)),right=ox+int(std::floor((p.left+i+1)*zoom)),edge=oy+int(std::floor((yy+1)*zoom));if(left>=right)continue;int end=std::max(edge+1,oy+int(std::floor((yy+2)*zoom)));end=std::max(end,previous);previous=edge;
   // Join steep/slightly downsampled support edges into a continuous strip.
   for(unsigned n=i+1;n<p.count;++n){int a=int(std::floor((p.left+n)*zoom)),b=int(std::floor((p.left+n+1)*zoom));if(a==b)continue;unsigned next=art::shadow_columns[p.offset+n];if(next!=255)end=std::max(end,oy+int(std::floor((next+1)*zoom)));break;}int thin=end-edge;
   for(int j=0;j<soft;++j)r.blend_span(left+spread,edge+thin+j,right-left,rgb(9,18,16),2);
   for(int j=0;j<thin;++j)r.blend_span(left,edge+j,right-left,rgb(9,18,16),building?7:8);
  }
 }
 static void infantry(Raster& r,int kind,unsigned facing,int x,int y,float zoom,float phase,float weight){
  auto gait=CombatArt::gait(kind,facing,phase,weight);ellipse(r,x+2*zoom,y+2*zoom,std::max(2,int(6*zoom)),std::max(1,int(2*zoom)),3);
  for(auto leg:gait.legs){int xx=x+std::lround((leg.ankle.x+leg.toe.x)*.5f*zoom),yy=y+std::lround((std::max(leg.ankle.y,leg.toe.y)+2+leg.lift)*zoom);ellipse(r,xx,yy,std::max(1,int(3*zoom)),std::max(1,int(zoom)),leg.lift>.5f?4:10);}
 }
};
}
