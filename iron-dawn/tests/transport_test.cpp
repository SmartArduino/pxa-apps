// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cassert>
#include <cstdio>

using namespace iron;

void advance(World& w,float seconds){
 for(int i=0;i<int(20*seconds);++i)w.tick(.05f);
}

int main(){
 Save save;
 auto& w=save.world;
 w.add(hq,0,{3,3});w.add(hq,1,{24,24});w.next_wave=100000;
 for(int y=1;y<27;++y)
  for(int x=10;x<=13;++x)w.terrain[y*N+x]=water;
 for(int x=10;x<=13;++x)w.terrain[11*N+x]=bridge;
 assert(!w.bank_cell({13.5f,11.5f},{13.5f,12.5f}));
 w.power_update();w.seen.fill(1);
 int ship=w.add(landing_craft,0,{13.5f,14.5f});
 int ids[5];
 for(int i=0;i<5;++i){ids[i]=w.add(i==0?commander:tank,0,{14.5f,13.5f+i*.6f});w.select_one(ids[i],true);}
 assert(w.assign_squad(1)&&w.order_board(ship));
 assert(w.transport_load(ship,true)==4&&!w.order_board(ship));
 advance(w,5);assert(w.transport_load(ship)==4&&deployed(w.e[ids[4]]));
 for(int i=0;i<4;++i)assert(passenger_ship(w.e[ids[i]])==ship&&!w.e[ids[i]].selected);
 w.select_all();
 assert(!w.e[ids[0]].selected&&!w.commander_support()&&w.squad_size(1)==1);
 float passenger_hp=w.e[ids[0]].hp;w.damage(ids[0],10000);
 assert(w.e[ids[0]].hp==passenger_hp);
 save.checksum=checksum(save);Save restored;
 assert(decode_save(std::as_bytes(std::span{&save,1}),restored));
 auto& battle=restored.world;
 assert(battle.transport_load(ship)==4&&battle.count(commander)==1);
 battle.select_one(ship);
 battle.order({10.5f,14.5f});
 assert(!battle.order_land(ship,{9.5f,14.5f}));
 advance(battle,5);
 assert(battle.e[ship].p.x<11&&battle.e[ids[0]].p.x==battle.e[ship].p.x);
 assert(!battle.order_land(ship,{9.5f,11.5f}));
 assert(battle.order_land(ship,{9.5f,14.5f})&&battle.transport_load(ship)==0);
 for(int i=0;i<4;++i){
  auto& unit=battle.e[ids[i]];
  assert(deployed(unit)&&unit.p.x<10&&unit.target==-1);
 }
 assert(battle.squad_size(1)==5&&battle.recall_squad(1));
 assert(battle.order_board(ship));
 advance(battle,4);assert(battle.transport_load(ship)==4);
 int losses=battle.lost;battle.damage(ship,10000);
 assert(battle.lost==losses+5&&battle.transport_load(ship,true)==0);
 int replacement=battle.add(landing_craft,0,{10.5f,14.5f});
 assert(replacement==ship&&battle.transport_load(replacement)==0);
 {
  World river;river.init(1);
  int dock=river.nearest({26,27},shipyard,0);
  assert(river.queue(landing_craft,dock));
  advance(river,9);
  assert(river.count(landing_craft)==2);
 }
 {
  World moving; moving.init(1);
  int boat=moving.nearest({24.5f,30.5f},landing_craft,0);
  moving.select_one(5);
  assert(moving.order_board(boat));
  moving.select_one(boat);moving.order({22.5f,30.5f});
  advance(moving,1);
  assert(boarding_ship(moving.e[5])<0&&moving.transport_load(boat)==0);
 }
 std::puts("Transport queue, reservation, crossing, safe disembarkation, squad membership, restoration and loss passed");
}
