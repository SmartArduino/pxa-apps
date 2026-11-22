// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cassert>
#include <cstdio>

using namespace iron;

void advance(World& world,float seconds){
 for(int i=0;i<int(seconds*20+.5f);++i)world.tick(.05f);
}

void island_route(HarborRoute route){
 World world;
 world.init(lighthouse_chapter,route);
 assert(world.side==64&&world.power_net>0);
 assert(world.terrain[cell({31.5f,31.5f})]==(route==harbor_shutdown?bridge:water));
 assert(world.terrain[cell({31.5f,45.5f})]==bridge);
 assert(world.count(rifle)==(route==harbor_beacon?2:1));
 assert(world.count(landing_craft,2)==2);
 assert(dist(world.objective_position(),island_grid_site)<.01f);
}

void lighthouse_sequence(){
 Save save;
 auto& world=save.world;
 world.init(lighthouse_chapter,harbor_beacon);
 for(auto& a:world.e)if(a.team==1)a.hp=0;
 world.power_update();
 int rifle_id=world.nearest(lighthouse_site,rifle,0);
 int flight=world.nearest(lighthouse_site,rotorcraft,0);
 int light=world.relay_at(lighthouse_site,2);
 int grid=world.relay_at(island_grid_site,2);
 int ferry=world.mission.convoy[0];
 assert(rifle_id>=0&&flight>=0&&light>=0&&grid>=0&&ferry>=0);
 world.e[rifle_id].p={54.5f,18.5f};
 world.e[flight].p=island_grid_site;
 advance(world,13);
 assert(world.mission.stage==restore_grid&&world.e[light].stuck==0&&world.e[grid].stuck==0);

 world.e[rifle_id].p={41,33};
 advance(world,12.2f);
 assert(world.mission.stage==capture_lighthouse&&world.e[grid].team==0);
 assert(world.mission.events==(1u<<island_grid_radio)&&dist(world.objective_position(),lighthouse_site)<.01f);
 world.mission.read_events=world.mission.events;
 world.e[rifle_id].p={54.5f,18.5f};
 advance(world,13);
 assert(world.mission.stage==guide_ferries&&world.e[light].team==0);
 assert(world.mission.events==((1u<<island_grid_radio)|(1u<<lighthouse_radio)));

 float elapsed=world.radio_time,y=world.e[ferry].p.y;
 world.power_net=-1;
 advance(world,2);
 assert(world.radio_time==elapsed&&world.e[ferry].p.y==y);
 world.power_update();
 world.e[rifle_id].p.x=49;
 advance(world,2);
 assert(world.radio_time==elapsed);
 world.e[rifle_id].p.x=54.5f;
 int enemy=world.add(rocket,1,{55,17});
 world.lighthouse_tick(.1f);
 assert(world.radio_time==elapsed);
 world.damage(enemy,1000);
 advance(world,4);
 assert(world.radio_time>elapsed&&world.e[ferry].p.y<y);

 save.career.harbor_plan=harbor_beacon;save.career.unlocked=lighthouse_chapter;
 save.has_run=1;save.checksum=checksum(save);
 Save restored;
 assert(decode_save(std::as_bytes(std::span{&save,1}),restored));
 assert(restored.career.harbor_plan==harbor_beacon&&restored.world.mission.harbor_plan==harbor_beacon);
 assert(restored.world.mission.events!=restored.world.mission.read_events);
 assert(restored.world.radio_time==world.radio_time&&restored.world.e[ferry].p.y==world.e[ferry].p.y);
 advance(restored.world,35);
 assert(restored.world.won&&restored.world.e[ferry].p.y==6.5f);
 world.damage(grid,2000);
 world.lighthouse_tick(.1f);
 assert(world.defeated);
}

void migrate_v10(bool route_known){
 struct LegacySave10 {
  uint32_t magic=0x4944573a,version=10,checksum=0,has_run=0,generation=24;
  LegacyCareer10 career{6,127,{14,20,30,40,50,60,70},true};
  World world;
  uint32_t defense_best=42;
 } legacy;
 static_assert(sizeof(LegacySave10)==30168);
 legacy.world.init(route_known?harbor_chapter:4);
 legacy.world.won=route_known;
 legacy.world.mission.stage=route_known?harbor_shutdown:0;
 // The new mission member occupies a padding byte on published V10 saves.
 legacy.world.mission.harbor_plan=HarborRoute(255);
 auto bytes=std::as_bytes(std::span{&legacy,1});
 legacy.checksum=checksum_bytes(bytes);
 Save restored;
 std::memcpy(&restored,bytes.data(),bytes.size());
 auto input=std::as_bytes(std::span{&restored,1}).first(bytes.size());
 assert(decode_save(input,restored));
 assert(restored.career.unlocked==lighthouse_chapter&&restored.career.wins==127);
 assert(restored.career.best[6]==70&&restored.career.best[7]==0&&restored.defense_best[0]==42);
 assert(restored.career.harbor_plan==(route_known?harbor_shutdown:harbor_choice));
 assert(restored.world.mission.harbor_plan==harbor_choice);
}

int main(){
 for(auto route:{harbor_choice,harbor_beacon,harbor_shutdown})island_route(route);
 lighthouse_sequence();
 migrate_v10(true);migrate_v10(false);
 std::puts("White Island routes, ground capture, signal pauses, ferries, save restore and V10 progression passed");
}
