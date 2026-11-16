// SPDX-License-Identifier: MIT
#include "../save.hpp"
#include "../fx.hpp"
#include <cassert>
#include <iostream>
#include <vector>
using namespace iron;
void advance(World& w,float seconds){for(int i=0;i<int(seconds*20);++i)w.tick(.05f);}
World arena(){World w;w.add(hq,0,{3,3});w.add(hq,1,{25,25});w.power_update();return w;}
int main(){
 {auto w=arena();int cache=w.add(supply,2,{12,12});int unit=w.add(tank,0,{10,12});w.e[unit].hp=200;int credits=w.credits;advance(w,5.2f);assert(w.e[cache].hp==0&&w.credits==credits+280);assert(w.e[unit].hp==326);assert(w.terrain[cell({12,12})]==3);advance(w,6);assert(w.credits==credits+280);}
 {auto w=arena();int cache=w.add(supply,2,{12,12});int a=w.add(tank,0,{10,12}),b=w.add(tank,1,{14,12});w.e[a].cool=w.e[b].cool=99;advance(w,6);assert(w.e[cache].hp>0&&w.e[cache].progress==0);}
 {auto w=arena();int cache=w.add(supply,2,{12,12});w.add(harvester,0,{10,12});advance(w,6);assert(w.e[cache].hp>0);}
 {auto w=arena();int unit=w.add(tank,0,{10,12});w.e[unit].hp=200;
  for(int k=0;k<2;++k){int enemy=w.add(rifle,1,{11,12});w.e[enemy].hp=1;w.e[enemy].cool=99;w.fire(unit,enemy);advance(w,.3f);assert(w.e[unit].kills==k+1);}
  assert(veteran_rank(w.e[unit])==1&&w.e[unit].hp==284);int enemy=w.add(tank,1,{11,12});w.fire(unit,enemy);bool found=false;for(auto& s:w.shots)if(s.alive){found=true;assert(shot_source(s)==unit&&shot_target(s)==enemy&&shot_team(s)==0);assert(s.damage>47&&s.damage<48);}assert(found);
  w.e[unit].selected=true;w.e[unit].moving=true;w.e[unit].target=enemy;w.stop_selected();assert(!w.e[unit].moving&&w.e[unit].target==-1);
 }
 {auto w=arena();int a=w.add(tank,0,{10,12}),b=w.add(rifle,1,{11,12});w.e[b].hp=1;w.e[b].cool=99;w.fire(a,b);++w.e[a].cargo;advance(w,.3f);assert(w.e[a].kills==0&&w.e[b].hp==0);}
 {auto w=arena();int a=w.add(tank,0,{10,12}),b=w.add(rifle,1,{11,12});w.e[b].hp=1;w.e[b].cool=99;w.shots[0]={{10,12},{10,12},{11,12},0,.22f,42,int16_t(b),0,tank,true};advance(w,.3f);assert(w.e[b].hp==0&&w.e[a].kills==0);}
 {World w;w.init(1);int producer=w.nearest({7,5},factory,0);w.select_one(producer);w.order({10,8});assert(w.e[producer].moving);int before=w.count(tank);assert(w.queue(tank));advance(w,6.3f);assert(w.count(tank)==before+1);bool rallied=false;for(auto& a:w.e)if(a.hp>0&&a.team==0&&a.kind==tank&&dist(a.goal,{10,8})<.01f)rallied=true;assert(rallied);}
 {World w;w.init(0);w.credits=10000;for(int i=0;i<31;++i)w.add(rifle,0,{7,12});assert(w.army()==35);assert(w.queue(rifle));assert(!w.queue(rifle));}
 {Save modern;modern.world.init(0);modern.checksum=checksum(modern);assert(modern.version==6&&valid_save(modern));Save copy;assert(decode_save(std::as_bytes(std::span{&modern,1}),copy));assert(copy.checksum==checksum(copy));modern.version=7;assert(!valid_save(modern));}
 {std::vector<uint16_t> pixels(480*320+2,0xa55a);Raster r(pixels.data()+1,480,320,480);for(unsigned fx=0;fx<vfx::clips.size();++fx)for(float t:{0.f,.2f,.5f,.85f})for(float zoom:{.72f,1.f,1.3f}){draw_fx(r,fx,0,0,t,zoom);draw_fx(r,fx,479,319,t,zoom);}assert(pixels.front()==0xa55a&&pixels.back()==0xa55a);}
 std::cout<<"Supply, contest, veterans, shot attribution, rally, stop, queue cap, v4 migration and pixelfx bounds passed\n";
}
