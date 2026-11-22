// SPDX-License-Identifier: MIT
#include "art.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
int main(){
 constexpr int w=480,h=480;std::vector<uint16_t> before(w*h,navy),after(w*h,navy);World world;world.init(1);world.select_all();world.seen[22*N+23]=1;Battlefield direct,cached;direct.cache_ground=false;direct.bottom=cached.bottom=h-113;direct.top=cached.top=50;direct.focus({25,24});cached.focus({25,24});Raster a(before.data(),w,h,w),b(after.data(),w,h,w);
 auto compare=[&](){direct.draw(a,world);cached.draw(b,world);assert(before==after);};compare();assert(cached.ground.misses==1);compare();assert(cached.ground.hits==1);
 for(int i=0;i<N*N;++i)if(world.ore[i]>1){--world.ore[i];break;}compare();assert(cached.ground.misses==1);
 int visible=24*N+25;world.fog[visible]^=1;compare();assert(cached.ground.misses==2);world.seen[visible]^=1;compare();world.ore[visible]=80;compare();world.ore[visible]=0;compare();world.terrain[visible]=5;compare();
 direct.clock=cached.clock=.51f;compare();direct.zoom=cached.zoom=1.3f;direct.focus({12,13});cached.focus({12,13});compare();direct.camx=cached.camx=30;compare();
 world.chapter=2;compare();assert(cached.ground.misses==10);assert(cached.ground.pixels.size()==w*cached.bottom);
 // An off-screen change must not redraw the current ground.
 world.init_defense(7,Faction::northern,DefenseMap::reef);
 direct.focus({36,25});cached.focus({36,25});
 compare();
 auto misses=cached.ground.misses;
 world.terrain[0]=ground;world.seen[0]=world.fog[0]=1;world.ore[0]=40;
 compare();
 assert(cached.ground.misses==misses);
 // A drag on the odd-width P4 canvas shifts every rendered layer by the
 // same amount. In-bounds drags must not reproject the camera center.
 constexpr int pan_w=427,pan_h=240;
 Battlefield pan;
 pan.width=pan_w;pan.height=pan_h;pan.top=38;pan.bottom=207;pan.zoom=.75f;
 pan.focus({35.4f,24.2f});
 world.init_defense(7,Faction::gray_tower,DefenseMap::reef);
 world.seen.fill(1);world.fog.fill(1);
 std::vector<uint16_t> still(pan_w*pan_h),moved(pan_w*pan_h);
 Raster still_r(still.data(),pan_w,pan_h,pan_w),moved_r(moved.data(),pan_w,pan_h,pan_w);
 pan.draw(still_r,world);
 const auto origin_x=pan.camx,origin_y=pan.camy;
 for(int i=1;i<=8;++i){
  pan.camx-=1;pan.camy-=1;
  pan.clamp_camera(world.side);
  assert(pan.camx==origin_x-i&&pan.camy==origin_y-i);
  pan.draw(moved_r,world);
  for(int y=68;y<156;++y)
   for(int x=133;x<290;++x)
    assert(still[y*pan_w+x]==moved[(y+i)*pan_w+x+i]);
 }
 pan.camx-=8000;pan.camy+=8000;
 pan.clamp_camera(world.side);
 const auto bounded=pan.view_center();
 assert(bounded.x>=.95f&&bounded.x<=world.side-1+.05f&&
        bounded.y>=.95f&&bounded.y<=world.side-1+.05f);
 // Compare the entire viewport, including the repainted strips, on each
 // defense map. Exercise thin strips, diagonal corners and fallback draws.
 Battlefield reference;
 reference.cache_ground=false;
 reference.width=pan.width;reference.height=pan.height;
 reference.top=pan.top;reference.bottom=pan.bottom;reference.zoom=pan.zoom;
 auto compare_pan=[&](){
  reference.camx=pan.camx;reference.camy=pan.camy;reference.clock=pan.clock;
  reference.draw(still_r,world);pan.draw(moved_r,world);
  assert(still==moved);
 };
 for(auto map:{DefenseMap::river,DefenseMap::reef,DefenseMap::ridge}){
  world.init_defense(7,Faction::gray_tower,map);
  world.seen.fill(1);world.fog.fill(1);
  pan.focus({world.side*.5f,world.side*.5f});
  compare_pan();
  auto scrolls=pan.ground.scrolls;
  for(auto delta:{Pt{2,0},Pt{-5,0},Pt{0,3},Pt{0,-7},Pt{-4,2},Pt{6,-3}}){
   pan.camx+=delta.x;pan.camy+=delta.y;
   compare_pan();
  }
  assert(pan.ground.scrolls>scrolls);
  auto tile=cell(pan.view_center());
  auto patches=pan.ground.patches;
  world.fog[tile]=0;world.ore[tile]=50;pan.camx+=2;
  compare_pan();
  assert(pan.ground.patches==patches+1);
  auto scrolls_before_jump=pan.ground.scrolls;
  pan.clock+=.5f;pan.focus({2,3});
  compare_pan();pan.camy+=3;compare_pan();
  assert(pan.ground.scrolls==scrolls_before_jump);
 }
 // A narrow channel animates while the view is scrolling. A newly seen shore
 // changes the water tile beside it, and new pavement changes the road's axis.
 world.init_defense(7,Faction::gray_tower,DefenseMap::reef);
 world.terrain.fill(ground);world.ore.fill(0);world.seen.fill(1);world.fog.fill(1);
 for(int x=15;x<29;++x)world.terrain[21*N+x]=water;
 world.terrain[24*N+23]=road;world.terrain[26*N+23]=bridge;
 world.seen[22*N+22]=0;
 pan.focus({22,22});compare_pan();
 auto patches=pan.ground.patches,scrolls=pan.ground.scrolls;
 pan.clock+=.5f;pan.camx-=2;pan.camy-=2;compare_pan();
 assert(pan.ground.patches==patches+1&&pan.ground.scrolls==scrolls+1);
 world.seen[22*N+22]=1;world.terrain[24*N+25]=road;compare_pan();
 assert(pan.ground.patches==patches+2);
 world.terrain[24*N+23]=crater;world.ore[26*N+23]=50;world.fog[21*N+23]=0;
 compare_pan();
 assert(pan.ground.patches==patches+3);
 world.seen.fill(0);world.fog.fill(0);
 compare_pan();
 assert(pan.ground.patches==patches+3);
 // Keep the interior during a vertical drag with the background visible
 // along the left edge. The screen-space gradient must not scroll.
 pan.focus({8,24});compare_pan();
 scrolls=pan.ground.scrolls;
 pan.camy+=5;compare_pan();
 assert(pan.ground.scrolls==scrolls+1);
 std::puts("Ground cache matches full rendering across scroll strips, water, fog, discovery, roads, damage, ore and map changes");
}
