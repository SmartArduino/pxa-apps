// SPDX-License-Identifier: MIT
#include "../player_pose.hpp"
#include <cassert>
#include <cstdio>
using namespace relic;
using namespace relic::motion;
static void check(const Pose& p){
 for(int i=0;i<2;i++){
  assert(std::abs(length(sub(p.knee[i],p.hip[i]))-.45f)<.001f);
  assert(std::abs(length(sub(p.ankle[i],p.knee[i]))-.44f)<.001f);
  assert(std::abs(length(sub(p.elbow[i],p.shoulder[i]))-.30f)<.001f);
  assert(std::abs(length(sub(p.wrist[i],p.elbow[i]))-.27f)<.001f);
  assert(p.ankle[i].y-.11f>=.019f); // lowest boot vertex stays above paving
  float c=std::cos(p.foot_pitch[i]),t=std::sin(p.foot_pitch[i]);
  for(float z: {-.14f,.26f})assert(p.ankle[i].y-.11f*c-z*t>.014f);
 }
 if(p.slide>.999f){float head=p.hip_height+(2.15f-.88f)*std::cos(p.pitch);assert(head<1.04f);}
}
int main(){
 State s;start(s,42);s.next_turn=s.next_spawn=1000000;
 unsigned frames=0;
 auto simultaneous=s;action(simultaneous,Action::Jump);action(simultaneous,Action::Slide);assert(simultaneous.slide>0&&!simultaneous.slide_queued&&simultaneous.jump==0);
 for(int i=0;i<1200;i++){update(s,1.f/120);auto p=pose(s,0);check(p);frames++;}
 auto paused=s;paused.mode=Mode::Paused;auto p=pose(paused,1),q=pose(paused,500);
 for(int i=0;i<2;i++)assert(length(sub(p.ankle[i],q.ankle[i]))<.00001f);
 for(int mode=0;mode<3;mode++){
  start(s,42);s.next_turn=s.next_spawn=1000000;
  action(s,mode==0?Action::Jump:Action::Slide);auto prev=pose(s,0);
  for(int i=0;i<140;i++){update(s,1.f/120);auto p=pose(s,0);check(p);
   assert(std::abs(p.hip_height-prev.hip_height)<.08f);assert(std::abs(p.pitch-prev.pitch)<.13f);
   for(int side=0;side<2;side++)assert(length(sub(p.ankle[side],prev.ankle[side]))<.12f);
   prev=p;frames++;
  }
 }
 // Cancelling a jump into a slide reflects the parabola continuously; the
 // body must descend through the remaining height rather than teleport.
 for(float elapsed: {.16f,.44f,.72f}){
  start(s,42);s.next_turn=s.next_spawn=1000000;action(s,Action::Jump);
  for(int i=0;i<int(elapsed*120);i++)update(s,1.f/120);
  float height=s.jump;auto before=pose(s,0);action(s,Action::Slide);auto after=pose(s,0);
  assert(s.jump==height&&s.slide_queued);assert(std::abs(after.jump-before.jump)<.0001f);
  auto previous=after;
  for(int i=0;i<180;i++){update(s,1.f/120);auto p=pose(s,0);check(p);assert(std::abs(p.hip_height-previous.hip_height)<.08f);previous=p;frames++;}
 }
 start(s,42);s.next_turn=s.next_spawn=1000000;action(s,Action::Slide);
 for(int i=0;i<25;i++)update(s,1.f/120);
 float crouch=s.pose_slide;action(s,Action::Jump);assert(s.pose_slide==crouch);
 // Sliding reads as an asymmetric low brace, with a lifted lead toe.
 start(s,42);s.next_turn=s.next_spawn=1000000;action(s,Action::Slide);
 for(int i=0;i<25;i++)update(s,1.f/120);
 auto slide=pose(s,0);assert(slide.slide>.99f&&slide.foot_pitch[1]<-.2f);
 assert(slide.wrist[0].y<slide.wrist[1].y-.2f);
 // Both corner directions build a lean, bank the body, then return smoothly.
 for(int direction: {-1,1}){
  start(s,42);s.distance=168;s.next_spawn=1000000;s.turn_direction=direction;
  action(s,direction<0?Action::Left:Action::Right);float peak=0,previous=0;
  for(int i=0;i<420;i++){
   update(s,1.f/120);assert(s.mode==Mode::Running);check(pose(s,0));
   assert(std::abs(s.turn_lean-previous)<.09f);previous=s.turn_lean;
   peak=std::max(peak,s.turn_lean*direction);frames++;
   if(i==150){auto hold=s;hold.mode=Mode::Paused;float lean=hold.turn_lean,fx=hold.turn_fx;
    update(hold,.05f);assert(hold.turn_lean==lean&&hold.turn_fx==fx);}
  }
  assert(s.turns==1&&peak>.65f&&std::abs(s.turn_lean)<.02f);
 }
 printf("Player motion passed: %u frames, constant limb lengths, ground clearance, slide height, pause freeze, continuous transitions\n",frames);
}
