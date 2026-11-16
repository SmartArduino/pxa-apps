// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
using namespace relic;
static void tick(State& s,float t){for(int i=0;i<int(t*120);i++)update(s,1.f/120);}
static State clean(){State s;start(s,42);s.next_spawn=s.next_turn=1000000;return s;}
static void coin(State& s,bool take=true){int i=int(s.next_coin/3);s.lane=take?coin_lane(i):(coin_lane(i)==1?-1:1);s.lane_x=s.lane*1.65f;s.distance=s.next_coin-.01f;update(s,.016f);}
static State hit(Kind kind){State s=clean();s.count=1;s.objects[0]={2,2,kind,false};return s;}
int main(){
 static_assert(sizeof(Save)==20,"Do not invalidate the installed 1.0 save record");
 State s=clean();for(int i=0;i<6;i++)coin(s);assert(multiplier(s)==2&&s.best_combo==6&&s.gold_score==175);
 for(int i=0;i<6;i++){coin(s);}
 assert(multiplier(s)==3&&s.gold_score>400);coin(s,false);assert(s.combo==0&&multiplier(s)==1);
 s=clean();for(int i=0;i<30;i++)coin(s);assert(s.rush>6&&s.feedback==Feedback::Rush&&multiplier(s)==4&&s.quests==1);
 auto gold=s.coins;auto points=s.gold_score;coin(s,false);assert(s.coins==gold+2&&s.gold_score==points+200);
 float rush=s.rush;action(s,Action::Pause);tick(s,2);assert(s.rush==rush);action(s,Action::Confirm);
 for(Kind k:{Kind::Root,Kind::Gate,Kind::Gap}){s=hit(k);s.rush=3;tick(s,.3f);assert(s.mode==Mode::Running);}
 s=clean();s.next_turn=2;s.rush=3;tick(s,.3f);assert(s.mode==Mode::Dead);action(s,Action::Rescue);assert(s.mode==Mode::Running&&s.next_turn>s.distance&&s.rescues==1&&s.shield>0);
 s=hit(Kind::Root);tick(s,.3f);assert(s.mode==Mode::Dead);float distance=s.distance;action(s,Action::Rescue);assert(s.mode==Mode::Running&&s.distance==distance&&s.objects[0].passed&&s.rescues==1);
 s.shield=s.rush=0;s.objects[0]={s.distance+2,2,Kind::Root,false};tick(s,.3f);assert(s.mode==Mode::Dead);action(s,Action::Rescue);assert(s.mode==Mode::Dead);action(s,Action::Confirm);assert(s.rescues==0&&s.mode==Mode::Running);
 s=hit(Kind::Rock);s.objects[0].lane=1;tick(s,.3f);assert(s.avoided==1&&s.feedback==Feedback::Dodge&&s.bonus==50);
 s=clean();s.next_turn=28;action(s,Action::Right);assert(s.queued_turn==1); // prompt accepts an early, deliberate turn
 s=clean();s.quests=1;s.quest_base=0;s.avoided=4;update(s,.01f);assert(s.quests==2&&s.bonus==500&&quest_progress(s)==0);
 s.turns=1;update(s,.01f);assert(s.quests==3&&s.bonus==1000&&quest_goal(s)==35&&quest_progress(s)==0);
 // Spacing remains traversable even when the energy reward is disabled.
 unsigned jumps=0,slides=0,turns=0;
 for(unsigned seed=1;seed<=20;seed++){
  start(s,seed);
  for(int frame=0;frame<12000;frame++){
   if(s.next_turn-s.distance<12&&s.queued_turn!=s.turn_direction)action(s,s.turn_direction<0?Action::Left:Action::Right);
   for(int i=0;i<s.count;i++){auto&o=s.objects[i];float left=o.distance-s.distance;if(o.passed||left<0||left>s.speed*.3f)continue;
    if(o.kind==Kind::Gate&&s.slide<=0){action(s,Action::Slide);slides++;}
    if((o.kind==Kind::Rock||o.kind==Kind::Root||o.kind==Kind::Gap)&&s.jump_t<=0){action(s,Action::Jump);jumps++;}
   }
   s.energy=-10000;s.rush=0;update(s,1.f/60);assert(s.mode==Mode::Running);
  }
  turns+=s.turns;assert(s.count<=20);
 }
 printf("Advanced rules passed: streaks, rush, quests, rescue, save schema, early turns; no-rush replay %u jumps / %u slides / %u turns\n",jumps,slides,turns);
}
