// SPDX-License-Identifier: MIT
#pragma once
#include "living.hpp"
#include <algorithm>
#include <cstdlib>
namespace farm {
struct WildTree {Region region;uint8_t x,y,variant;};
inline constexpr WildTree WildTrees[]={
 {Region::Forest,6,5,1},{Region::Forest,18,5,0},{Region::Forest,24,5,2},
 {Region::Forest,7,10,0},{Region::Forest,18,10,2},{Region::Forest,24,10,1},
 {Region::Forest,6,15,2},{Region::Forest,18,15,1},{Region::Forest,24,15,0},
 {Region::Forest,6,20,1},{Region::Forest,18,19,0},{Region::Forest,24,20,2},{Region::Forest,22,6,0},
 {Region::Lake,22,6,1},{Region::Lake,28,15,0},{Region::Lake,22,18,2},{Region::Lake,27,18,0},
 {Region::Orchard,6,7,3},{Region::Orchard,12,7,3},{Region::Orchard,20,7,3},{Region::Orchard,26,7,3},
 {Region::Orchard,6,16,3},{Region::Orchard,12,16,3},{Region::Orchard,20,16,3},{Region::Orchard,26,16,3}};
static_assert(sizeof(WildTrees)/sizeof(*WildTrees)==WildTreeCount);
inline int wild_tree_at(const State&s,int x,int y) {
 for(int i=0;i<WildTreeCount;++i){auto &t=WildTrees[i];if(unsigned(t.region)==s.region&&t.x==x&&t.y==y)return i;}return -1;
}
inline int wild_stage(const State&s,int i){return s.wild_age[i]>=6?3:s.wild_age[i]<2?0:s.wild_age[i]<5?1:2;}
inline int wild_hp(const State&s,int i){constexpr int hp[]={5,5,7,6};int stage=wild_stage(s,i);return stage==3?0:stage==0?1:stage==1?3:hp[WildTrees[i].variant];}
inline int wild_hits(const State&s,int i){int power=1+s.tool_grade[4];return (wild_hp(s,i)-s.wild_damage[i]+power-1)/power;}
inline Result chop_wild(State&s,int i) {
 if(i<0||i>=WildTreeCount)return Result::Blocked;
 if(s.wild_age[i]==6) {
   if(Tool(s.tool)!=Tool::Axe||s.wood>=999)return Result::Blocked;
   if(!spend_energy(s,2))return Result::NoEnergy;
   s.wild_age[i]=7;change_item(s,17,1);sync_inventory(s);return Result::Cleared;
 }
 if(s.wild_age[i]==7)return Result::Blocked;
 auto t=Tool(s.tool);bool seed=wild_stage(s,i)==0;
 if(t!=Tool::Axe&&!(seed&&(t==Tool::Hand||t==Tool::Hoe)))return Result::Blocked;
 int power=t==Tool::Axe?1+s.tool_grade[4]:1,wood=wild_stage(s,i)==2?4:wild_stage(s,i)==1?1:0;
 bool final=s.wild_damage[i]+power>=wild_hp(s,i);
 if(final&&wood&&s.wood>999-wood)return Result::Blocked;
 if(!spend_energy(s,t==Tool::Axe&&s.tool_grade[4]?2:3))return Result::NoEnergy;
 if(!final){s.wild_damage[i]+=power;return Result::Struck;}
 s.wild_age[i]=wild_stage(s,i)==2?6:7;s.wild_damage[i]=0;if(wood)change_item(s,17,wood);sync_inventory(s);return Result::Cleared;
}
inline bool growth_roll(const State&s,int x,int y,int percent) {
 uint32_t h=uint32_t(s.day)*747796405u+uint32_t(x)*2891336453u+uint32_t(y)*277803737u;
 h^=h>>16;h*=2246822519u;h^=h>>13;return h%100<unsigned(percent);
}
inline bool tree_space(const State&s,Region region,int x,int y,int age) {
 if(season_of(s.day)==3)return false;
 if(unsigned(region)==s.region&&int(s.px)/16==x&&int(s.py)/16==y)return false;
 State probe=s;probe.region=unsigned(region);
 if(region==Region::Farm){int i=plot_index(x,y);if(i>=0&&s.plots[i].tilled)return false;}
 if(age>=4)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(dx||dy) {
  auto g=ground(probe,x+dx,y+dy);
  if(g==Ground::Tree||g==Ground::Rock||g==Ground::Wall)return false;
  if(region==Region::Farm){int i=plot_index(x+dx,y+dy);if(i>=0&&s.plots[i].tilled)return false;}
 }
 return true;
}
inline void wild_night(State&s) {
 for(int i=0;i<WildTreeCount;++i) {
  auto &age=s.wild_age[i];auto &t=WildTrees[i];
  if(age==6||age==5)continue;
  if(!tree_space(s,t.region,t.x,t.y,age))continue;
  if(age==7){if(growth_roll(s,t.x,t.y,15))age=0;}
  else if(growth_roll(s,t.x,t.y,45))++age;
 }
}
inline bool fruit_tree_ready(const State&s,int x,int y) {
 if(s.region!=unsigned(Region::Orchard))return true;
 for(int i=17;i<WildTreeCount;++i)if(std::abs(int(WildTrees[i].x)-x)<=1&&std::abs(int(WildTrees[i].y)-y)<=1)
  return wild_stage(s,i)==2;
 return true;
}
}
