// SPDX-License-Identifier: MIT
#include "hud_layout.hpp"
#include <cassert>
#include <cstdio>
using namespace iron;
bool inside(Rect a,Rect b){return a.x>=b.x&&a.y>=b.y&&a.x+a.w<=b.x+b.w&&a.y+a.h<=b.y+b.h;}
bool overlap(Rect a,Rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
int main(){
 for(auto size:{std::pair{854,480},std::pair{1024,600},std::pair{1280,800},std::pair{1366,768},std::pair{1920,1080},std::pair{3840,2160}})for(int dpi:{120,160,200,240,320,480})for(float fs:{1.f,1.25f,1.5f,2.f}){
  auto p=DisplayPlan::fit(size.first,size.second,dpi*65536u/160,uint32_t(fs*65536));assert(p.scale==1||p.scale==2||p.scale==4);assert(int64_t((p.width*2+63)/64*64)*p.height*2<=2097152);assert(p.offset_x>=0&&p.offset_y>=0);if(!p.wide)continue;
  LargeHudLayout l(p.width,p.height,p.units_per_dp,p.font_scale);Rect screen{0,0,p.width,p.height};assert(l.world_width>=p.width*.62f&&l.bottom-l.top>p.height*.6f);assert(inside(l.sidebar(),screen)&&inside(l.map(),l.sidebar()));if(l.card_height<l.line*2)std::fprintf(stderr,"%dx%d dpi=%d font=%.2f canvas=%dx%d card=%d line=%d\n",size.first,size.second,dpi,fs,p.width,p.height,l.card_height,l.line);assert(l.card_height>=l.line*2);
  assert(inside(l.selection_info(),l.sidebar())&&inside(l.squad_button(),l.selection_info()));
  assert(!overlap(l.map(),l.selection_info())&&!overlap(l.squad_button(),l.tab(1)));
  auto page=page_column(p.width,p.height);
  assert(inside(page,screen)&&page.w<=760&&page.x>=24&&std::abs(p.width-(2*page.x+page.w))<=1);
  for(int i=0;i<6;++i){assert(inside(l.card(i),l.sidebar()));assert(inside(l.command(i),screen));assert(!overlap(l.command(i),l.sidebar()));assert(l.command(i).h*p.scale*160.f/dpi>=24);for(int j=0;j<i;++j){assert(!overlap(l.card(i),l.card(j)));assert(!overlap(l.command(i),l.command(j)));}}
  for(int i=0;i<2;++i){assert(inside(l.tab(i),l.sidebar())&&inside(l.foot(i),l.sidebar()));for(int j=0;j<6;++j)assert(!overlap(l.tab(i),l.card(j))&&!overlap(l.foot(i),l.card(j)));}
  NativeText text;text.reset(p.width,p.height,p.width,p.height,p.units_per_dp*p.font_scale);auto style=text.style("部队",12);assert(style.scale>=1&&style.scale<=8);
 }
 assert(DisplayPlan::fit(854,480).wide);assert(!DisplayPlan::fit(854,480,98304).wide);assert(DisplayPlan::fit(1280,800,98304).wide);assert(!DisplayPlan::fit(1280,800,131072).wide);
 std::puts("Large HUD bounds, sidebar isolation, touch sizes, DPI/font scaling and mapped buffer budget passed across six resolutions and six DPIs");
}
