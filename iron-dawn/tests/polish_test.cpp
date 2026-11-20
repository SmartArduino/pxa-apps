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
 {World w;w.init(0);int first=w.nearest(w.base_position(),barracks,0);
  int second=w.add(barracks,0,{5,17});w.power_update();w.select_one(second);
  assert(w.selected_producer()==second&&!w.queue(tank,second));
  w.order({7,17});assert(w.queue(rocket,second)&&w.jobs[0].producer==second+1);
  assert(w.queue(rifle,first));int cash=w.credits;w.damage(first,10000);
  assert(w.job_count==1&&w.credits==cash+specs[rifle].cost);
  advance(w,4.1f);bool produced=false;
  for(auto& a:w.e)if(a.hp>0&&a.kind==rocket&&a.team==0&&dist(a.goal,{7,17})<.01f){
   assert(dist(a.p,w.e[second].p)<3);produced=true;
  }
  assert(produced);
 }
 {Save s;s.world.init(0);auto& w=s.world;int p=w.nearest(w.base_position(),barracks,0);
  assert(w.queue(rifle,p)&&w.start_construction(power));
  w.tick(.05f);s.checksum=checksum(s);Save restored;
  assert(decode_save(std::as_bytes(std::span{&s,1}),restored));
  auto& r=restored.world;assert(r.jobs[0].producer==p+1&&r.construction.left==w.construction.left);
  advance(r,8);assert(r.build_ready()&&r.cancel_construction()&&!r.build_active());
 }
 {auto w=arena();
  int guard=w.add(tank,0,{6,8}),truck=w.add(harvester,0,{11,8}),enemy=w.add(rifle,1,{8,8});
  w.e[enemy].cool=999;w.select_one(guard);
  assert(w.order_escort(truck)&&escort_id(w.e[guard])==truck);
  advance(w,.1f);bool fighting=false;
  for(auto& shot:w.shots)fighting|=shot.alive&&shot_source(shot)==guard;
  assert(fighting&&dist(w.e[guard].p,{6,8})<.15f);
  w.damage(enemy,10000);advance(w,5);
  assert(dist(w.e[guard].p,w.e[truck].p)<2.6f);
  w.e[truck].p={16,8};advance(w,.5f);assert(w.e[guard].moving);
  Save saved;saved.world=w;saved.checksum=checksum(saved);Save restored;
  assert(decode_save(std::as_bytes(std::span{&saved,1}),restored));
  assert(escort_id(restored.world.e[guard])==truck);
  w.damage(truck,10000);assert(escort_id(w.e[guard])<0&&!w.e[guard].moving);
  assert(w.add(harvester,0,{22,8})==truck);
  advance(w,.8f);assert(escort_id(w.e[guard])<0&&!w.e[guard].moving);
 }
 {auto w=arena();
  int unit=w.add(tank,0,{8,8}),enemy=w.add(rifle,1,{10,8});
  w.e[enemy].cool=999;w.select_one(unit);
  assert(!w.order_retreat({3,3})&&w.order_retreat({17,8}));
  assert(w.e[unit].target==retreat_order);
  advance(w,1.5f);
  assert(w.e[unit].p.x>9&&w.e[enemy].hp==specs[rifle].hp);
  for(auto& shot:w.shots)assert(!shot.alive||shot_source(shot)!=unit);
  Save saved;saved.world=w;saved.checksum=checksum(saved);Save restored;
  assert(decode_save(std::as_bytes(std::span{&saved,1}),restored));
  auto& battle=restored.world;assert(battle.e[unit].target==retreat_order);
  advance(battle,8);assert(battle.e[unit].target==-1&&!battle.e[unit].moving);
  battle.e[enemy].p={battle.e[unit].p.x+2,battle.e[unit].p.y};
  advance(battle,.5f);assert(battle.e[enemy].hp<specs[rifle].hp);
 }
 {Save modern;modern.world.init(0);modern.checksum=checksum(modern);assert(modern.version==save_version&&valid_save(modern));Save copy;assert(decode_save(std::as_bytes(std::span{&modern,1}),copy));assert(copy.checksum==checksum(copy));modern.version=save_version+1;assert(!valid_save(modern));}
 {std::vector<uint16_t> pixels(480*320+2,0xa55a);Raster r(pixels.data()+1,480,320,480);for(unsigned fx=0;fx<vfx::clips.size();++fx)for(float t:{0.f,.2f,.5f,.85f})for(float zoom:{.72f,1.f,1.3f}){draw_fx(r,fx,0,0,t,zoom);draw_fx(r,fx,479,319,t,zoom);}assert(pixels.front()==0xa55a&&pixels.back()==0xa55a);}
 std::cout<<"Supply, contest, veterans, shot attribution, rally, stop, queue cap, v4 migration and pixelfx bounds passed\n";
}
