// SPDX-License-Identifier: MIT
#include "hud_layout.hpp"
#include <cassert>
#include <cstdio>
#include <array>
using namespace iron;
bool inside(Rect child,Rect parent){return child.x>=parent.x&&child.y>=parent.y&&child.x+child.w<=parent.x+parent.w&&child.y+child.h<=parent.y+parent.h;}
bool overlaps(Rect a,Rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
int main(){
 for(auto [pw,ph]:std::array<std::pair<int,int>,9>{{{240,240},{296,240},{320,240},{480,320},{480,480},{320,640},{480,854},{854,480},{480,270}}}){
  auto view=GameViewport::fit(pw,ph);int h=view.height,w=view.width;assert(std::abs(double(w)/h-double(pw)/ph)<.005);if(!CompactHudLayout::applies(pw,ph)){assert(pw==480&&ph==854);continue;}CompactHudLayout layout(h,ph,w);int projection_bottom=layout.top;assert(layout.top>50);assert((h-layout.top)*ph/h<=(pw<=320?40:60));assert((layout.top-50)>(HudLayout(h).top-50));
  Rect screen{0,0,w,h},rail{0,layout.top,w,h-layout.top};
  for(int i=0;i<5;++i){auto r=layout.command(i);assert(inside(r,rail));assert(r.h*ph/h>=28);for(int j=0;j<i;++j)assert(!overlaps(r,layout.command(j)));}
  for(auto mode:{DockPanel::units,DockPanel::buildings,DockPanel::tactics}){auto panel=layout.overlay(mode);assert(inside(panel,screen)&&panel.y>50&&panel.y+panel.h==layout.top);assert(layout.top==projection_bottom);
   if(mode==DockPanel::tactics){for(int i=0;i<8;++i){auto r=layout.tactic(i);assert(inside(r,panel)&&r.h*ph/h>=28);for(int j=0;j<i;++j)assert(!overlaps(r,layout.tactic(j)));}}
   else for(int i=0;i<5;++i){auto card=layout.card(i,mode),image=layout.image(i,mode),name=layout.name(i,mode),price=layout.price(i,mode),progress=layout.progress(i,mode);assert(inside(card,panel)&&inside(image,card)&&inside(name,card)&&inside(price,card)&&inside(progress,card));assert(image.y+image.h<=name.y&&name.y+name.h<=price.y&&price.y+price.h<=progress.y);assert(name.h*ph/h>=14);for(int j=0;j<i;++j)assert(!overlaps(card,layout.card(j,mode)));}
  }
  assert(layout.overlay(DockPanel::closed).h==0&&layout.overlay(DockPanel::closed).y==projection_bottom);
 }
 DockState dock;assert(!dock.open());dock.toggle(DockPanel::units);assert(dock.panel==DockPanel::units);dock.toggle(DockPanel::buildings);assert(dock.panel==DockPanel::buildings);dock.toggle(DockPanel::buildings);assert(!dock.open());dock.toggle(DockPanel::tactics);assert(dock.open());dock.close();assert(!dock.open());
 std::puts("Compact rail touch sizes, drawer/card clipping, mutually exclusive panels and stable projection bounds passed at nine display sizes");
}
