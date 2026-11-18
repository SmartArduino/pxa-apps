// SPDX-License-Identifier: MIT
#include "save.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
using namespace iron;
void step(World& w,float seconds){for(int i=0;i<int(seconds*20);++i)w.tick(.05f);}
int main(int argc,char** argv){assert(argc==2);
 for(int version:{4,5,6}){std::ifstream f(std::string(argv[1])+"/save-v"+std::to_string(version)+".bin",std::ios::binary);std::vector<char> raw((std::istreambuf_iterator<char>(f)),{});assert(!raw.empty());Save s;assert(decode_save(std::as_bytes(std::span{raw.data(),raw.size()}),s));assert(s.version==7&&s.world.side==28&&s.world.chapter==1&&s.world.credits==1234&&s.world.time==43&&s.generation==73&&s.career.unlocked==3&&s.career.best[2]==330&&s.career.easy);assert(s.world.e[5].p.x==8.5f&&s.world.ore[14*N+4]>0);assert(s.world.extra.damage==(version==6?1:0));assert(s.defense_best==(version==6?9:0));Save inplace;std::memcpy(&inplace,raw.data(),raw.size());assert(decode_save(std::as_bytes(std::span{&inplace,1}).first(raw.size()),inplace));raw[19]^=1;assert(!decode_save(std::as_bytes(std::span{raw.data(),raw.size()}),s));}
 for(int ch=0;ch<CHAPTERS;++ch){Save s;s.world.init(ch);s.career.unlocked=5;s.has_run=1;s.checksum=checksum(s);Save decoded;assert(decode_save(std::as_bytes(std::span{&s,1}),decoded));assert(decoded.world.side==(ch<3?28:ch==3?40:ch==4?44:48));assert(sizeof(Save)<24000);if(ch<3)continue;auto& w=s.world;for(auto& a:w.e)if(a.hp>0&&(a.kind==relay||a.kind==factory)&&a.team!=0){Vec from{8,12};for(int n=0;n<500&&dist(from,a.p)>2.3f;++n){auto next=w.path(from,a.p,-1);assert(dist(from,next)>.01f);from=next;}assert(dist(from,a.p)<=2.3f);}}
 {World w;w.init(3);w.next_wave=100000;for(auto& a:w.e)if(a.team==1)a.hp=0;w.tick(.05f);assert(!w.won);for(auto& node:w.e)if(node.hp>0&&node.kind==relay){int id=w.add(rifle,0,{node.p.x-1,node.p.y});assert(id>=0);}step(w,12.5f);assert(w.won&&w.mission.stage==1&&w.captured_nodes()==2&&(w.mission.events&1));}
 {World w;w.init(3);w.next_wave=100000;int n=w.nearest({18,10},relay,2);int ours=w.add(rifle,0,{17,10}),enemy=w.add(rifle,1,{19,10});w.frontier_tick(10);assert(w.e[n].team==2&&w.e[n].stuck==0);w.e[enemy].hp=0;w.frontier_tick(6);assert(w.e[n].stuck==6);w.e[ours].hp=0;w.frontier_tick(5);assert(w.e[n].stuck==4);}
 {World w;w.init(4);w.next_wave=100000;for(auto& a:w.e)if(a.team==1)a.hp=0;step(w,25);assert(!w.won);for(int i=0;i<3;++i)assert(w.mission.waypoint[i]==1&&w.e[w.mission.convoy[i]].hp>0);for(auto& a:w.e)if(a.kind==relay&&a.hp>0)a.team=0;w.power_update();step(w,180);assert(w.won&&__builtin_popcount(w.mission.delivered)>=2);}
 {World w;w.init(4);w.e[w.mission.convoy[0]].hp=0;w.e[w.mission.convoy[1]].hp=0;w.tick(.05f);assert(w.defeated&&!w.won);}
 {World w;w.init(5);w.next_wave=100000;int core=w.nearest({42,41},hq,1);float hp=w.e[core].hp;w.damage(core,100000);assert(w.e[core].hp==hp&&!w.won);for(auto& a:w.e)if(a.kind==relay&&a.hp>0)a.team=0;w.frontier_tick(.05f);assert(w.mission.stage==1);for(int i=0;i<MAX;++i)if(w.e[i].kind==power&&w.e[i].team==1)w.damage(i,100000);w.frontier_tick(.05f);assert(w.mission.stage==2);w.damage(core,100000);w.frontier_tick(.05f);assert(w.won&&(w.mission.events&(1u<<4)));}
 {World w;w.init(3);w.mission.stage=1;for(auto& a:w.e)if(a.kind==factory&&a.team==1)a.hp=0;int node=w.nearest({18,10},relay,2);w.e[node].hp=0;w.tick(.05f);assert(w.defeated&&!w.won);}
 std::puts("V4/V5/V6 migration and in-place recovery; six map sizes/routes; contested nodes; convoy gates/delivery/loss; final shield and staged victory passed");
}
