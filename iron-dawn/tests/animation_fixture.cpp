// SPDX-License-Identifier: MIT
// Deterministic simulator QA saves; never install into a user's game data.
#include "save.hpp"
#include <fstream>
#include <cstdio>
#include <string>
using namespace iron;
int main(int argc,char** argv){if(argc!=3)return 2;Save save;std::string mode=argv[1];
 if(mode=="inspect"){std::ifstream f(argv[2],std::ios::binary);f.read(reinterpret_cast<char*>(&save),sizeof save);if(!f||!valid_save(save)||checksum(save)!=save.checksum)return 3;std::printf("{\"time\":%.4f,\"units\":[",save.world.time);bool first=true;for(int i=0;i<MAX;++i){auto a=save.world.e[i];if(a.hp<=0||a.team||a.kind>=hq)continue;if(!first)std::putchar(',');first=false;std::printf("{\"id\":%d,\"kind\":%u,\"x\":%.6f,\"y\":%.6f,\"moving_intent\":%s}",i,a.kind,a.p.x,a.p.y,a.moving?"true":"false");}std::puts("]}");return 0;}
 save.has_run=1;save.generation=20;auto& w=save.world;w.init(0);for(auto& a:w.e)a.hp=0;w.terrain.fill(0);w.ore.fill(0);w.fog.fill(1);w.seen.fill(1);w.next_wave=99999;w.add(hq,0,{2,2});w.add(hq,1,{25,25});w.add(power,0,{3,4});
 if(mode=="walk"){for(int k=0;k<2;++k)for(int d=0;d<8;++d){int i=k*8+d;Vec p{float(5+i%4*2),float(7+i/4*2)};int id=w.add(k,0,p);auto& a=w.e[id];float angle=d*3.14159265f/4;a.goal=a.step={p.x+std::cos(angle)*3,p.y+std::sin(angle)*3};a.dir=angle;a.moving=true;}}
 else if(mode=="fire-south"){int id=w.add(rocket,0,{7,10}),target=w.add(rifle,1,{9,12});w.e[target].hp=100000;w.e[target].cool=99999;w.e[id].target=target;w.e[id].moving=true;w.e[id].goal=w.e[id].step={12,15};}
 else if(mode=="fire"){constexpr Vec positions[]={{5,8},{7,10},{8,8},{10,10},{8,13}};for(int k=0;k<5;++k){int kind=k<4?k:int(turret);auto p=positions[k];int id=w.add(kind,0,p);auto& a=w.e[id];int target=w.add(rifle,1,{p.x+2.8f,p.y});w.e[target].hp=100000;w.e[target].cool=99999;a.target=target;a.goal=a.step={p.x+5,p.y};a.moving=true;}}
 else return 4;
 w.power_update();save.checksum=checksum(save);if(!valid_save(save))return 5;std::ofstream f(argv[2],std::ios::binary);f.write(reinterpret_cast<const char*>(&save),sizeof save);return f?0:6;
}
