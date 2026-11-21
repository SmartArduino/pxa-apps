// SPDX-License-Identifier: MIT
#include "../combat.hpp"
#include <cassert>
#include <cstdio>
using namespace ash;
Arena setup(unsigned seed=1){
    Arena a;a.reset();a.phase=Phase::fight;a.rng=seed;a.difficulty=Difficulty::hard;
    a.f[0].x=145;a.f[1].x=185;return a;
}
void watch(Arena& a,int frames){for(int n=0;n<frames;++n){a.computer();++a.tick;}}
int main(){
    int cases=0;
    // Identical visible poses with different hidden inputs yield identical AI.
    for(unsigned seed=1;seed<=64;++seed){
        auto a=setup(seed),b=a;b.f[0].held=Up|Skill;b.f[0].buffer=Punch;
        b.f[0].command.push(2,0);b.f[0].command.push(3,1);b.f[0].command.push(6,2);
        for(int n=0;n<100;++n){a.computer();b.computer();++a.tick;++b.tick;
            assert(a.f[1].held==b.f[1].held&&a.rng==b.rng);}
        ++cases;
    }
    // Even an obvious jump cannot be reacted to before the perception delay.
    for(int level=0;level<3;++level){
        int delay=level==0?22:level==1?14:9,anti=0;
        for(unsigned seed=1;seed<=64;++seed){auto a=setup(seed);a.difficulty=Difficulty(level);
            a.f[0].x=40;watch(a,36);a.f[0].x=150;a.f[0].y=35;a.f[0].action=Action::jump;
            for(int n=0;n<delay;++n){a.computer();++a.tick;assert(a.brain.tactic!=Tactic::anti_air);}
            for(int n=0;n<24;++n){a.computer();++a.tick;anti+=a.brain.tactic==Tactic::anti_air;}
        }
        assert(anti>0);++cases;
    }
    int punished=0,confirmed=0;
    for(unsigned seed=1;seed<=64;++seed){
        auto a=setup(seed);a.f[0].action=Action::rise;a.f[0].age=19;
        for(int n=0;n<19;++n){a.computer();++a.tick;++a.f[0].age;
            if(a.brain.tactic==Tactic::punish){++punished;break;}}
        auto hit=setup(seed),blocked=hit;
        for(auto* arena:{&hit,&blocked}){arena->f[1].set(Action::kick);arena->f[1].age=15;
            arena->f[1].connected=true;arena->f[1].meter=100;arena->hitOwner=1;}
        hit.f[0].set(Action::hurt);hit.f[0].stun=38;
        blocked.f[0].set(Action::guard);blocked.hit.block=true;
        for(int n=0;n<24;++n){
            hit.computer();blocked.computer();++hit.tick;++blocked.tick;
            assert(!(blocked.f[1].held&Skill));
            if((hit.f[1].held&(Down|Skill))==(Down|Skill)){++confirmed;break;}
        }
        // A receding projectile must not trigger a projectile defense.
        auto clear=setup(seed),away=clear;clear.f[0].x=away.f[0].x=20;
        away.shots[0]={true,0,12,100,100,50,-3,0};
        for(int n=0;n<90;++n){clear.computer();away.computer();++clear.tick;++away.tick;
            assert(clear.f[1].held==away.f[1].held);}
        ++cases;
    }
    assert(punished>=48&&confirmed>=48);
    // A queued motion cannot execute after a hit or leak into the next round.
    {auto a=setup();a.brain.motion={Down,Down|Left,Left|Kick};a.brain.motionCount=3;
     a.f[1].stun=10;a.computer();assert(a.brain.motionCount==0&&a.f[1].held==0);
     a.newRound();assert(a.brain.samples==0&&a.brain.intent==0);++cases;}
    // Training guard/stand/after-hit behavior stays exact, independently of AI.
    for(Practice mode:{Practice::stand,Practice::guard,Practice::after_hit}){
        auto a=setup();a.training=true;a.practice=mode;a.f[0].set(Action::low);
        a.computer();assert(a.f[1].held==(mode==Practice::guard?uint32_t(Right|Down):0u));
        a.practiceHit=true;a.computer();assert(a.f[1].held==(mode==Practice::stand?0u:uint32_t(Right|Down)));++cases;
    }
    {auto a=setup();a.ai=false;a.feed(1,Left|Punch);watch(a,60);
     assert(a.f[1].held==(Left|Punch)&&a.brain.samples==0);++cases;}
    // Real legal commands must connect/cancel, rather than directly mutating moves.
    for(int c=0;c<4;++c){auto a=setup();a.reset(true,0,c);a.phase=Phase::fight;
        a.practice=Practice::spar;a.difficulty=Difficulty::hard;bool linked=false,super=false;
        for(int n=0;n<3600;++n){a.step();linked|=a.f[1].combo>=2;super|=a.f[1].action==Action::super;}
        // Han's slower super needs a longer opening than his ordinary pokes.
        // Its real confirmation is checked after throws in the guard scenario.
        assert(linked&&(c==2||super));++cases;
    }
    // The grappler must open a passive guard rather than loop blocked pokes.
    for(int level=1;level<=2;++level)for(bool mirrored:{false,true}){
        auto a=setup();a.reset(true,0,2);a.phase=Phase::fight;a.practice=Practice::spar;a.difficulty=Difficulty(level);
        a.f[0].x=mirrored?185:145;a.f[1].x=mirrored?145:185;
        a.f[0].face=mirrored?-1:1;a.f[1].face=-a.f[0].face;bool threw=false,super=false;
        for(int n=0;n<2400;++n){a.feed(0,a.f[0].face>0?Left:Right);a.step();threw|=a.f[1].action==Action::throwing;super|=a.f[1].action==Action::super;}
        assert(threw&&(level!=2||super));++cases;
    }
    assert(sizeof(ComputerState)<3072);
    std::printf("%d AI checks passed: delayed reaction, hidden-input independence, recovery punish, unblocked hit confirm, projectile direction, training modes, legal combos and round reset. State=%zu bytes, Arena=%zu bytes\n",cases,sizeof(ComputerState),sizeof(Arena));
}
