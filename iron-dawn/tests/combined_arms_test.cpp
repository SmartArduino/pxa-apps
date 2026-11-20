// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cassert>
#include <cstdio>

using namespace iron;

void advance(World& w,float seconds){
 for(int i=0;i<int(20*seconds);++i)w.tick(.05f);
}

int main(){
 {
  World w;w.add(hq,0,{3,3});w.add(hq,1,{24,24});
  for(int y=0;y<28;++y){w.terrain[y*N+10]=water;w.terrain[y*N+12]=ridge;}
  w.terrain[13*N+10]=bridge;w.power_update();
  int boat=w.add(patrol_boat,0,{10.5f,19.5f});
  int flyer=w.add(rotorcraft,0,{7.5f,17.5f});
  w.select_one(boat);w.order({15.5f,17.5f});assert(!w.e[boat].moving);
  w.order({10.5f,8.5f});assert(w.e[boat].moving);
  w.select_one(flyer);w.order({19.5f,17.5f});
  advance(w,10);
  assert(w.e[boat].p.y<9.5f&&w.open_for(patrol_boat,w.e[boat].p));
  assert(w.e[flyer].p.x>18.f&&w.fog[cell({24.5f,17.5f})]);
 }
 {
  World w;w.add(hq,0,{3,3});w.add(hq,1,{25,25});
  int armor=w.add(tank,0,{10,13}),sky=w.add(rotorcraft,1,{13,13}),aa=w.add(aa_tower,0,{10,16});
  w.reveal();w.power_update();w.e[sky].cool=999;
  w.fire(armor,sky);assert(!w.shots[0].alive);
  advance(w,1);assert(w.e[sky].hp==specs[rotorcraft].hp);
  w.add(power,0,{4,7});w.power_update();assert(w.power_net>0);
  advance(w,2);assert(w.e[sky].hp<specs[rotorcraft].hp&&w.count(rotorcraft,1));
 }
 {
  World w;w.init(1);int dock=w.nearest({26,27},shipyard,0);
  assert(w.can_train_at(patrol_boat,dock)&&!w.can_train_at(rotorcraft,dock));
  assert(w.can_place(shipyard,{26,31})&&!w.can_place(shipyard,{37,25}));
  assert(w.queue(patrol_boat,dock));
  advance(w,7);assert(w.count(patrol_boat)==2);
  for(const auto& a:w.e)if(a.hp>0&&a.kind==patrol_boat)assert(w.open_for(a.kind,a.p));
 }
 {
  World w;w.init(3);int pad=w.nearest({5,33},helipad,0);
  assert(w.can_train_at(rotorcraft,pad)&&w.queue(rotorcraft,pad));
  advance(w,11);assert(w.count(rotorcraft)==2);
 }
 {
  Save s;auto& w=s.world;w.init(0);int home=w.nearest(w.base_position(),hq,0);
  assert(w.queue(commander,home)&&!w.queue(commander,home));
  advance(w,12);int hero=w.nearest(w.base_position(),commander,0);
  assert(hero>=0&&!w.queue(commander,home));w.select_one(hero);
  assert(!w.commander_support());
  auto p=w.e[hero].p;int wounded=w.add(tank,0,{p.x+2,p.y});
  int far=w.add(rifle,0,{p.x+8,p.y});
  int aircraft=w.add(rotorcraft,0,p);
  w.e[wounded].hp=100;w.e[far].hp=10;w.e[aircraft].hp=50;
  assert(w.commander_support());
  assert(w.e[wounded].hp>200&&w.e[far].hp==10&&w.e[aircraft].hp==50);
  assert(!w.commander_support()&&w.mission.support_left==support_cooldown);
  s.checksum=checksum(s);Save restored;
  assert(decode_save(std::as_bytes(std::span{&s,1}),restored));
  assert(restored.world.mission.support_left==support_cooldown&&restored.world.count(commander)==1);
 }
 std::puts("Ship production and channel paths, aircraft traversal, powered anti-air, unique commander, support and saved cooldown passed");
}
