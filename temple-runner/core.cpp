// SPDX-License-Identifier: MIT
#include "core.hpp"
#include <algorithm>
#include <cmath>
namespace relic {
uint32_t random(State& s) {auto x=s.seed; x^=x<<13;x^=x>>17;x^=x<<5;return s.seed=x;}
void start(State& s,uint32_t seed){s=State{};s.seed=seed?seed:1;s.mode=Mode::Running;}
int coin_lane(int i){return ((i/9)%3)-1;}
float coin_height(int i){return .65f+((i%9>=3&&i%9<=5)?.18f:0.f);}
bool coin_taken(const State& s,int i){return i*3.f<s.next_coin;}
static void die(State& s,const char* why){s.mode=Mode::Dead;s.reason=why;s.death_t=0;s.flash=.45f;}
void action(State& s,Action a){
 if(a==Action::Pause){if(s.mode==Mode::Running)s.mode=Mode::Paused;else if(s.mode==Mode::Paused)s.mode=Mode::Running;return;}
 if(s.mode==Mode::Title||s.mode==Mode::Dead){if(a==Action::Confirm)start(s,s.seed+0x9e3779b9);return;}
 if(s.mode==Mode::Paused){if(a==Action::Confirm)s.mode=Mode::Running;return;}
 if(s.mode!=Mode::Running)return;
 if(a==Action::Left||a==Action::Right){int d=a==Action::Left?-1:1;
   if(s.next_turn-s.distance<16&&s.next_turn-s.distance>0){s.queued_turn=d;return;}
   s.lane=std::clamp(s.lane+d,-1,1);
 }
 if(a==Action::Jump&&s.jump_t<=0){s.jump_t=.94f;s.slide=0;}
 if(a==Action::Slide){s.slide=.88f;s.jump_t=0;s.jump=0;}
}
void update(State& s,float dt){
 dt=std::clamp(dt,0.f,.05f);s.flash=std::max(0.f,s.flash-dt);s.turn_fx*=std::max(0.f,1-dt*3.5f);
 if(s.mode==Mode::Dead){s.death_t+=dt;return;}
 if(s.mode!=Mode::Running)return;
 s.elapsed+=dt;s.steps++;float before=s.distance;
 s.speed=std::min(16.f,8.8f+s.distance*.0035f);s.distance+=s.speed*dt;
 s.lane_x+=(s.lane*1.65f-s.lane_x)*std::min(1.f,dt*15.f);
 if(s.jump_t>0){s.jump_t=std::max(0.f,s.jump_t-dt);float t=1-s.jump_t/.94f;s.jump=6.6f*t*(1-t);}else s.jump=0;
 s.slide=std::max(0.f,s.slide-dt);s.shield=std::max(0.f,s.shield-dt);s.magnet=std::max(0.f,s.magnet-dt);s.combo_t=std::max(0.f,s.combo_t-dt);
 while(s.distance>=s.next_coin){int index=int(s.next_coin/3);if(s.magnet>0||std::abs(s.lane_x-coin_lane(index)*1.65f)<.8f){s.coins++;s.combo++;s.combo_t=.8f;}else s.combo=0;s.next_coin+=3;}
 if(s.distance>=s.next_spawn){
  if(s.count<20&&std::abs(s.next_spawn+53-s.next_turn)>24){auto r=random(s);Kind k=Kind(r%4);int lane=int((r>>7)%3)-1;
   if((r>>16)%9==0)k=(r&16)?Kind::Shield:Kind::Magnet;
   if(s.distance<100&&k==Kind::Gap)k=Kind::Root;
   s.objects[s.count++]={s.next_spawn+53,int8_t((k==Kind::Root||k==Kind::Gate||k==Kind::Gap)?2:lane),k,false};}
  s.next_spawn+=std::max(17.f,s.speed*1.4f);
 }
 for(int i=0;i<s.count;i++){auto &o=s.objects[i];if(o.passed||o.distance> s.distance+.5f)continue;
  bool hit=o.lane==2||std::abs(s.lane_x-o.lane*1.65f)<.85f;
  o.passed=true;if(!hit)continue;
  if(o.kind==Kind::Shield){s.shield=12;s.pickups++;s.flash=.2f;continue;}
  if(o.kind==Kind::Magnet){s.magnet=12;s.pickups++;s.flash=.2f;continue;}
  bool safe=o.kind==Kind::Gate?s.slide>0:o.kind==Kind::Gap?s.jump>.55f:s.jump>.65f;
  if(!safe){if(s.shield>0){s.shield=0;s.flash=.35f;}else{die(s,o.kind==Kind::Gap?"gap":o.kind==Kind::Gate?"gate":"rock");break;}}
 }
 for(int i=0;i<s.count;){if(s.objects[i].distance<s.distance-8){s.objects[i]=s.objects[--s.count];}else i++;}
 if(before<s.next_turn&&s.distance>=s.next_turn){if(s.queued_turn!=s.turn_direction){die(s,"turn");}else{s.turn_fx=float(s.turn_direction);s.turns++;s.next_turn+=180;s.turn_direction=(random(s)&1)?1:-1;s.queued_turn=0;}}
 s.score=uint32_t(s.distance*5)+s.coins*25+s.turns*250;
}
}
