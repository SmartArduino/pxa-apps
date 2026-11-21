// SPDX-License-Identifier: MIT
#include "save.hpp"
#include "sprites.hpp"
#include <cassert>
#include <cstdio>

using namespace iron;

int main(){
 Save save;
 auto& world=save.world;
 world.init_defense(17,Faction::gray_tower);
 assert(world.faction()==Faction::gray_tower&&world.faction(1)==Faction::northern);
 assert(world.spec(tank).hp>world.spec(tank,1).hp);
 assert(world.spec(tank).speed<world.spec(tank,1).speed);
 assert(world.e[world.nearest(world.base_position(),tank,0)].hp==world.spec(tank).hp);

 int starting_credits=world.credits;
 assert(world.start_construction(helipad));
 assert(world.credits==starting_credits-world.spec(helipad).cost);
 assert(world.construction.total==world.spec(helipad).build);
 assert(world.cancel_construction()&&world.credits==starting_credits);
 int pad=world.add(helipad,0,{10,6});
 assert(world.queue(rotorcraft,pad));
 assert(world.jobs[0].total==world.spec(rotorcraft).build);
 save.has_run=1;
 save.checksum=checksum(save);
 Save restored;
 assert(decode_save(std::as_bytes(std::span{&save,1}),restored));
 assert(restored.world.faction()==Faction::gray_tower&&restored.world.jobs[0].kind==rotorcraft);

 World battle;
 battle.add(hq,0,{3,3});
 battle.add(hq,1,{24,24});
 int interceptor=battle.add(rotorcraft,1,{10,10});
 battle.add(tank,0,{11,10});
 int helicopter=battle.add(rotorcraft,0,{14,10});
 battle.tick(.05f);
 bool targeted_air=false;
 for(const auto& shot:battle.shots)if(shot.alive&&shot_source(shot)==interceptor){
  assert(shot_target(shot)==helicopter);
  assert(shot.damage>battle.spec(rotorcraft).damage);
  targeted_air=true;
 }
 assert(targeted_air);
 for(int kind:{int(rifle),int(rocket),int(tank),int(artillery),int(rotorcraft)}){
  for(int i=0;i<8;++i){
   float angle=i*3.14159265f/4;
   assert(Sprites::unit_frame(kind,angle,0,Faction::gray_tower)!=Sprites::unit_frame(kind,angle));
  }
 }
 assert(Sprites::unit_frame(rifle,0,1,Faction::gray_tower)!=Sprites::unit_frame(rifle,0,0,Faction::gray_tower));
 for(int kind:{int(hq),int(power),int(refinery),int(barracks),int(factory),int(turret),int(shipyard),int(helipad),int(aa_tower)})
  assert(Sprites::building_frame(kind,Faction::gray_tower)!=Sprites::building_frame(kind));
 std::puts("Faction rosters, aircraft target priority, production, save restore and sprite variants passed");
}
