// SPDX-License-Identifier: MIT
#include "art.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
int main(){
 constexpr int w=480,h=480;std::vector<uint16_t> before(w*h,navy),after(w*h,navy);World world;world.init(1);world.select_all();Battlefield direct,cached;direct.cache_ground=false;direct.bottom=cached.bottom=h-113;direct.top=cached.top=50;direct.focus({8,10});cached.focus({8,10});Raster a(before.data(),w,h,w),b(after.data(),w,h,w);
 auto compare=[&](){direct.draw(a,world);cached.draw(b,world);assert(before==after);};compare();assert(cached.ground.misses==1);compare();assert(cached.ground.hits==1);
 for(int i=0;i<N*N;++i)if(world.ore[i]>1){--world.ore[i];break;}compare();assert(cached.ground.misses==1);
 int visible=10*N+8;world.fog[visible]^=1;compare();assert(cached.ground.misses==2);world.seen[visible]^=1;compare();world.ore[visible]=80;compare();world.ore[visible]=0;compare();world.terrain[visible]=5;compare();
 direct.clock=cached.clock=.51f;compare();direct.zoom=cached.zoom=1.3f;direct.focus({12,13});cached.focus({12,13});compare();direct.camx=cached.camx=30;compare();
 world.chapter=2;compare();assert(cached.ground.misses==10);assert(cached.ground.pixels.size()==w*cached.bottom);
 std::puts("Ground cache matches direct rendering; camera, zoom, water, fog, discovery, ore depletion, scars and chapter invalidate correctly");
}
