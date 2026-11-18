// SPDX-License-Identifier: MIT
#include "save.hpp"
#include "locomotion.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace iron;
float travel(int kind,int surface=ground,unsigned cargo=0){World w;w.terrain.fill(surface);int id=w.add(kind,0,{5.5f,8.5f});auto& a=w.e[id];a.cargo=cargo;a.goal={24.5f,8.5f};a.moving=true;w.power_update();for(int i=0;i<100;++i){w.route_credits=6;w.move(id,.05f);}return a.p.x-5.5f;}
void reaches(const World& w,Vec from,Vec to,float tolerance=1.8f){for(int n=0;n<160&&dist(from,to)>tolerance;++n){auto next=w.path(from,to,-1);assert(w.open(next)&&dist(next,from)>.01f);from=next;}assert(dist(from,to)<=tolerance);}
int main(){
 float infantry=travel(rifle),armor=travel(tank),gun=travel(artillery),empty=travel(harvester),loaded=travel(harvester,ground,85);
 assert(armor>infantry*1.5f&&gun>infantry&&empty>loaded&&loaded>infantry);assert(travel(tank,road)>armor*1.15f);assert(travel(tank,bridge)>armor*1.15f);assert(travel(tank,ruins)<armor);assert(travel(artillery,ruins)>travel(rifle,ruins));
 std::printf("Five-second travel: infantry=%.2f tank=%.2f artillery=%.2f truck empty=%.2f loaded=%.2f road tank=%.2f\n",infantry,armor,gun,empty,loaded,travel(tank,road));
 {World w;int id=w.add(tank,0,{5.5f,8.5f});w.e[id].dir=3.14159265f;w.e[id].goal={20.5f,8.5f};w.power_update();w.move(id,.05f);assert(w.e[id].p.x==5.5f&&std::abs(w.e[id].dir-3.14159265f)<=.176f);}
 for(int chapter=0;chapter<3;++chapter){World w;w.init(chapter);assert(w.extra.map_rules==1);int roads=0,rocks=0;for(auto t:w.terrain){roads+=t==road||t==bridge;rocks+=t==ridge;}assert(roads>=70&&rocks>0);reaches(w,{10.5f,12.5f},{22,20});reaches(w,{10.5f,12.5f},{16.5f,21.5f});if(chapter==1){for(float y:{6.5f,17.5f,24.5f})reaches(w,{11.5f,y},{17.5f,y},.1f);assert(!w.can_place(power,{14.5f,6.5f}));}}
 for(unsigned seed=1;seed<=12;++seed){World w;w.init_defense(seed);for(Vec from:{Vec{23.5f,5.5f},Vec{24.5f,22.5f},Vec{5.5f,24.5f}})reaches(w,from,{7,10});for(int c=0;c<N*N;++c)if(w.terrain[c]==bridge)assert(!w.ore[c]);}
 {World w;w.init(1);int id=w.add(tank,0,{14.5f,6.5f});w.damage(id,10000);assert(w.terrain[cell(w.e[id].p)]==bridge&&w.open(w.e[id].p)&&!w.can_place(power,w.e[id].p));}
 {World w;for(int y=1;y<N-1;++y)w.terrain[y*N+14]=water;w.ore[8*N+15]=10;w.ore[20*N+5]=10;w.power_update();assert(w.ore_near({5.5f,8.5f})==20*N+5);}
 {World w;w.add(hq,0,{3,3});w.add(hq,1,{25,25});w.add(refinery,0,{6,6});int truck=w.add(harvester,0,{8.5f,8.5f});w.ore[8*N+8]=5;w.power_update();int cash=w.credits;for(int n=0;n<160;++n)w.tick(.05f);assert(w.credits==cash+5&&w.earned==5&&w.e[truck].cargo==0);}
 {World w;w.extra.map_rules=1;int a=w.add(rifle,0,{9.5f,9.5f}),enemy=w.add(rifle,1,{11.5f,9.5f}),gunner=w.add(artillery,1,{15,15});w.terrain[cell(w.e[a].p)]=woodland;assert(w.in_cover(w.e[a]));w.damage(a,40,enemy);assert(w.e[a].hp==70);w.damage(a,20,gunner);assert(w.e[a].hp==50);w.extra.map_rules=0;w.damage(a,20,enemy);assert(w.e[a].hp==30);}
 {World w;w.init_defense(4);w.wave=2;w.extra.cleared=2;w.defense_wave();bool mine=false,front=false;for(auto& a:w.e)if(a.hp>0&&a.team==1){mine|=dist(a.goal,{5,13})<.1f;front|=dist(a.goal,{7,10})<.1f;}assert(mine&&front);for(auto& a:w.e)if(a.team==1)a.hp=0;w.wave=16;w.defense_wave();bool veterans=false;for(auto& a:w.e)if(a.team==1&&a.hp>0)veterans|=veteran_rank(a)>=2;assert(veterans);}
 {Save s;s.world.init(1);s.has_run=1;s.checksum=checksum(s);Save d;assert(decode_save(std::as_bytes(std::span{&s,1}),d)&&d.world.extra.map_rules==1&&d.world.terrain==s.world.terrain);s.world.extra.map_rules=0;s.checksum=checksum(s);assert(decode_save(std::as_bytes(std::span{&s,1}),d)&&!d.world.extra.map_rules);assert(sizeof(Save)<24000);}
 {World w;int id=w.add(tank,0,{8,8});Locomotion l;l.update(w);int last=0,changes=0;for(int f=0;f<14;++f){w.time+=1/14.f;w.e[id].p.x+=2.34f/14;l.update(w);int pose=int(l.get(id).phase*2)&1;changes+=pose!=last;last=pose;}assert(changes>=4&&changes<=10);}
 std::puts("Road speed, turning, connected crossings, reachable ore, exact partial cargo, cover counters, raids, tread cadence and V7 save roundtrip passed");
}
