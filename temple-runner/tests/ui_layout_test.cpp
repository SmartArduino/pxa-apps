// SPDX-License-Identifier: MIT
#include "../ui_layout.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <utility>
using namespace relic;
int main(){
 for(auto size:{std::pair{296,240},std::pair{320,240},std::pair{240,240},std::pair{480,480},std::pair{480,854},std::pair{800,480},std::pair{1080,1920},std::pair{1920,1080}}){
  float u=ui_pixel_scale(size.first,size.second);UiLayout ui(size.first,size.second,u);
  for(auto r:{ui.record,ui.start,ui.help,ui.quality,ui.pause,ui.paused,ui.resume,ui.sound,ui.home,ui.result,ui.rescue,ui.retry,ui.guide}){
   assert(r.x>=0&&r.y>=0&&r.x+r.w<=size.first+.01f&&r.y+r.h<=size.second+.01f);
   assert(r.contains(r.cx(),r.cy()));
  }
  assert(!ui.start.contains(ui.help.cx(),ui.help.cy()));
  assert(!ui.resume.contains(ui.sound.cx(),ui.sound.cy()));
  assert(!ui.sound.contains(ui.home.cx(),ui.home.cy()));
  assert(!ui.rescue.contains(ui.retry.cx(),ui.retry.cy()));
  assert(std::abs(ui.paused_heading.cx()-size.first*.5f)<.01f);
  assert(std::abs(ui.score.cx()-size.first*.5f)<.01f);
  for(float density:{.75f,1.f,1.25f,1.5f,2.f,3.f}){
   float physical=24*u,logical=physical/density;
   assert(std::abs(logical*density-physical)<.01f);
  }
 }
 {
  constexpr float side=412,radius=side*.5f;
  UiLayout round(side,side,ui_pixel_scale(side,side),radius*.45f);
  UiLayout hud(side,side,ui_pixel_scale(side,side),radius*.45f,ui_hud_top(side,radius*.45f,0));
  for(auto r:{round.record,round.start,round.help,round.quality,hud.pause})for(float x:{r.x,r.x+r.w})for(float y:{r.y,r.y+r.h})
   assert(std::hypot(x-radius,y-radius)<radius);
 }
 assert(ui_pixel_scale(1080,1920)>3.6f); // Regression: no 1.6x ceiling.
 assert(std::abs(ui_pixel_scale(1080,1920)/ui_pixel_scale(480,854)-2.25f)<.001f);
 puts("UI layout passed: eight resolutions, six densities, safe bounds, distinct hit targets and centered content");
}
