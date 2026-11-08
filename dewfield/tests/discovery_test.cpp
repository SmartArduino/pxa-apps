// SPDX-License-Identifier: MIT
#include "../collections.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace farm;
static void checksum(uint8_t *bytes) {
  uint32_t h=2166136261u;for(int i=0;i<SaveBytes-4;++i)h=(h^bytes[i])*16777619u;
  for(int i=0;i<4;++i)bytes[SaveBytes-4+i]=h>>(i*8);
}
int main() {
  State s;fresh(s);uint8_t seen=0;
  // Every offered fish obeys its actual location, season, weather and time.
  for(int region:{0,1,2,3,5})for(int season=0;season<4;++season)
    for(int weather=0;weather<5;++weather)for(int minute:{360,600,1080,1440}) {
      s.region=region;s.day=season*28+1;s.weather=weather;s.minute=minute;
      assert(fish_available(s,0));
      for(int n=0;n<150;++n) {int id=choose_fish(s);assert(fish_available(s,id));seen|=1u<<id;}
    }
  assert(seen==255);s.region=4;assert(choose_fish(s)==-1);
  fresh(s);s.day=1;s.weather=0;s.region=2;s.minute=1100;
  assert(!fish_available(s,1)&&!fish_available(s,4)&&fish_available(s,5));
  FishingSession f;f.begin(s);
  assert(!f.press(s)&&f.phase==FishingSession::Phase::Waiting);
  for(int i=0;f.phase==FishingSession::Phase::Waiting;++i){assert(i<100);f.tick(33);}
  assert(f.phase==FishingSession::Phase::Bite);
  f.press(s);assert(f.phase==FishingSession::Phase::Reeling);
  // Real fixed-time steps can reach a perfect zone at both slow and fast rates.
  for(int frame:{16,33,100,200}) {
    fresh(s);f.begin(s);while(f.phase==FishingSession::Phase::Waiting)f.tick(33);assert(f.phase==FishingSession::Phase::Bite);
    f.press(s);
    for(int i=0;std::abs(f.cursor()-f.center)>(frame<=100?55:f.radius);++i){assert(i<100);f.tick(frame);}
    assert(f.press(s)&&f.count==(f.perfect?2:1)&&f.discovered&&f.record);
    if(frame<=100)assert(f.perfect);
    assert(s.fish==f.count&&s.fish_caught==1&&s.perfect_catches==int(f.perfect)&&s.fish_records[f.species]==f.length);
    assert(!f.press(s)&&s.fish==f.count); // terminal inputs cannot duplicate rewards
  }
  fresh(s);f.begin(s);while(f.phase==FishingSession::Phase::Waiting)f.tick(33);f.press(s);
  assert(!f.press(s)&&f.attempts==1);assert(!f.press(s)&&f.phase==FishingSession::Phase::Escaped&&s.fish==0);
  f.begin(s);for(int i=0;i<6;++i)f.tick(1000);assert(f.phase==FishingSession::Phase::Escaped);
  fresh(s);s.fish=999;s.px=23*16+8;s.py=15*16+8;select_slot(s,6);
  int energy=s.energy;assert(act(s,24,15)==Result::FullFish&&s.energy==energy);
  s.fish=998;f.begin(s);while(f.phase==FishingSession::Phase::Waiting)f.tick(33);f.press(s);
  while(std::abs(f.cursor()-f.center)>55)f.tick(16);
  assert(f.press(s)&&s.fish==999&&f.count==1);
  assert(!change_item(s,12,1000)&&!discovered_item(s,12));
  assert(change_item(s,12,1)&&discovered_item(s,12));change_item(s,12,-1);
  assert(discovered_item(s,12)); // selling/eating never erases a discovery
  fresh(s);
  for(int item:GardenCollection)assert(change_item(s,item,1));
  for(int item:WildCollection)assert(change_item(s,item,1));
  for(int i=0;i<8;++i)s.fish_records[i]=FishSpeciesList[i].min_cm;
  s.fish_caught=8;s.harvested=10;s.visited=255;
  assert(discovered_total(s)==22&&ready_collection_rewards(s)==6);
  unsigned gold=s.gold;
  for(int i=0;i<MilestoneCount;++i) {auto m=milestone(s,i);assert(claim_collection_reward(s,i));gold+=m.reward;assert(s.gold==gold&&!claim_collection_reward(s,i));}
  assert(ready_collection_rewards(s)==0&&!claim_collection_reward(s,-1)&&!claim_collection_reward(s,6));
  uint8_t bytes[SaveBytes];State restored;assert(encode(s,bytes,sizeof(bytes))==SaveBytes&&bytes[4]==9);
  assert(decode(restored,bytes,sizeof(bytes))&&restored.discoveries==s.discoveries&&restored.collection_rewards==63&&restored.fish_caught==8);
  assert(!std::memcmp(s.fish_records,restored.fish_records,8));
  bytes[564]=1;checksum(bytes);assert(!decode(restored,bytes,sizeof(bytes))); // below this species' minimum length
  encode(s,bytes,sizeof(bytes));bytes[574]=9;checksum(bytes);assert(!decode(restored,bytes,sizeof(bytes))); // more perfect catches than catches
  encode(s,bytes,sizeof(bytes));bytes[4]=7;std::memset(bytes+560,0,SaveBytes-564);checksum(bytes);
  assert(decode(restored,bytes,sizeof(bytes))&&discovered_item(restored,12)&&restored.fish_caught==0&&restored.collection_rewards==0);
  // Seasonal collectibles must have an approachable shore, never an unreachable water tile.
  fresh(s);s.story=3;s.visited=255;
  for(unsigned i=0;i<sizeof(Foraging)/sizeof(*Foraging);++i) {
    auto &f=Foraging[i];s.region=unsigned(f.region);s.px=(f.region==Region::Lake?24:16)*16+8;s.py=12*16+8;
    assert(walkable(s,f.x,f.y));int nx,ny;assert(next_step(s,f.x,f.y,nx,ny,true));
  }
  puts("PASS: conditioned fish populations, frame-independent timing, perfect/failed/full catches, permanent discoveries, one-time milestones, guarded v8/v7 saves and reachable collectibles");
}
