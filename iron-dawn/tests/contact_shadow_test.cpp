// SPDX-License-Identifier: MIT
#include "art.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
int main(){
 unsigned checked=0;for(unsigned id=art::unit_first[tank];id<art::environment_first+8;++id){bool vehicle=id<art::buildings_first,building=id>=art::buildings_first&&id<art::terrain_first,prop=id>=art::environment_first;if(!vehicle&&!building&&!prop)continue;
  auto f=art::frames[id];auto p=art::shadow_profiles[id];assert(p.count>0&&p.left+p.count<=f.w&&p.offset+p.count<=art::shadow_columns.size());std::vector<uint8_t> alpha(f.w*f.h);auto src=art::bytes.data()+f.offset;for(unsigned run=0;run<f.count;++run){int x=src[0],y=src[1],n=src[2];src+=3+n;for(int k=0;k<n;++k)alpha[y*f.w+x+k]=1;}
  int low=255,high=0,supports=0;for(unsigned i=0;i<p.count;++i){int y=art::shadow_columns[p.offset+i],x=p.left+i;assert(y>=2&&y<f.h);bool supported=true;for(int j=0;j<3;++j)supported&=alpha[(y-j)*f.w+x];supports+=supported;if(i==0||i+1==p.count)assert(supported);low=std::min(low,y);high=std::max(high,y);}assert(supports>=2);
  if(vehicle&&((id-art::unit_first[tank])%8)%2==0)assert(high-low>4); // Sloping tracks cannot collapse into a flat oval.
  for(float zoom:{.72f,1.f,1.3f,2.f}){constexpr int width=256,height=240;constexpr uint16_t ground=0xa55a;std::vector<uint16_t> pixels(width*height+2,ground),mask(width*height);Raster r(pixels.data()+1,width,height,width),m(mask.data(),width,height,width);int x=120,y=140,ox=x-int(std::floor(f.ox*zoom)),oy=y-int(std::floor(f.oy*zoom));
   ShadowArt::base(r,id,x,y,zoom,building);for(int yy=0;yy<f.h;++yy)for(int xx=0;xx<f.w;++xx)if(alpha[yy*f.w+xx]){int a=ox+int(std::floor(xx*zoom)),b=ox+int(std::floor((xx+1)*zoom)),c=oy+int(std::floor(yy*zoom)),d=oy+int(std::floor((yy+1)*zoom));m.rect(a,c,b-a,d-c,1);}
   // Every connected shadow region must reach real opaque contact pixels.
   // Wheel gaps may contain ground shade, but no detached dark blob is valid.
   std::vector<uint8_t> connected(width*height);std::vector<int> queue;for(int at=0;at<width*height;++at)if(mask[at]){connected[at]=1;queue.push_back(at);}for(unsigned at=0;at<queue.size();++at){int xx=queue[at]%width,yy=queue[at]/width;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int xx2=xx+dx,yy2=yy+dy;if(xx2<0||xx2>=width||yy2<0||yy2>=height)continue;int n=yy2*width+xx2;if(!connected[n]&&(mask[n]||pixels[n+1]!=ground)){connected[n]=1;queue.push_back(n);}}}
   unsigned visible=0,touching=0;for(int yy=1;yy<height-1;++yy)for(int xx=1;xx<width-1;++xx)if(pixels[1+yy*width+xx]!=ground&&!mask[yy*width+xx]){++visible;if(!connected[yy*width+xx])std::fprintf(stderr,"Detached shadow id=%u zoom=%.2f at %d,%d\n",id,zoom,xx,yy);assert(connected[yy*width+xx]);bool contact=false;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)contact|=mask[(yy+dy)*width+xx+dx]!=0;touching+=contact;}
   assert(visible>0&&touching>0);assert(pixels.front()==ground&&pixels.back()==ground);
   r.clip={119,139,3,2};std::fill(pixels.begin(),pixels.end(),ground);ShadowArt::base(r,id,x,y,zoom,building);for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx)if(!r.clip.has(xx,yy))assert(pixels[1+yy*width+xx]==ground);
  }++checked;
 }
 for(int kind=0;kind<2;++kind)for(unsigned facing=0;facing<8;++facing)for(int phase=0;phase<8;++phase){auto gait=CombatArt::gait(kind,facing,phase/8.f,1);for(auto leg:gait.legs)assert(std::max(leg.ankle.y,leg.toe.y)+2+leg.lift>=leg.toe.y+2);}
 std::printf("%u vehicle/building/prop contours: solid support, sloping tracks, attached shadows at four scales and clipping bounds; 128 infantry gait contacts passed\n",checked);
}
