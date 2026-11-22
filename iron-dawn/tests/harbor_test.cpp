// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cassert>
#include <cstdio>

using namespace iron;

void advance(World& w,float seconds){
 for(int i=0;i<int(20*seconds);++i)w.tick(.05f);
}

int main(){
 Save saved;
 auto& w=saved.world;
 w.init(harbor_chapter);
 int ferry=w.nearest({39,18},landing_craft,0);
 int commander_id=w.nearest({41,15},commander,0);
 int rockets=w.nearest({42,18},rocket,0);
 assert(w.side==64&&ferry>=0&&commander_id>=0&&rockets>=0&&w.power_net>=0);
 assert(w.terrain[cell({30.5f,40.5f})]==bridge);
 assert(!w.open({37.5f,25.5f})&&w.open_for(landing_craft,{37.5f,25.5f}));
 w.select_one(commander_id);w.select_one(rockets,true);
 assert(w.assign_squad(1)&&w.order_board(ferry));
 advance(w,9);
 assert(w.transport_load(ferry)==2);
 w.select_one(ferry);w.order({36.5f,25.5f});
 advance(w,9);
 assert(w.order_land(ferry,{35.5f,25.5f})&&w.recall_squad(1));
 w.order({32.5f,25.5f});
 advance(w,16);
 assert(w.mission.stage==harbor_beacon&&(w.mission.events&(1u<<harbor_beacon_radio)));
 assert(w.relay_at(harbor_switch_site,2)>=0);
 int center=w.nearest({12,52},hq,1);
 assert(w.shielded(w.e[center]));
 float elapsed=w.radio_time;
 w.e[commander_id].p=w.e[rockets].p={35.5f,25.5f};
 w.harbor_tick(1);
 assert(w.radio_time==elapsed);
 w.e[commander_id].p={32.5f,25.5f};
 int contest=w.add(rifle,1,{30.5f,23.5f});
 w.harbor_tick(1);assert(w.radio_time==elapsed);
 w.damage(contest,10000);w.power_net=-1;
 w.harbor_tick(1);assert(w.radio_time==elapsed);
 w.power_update();w.harbor_tick(1);assert(w.radio_time>elapsed);
 saved.checksum=checksum(saved);Save restored;
 assert(decode_save(std::as_bytes(std::span{&saved,1}),restored));
 auto& resumed=restored.world;
 assert(resumed.radio_time==w.radio_time&&resumed.mission.stage==harbor_beacon);
 resumed.harbor_raid();
 assert(resumed.count(patrol_boat,1)>=2);
 int beacon=resumed.relay_at(harbor_beacon_site,0);
 resumed.damage(beacon,10000);resumed.harbor_tick(1);
 assert(resumed.defeated&&!resumed.won);
 w.harbor_tick(harbor_hold_seconds);
 assert(w.won&&!w.defeated&&w.count(hq,1)==1);

 World land;
 land.init(harbor_chapter);
 int hq_id=land.nearest({12,52},hq,1);
 land.damage(hq_id,10000);
 assert(land.e[hq_id].hp>0&&land.enemy_defences_powered());
 int infantry=land.nearest({41,17},rifle,0);
 land.e[infantry].p={16.5f,35.5f};
 land.harbor_tick(12);
 assert(land.mission.stage==harbor_shutdown&&!land.enemy_defences_powered());
 assert(land.mission.events&(1u<<harbor_shutdown_radio));
 assert(land.relay_at(harbor_beacon_site,2)>=0);
 land.damage(hq_id,10000);land.harbor_tick(.05f);
 assert(land.won&&!land.defeated);

 // Exercise the frozen V9 layout in the same buffer used by the game.
 saved.world.init(5);int ship=saved.world.add(landing_craft,0,{21,43});
 saved.world.squads[ship]=3;saved.world.mission.support_left=17;
 LegacyCareer9 old{5,63,{40,50,60,70,80,90},true};
 auto bytes=std::as_writable_bytes(std::span{&restored,1}).first(30164);
 constexpr size_t prefix=20+sizeof old;
 uint32_t head[]={0x49445739,9,0,1,123};
 uint32_t defense=42;
 std::memcpy(bytes.data()+20,&old,sizeof old);
 std::memcpy(bytes.data()+prefix,&saved.world,sizeof saved.world);
 std::memcpy(bytes.data()+bytes.size()-4,&defense,4);
 std::memcpy(bytes.data(),head,sizeof head);
 head[2]=checksum_bytes(bytes);
 std::memcpy(bytes.data()+8,&head[2],sizeof head[2]);
 assert(decode_save(bytes,restored));
 assert(restored.generation==123&&restored.has_run&&restored.defense_best[0]==42);
 assert(restored.career.unlocked==harbor_chapter&&restored.career.wins==63);
 assert(restored.career.best[5]==90&&restored.career.best[6]==0&&restored.career.easy);
 assert(restored.world.chapter==5&&restored.world.e[ship].kind==landing_craft);
 assert(restored.world.squads[ship]==3&&restored.world.mission.support_left==17);
 std::puts("Harbor ferry approach, route choice, beacon power/garrison/contesting, raid, both outcomes and V9 career migration passed");
}
