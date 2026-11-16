// SPDX-License-Identifier: MIT
// Background CPU microbenchmark, not physical LCD FPS. JSON goes to stdout.
#include "../background_cache.hpp"
#include "../pixel_fill.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <tuple>
#include <vector>

static starsoil::State state;
static starsoil_scene::BackgroundCache cache;
constexpr int W=288,H=288,Top=25;
static std::vector<uint16_t> frame(W*H);
static volatile uint16_t observed;
static void render(int cx,int cy) {
  auto pixel=[](int x,int y,uint16_t c) {
    if (unsigned(x)<W && y>=Top && y<H) frame[y*W+x]=c;
  };
  auto rect=[](int x,int y,int w,int h,uint16_t c) {
    starsoil_pixels::rectangle(frame.data(),W,std::max(0,x),std::max(Top,y),
                             std::min(W,x+w),std::min(H,y+h),c);
  };
  auto line=[&](int x,int y,int ex,int ey,uint16_t c) {
    int dx=std::abs(ex-x),sx=x<ex?1:-1,dy=-std::abs(ey-y),sy=y<ey?1:-1,e=dx+dy;
    for (;;) {pixel(x,y,c);if(x==ex && y==ey)break;int v=e*2;
      if(v>=dy){e+=dy;x+=sx;}if(v<=dx){e+=dx;y+=sy;}}
  };
  starsoil_scene::draw_world(state,W,H,cx,cy,12345,rect,line,pixel,
    [](int x,int y){return frame[y*W+x];},Top,false
#if __has_include("../background_dither.hpp")
    ,[](int x,int y,int count,const starsoil_dither::Row &row) {
      starsoil_dither::fill(frame.data()+y*W+x,size_t(count),row.data(),unsigned(x)&3);
    }
#endif
    );
}
int main() {
  state.width=384;state.height=112;state.clock=100000;
  std::fill(std::begin(state.heights),std::end(state.heights),uint8_t(32));
  std::puts("{\"metric\":\"native background CPU microseconds per frame, 288x288; not device FPS\",\"cases\":[");
  bool comma=false;
  for (const auto &[name,cy,cached,night] : {
       std::tuple{"surface redraw",300,false,false},
       std::tuple{"cave transition redraw",420,false,false},
       std::tuple{"deep cave redraw",1200,false,false},
       std::tuple{"night redraw",300,false,true},
       std::tuple{"surface cached",300,true,false},
       std::tuple{"deep cave cached",1200,true,false}}) {
    state.clock=night?10000:100000;
    auto key=starsoil_scene::BackgroundKey::make(state,W,H,500,cy,12345,Top);
    for (int i=0;i<4;++i) {if(!cache.restore(key,frame.data())){render(500,cy);cache.store(key,frame.data());}}
    std::vector<double> samples;
    for(int batch=0;batch<15;++batch) {
      auto began=std::chrono::steady_clock::now();
      for (int i=0;i<200;++i) {
        if (!cached) render(500+(i%32),cy);
        else if(!cache.restore(key,frame.data())){render(500,cy);cache.store(key,frame.data());}
        observed=frame[(Top+((i*17)%(H-Top)))*W+(i*13)%W];
      }
      samples.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-began).count()/200);
    }
    std::sort(samples.begin(),samples.end());
    std::printf("%s{\"name\":\"%s\",\"median_us\":%.3f,\"min_us\":%.3f,\"max_us\":%.3f}",
                comma?",\n":"",name,samples[7],samples.front(),samples.back());comma=true;
  }
  std::puts("\n]}");
}
