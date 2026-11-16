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
int multiplier(const State& s){return s.rush>0?4:s.combo>=24?4:s.combo>=12?3:s.combo>=6?2:1;}
uint32_t quest_goal(const State& s){return s.quests%3==0?25+10*(s.quests/3):s.quests%3==1?4+s.quests/3:1+s.quests/3;}
static uint32_t quest_counter(const State& s){return s.quests%3==0?s.coins:s.quests%3==1?s.avoided:uint32_t(s.turns);}
uint32_t quest_progress(const State& s){return std::min(quest_goal(s),quest_counter(s)-std::min(quest_counter(s),s.quest_base));}
static void feedback(State& s,Feedback type,float seconds=1.15f){s.feedback=type;s.feedback_t=seconds;}
static void charge(State& s,float amount){if(s.rush>0)return;s.energy=std::min(60.f,s.energy+amount);if(s.energy>=60){s.energy=0;s.rush=7;feedback(s,Feedback::Rush,1.7f);}}
static void die(State& s,const char* why){s.mode=Mode::Dead;s.reason=why;s.death_t=0;s.flash=.45f;s.combo=0;s.energy*=.5f;}
static void score(State& s){s.score=uint32_t(s.distance*5)+s.gold_score+s.turns*250+s.bonus;}
void action(State& s,Action a){
 if(a==Action::Pause){if(s.mode==Mode::Running)s.mode=Mode::Paused;else if(s.mode==Mode::Paused)s.mode=Mode::Running;return;}
 if(s.mode==Mode::Dead&&a==Action::Rescue&&s.rescues==0){
  s.rescues=1;s.mode=Mode::Running;s.death_t=0;s.saved=false;s.shield=3;s.rush=0;s.chase=2.5f;s.flash=.25f;
  if(s.next_turn<=s.distance+2){s.next_turn+=180;s.queued_turn=0;}
  for(int i=0;i<s.count;i++)if(s.objects[i].distance<s.distance+12)s.objects[i].passed=true;
  feedback(s,Feedback::Rescue,1.6f);return;
 }
 if(s.mode==Mode::Title||s.mode==Mode::Dead){if(a==Action::Confirm)start(s,s.seed+0x9e3779b9);return;}
 if(s.mode==Mode::Paused){if(a==Action::Confirm)s.mode=Mode::Running;return;}
 if(s.mode!=Mode::Running)return;
 if(a==Action::Left||a==Action::Right){int d=a==Action::Left?-1:1;
   // The visible direction prompt and the accepted turn window agree.
   if(s.next_turn-s.distance<30&&s.next_turn-s.distance>0){s.queued_turn=d;return;}
   s.lane=std::clamp(s.lane+d,-1,1);
 }
 if(a==Action::Jump&&s.jump_t<=0){s.jump_t=.94f;s.slide=0;s.slide_queued=false;}
 if(a==Action::Slide){
  if(s.jump_t>0){s.jump_t=std::min(s.jump_t,.94f-s.jump_t);s.slide_queued=true;if(s.jump_t<=.0001f){s.jump_t=s.jump=0;s.slide=.88f;s.slide_queued=false;}}
  else s.slide=.88f;
 }
}
void update(State& s,float dt){
 dt=std::clamp(dt,0.f,.05f);s.flash=std::max(0.f,s.flash-dt);s.turn_fx*=std::max(0.f,1-dt*3.5f);
 if(s.mode==Mode::Dead){s.death_t+=dt;return;}
 if(s.mode!=Mode::Running)return;
 s.elapsed+=dt;s.steps++;float before=s.distance;
 s.speed=std::min(16.f,8.8f+s.distance*.0035f);s.distance+=s.speed*dt;
 s.lane_x+=(s.lane*1.65f-s.lane_x)*std::min(1.f,dt*15.f);
 bool airborne=s.jump_t>0;
 if(airborne){s.jump_t=std::max(0.f,s.jump_t-dt*(s.slide_queued?1.45f:1.f));float t=1-s.jump_t/.94f;s.jump=6.6f*t*(1-t);}else s.jump=0;
 s.landing=std::max(0.f,s.landing-dt);if(airborne&&s.jump_t<=0){s.landing=.18f;if(s.slide_queued){s.slide=.88f;s.slide_queued=false;}}
 s.slide=std::max(0.f,s.slide-dt);s.shield=std::max(0.f,s.shield-dt);s.magnet=std::max(0.f,s.magnet-dt);s.combo_t=std::max(0.f,s.combo_t-dt);
 float slide_goal=s.slide>.12f?1.f:0.f;s.pose_slide+=std::clamp(slide_goal-s.pose_slide,-dt*8,dt*8);
 s.rush=std::max(0.f,s.rush-dt);s.feedback_t=std::max(0.f,s.feedback_t-dt);s.chase=std::max(0.f,s.chase-dt);
 while(s.distance>=s.next_coin){int index=int(s.next_coin/3);
  if(s.magnet>0||s.rush>0||std::abs(s.lane_x-coin_lane(index)*1.65f)<.8f){
   s.combo++;s.best_combo=std::max(s.best_combo,uint32_t(s.combo));s.combo_t=.6f;
   uint32_t units=s.rush>0?2:1;s.coins+=units;s.gold_score+=25*multiplier(s)*units;charge(s,2);
  }else s.combo=0;
  s.next_coin+=3;
 }
 if(s.distance>=s.next_spawn){
  if(s.count<20&&std::abs(s.next_spawn+53-s.next_turn)>30){auto r=random(s);Kind k=Kind(r%4);int lane=int((r>>7)%3)-1;
   if((r>>16)%9==0)k=(r&16)?Kind::Shield:Kind::Magnet;
   if(s.distance<100&&k==Kind::Gap)k=Kind::Root;
   s.objects[s.count++]={s.next_spawn+53,int8_t((k==Kind::Root||k==Kind::Gate||k==Kind::Gap)?2:lane),k,false};}
  s.next_spawn+=std::max(17.f,s.speed*1.4f);
 }
 for(int i=0;i<s.count;i++){auto &o=s.objects[i];if(o.passed||o.distance>s.distance+.5f)continue;
  float separation=std::abs(s.lane_x-o.lane*1.65f);bool hit=o.lane==2||separation<.85f;
  o.passed=true;
  if(!hit){if(o.kind==Kind::Rock&&separation<2.f){s.avoided++;s.bonus+=50*multiplier(s);charge(s,4);if(s.feedback!=Feedback::Rush||s.feedback_t<=0)feedback(s,Feedback::Dodge);}continue;}
  if(o.kind==Kind::Shield){s.shield=12;s.pickups++;s.flash=.2f;continue;}
  if(o.kind==Kind::Magnet){s.magnet=12;s.pickups++;s.flash=.2f;continue;}
  bool safe=o.kind==Kind::Gate?s.slide>0:o.kind==Kind::Gap?s.jump>.55f:s.jump>.65f;
  if(safe){s.avoided++;s.bonus+=75*multiplier(s);charge(s,8);if(s.feedback!=Feedback::Rush||s.feedback_t<=0)feedback(s,o.kind==Kind::Gate?Feedback::Slide:Feedback::Jump);}
  else if(s.rush>0){s.avoided++;s.bonus+=50;}
  else if(s.shield>0){s.shield=0;s.flash=.35f;s.chase=2;feedback(s,Feedback::Shield);}
  else{die(s,o.kind==Kind::Gap?"gap":o.kind==Kind::Gate?"gate":"rock");break;}
 }
 for(int i=0;i<s.count;){if(s.objects[i].distance<s.distance-8){s.objects[i]=s.objects[--s.count];}else i++;}
 if(s.mode==Mode::Running&&before<s.next_turn&&s.distance>=s.next_turn){
  if(s.queued_turn!=s.turn_direction)die(s,"turn");
  else{s.turn_fx=float(s.turn_direction);s.turns++;s.bonus+=100*multiplier(s);feedback(s,Feedback::Turn);charge(s,8);s.next_turn+=180;s.turn_direction=(random(s)&1)?1:-1;s.queued_turn=0;}
 }
 if(s.mode==Mode::Running&&quest_progress(s)>=quest_goal(s)){
  s.bonus+=500;s.quests++;s.quest_base=quest_counter(s);feedback(s,Feedback::Quest,1.5f);charge(s,10);
 }
 score(s);
}
}
