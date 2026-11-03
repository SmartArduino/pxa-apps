// SPDX-License-Identifier: MIT
#include "../background.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace starsoil;
static State s;
constexpr int W=288,H=232,Top=25;
static std::vector<uint16_t> render(int cx,int cy) {
  std::vector<uint16_t> pixels(W*H,0x1234);
  auto dot=[&](int x,int y,uint16_t c) {
    if (unsigned(x)<W && y>=Top && y<H) pixels[y*W+x]=c;
  };
  auto rect=[&](int x,int y,int w,int h,uint16_t c) {
    for (int yy=std::max(Top,y); yy<std::min(H,y+h); ++yy)
      for (int xx=std::max(0,x);xx<std::min(W,x+w);++xx)dot(xx,yy,c);
  };
  auto line=[&](int x,int y,int ex,int ey,uint16_t c) {
    int dx=std::abs(ex-x),sx=x<ex?1:-1,dy=-std::abs(ey-y),sy=y<ey?1:-1,e=dx+dy;
    for (;;) { dot(x,y,c);if(x==ex && y==ey)break;int v=e*2;
      if(v>=dy){e+=dy;x+=sx;}if(v<=dx){e+=dx;y+=sy;} }
  };
  starsoil_scene::draw_world(s,W,H,cx,cy,12345,rect,line,dot,
    [&](int x,int y){return pixels[y*W+x];},Top);
  for (int y=0;y<Top;++y)for(int x=0;x<W;++x)assert(pixels[y*W+x]==0x1234);
  for (int y=Top;y<H;++y)for(int x=0;x<W;++x)assert(pixels[y*W+x]!=0x1234);
  return pixels;
}
static double difference(const std::vector<uint16_t>&a,const std::vector<uint16_t>&b) {
  long total=0;
  for(size_t i=Top*W;i<a.size();++i) {
    total+=std::abs(int(a[i]>>11)-int(b[i]>>11));
    total+=std::abs(int((a[i]>>5)&63)-int((b[i]>>5)&63));
    total+=std::abs(int(a[i]&31)-int(b[i]&31));
  }
  return double(total)/((H-Top)*W);
}
int main() {
  new_game(s,1234567);
  std::fill(std::begin(s.heights),std::end(s.heights),uint8_t(32));
  s.clock=100000;
  auto ground=render(500,320);
  for (int y : {30,31,35,36,37,67,68,69,90}) {
    s.player.y=y*16; assert(render(500,320)==ground);
  }
  puts("PASS background ignores actor height, including old surface/cave and crystal-biome thresholds");
  double largest=0;
  // Actual rendered frames, one viewport, with only a one-pixel camera step.
  for (int center : {32*16+64-185,64*16-116,68*16-116,72*16-116}) {
    auto before=render(500,center-8);
    for (int cy=center-7;cy<=center+8;++cy) {
      auto after=render(500,cy);double d=difference(before,after);
      largest=std::max(largest,d);assert(d<3.0);before=after;
    }
  }
  printf("PASS continuous surface/cave/crystal transitions: largest RGB565 channel-step mean %.3f, no full-frame threshold switch\n",largest);
  for(int x=100;x<140;++x)s.heights[x]=60;
  for(int x=90*16;x<150*16;++x)
    assert(std::abs(starsoil_scene::surface_level(s,x+1)-starsoil_scene::surface_level(s,x))<=1);
  uint64_t fingerprint=1469598103934665603ull;
  for(uint32_t clock:{10000u,100000u}) {
    s.clock=clock;
    for(int x:{30,120,180,250,330})for(int cy:{0,450,1200})
      for(auto value:render(x*16,cy))fingerprint=(fingerprint^value)*1099511628211ull;
  }
  printf("Background pixel fingerprint: %llu\n",(unsigned long long)fingerprint);
  assert(fingerprint==2066120640922625989ull); // Before the boundary-span optimization.
  puts("PASS distant ground interpolates across cliffs; full coverage and clipping in every biome at surface and cave depths");
}
