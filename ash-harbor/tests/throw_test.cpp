// SPDX-License-Identifier: MIT
#include "../combat.hpp"
#include <cassert>
#include <cstdio>
using namespace ash;
Arena duel(int owner=0,int character=0){Arena a;a.reset(false,character,character);a.ai=false;a.phase=Phase::fight;
    a.f[0].x=145;a.f[1].x=169;a.f[0].face=1;a.f[1].face=-1;
    a.feed(owner,Punch|Kick);return a;}
void step(Arena& a,int n){while(n--)a.step();}
void catchThrow(Arena& a,int owner){step(a,3);assert(a.grabOwner==owner&&a.damageEvents==0);assert(a.f[0].hp==100&&a.f[1].hp==100);}
int main(){int count=0;
 for(int owner=0;owner<2;++owner)for(int c=0;c<4;++c){
    for(int frame=0;frame<Arena::throwWindow;++frame){auto a=duel(owner,c);catchThrow(a,owner);step(a,frame);
        a.feed(1-owner,Punch|Kick);a.step();assert(a.grabOwner<0&&a.escapes==1&&a.escapeOwner==1-owner);
        assert(a.damageEvents==0&&a.f[0].hp==100&&a.f[1].hp==100);assert(a.f[0].throwProtect&&a.f[1].throwProtect);++count;}
    {auto a=duel(owner,c);catchThrow(a,owner);step(a,Arena::throwWindow);
     assert(a.grabOwner<0&&a.damageEvents==1&&a.f[1-owner].hp==100-moveFor(a.f[owner]).damage);
     assert(a.f[owner].combo==1);a.feed(1-owner,Punch|Kick);a.step();assert(a.escapes==0);++count;}
 }
 {auto a=duel();a.feed(1,Punch|Kick);step(a,3);assert(a.escapes==1&&a.damageEvents==0&&a.grabOwner<0);++count;}
 {auto a=duel();a.f[1].x=290;a.feed(1,Punch|Kick);step(a,30);a.f[0].set(Action::idle);a.f[1].set(Action::idle);
  a.f[0].x=145;a.f[1].x=169;a.feed(0,0);a.feed(0,Punch|Kick);catchThrow(a,0);step(a,12);
  assert(a.escapes==0&&a.damageEvents==1);++count;}
 {auto a=duel();step(a,2);a.feed(1,Up);a.step();assert(a.f[1].y>0&&a.grabOwner<0&&a.damageEvents==0);++count;}
 {auto a=duel();a.f[1].set(Action::guard);a.f[1].stun=1;a.f[0].buffer=0;a.step();assert(a.f[1].throwProtect>=12);
  a.feed(0,0);a.feed(0,Punch|Kick);step(a,4);assert(a.grabOwner<0);++count;}
 {auto a=duel();a.f[0].set(Action::throwing);a.f[0].age=2;a.f[0].buffer=0;
  a.f[1].set(Action::jump);a.f[1].y=1;a.f[1].vy=-1;a.step();assert(a.f[1].grounded()&&a.f[1].throwProtect>=8&&a.grabOwner<0);++count;}
 for(int target=0;target<2;++target){auto a=duel();catchThrow(a,0);
  a.shots[0]={true,1-target,12,100,a.f[target].x,40,target?-2.f:2.f,1};a.step();
  assert(a.grabOwner<0&&a.damageEvents==1&&a.f[target].hp==88);assert(a.f[1].stun==0||target==1);++count;}
 {auto a=duel();a.f[1].hp=1;step(a,15);assert(a.phase==Phase::round_end&&a.winner==0&&a.damageEvents==1);++count;}
 // Hitstop retains the posture at the attack press after all buttons release.
 struct Buffered {uint32_t keys;Action result;};
 for(auto b:{Buffered{Up|Skill,Action::rise},Buffered{Down|Kick,Action::low},Buffered{Left|Evade,Action::dodge},Buffered{Up|Right|Kick,Action::air},Buffered{Down|Skill,Action::super}}){
  Arena a;a.reset();a.ai=false;a.phase=Phase::fight;a.freeze=4;a.f[0].meter=100;a.feed(0,b.keys);a.feed(0,0);step(a,5);
  assert(a.f[0].action==b.result);if(b.result==Action::dodge)assert(a.f[0].vx<0);if(b.result==Action::air)assert(a.f[0].vx>0&&a.f[0].y>0);++count;
 }
 {auto a=duel();a.feed(0,0);a.f[1].throwProtect=12;a.feed(0,Punch);step(a,5);assert(a.f[1].hp==93);++count;}
 for(bool expired:{false,true}){Arena a;a.reset();a.ai=false;a.phase=Phase::fight;
  a.feed(0,Down);++a.tick;a.feed(0,Down|Right);++a.tick;a.feed(0,Right);
  if(expired)a.tick+=40;
  a.freeze=12;a.feed(0,Right|Punch);a.feed(0,0);step(a,13);
  assert(a.f[0].action==(expired?Action::punch:Action::fire));++count;}
 int escaped[3]{};
 for(int level=0;level<3;++level)for(int seed=1;seed<=128;++seed){auto a=duel();a.ai=true;a.difficulty=Difficulty(level);a.rng=seed*719;
  step(a,18);escaped[level]+=a.escapes>0;assert(a.escapes<=1);}
 assert(escaped[0]==0&&escaped[1]>25&&escaped[1]<100&&escaped[2]>55&&escaped[2]<120);
 {auto a=duel();catchThrow(a,0);a.clearInput();a.newRound();assert(a.grabOwner<0&&a.escapeAge==100&&!a.f[0].throwPressed&&!a.f[0].bufferHeld);++count;}
 std::printf("%d throw/input checks passed; escape frames 0..11, both sides/all fighters, late/held inputs, jump/guard protection, projectile interruption, hitstop posture, AI escapes=%d/%d/%d of 128\n",count,escaped[0],escaped[1],escaped[2]);
}
