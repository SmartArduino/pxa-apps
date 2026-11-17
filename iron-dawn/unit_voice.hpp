// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "voice_data.hpp"
namespace iron {
enum class UnitCue {select,move,attack,stop,ready};
// One radio speaker at a time. A recent command may wait for a sentence to
// finish; repeated selections and obsolete acknowledgements are discarded.
struct UnitVoice {
 int pending=-1,last_kind=-1;unsigned priority=0,sequence=0;
 uint64_t expires=0,busy_until=0,next_start=0;
 std::array<uint8_t,15> variation{};
 static int speaker(const World& w,int preferred=-1,int previous=-1,bool combat=false){
  auto eligible=[&](int i){auto& a=w.e[i];return a.hp>0&&a.team==0&&a.selected&&a.kind<(combat?harvester:hq);};
  if(preferred>=0&&preferred<MAX&&eligible(preferred))return w.e[preferred].kind;
  for(auto& a:w.e)if(a.hp>0&&a.team==0&&a.selected&&a.kind==previous&&a.kind<(combat?harvester:hq))return a.kind;
  for(int i=0;i<MAX;++i)if(eligible(i))return w.e[i].kind;
  return -1;
 }
 static std::array<bool,MAX> roster(const World& w){std::array<bool,MAX> alive{};for(int i=0;i<MAX;++i)alive[i]=w.e[i].hp>0&&w.e[i].team==0&&w.e[i].kind<hq;return alive;}
 void clear(){pending=-1;priority=0;expires=busy_until=next_start=0;++sequence;}
 bool offer(int kind,UnitCue cue,uint64_t now){
  if(kind<0||kind>harvester||(kind==harvester&&(cue==UnitCue::attack||cue==UnitCue::stop)))return false;
  last_kind=kind;unsigned rank=cue==UnitCue::select?1:cue==UnitCue::ready?2:3;
  if(now<next_start||(rank<3&&now<busy_until)||(pending>=0&&priority>rank))return false;
  int slot=cue==UnitCue::select?0:cue==UnitCue::move?2:cue==UnitCue::attack?4:cue==UnitCue::stop?6:7;
  if(slot<6)slot+=variation[kind*3+slot/2]++%2;
  pending=kind*8+slot;priority=rank;expires=now+1200;++sequence;return true;
 }
 int take(uint64_t now){if(pending<0)return -1;if(now>expires){pending=-1;priority=0;return -1;}if(now<busy_until||now<next_start)return -1;int id=pending;pending=-1;priority=0;return id;}
 void started(int id,uint64_t now){busy_until=now+voice_lengths[id]+100;next_start=now+250;}
};
}
