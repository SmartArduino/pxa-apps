// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace relic;
void advance(State& s,float seconds){for(int i=0;i<int(seconds*120);i++)update(s,1.f/120);}
State obstacle(Kind k){State s;start(s,42);s.objects[0]={2,2,k,false};s.count=1;return s;}
int main(){
 State s;start(s,42);action(s,Action::Left);advance(s,.3f);assert(s.lane==-1&&s.lane_x<-1.6f);action(s,Action::Left);assert(s.lane==-1);
 action(s,Action::Pause);float d=s.distance;advance(s,2);assert(s.distance==d);action(s,Action::Confirm);assert(s.mode==Mode::Running);
 s=obstacle(Kind::Root);advance(s,.3f);assert(s.mode==Mode::Dead);
 s=obstacle(Kind::Root);action(s,Action::Jump);advance(s,.3f);assert(s.mode==Mode::Running&&s.jump>1);advance(s,1);assert(s.jump==0);
 s=obstacle(Kind::Gate);action(s,Action::Slide);advance(s,.3f);assert(s.mode==Mode::Running);advance(s,1);assert(s.slide==0);
 s=obstacle(Kind::Gate);action(s,Action::Jump);advance(s,.3f);assert(s.mode==Mode::Dead);
 s=obstacle(Kind::Gap);action(s,Action::Jump);advance(s,.3f);assert(s.mode==Mode::Running);
 s=obstacle(Kind::Rock);s.shield=2;advance(s,.3f);assert(s.mode==Mode::Running&&s.shield==0);
 start(s,42);s.next_turn=5;action(s,Action::Right);advance(s,.7f);assert(s.mode==Mode::Running&&s.turns==1);
 start(s,42);s.next_turn=5;action(s,Action::Left);advance(s,.7f);assert(s.mode==Mode::Dead&&!strcmp(s.reason,"turn"));
 action(s,Action::Confirm);assert(s.mode==Mode::Running&&s.distance==0&&s.count==0);
 // Run a timed player through 20 seeds, all biomes and >150 turns. This
 // exercises generated obstacle spacing with actual jump/slide recovery.
 unsigned total_turns=0;
 for(int seed=1;seed<=20;seed++){
  start(s,uint32_t(seed));
  for(int frame=0;frame<22000&&s.mode==Mode::Running;frame++){
   if(s.next_turn-s.distance<12&&s.queued_turn!=s.turn_direction)action(s,s.turn_direction<0?Action::Left:Action::Right);
   for(int i=0;i<s.count;i++){auto&o=s.objects[i];float left=o.distance-s.distance;
    if(o.passed||left<0||left>s.speed*.30f)continue;
    if(o.kind==Kind::Gate&&s.slide<=0)action(s,Action::Slide);
    if((o.kind==Kind::Rock||o.kind==Kind::Root||o.kind==Kind::Gap)&&s.jump_t<=0)action(s,Action::Jump);
   }
   update(s,1.f/60);
  }
  if(s.mode!=Mode::Running){printf("seed=%d died at %.1f: %s\n",seed,s.distance,s.reason);return 1;}
  assert(s.distance>4000);assert(s.count<=20);total_turns+=s.turns;
 }
 printf("Core tests passed: input, pause, restart, collisions, powers, turns; 20 seeds >80 km, %u turns\n",total_turns);
}
