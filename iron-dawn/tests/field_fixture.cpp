// SPDX-License-Identifier: MIT
// Controlled V6 saves for real simulator rendering/audio checks.
#include "save.hpp"
#include <cstdio>
#include <string_view>
using namespace iron;
int main(int argc,char** argv){if(argc!=3)return 2;Save s;s.has_run=1;s.generation=1000;s.career.unlocked=2;auto& w=s.world;
 if(std::string_view(argv[1])=="demolition"){
  w.terrain.fill(ground);w.add(hq,0,{3,5});w.add(tank,0,{8,10});int a=w.add(tank,1,{10,10}),b=w.add(power,1,{11,10});w.e[a].hp=w.e[b].hp=1;w.add(hq,1,{24,24});
 }else{w.init(1);for(auto& a:w.e)if(a.team==1&&a.kind<hq)a.hp=0;int tower=w.add(turret,0,{12,6.5f}),target=w.add(harvester,1,{17,6.5f});w.e[target].hp=100000;w.e[tower].dir=0;w.add(tank,0,{13.5f,7});w.add(rifle,0,{14.5f,6});}
 w.fog.fill(1);w.seen.fill(1);w.reveal_timer=100000;w.next_wave=100000;w.power_update();s.checksum=checksum(s);if(!valid_save(s))return 3;FILE* f=std::fopen(argv[2],"wb");if(!f)return 4;bool ok=std::fwrite(&s,1,sizeof s,f)==sizeof s;std::fclose(f);return ok?0:5;}
