// SPDX-License-Identifier: MIT
#include "hud_layout.hpp"
#include "story.hpp"
#include "story_text.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace iron;
void check_label(Raster& canvas,Raster& output,Rect area,std::string_view label,int size,int align=1,int expected_scale=0){
 auto* native=canvas.native_text;
 if(native) native->reset(native->width,native->height,native->canvas_width,native->canvas_height,native->text_scale,native->minimum_scale);
 std::fill_n(output.pixels,output.stride*output.h,uint16_t{0});
 canvas.label(area,label,cream,size,align);
 if(native){
  draw_native_text(*native,output);
  int x=pixel_scale(area.x,output.w,canvas.w),y=pixel_scale(area.y,output.h,canvas.h);
  area={x,y,pixel_scale(area.x+area.w,output.w,canvas.w)-x,pixel_scale(area.y+area.h,output.h,canvas.h)-y};
 }
 int left=output.w,top=output.h,right=-1,bottom=-1;
 for(int y=0;y<output.h;++y)for(int x=0;x<output.w;++x)if(output.pixels[y*output.stride+x]==cream){
  assert(area.has(x,y));
  left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);
 }
 assert(right>=left&&bottom>=top);
 if(expected_scale){
  auto ink=Raster::ink_bounds(label,0);
  assert(right-left+1==ink.w*expected_scale&&bottom-top+1==ink.h*expected_scale);
 }
 assert(std::abs((top-area.y)-(area.y+area.h-1-bottom))<=1);
 if(align==1)assert(std::abs((left-area.x)-(area.x+area.w-1-right))<=1);
 else if(align==0)assert(left==area.x);
 else assert(right==area.x+area.w-1);
}

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
  check_label(r,r,{24,400,102,hud.row},"战役档案",15,1,2);
  // Every chapter and radio line survives wrapping and pagination at 305 DPI.
  auto check=[&](const std::array<const char*,3>& paragraphs){Rect area{94,138,362,164};StoryText text(r,paragraphs,area.w);std::string original,visible;for(auto p:paragraphs)original+=p;for(unsigned page=0;page<text.pages(r,area);++page){native.reset(480,480,480,480,plan.text_scale(480),plan.minimum_text_scale());text.draw(r,area,page);for(unsigned i=0;i<native.count;++i){auto c=native.commands[i];assert(c.y>=area.y&&c.y+r.text_height("中",c.size)<=area.y+area.h);visible.append(native.strings.data()+c.offset,c.length);}}assert(original==visible);};
  for(auto& chapter:chapters)check(chapter.briefing);for(auto& radio:radio_scripts)check(radio.lines);
  check_label(r,r,{17,19,101,29},"停止",15);
  check_label(r,r,{17,19,65,29},"保存并返回指挥部",15);
  check_label(r,r,{17,19,80,17},"已保存",15);
 }
 // ESPP4-A draws the battle HUD directly into its 2x mapped surface.
 auto native_plan=DisplayPlan::fit(854,480,98304);
 assert(!native_plan.wide&&native_plan.scale==2&&native_plan.width==427&&native_plan.height==240);
 CompactHudLayout battle(240,480,427,1.5f);
 NativeText mapped_text;
 mapped_text.reset(427,240,427,240,native_plan.text_scale(427),native_plan.minimum_text_scale());
 std::vector<uint16_t> mapped_pixels(427*240);
 Raster mapped(mapped_pixels.data(),427,240,427);
 mapped.native_text=&mapped_text;
 assert(battle.status_height*2>=72&&battle.row*2>=44);
 assert(battle.map().w*2==120&&battle.overlay(DockPanel::buildings).y>battle.status_height);
 check_label(mapped,mapped,battle.command(0),"生产",15,1,1);
 check_label(mapped,mapped,battle.command(1),"建造",15,1,1);
 check_label(mapped,mapped,battle.command(3),"停止",15,1,1);
 check_label(mapped,mapped,battle.name(0,DockPanel::buildings),"电站",12,1,1);
 auto normal=DisplayPlan::fit(480,480);assert(normal.text_scale(480)==1&&normal.minimum_text_scale()==1);
 for(auto [width,height]:{std::pair{296,240},std::pair{800,480}}){
  auto plan=DisplayPlan::fit(width,height);
  auto view=plan.wide?GameViewport{width,height}:GameViewport::fit(width,height);
  NativeText native;native.reset(width,height,view.width,view.height);
  std::vector<uint16_t> pixels(width*height);
  Raster canvas(nullptr,view.width,view.height,view.width),output(pixels.data(),width,height,width);
  canvas.native_text=&native;
  for(int align:{0,1,2}){
   check_label(canvas,output,{19,27,91,31},"战役档案",15,align);
   check_label(canvas,output,{18,51,67,27},"+950",15,align);
  }
  check_label(canvas,output,{25,85,114,23},"作战 01:21 · 击毁 12",12);
 }
 std::vector<uint16_t> pixels(200*96);Raster direct(pixels.data(),200,96,200);
 check_label(direct,direct,{13,9,99,33},"15秒",20);
 std::puts("Pixel bounds and centering passed on the native text path and direct raster; 305 DPI pagination retained all dialogue");
}
