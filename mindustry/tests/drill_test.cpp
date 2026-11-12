// SPDX-License-Identifier: GPL-3.0-only
// v154 reference rules: Drill.java, Blocks.java and Items.java.
#include "../core.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
using namespace md;
int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
State miner(Block kind, Item item) {
  State state;
  state.generate(154,0,true);
  for (int i=0;i<MaxBuildings;++i)
    if (state.buildings[i].type && state.buildings[i].type!=Core)
      state.remove(i);
  for (int y=35;y<37;++y)
    for (int x=19;x<21;++x)
      state.tiles[y*W+x]={Stone,item};
  CHECK(state.build(kind,19,35,0,true)>=0);
  return state;
}
int produce(State &state,int steps,bool wet) {
  auto &b=state.buildings[state.at(19,35)];
  int count=0;
  for (int i=0;i<steps;++i) {
    if (wet) b.water=50;
    state.step();
    for (auto &n:b.inv) {count+=n;n=0;}
  }
  return count;
}
uint32_t checksum(const uint8_t *p,size_t n) {
  uint32_t h=2166136261u;
  for(size_t i=0;i<n;++i) h=(h^p[i])*16777619u;
  return h;
}
int main() {
  auto state=miner(Drill,Copper);
  auto &b=state.buildings[state.at(19,35)];
  CHECK(b.ore.item==Copper && b.ore.count==4 && b.ore.delay==650);
  state.step();
  CHECK(b.inv[Copper]==0 && b.timer>0 && b.timer<1000);
  state.tiles[35*W+19]={Stone,Lead};
  state.tiles[35*W+20]={Sandy,Sand};
  state.tiles[36*W+19]=state.tiles[36*W+20]={Sandy,Sand};
  state.rebuild();
  CHECK(b.ore.item==Lead && b.ore.count==1); // one ore beats three sand
  state.tiles[35*W+20]={Stone,Copper};
  state.rebuild();
  CHECK(b.ore.item==Lead); // tie: original lead ID > copper ID
  state.tiles[36*W+19]={Stone,Copper};
  state.rebuild();
  CHECK(b.ore.item==Copper && b.ore.count==2);
  state.tiles[36*W+20]={Stone,Titanium};
  CHECK(state.drillOre(Drill,20,36).item!=Titanium);
  for(auto kind:{Drill,Pneumatic})
    for(auto item:{Copper,Lead,Coal,Sand,Titanium}) {
      if(kind==Drill && item==Titanium) continue;
      auto dry=miner(kind,item),wet=dry;
      auto &d=dry.buildings[dry.at(19,35)];
      auto &w=wet.buildings[wet.at(19,35)];
      d.timer=1000;w.timer=1600; // steady state, no startup transient
      int a=produce(dry,1200,false),c=produce(wet,1200,true);
      int hardness=item==Sand?0:item==Coal?2:item==Titanium?3:1;
      float perMinute=4.f*3600/((kind==Drill?600:400)+50*hardness);
      CHECK(std::abs(a-perMinute)<1.01f);
      CHECK(std::abs(c-perMinute*2.56f)<1.01f);
      std::printf("%s item=%d: dry=%d wet=%d / minute\n",defs[kind].en,item,a,c);
    }
  auto mechanical=miner(Drill,Copper),pneumatic=miner(Pneumatic,Copper);
  auto &m=mechanical.buildings[mechanical.at(19,35)];
  auto &p=pneumatic.buildings[pneumatic.at(19,35)];
  m.water=p.water=50;
  for(int i=0;i<20;++i){mechanical.step();pneumatic.step();}
  CHECK(m.water==20 && p.water==15); // 3 vs 3.5 liquid units per second
  p.inv[Copper]=5;p.inv[Lead]=5;
  auto water=p.water,progress=p.progress;
  for(int i=0;i<40;++i)pneumatic.step();
  CHECK(p.water==water && p.progress==progress && p.timer==0);
  CHECK(p.drillRate==0);
  float angle=p.drillRotation;
  pneumatic.step();CHECK(p.drillRotation==angle);
  // Current saves preserve warmup and fractional drill work byte for byte.
  state=miner(Drill,Copper);produce(state,37,true);
  std::array<uint8_t,3900> bytes{},again{};
  size_t n=state.save(bytes.data(),bytes.size());CHECK(n>0 && bytes[3]=='8');
  State restored;CHECK(restored.load(bytes.data(),n));
  CHECK(restored.save(again.data(),again.size())==n);
  CHECK(!std::memcmp(bytes.data(),again.data(),n));
  auto a=produce(state,200,false),c=produce(restored,200,false);CHECK(a==c);
  // Actual previous-release MND3 fixture keeps inventory and migrates cooldown.
  std::ifstream old("local/pxa-apps/mindustry/tests/fixtures/v0.4.mnd",std::ios::binary);
  std::vector<uint8_t> v3((std::istreambuf_iterator<char>(old)),{});
  CHECK(!v3.empty() && restored.load(v3.data(),v3.size()));
  CHECK(restored.buildings[restored.at(19,35)].timer==1000);
  CHECK(restored.save(bytes.data(),bytes.size())>0 && bytes[3]=='8');
  // Reject corrupt warmup/progress with a repaired checksum, without changing
  // the current state. The drill record follows the existing compact mask.
  n=restored.save(bytes.data(),bytes.size());
  size_t pos=52;
  for(unsigned i=0;i<bytes[51];++i) {
    unsigned kind=bytes[pos];pos+=6;
    unsigned mask=bytes[pos++];
    for(int j=0;j<Items;++j) if(mask&(1u<<j)) ++pos;
    if(kind==Drill || kind==Pneumatic) {
      for(auto offset:{size_t(0),size_t(7)}) {
        auto corrupt=bytes;
        unsigned value=offset?22400:1601;
        corrupt[pos+offset]=uint8_t(value);
        corrupt[pos+offset+1]=uint8_t(value>>8);
        uint32_t h=checksum(corrupt.data(),n-4);
        for(int j=0;j<4;++j) corrupt[n-4+j]=uint8_t(h>>(8*j));
        CHECK(!restored.load(corrupt.data(),n));
        CHECK(restored.save(again.data(),again.size())==n);
        CHECK(!std::memcmp(bytes.data(),again.data(),n));
      }
      break;
    }
    pos+=9;
  }
  std::printf("%d drill checks passed\n",checks);
}
