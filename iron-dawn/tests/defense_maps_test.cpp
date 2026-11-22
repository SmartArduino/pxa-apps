// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cassert>
#include <cstdio>

using namespace iron;

void accessible(const World& w,Vec from,Vec to,int entity=-1){
 for(int step=0;step<160&&dist(from,to)>1.8f;++step){
  Vec next=w.path(from,to,entity);
  assert(dist(next,from)>.01f);
  from=next;
 }
 assert(dist(from,to)<=1.8f);
}

void check_map(DefenseMap map,Faction faction){
 World w;
 w.init_defense(17,faction,map);
 assert(w.defense_map()==map&&w.faction()==faction&&w.side==w.arena().side);
 assert(w.power_net>0&&w.count(hq)==1&&w.ready_count(refinery));
 assert(w.deployment_view().x==w.arena().view.x);
 for(const auto& a:w.e)if(deployed(a)){
  if(unit_kind(a.kind))assert(w.open_for(a.kind,a.p));
  else assert(w.terrain[cell(a.p)]!=water&&w.terrain[cell(a.p)]!=ridge);
 }
 for(int i=1;i<=3;++i)accessible(w,w.wave_entry(i),w.base_position());
 if(map==DefenseMap::reef){
  assert(w.has_shore_water()&&w.ready_count(shipyard)&&w.count(patrol_boat));
  accessible(w,w.naval_entry(2),w.naval_objective(w.base_position()),w.nearest(w.base_position(),patrol_boat,0));
 }else assert(!w.has_shore_water()&&w.count(artillery));
}

int main(){
 for(auto faction:{Faction::northern,Faction::gray_tower}){
  check_map(DefenseMap::reef,faction);
  check_map(DefenseMap::ridge,faction);
 }
 Save game;
 auto& w=game.world;
 w.init_defense(9,Faction::gray_tower,DefenseMap::reef);
 w.wave=1;w.extra.cleared=1;
 w.spawn_raid();
 assert(w.wave==2&&w.count(patrol_boat,1)==1&&w.count(tank,1)>0);
 int boat=w.nearest(w.naval_entry(2),patrol_boat,1);
 assert(boat>=0&&rallying(w.e[boat]));
 w.time=w.next_wave;
 w.enemy_orders();
 assert(raider(w.e[boat])&&!rallying(w.e[boat]));
 assert(w.open_for(patrol_boat,w.e[boat].goal)&&dist(w.e[boat].goal,w.naval_objective(w.base_position()))<2);
 game.has_run=1;game.defense_best={12,4,7};game.checksum=checksum(game);
 Save restored;
 assert(decode_save(std::as_bytes(std::span{&game,1}),restored));
 assert(restored.defense_best==game.defense_best&&restored.world.defense_map()==DefenseMap::reef);
 assert(restored.world.e[boat].target==w.e[boat].target);

 w.init_defense(7,Faction::northern,DefenseMap::ridge);
 w.wave=1;
 w.spawn_raid();
 assert(w.count(artillery,1)>=1&&!w.count(patrol_boat,1));
 w.wave=5;
 w.spawn_raid();
 assert(w.count(rotorcraft,1)&&w.count(artillery,1)>=2);

 // The first record stays at its published V11 offset. Older layouts used
 // the river map exclusively and had no records for the two newer maps.
 w.init_defense(81,Faction::gray_tower);
 game.magic=0x4944573b;game.version=11;game.generation=42;
 auto old=std::as_bytes(std::span{&game,1}).first(legacy_save11_bytes);
 game.checksum=checksum_bytes(old);
 std::memcpy(&restored,old.data(),old.size());
 assert(decode_save(std::as_bytes(std::span{&restored,1}).first(old.size()),restored));
 assert(restored.generation==42&&restored.version==save_version);
 assert(restored.defense_best==(DefenseRecords{12,0,0}));
 assert(restored.world.defense_map()==DefenseMap::river&&restored.world.faction()==Faction::gray_tower);
 std::puts("New defense maps, connected approaches, mixed waves, saved raiding boats and V11 records passed");
}
