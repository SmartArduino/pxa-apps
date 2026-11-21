// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cassert>
#include <cstdio>

using namespace iron;

void step(World& world,float seconds){
 for(int i=0;i<int(seconds*20);++i)world.tick(.05f);
}

int main(){
 Save save;
 auto& w=save.world;
 w.init_defense(4);w.wave=2;w.extra.cleared=2;w.next_wave=0;
 w.tick(.05f);
 assert(w.wave==3&&w.army(1)==6&&w.dual_raid_entry(w.wave));
 int assault=-1,flank=-1;
 for(int i=0;i<MAX;++i)if(deployed(w.e[i])&&raider(w.e[i])){
  assert(rallying(w.e[i])&&w.open_for(w.e[i].kind,w.e[i].p));
  if(raid_role(w.e[i])==RaidRole::assault)assault=i;
  if(raid_role(w.e[i])==RaidRole::flank)flank=i;
 }
 assert(assault>=0&&flank>=0&&dist(w.e[assault].p,w.e[flank].p)>8);
 save.checksum=checksum(save);Save loaded;
 assert(decode_save(std::as_bytes(std::span{&save,1}),loaded));
 auto& resumed=loaded.world;
 assert(rallying(resumed.e[flank])&&resumed.next_wave==w.next_wave);
 step(resumed,7);
 assert(raider(resumed.e[flank])&&!rallying(resumed.e[flank]));
 assert(dist(resumed.e[assault].goal,resumed.e[flank].goal)>2);

 // A group under fire leaves the rally despite a member making no progress.
 for(auto& a:w.e)if(rallying(a))a.moving=false;
 w.time=w.next_wave;w.enemy_orders();
 assert(!rallying(w.e[assault])&&!rallying(w.e[flank]));
 for(int i=0;i<MAX;++i)if(w.e[i].hp>0&&w.e[i].team==1)w.damage(i,10000);
 w.tick(.05f);assert(w.extra.cleared==3);

 World convoy;convoy.init(4);convoy.spawn_raid();
 int lead=convoy.pursued_convoy();
 assert(lead>=0);
 convoy.e[lead].p={18.5f,39.5f};
 assert(convoy.pursued_convoy()==lead);
 convoy.time=convoy.next_wave;convoy.enemy_orders();
 int pursuer=-1;
 for(int i=0;i<MAX;++i)if(raider(convoy.e[i])&&raid_role(convoy.e[i])==RaidRole::intercept)pursuer=i;
 assert(pursuer>=0&&dist(convoy.e[pursuer].goal,convoy.e[lead].p)<2);
 auto old=convoy.e[pursuer].goal;
 convoy.damage(lead,10000);
 convoy.add(rifle,0,{18.5f,39.5f}); // The vacated slot is no longer a convoy target.
 convoy.enemy_orders();
 assert(convoy.pursued_convoy()!=lead&&dist(convoy.e[pursuer].goal,old)>5);

 World city;city.init(5);
 int pad=city.nearest({34,14},helipad,1),works=city.nearest({9,8},factory,1);
 assert(pad>=0&&works>=0);
 assert(!city.scout_raid().air);
 city.add(rotorcraft,0,{34,17});city.add(tank,0,{34,18});
 auto observed=city.scout_raid();
 assert(observed.air&&observed.armor);
 city.spawn_raid();assert(city.wave==1);
 assert(city.count(rotorcraft,1)==2);
 city.damage(pad,10000);city.damage(works,10000);city.spawn_raid();
 assert(city.count(rotorcraft,1)==2);
 for(auto& a:city.e)if(rallying(a))assert(a.kind!=artillery);
 std::puts("Split raid approaches, rally and timeout, save/resume, convoy retargeting, scouting and facility disruption passed");
}
