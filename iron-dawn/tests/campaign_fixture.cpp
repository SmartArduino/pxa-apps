// SPDX-License-Identifier: MIT
// Controlled scenes for actual SDK simulator QA; never device/user saves.
#include "save.hpp"
#include <cstdlib>
#include <cstdio>
#include <string_view>
using namespace iron;
int main(int argc,char** argv){if(argc!=3)return 2;std::string_view mode=argv[1];Save s;s.has_run=1;s.generation=1000;s.career.unlocked=5;s.career.wins=7;auto& w=s.world;
 if(mode.starts_with("construction")){w.init(0);constexpr Vec sites[]={{8,20},{6,21},{13,25},{8,32},{12,24}};int stage=std::clamp(std::atoi(argv[1]+12),0,3);for(int k=power;k<=turret;++k){int id=w.add(k,0,sites[k-power]);if(id<0)return 3;w.e[id].progress=specs[k].build*(.875f-stage*.25f);}w.seen.fill(1);w.fog.fill(1);w.story_beat=true;}
 else if(mode=="transport"){
  w.init(1);w.e[5].p={25.5f,30.5f};w.e[6].p={26.5f,31.5f};
  w.select_one(5);w.select_one(6,true);w.assign_squad(1);w.fog.fill(1);w.seen.fill(1);
 }
 else if(mode=="combined-water")w.init(1);
 else if(mode=="combined-sky")w.init(3);
 else if(mode=="combined-hero"){w.init(5);w.e[5].hp=100;}
 else if(mode=="tactics"){w.init(4);w.mission.waypoint.fill(1);w.select_one(5);}
 else if(mode=="squads"){w.init(0);w.select_one(5);w.select_one(6,true);}
 else {w.init((mode=="radio"||mode=="far")?5:std::clamp(std::atoi(argv[1]),0,5));if(mode=="far"){for(auto& a:w.e)if(a.kind==relay&&a.p.x<45)a.team=0;}if(mode=="radio"){w.mission.events=1u<<3;w.mission.stage=1;}}
 w.next_wave=100000;w.reveal_timer=100000;w.power_update();s.checksum=checksum(s);if(!valid_save(s))return 4;FILE* f=std::fopen(argv[2],"wb");if(!f)return 5;bool ok=std::fwrite(&s,1,sizeof s,f)==sizeof s;std::fclose(f);return ok?0:6;
}
