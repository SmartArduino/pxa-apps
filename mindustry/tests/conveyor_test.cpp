// SPDX-License-Identifier: GPL-3.0-only
#include "../core.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace md;
int checks;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
State empty() {
  State s;s.generate(154,0,true);
  for(auto &b:s.buildings)if(b.type!=Core)b={};
  s.rebuild();return s;
}
void ticks(State &s,int n){for(int i=0;i<n;++i)s.step();}
int main(int argc,char **argv) {
  if(argc>1){
    auto s=empty();s.playerX=s.targetX=24.5f;s.playerY=s.targetY=29.5f;
    // Four direction/creation-order combinations in the guaranteed stone lane.
    for(int x:{22,26})for(int i=0;i<10;++i){int y=x==22?33-i:24+i;CHECK(s.build(Conveyor,x,y,1, true)>=0);}
    CHECK(s.buildLine(Conveyor,22,23,26,23)==5);
    CHECK(s.buildLine(Conveyor,26,21,22,21)==5);
    // Both corner orientations and one- and two-sided merges.
    for(int y:{17,19}){CHECK(s.build(Conveyor,24,y,0,true)>=0);CHECK(s.build(Conveyor,24,y-1,3,true)>=0);}
    CHECK(s.build(Conveyor,24,20,1,true)>=0);
    std::array<uint8_t,3900> bytes{};auto n=s.save(bytes.data(),bytes.size());CHECK(n);
    auto *f=std::fopen(argv[1],"wb");CHECK(f);CHECK(std::fwrite(bytes.data(),1,n,f)==n);std::fclose(f);return 0;
  }
  constexpr int dx[]={1,0,-1,0},dy[]={0,-1,0,1};
  // Golden cases from v154 Autotiler: back, relative side 1, side 3.
  const unsigned shapes[]={0,0,1,2,1,2,4,3};
  const bool flips[]={false,false,true,false,false,true,false,false};
  for(int dir=0;dir<4;++dir)for(unsigned mask=0;mask<8;++mask){
    auto s=empty();int x=24,y=18;CHECK(s.build(Conveyor,x,y,dir,true)>=0);
    for(int bit=0;bit<3;++bit)if(mask&(1<<bit)){
      int rel=bit==0?2:bit==1?1:3,side=(dir-rel+4)&3;
      CHECK(s.build(Conveyor,x+dx[side],y+dy[side],(side+2)&3,true)>=0);
    }
    auto t=s.conveyorTiling(x,y,dir);CHECK(t.shape==shapes[mask] && t.flip==flips[mask]);
    CHECK(bool(t.connections&4)==bool(mask&1));
    CHECK(bool(t.connections&2)==bool(mask&2));CHECK(bool(t.connections&8)==bool(mask&4));
  }
  auto s=empty();int center=s.build(Conveyor,24,18,0,true);
  int side=s.build(Conveyor,24,17,0,true);CHECK(center>=0 && side>=0);
  CHECK(s.conveyorTiling(24,18,0).shape==0); // Parallel belts do not blend.
  s.buildings[side].dir=3;CHECK(s.conveyorTiling(24,18,0).shape==1);
  s.buildings[side].buildState=1;CHECK(s.conveyorTiling(24,18,0).shape==0);
  CHECK(s.conveyorTiling(24,18,0,true).shape==1);
  s.buildings[side].buildState=3;CHECK(s.conveyorTiling(24,18,0,true).shape==0);
  s.buildings[side]={};s.rebuild();CHECK(s.conveyorTiling(24,18,0).shape==0);
  // A wall cannot turn a conveyor. Material machines can feed it from a side.
  side=s.build(Wall,24,17,0,true);CHECK(side>=0 && s.conveyorTiling(24,18,0).shape==0);
  s.buildings[side]={};s.rebuild();side=s.build(Generator,24,17,0,true);
  CHECK(side>=0 && s.conveyorTiling(24,18,0).shape==0);
  s.buildings[side]={};s.rebuild();side=s.build(Junction,24,17,0,true);
  CHECK(side>=0 && s.conveyorTiling(24,18,0).shape==1);
  // Existing transport direction and no-reverse-input rules remain intact.
  s=empty();center=s.build(Conveyor,24,18,0,true);CHECK(!s.accept(center,Copper,2));CHECK(s.accept(center,Copper,0));
  ticks(s,12);CHECK(s.buildings[center].conveyorClog>10);
  int router=s.build(Router,25,18,0,true);CHECK(router>=0);ticks(s,6);
  CHECK(!s.buildings[center].inv[Copper] && !s.buildings[center].conveyorClog);
  std::printf("%d conveyor checks passed: v154 shapes, mirrors, neighbours, plans, reverse rejection and jams\n",checks);
}
