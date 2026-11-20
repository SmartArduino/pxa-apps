// SPDX-License-Identifier: MIT
#include "../selection.hpp"
#include "../save.hpp"
#include <cassert>
#include <iostream>
#include <vector>
using namespace iron;
struct Scene {
 World w;Battlefield f;SelectionTool tool;
 Scene(int height=360,float zoom=1){f.height=height;f.top=43;f.bottom=height-113;f.zoom=zoom;f.focus({8,10});
  w.add(tank,0,{8.5f,11});w.add(tank,0,{9.5f,12});w.add(rifle,0,{8,13});w.add(rocket,0,{10.5f,11.5f});
  w.add(harvester,0,{8.5f,11});w.add(hq,0,{8.5f,11});w.add(tank,1,{8.5f,11});w.add(relay,2,{8.5f,11});w.add(tank,0,{8.5f,11});w.e[8].hp=0;
 }
 Rect area()const{return {0,f.top,f.width,f.bottom-f.top};}Rect map()const{return {390,f.top+8,80,80};}Rect button()const{return {390,f.top+96,80,28};}
 bool begin(int x,int y,bool ui=false,bool placing=false){tool.armed=true;return tool.begin(x,y,area(),map(),button(),ui,placing);}
 std::vector<int> ids()const{std::vector<int> result;for(int i=0;i<MAX;++i)if(w.e[i].selected)result.push_back(i);return result;}
};
int main(){
 {Scene s;assert(s.begin(210,140));s.tool.move(248,187);assert(s.tool.preview(s.w,s.f)==2);assert(s.tool.commit(s.w,s.f)==2);assert((s.ids()==std::vector<int>{0,1}));assert(!s.tool.armed&&!s.tool.tracking);for(auto& a:s.w.e)assert(!a.moving);}
 {Scene s;assert(s.begin(248,187));s.tool.move(210,140);s.tool.commit(s.w,s.f);assert((s.ids()==std::vector<int>{0,1}));}
 {Scene s;s.w.select_all();assert(s.begin(210,140));s.tool.move(248,187);s.tool.cancel();assert(s.w.selected()==4);assert(s.tool.commit(s.w,s.f)==-1);}
 {Scene s;s.w.select_all();assert(s.begin(10,180));s.tool.move(60,205);assert(s.tool.commit(s.w,s.f)==0);assert(s.ids().empty());}
 {Scene s;assert(s.begin(230,152));assert(s.tool.commit(s.w,s.f)==1);assert((s.ids()==std::vector<int>{0}));assert(!s.w.e[0].moving);}
 {Scene s;s.w.select_all();assert(!s.begin(425,90));assert(!s.begin(425,150));assert(!s.begin(200,12));assert(!s.begin(200,270));assert(!s.begin(200,180,true));assert(!s.begin(200,180,false,true));assert(s.w.selected()==4);}
 {Scene s;s.tool.armed=false;assert(!s.tool.begin(210,140,s.area(),s.map(),s.button(),false,false));}
 {Scene s;assert(s.begin(210,140));s.tool.move(-900,900);auto b=s.tool.bounds();assert(b.x==0&&b.y>=s.f.top&&b.y+b.h<=s.f.bottom);}
 {Scene s;int hidden=s.w.add(tank,0,s.f.unproject(425,90));s.w.e[hidden].p=s.f.unproject(425,98);assert(s.begin(1,80));s.tool.move(479,240);s.tool.commit(s.w,s.f);assert(!s.w.e[hidden].selected);}
 {Scene s;assert(s.begin(210,140));s.tool.move(248,187);auto cx=s.f.camx,cy=s.f.camy;s.tool.commit(s.w,s.f);assert(s.f.camx==cx&&s.f.camy==cy);s.w.order({12,13});for(int i=0;i<4;++i)assert(s.w.e[i].moving==(i==0||i==1));}
 {Scene s;
  s.w.select_one(0);s.w.select_one(1,true);
  assert(s.w.assign_squad(1)&&s.w.squad_size(1)==2&&s.w.squad_selected(1));
  s.w.select_one(2);s.w.select_one(4,true);
  assert(s.w.assign_squad(2)&&s.w.squad_size(2)==1);
  s.w.select_one(5);
  assert(!s.w.assign_squad(1)&&s.w.squad_size(1)==2);
  assert(!s.w.recall_squad(3)&&s.ids()==std::vector<int>{5});
  assert(s.w.recall_squad(1)&&(s.ids()==std::vector<int>{0,1}));
  s.w.order({14,13});
  assert(s.w.e[0].moving&&s.w.e[1].moving&&!s.w.e[2].moving);
  s.w.damage(0,10000);
  assert(s.w.squad_size(1)==1);
  int reinforcement=s.w.add(rifle,0,{8,10});
  assert(reinforcement==0&&s.w.squads[0]==0);
  s.w.select_one(1);
  assert(s.w.assign_squad(3)&&s.w.squad_size(1)==0);
  Save saved;saved.world=s.w;
  saved.checksum=checksum(saved);
  Save restored;assert(decode_save(std::as_bytes(std::span{&saved,1}),restored));
  assert(restored.world.squad_size(2)==1&&restored.world.squad_size(3)==1);
  assert(restored.world.recall_squad(2)&&restored.world.e[2].selected&&!restored.world.e[1].selected);
 }
 for(int height:{320,389,480,854})for(float zoom:{.72f,1.f,1.3f}){
  Scene s(height,zoom);auto a=s.f.project(s.w.e[0].p,8),b=s.f.project(s.w.e[1].p,8);
  assert(s.begin(int(a.x)-6,int(std::min(a.y,b.y))-5));s.tool.move(int(b.x)+6,int(std::max(a.y,b.y))+5);assert(s.tool.commit(s.w,s.f)==2);
 }
 {Scene s;std::vector<uint16_t> pixels(480*360+2,0xa55a);Raster r(pixels.data()+1,480,360,480);assert(s.begin(210,140));s.tool.move(248,187);s.tool.draw(r,s.w,s.f);assert(pixels.front()==0xa55a&&pixels.back()==0xa55a);assert(s.w.selected()==0);}
 std::cout<<"Selection: reverse drag, body picking, exclusions, cancellation, bounds, zoom, commands and overlay passed\n";
}
