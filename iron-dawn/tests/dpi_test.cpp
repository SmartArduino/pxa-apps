// SPDX-License-Identifier: MIT
#include "hud_layout.hpp"
#include "story.hpp"
#include "story_text.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace iron;
int main(){
 for(float font:{1.f,1.25f}){
  auto plan=DisplayPlan::fit(480,480,305*65536u/160,unsigned(font*65536));
  auto view=GameViewport::fit(plan.width,plan.height);assert(!plan.wide&&view.width==480&&view.height==480);
  NativeText native;native.reset(plan.width,plan.height,view.width,view.height,plan.text_scale(view.width),plan.minimum_text_scale());
  std::vector<uint16_t> pixels(480*480);Raster r(pixels.data(),480,480,480);r.native_text=&native;
  assert(r.text_height("部队",12)==24);assert(r.text_height("矿石",10)==24);
  CompactHudLayout hud(480,480,480,305/160.f,font);assert(hud.line>=28&&hud.row>=58&&hud.status_height>=64);
  for(auto mode:{DockPanel::units,DockPanel::buildings,DockPanel::tactics}){auto panel=hud.overlay(mode);assert(panel.y>hud.status_height&&panel.y+panel.h==hud.top);}
  for(int i=0;i<5;++i){auto name=hud.name(i,DockPanel::units),price=hud.price(i,DockPanel::units),card=hud.card(i,DockPanel::units);assert(r.measure("坦克",12)<=name.w&&r.text_height("坦克",12)<=name.h);assert(price.y+price.h<=card.y+card.h-4);assert(r.measure("450",12)<=price.w);}
  // A long label must ellipsize instead of silently reverting to 12 px.
  r.label({20,100,84,28},"先锋坦克",cream,12);assert(native.count==1);assert(native.style("中",native.commands[0].size).scale==2);
  r.label({24,400,102,hud.row},"战役档案",cream,15);auto menu=native.commands[native.count-1];assert(r.measure("战役档案",menu.size)<=102);assert(r.text_height("战役档案",menu.size)<hud.row);
  // Every chapter and radio line survives wrapping and pagination at 305 DPI.
  auto check=[&](const std::array<const char*,3>& paragraphs){Rect area{94,138,362,164};StoryText text(r,paragraphs,area.w);std::string original,visible;for(auto p:paragraphs)original+=p;for(unsigned page=0;page<text.pages(r,area);++page){native.reset(480,480,480,480,plan.text_scale(480),plan.minimum_text_scale());text.draw(r,area,page);for(unsigned i=0;i<native.count;++i){auto c=native.commands[i];assert(c.y>=area.y&&c.y+r.text_height("中",c.size)<=area.y+area.h);visible.append(native.strings.data()+c.offset,c.length);}}assert(original==visible);};
  for(auto& chapter:chapters)check(chapter.briefing);for(auto& radio:radio_scripts)check(radio.lines);
 }
 auto normal=DisplayPlan::fit(480,480);assert(normal.text_scale(480)==1&&normal.minimum_text_scale()==1);
 std::puts("305 DPI: 24px minimum text, touch rows, readable cards and lossless campaign/radio pagination passed");
}
