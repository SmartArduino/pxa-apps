// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <array>
#include <cstdint>
namespace relic {
enum class Cue:uint8_t {Coin,Jump,Slide,Land,Turn,Hit,Confirm,Reward,None};
struct CueInfo {const char* path;uint8_t track,priority;int16_t gain;uint32_t gap_us;};
inline constexpr CueInfo cue_info[]={
 {"assets/sfx/coin.pcm",0,1,-11*256,160000},
 {"assets/sfx/jump.pcm",1,2,-12*256,70000},
 {"assets/sfx/slide.pcm",1,3,-12*256,70000},
 {"assets/sfx/land.pcm",1,1,-15*256,100000},
 {"assets/sfx/turn.pcm",1,4,-13*256,100000},
 {"assets/sfx/hit.pcm",2,5,-10*256,350000},
 {"assets/sfx/confirm.pcm",2,1,-14*256,120000},
 {"assets/sfx/reward.pcm",2,4,-11*256,550000}
};
struct SoundSnapshot {
 Mode mode;int lane,queued,turns;uint32_t coins,quests,pickups;bool airborne,slide_queued,rush;
 explicit SoundSnapshot(const State& s):mode(s.mode),lane(s.lane),queued(s.queued_turn),turns(s.turns),coins(s.coins),quests(s.quests),pickups(s.pickups),airborne(s.jump_t>0),slide_queued(s.slide_queued),rush(s.rush>0){}
};
// Three fixed categories, latest relevant events only. Nothing is synthesized
// or allocated during play, and cold asset loading never stalls a game frame.
struct Soundscape {
 struct Pending {Cue cue=Cue::None;uint64_t at=0;int16_t gain=0;};
 std::array<Pending,3> pending{};std::array<uint64_t,3> next{};
 bool enabled=true;uint32_t played=0,dropped=0;uint8_t played_mask=0;
 void clear(){pending={};next={};}
 void enable(bool value){enabled=value;if(!value)clear();}
 void request(Cue cue,uint64_t now,int16_t gain=0){
  if(!enabled||cue==Cue::None)return;
  auto info=cue_info[unsigned(cue)];auto& p=pending[info.track];
  if(now<next[info.track]){dropped++;return;}
  if(p.cue!=Cue::None&&cue_info[unsigned(p.cue)].priority>=info.priority)return;
  p={cue,now,gain?gain:info.gain};
 }
 const Pending* ready(unsigned track,uint64_t now){
  auto& p=pending[track];if(p.cue==Cue::None)return nullptr;
  if(!enabled||now-p.at>200000){p={};dropped++;return nullptr;}
  return &p;
 }
 void accepted(unsigned track,uint64_t now){
  auto cue=pending[track].cue;next[track]=now+cue_info[unsigned(cue)].gap_us;
  played_mask|=uint8_t(1u<<unsigned(cue));played++;pending[track]={};
 }
 void action(Action action,const SoundSnapshot& before,const State& after,uint64_t now){
  if(before.mode!=after.mode){
   clear();
   if(after.mode==Mode::Running)request(action==Action::Rescue?Cue::Reward:Cue::Confirm,now);
   return;
  }
  if(after.mode!=Mode::Running)return;
  if(action==Action::Jump&&!before.airborne&&after.jump_t>0)request(Cue::Jump,now);
  else if(action==Action::Slide&&(!before.airborne||(!before.slide_queued&&after.slide_queued)))request(Cue::Slide,now);
  else if(before.lane!=after.lane||before.queued!=after.queued_turn)request(Cue::Turn,now,-18*256);
 }
 void frame(const SoundSnapshot& before,const State& after,uint64_t now){
  if(before.mode==Mode::Running&&after.mode==Mode::Dead){clear();request(Cue::Hit,now);return;}
  if(after.mode!=Mode::Running)return;
  const bool reward=after.quests>before.quests||(!before.rush&&after.rush>0)||after.pickups>before.pickups;
  if(reward){pending[0]={};request(Cue::Reward,now);}
  else if(after.coins>before.coins)request(Cue::Coin,now);
  if(after.turns>before.turns)request(Cue::Turn,now);
  else if(before.airborne&&after.jump_t<=0)request(after.slide>0?Cue::Slide:Cue::Land,now);
 }
};
}
