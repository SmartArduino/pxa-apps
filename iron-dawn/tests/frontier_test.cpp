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
 for(int version:{4,5,6}){std::ifstream f(std::string(argv[1])+"/save-v"+std::to_string(version)+".bin",std::ios::binary);std::vector<char> raw((std::istreambuf_iterator<char>(f)),{});assert(!raw.empty());Save s;assert(decode_save(std::as_bytes(std::span{raw.data(),raw.size()}),s));assert(s.version==save_version&&s.world.side==28&&s.world.chapter==1&&s.world.credits==1234&&s.world.time==43&&s.generation==73&&s.career.unlocked==3&&s.career.best[2]==330&&s.career.easy);assert(s.world.e[5].p.x==8.5f&&s.world.ore[14*N+4]>0);assert(s.world.extra.damage==(version==6?1:0));assert(s.defense_best[0]==(version==6?9:0));Save inplace;std::memcpy(&inplace,raw.data(),raw.size());assert(decode_save(std::as_bytes(std::span{&inplace,1}).first(raw.size()),inplace));raw[19]^=1;assert(!decode_save(std::as_bytes(std::span{raw.data(),raw.size()}),s));}
 {
  std::ifstream f(std::string(argv[1])+"/save-v7.bin",std::ios::binary);
  std::vector<char> raw((std::istreambuf_iterator<char>(f)),{});
  Save original,inplace;
  auto source=std::as_bytes(std::span{raw.data(),raw.size()});
  assert(decode_save(source,original));
  assert(original.version==save_version&&original.generation==1000&&original.career.unlocked==5);
  assert(original.world.side==44&&original.world.chapter==4&&!original.world.authored_map());
  assert(original.world.convoy_stop(3).x==37&&original.world.ore[14*N+3]>0);
  std::memcpy(&inplace,raw.data(),raw.size());
  assert(decode_save(std::as_bytes(std::span{&inplace,1}).first(raw.size()),inplace));
  assert(inplace.checksum==original.checksum&&inplace.world.mission.convoy==original.world.mission.convoy);
  auto& w=original.world;
  for(auto& a:w.e){if(a.team==1)a.hp=0;if(a.kind==relay&&a.hp>0)a.team=0;}
  w.power_update();step(w,180);assert(w.won);
 }
 {
  std::ifstream f(std::string(argv[1])+"/save-v8.bin",std::ios::binary);
  std::vector<char> raw((std::istreambuf_iterator<char>(f)),{});
  auto bytes=std::as_bytes(std::span{raw.data(),raw.size()});
  Save s;assert(bytes.size()==30064);
  std::memcpy(&s,bytes.data(),bytes.size());
  assert(decode_save(std::as_bytes(std::span{&s,1}).first(bytes.size()),s));
  auto& w=s.world;
  assert(s.generation==2243&&s.defense_best[0]==17&&w.chapter==4&&w.side==60);
  assert(w.construction.kind==power&&w.construction.left>0&&w.construction.left<5);
  assert(w.job_count==1&&w.jobs[0].kind==tank&&w.can_train_at(tank,w.jobs[0].producer-1));
  assert(std::all_of(w.squads.begin(),w.squads.end(),[](int squad){return squad==0;}));
 }
 for(int ch=0;ch<CHAPTERS;++ch){
  Save s;s.world.init(ch);s.career.unlocked=5;s.has_run=1;s.checksum=checksum(s);
  Save decoded;assert(decode_save(std::as_bytes(std::span{&s,1}),decoded));
  assert(decoded.world.side==campaign_maps[ch].side&&decoded.world.authored_map());
  assert(decoded.world.terrain==s.world.terrain&&decoded.world.ore==s.world.ore);
 }
 {World w;w.init(3);w.next_wave=100000;for(auto& a:w.e)if(a.team==1)a.hp=0;w.tick(.05f);assert(!w.won);for(auto& node:w.e)if(node.hp>0&&node.kind==relay){int id=w.add(rifle,0,{node.p.x-1,node.p.y});assert(id>=0);}step(w,12.5f);assert(w.won&&w.mission.stage==1&&w.captured_nodes()==2&&(w.mission.events&1));}
 {World w;w.init(3);w.next_wave=100000;auto node=w.objective_position();int n=w.nearest(node,relay,2);int ours=w.add(rifle,0,{node.x-1,node.y}),enemy=w.add(rifle,1,{node.x+1,node.y});w.frontier_tick(10);assert(w.e[n].team==2&&w.e[n].stuck==0);w.e[enemy].hp=0;w.frontier_tick(6);assert(w.e[n].stuck==6);w.e[ours].hp=0;w.frontier_tick(5);assert(w.e[n].stuck==4);}
 {World w;w.init(4);w.next_wave=100000;for(auto& a:w.e)if(a.team==1)a.hp=0;step(w,25);assert(!w.won);for(int i=0;i<3;++i)assert(w.mission.waypoint[i]==1&&w.e[w.mission.convoy[i]].hp>0);int near=w.convoy_checkpoint(),far=w.nearest({27,39},relay,2);w.e[far].team=0;step(w,20);assert(w.mission.stage==0);for(unsigned i:w.mission.waypoint)assert(i==1);w.e[near].team=0;w.power_update();step(w,180);assert(w.won&&__builtin_popcount(w.mission.delivered)>=2);}
 {World w;w.init(4);w.e[w.mission.convoy[0]].hp=0;w.e[w.mission.convoy[1]].hp=0;w.tick(.05f);assert(w.defeated&&!w.won);}
 {World w;w.init(4);
  int a=w.mission.convoy[0],b=w.mission.convoy[1],c=w.mission.convoy[2],guard=5;
  w.select_one(guard);assert(w.order_escort(a));
  w.damage(a,10000);int next=escort_id(w.e[guard]);assert(next==b||next==c);
  int slot=next==b?1:2;w.mission.waypoint[slot]=3;w.e[next].p=w.convoy_stop(3);
  w.convoy_tick(.05f);assert(w.mission.delivered&(1<<slot));
  assert(escort_id(w.e[guard])==(next==b?c:b));
  w.stop_selected();assert(escort_id(w.e[guard])<0&&!w.e[guard].moving);
 }
 {World w;w.init(5);w.next_wave=100000;int core=w.nearest({42,41},hq,1);float hp=w.e[core].hp;w.damage(core,100000);assert(w.e[core].hp==hp&&!w.won);for(auto& a:w.e)if(a.kind==relay&&a.hp>0)a.team=0;w.frontier_tick(.05f);assert(w.mission.stage==1);for(int i=0;i<MAX;++i)if(w.e[i].kind==power&&w.e[i].team==1)w.damage(i,100000);w.frontier_tick(.05f);assert(w.mission.stage==2);w.damage(core,100000);w.frontier_tick(.05f);assert(w.won&&(w.mission.events&(1u<<4)));}
 {World w;w.init(3);w.mission.stage=1;for(auto& a:w.e)if(a.kind==factory&&a.team==1)a.hp=0;int node=w.nearest(w.base_position(),relay,2);w.e[node].hp=0;w.tick(.05f);assert(w.defeated&&!w.won);}
 std::puts("V4–V8 migration and in-place recovery; six map sizes/routes; contested nodes; convoy gates/delivery/loss; final shield and staged victory passed");
}
