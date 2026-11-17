// SPDX-License-Identifier: MIT
// A controlled V6 save for exercising real radio commands in the signed app.
#include "save.hpp"
#include <cstdio>
#include <cstdlib>
using namespace iron;
int main(int argc,char** argv){
 if(argc!=3)return 2;int kind=std::atoi(argv[1]);if(kind<0||kind>harvester)return 2;
 Save s;s.has_run=1;s.generation=100;s.career.unlocked=2;auto& w=s.world;w.terrain.fill(ground);w.fog.fill(1);w.seen.fill(1);w.credits=10000;w.next_wave=100000;
 w.add(hq,0,{3,5});w.add(power,0,{2,8});w.add(refinery,0,{3,10});w.add(barracks,0,{5,5});w.add(factory,0,{5,8});w.add(power,0,{2,11});
 w.add(kind,0,{8,10});int target=w.add(hq,1,{10,10});w.e[target].hp=100000;w.power_update();w.reveal();s.checksum=checksum(s);
 if(!valid_save(s))return 3;FILE* f=std::fopen(argv[2],"wb");if(!f)return 4;bool ok=std::fwrite(&s,1,sizeof s,f)==sizeof s;std::fclose(f);return ok?0:5;
}
