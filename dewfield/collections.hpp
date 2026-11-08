// SPDX-License-Identifier: MIT
#pragma once
#include "fishing.hpp"
namespace farm {
inline constexpr int GardenCollection[]={12,13,14,15,22,23};
inline constexpr int WildCollection[]={19,20,21,24,25,26,27,28};
inline bool discovered_item(const State &s,int item) {
  return item>=12&&item<=28&&bool(s.discoveries&(1u<<(item-12)));
}
inline int collection_size(int category) {return category==0?6:8;}
inline int collection_item(int category,int index) {
  return category==0?GardenCollection[index]:WildCollection[index];
}
inline bool collected(const State &s,int category,int index) {
  return category==2?s.fish_records[index]!=0:discovered_item(s,collection_item(category,index));
}
inline int collection_count(const State &s,int category) {
  int total=0;for(int i=0;i<collection_size(category);++i)total+=collected(s,category,i);return total;
}
inline constexpr int CollectionTotal=22, MilestoneCount=6;
inline int discovered_total(const State &s) {
  return collection_count(s,0)+collection_count(s,1)+collection_count(s,2);
}
struct Milestone {const char *cn,*en;int progress,goal,reward;};
inline Milestone milestone(const State &s,int id) {
  switch(id) {
  case 0:return {"田园新手","First harvests",int(std::min(10u,s.harvested)),10,80};
  case 1:{int n=0;for(int i=12;i<=15;++i)n+=discovered_item(s,i);return {"四季农人","Four crops",n,4,180};}
  case 2:return {"林间寻宝","Wild finds",collection_count(s,1),5,140};
  case 3:return {"溪畔钓手","Fish explorer",collection_count(s,2),4,180};
  case 4:{int n=0;for(int i=0;i<int(Region::Mine);++i)n+=bool(s.visited&(1u<<i));return {"走遍晴露","Valley explorer",n,int(Region::Mine),220};}
  default:return {"收藏大师","Valley collector",discovered_total(s),CollectionTotal,600};
  }
}
inline bool claim_collection_reward(State &s,int id) {
  if(id<0||id>=MilestoneCount||(s.collection_rewards&(1u<<id)))return false;
  auto m=milestone(s,id);
  if(m.progress<m.goal || s.gold>999999u-unsigned(m.reward))return false;
  s.gold+=m.reward;s.collection_rewards|=1u<<id;return true;
}
inline int ready_collection_rewards(const State &s) {
  int count=0;for(int i=0;i<MilestoneCount;++i) {
    auto m=milestone(s,i);count+=m.progress>=m.goal&&!(s.collection_rewards&(1u<<i));
  }return count;
}
} // namespace farm
