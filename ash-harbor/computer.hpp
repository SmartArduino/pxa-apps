// SPDX-License-Identifier: MIT
#pragma once
#include "combat.hpp"

namespace ash {
inline void Arena::computer(){
    auto& p=f[1];const auto& q=f[0];
    if(training&&practice!=Practice::spar){
        // These are deliberately deterministic training dummies, not opponents.
        if(brain.samples)brain={};
        uint32_t buttons=0;
        if(practice==Practice::guard||(practice==Practice::after_hit&&practiceHit))
            buttons=(p.x>=q.x?Right:Left)|((q.action==Action::low)?Down:0u);
        feed(1,buttons);return;
    }
    if(!ai&&!training)return;
    const int level=std::clamp(int(difficulty),0,2),stage=std::clamp(aiStage,0,2);
    const int reaction=(level==0?22:level==1?14:9)-(level?stage:0);
    const int cadence=level==0?12:level==1?6:4;
    ComputerView now;
    now.x=q.x;now.y=q.y;now.vx=q.vx;now.vy=q.vy;now.action=q.action;
    now.age=q.age;now.character=q.character;now.stun=q.stun;now.hp=q.hp;now.stop=freeze;
    now.selfAction=p.action;now.selfAge=p.age;
    now.confirmed=grabOwner<0&&p.connected&&q.action==Action::hurt&&hitOwner==1&&!hit.block;
    float nearest=1000;
    for(const auto& shot:shots)if(shot.active&&shot.owner==0&&
        (p.x-shot.x)*shot.speed>0&&std::abs(p.x-shot.x)<nearest){
        nearest=std::abs(p.x-shot.x);now.shot=true;
        now.shotX=shot.x;now.shotY=shot.y;now.shotSpeed=shot.speed;
    }
    brain.history[brain.samples%brain.history.size()]=now;
    ++brain.samples;
    if(grabOwner>=0){
        brain.motionCount=brain.motionIndex=0;brain.intent=0;brain.think=0;
        // Being visibly grabbed is self-feedback, not knowledge of an input.
        // One delayed attempt per grab, with mistakes even on hard difficulty.
        if(grabOwner==0&&!brain.escapeDecided&&grabAge>=(level==0?14:level==1?10:7)){
            brain.escapeDecided=true;
            if(random()%100<unsigned(level==0?15:level==1?45:70)){feed(1,Punch|Kick);return;}
        }
        feed(1,0);return;
    }
    brain.escapeDecided=false;
    const bool recovered=brain.attacking&&!attack(p.action);
    brain.attacking=attack(p.action);
    if(recovered&&p.free()){
        brain.think=std::max(brain.think,level==0?20:level==1?3:0);brain.intent=0;
    }
    if(p.stun||p.action==Action::down||p.action==Action::win){
        brain.motionCount=brain.motionIndex=0;brain.intent=0;brain.think=0;feed(1,0);return;
    }
    if(brain.samples<=unsigned(reaction)){feed(1,0);return;}
    const auto& seen=brain.history[(brain.samples-1-reaction)%brain.history.size()];
    const uint32_t forward=p.face>0?Right:Left,back=p.face>0?Left:Right;
    const float distance=std::abs(p.x-seen.x);
    auto chance=[&](unsigned percent){return random()%100<percent;};
    auto act=[&](Tactic tactic,uint32_t buttons,int delay=0){
        brain.tactic=tactic;brain.think=delay?delay:cadence;
        brain.intent=buttons&(Left|Right|Down); // Attacks and jumps are single presses.
        feed(1,buttons);
    };
    auto ownMove=[&](Action action){Fighter probe;probe.character=p.character;probe.action=action;return moveFor(probe);};
    // Committed 214+K uses the same directional history and cancel rules as a player.
    if(brain.motionIndex<brain.motionCount){feed(1,brain.motion[brain.motionIndex++]);return;}
    if(brain.think>0){--brain.think;feed(1,brain.intent);return;}
    brain.intent=0;
    // Only a delayed, unblocked hit permits a planned cancel. A blocked poke
    // recovers normally; meter is never spent on a blind guaranteed reversal.
    if(attack(p.action)){
        if(seen.confirmed&&seen.selfAction==p.action&&seen.selfAge<=p.age&&p.connected&&
            chance(level==0?10:level==1?66:90)){
            const int remaining=seen.stun-std::max(0,reaction-seen.stop);
            if(p.grounded()&&p.meter>=100&&p.action!=Action::super&&distance<75&&ownMove(Action::super).startup<remaining)
                {act(Tactic::confirm,Down|Skill);return;}
            if(move(p.action).cancel){
                if(p.action==Action::punch&&distance<ownMove(Action::kick).reach+7&&ownMove(Action::kick).startup<remaining)
                    {act(Tactic::confirm,Kick);return;}
                if(p.grounded()&&distance<48&&ownMove(Action::rise).startup<remaining){act(Tactic::confirm,Up|Skill);return;}
                if(p.grounded()&&ownMove(Action::fire).startup<remaining){act(Tactic::confirm,Skill);return;}
            }
        }
        feed(1,0);return;
    }
    if(!p.grounded()){
        if(p.action==Action::jump&&p.y>9&&p.vy<1.8f&&distance<61&&seen.hp>0)
            {act(Tactic::poke,Kick|(p.vx>0?Right:p.vx<0?Left:0u));return;}
        feed(1,0);return;
    }
    if(!p.free()||seen.action==Action::down||seen.action==Action::win||seen.hp<=0){feed(1,0);return;}
    Fighter opponent;opponent.character=seen.character;opponent.action=seen.action;
    const Move enemy=moveFor(opponent);
    const bool enemyAttack=attack(seen.action);
    if(enemyAttack&&(brain.threat!=seen.action||seen.age<brain.threatAge))
        brain.guardAttack=chance(level==0?36:level==1?67:84);
    if(!enemyAttack)brain.guardAttack=false;
    brain.threat=seen.action;brain.threatAge=seen.age;
    // Read an incoming projectile's flight, not the opponent's fire button.
    const float shotEta=seen.shot?((p.x-seen.shotX)/seen.shotSpeed-reaction):1000;
    const bool shotDanger=shotEta>-6&&shotEta<30&&p.y<seen.shotY+10&&p.y+72>seen.shotY-8;
    if(shotDanger){
        if(shotEta>14&&distance>88&&chance(level==0?12:p.character==3?52:28))
            {act(Tactic::jump,Up|forward,12);return;}
        if(chance(level==0?48:level==1?78:92)){act(Tactic::guard,back,6);return;}
    }
    // Check projected horizontal jump position before committing to a rising attack.
    const float landingX=seen.x+seen.vx*float(std::min(reaction+6,18));
    if(seen.y>12&&seen.y<100&&std::abs(landingX-p.x)<53&&
        chance(level==0?12:level==1?62:85)){
        act(Tactic::anti_air,Up|Skill,12);return;
    }
    const bool imminent=enemyAttack&&seen.age<enemy.startup+enemy.active&&
        enemy.reach>0&&distance<enemy.reach+18&&seen.y<80;
    if(imminent&&brain.guardAttack){
        act(Tactic::guard,back|(enemy.low_attack?Down:0u),cadence);return;
    }
    // Respect startup, active and recovery frames. A slow kick does not punish
    // a jab that has already recovered by the time the observation is processed.
    if(enemyAttack&&seen.age>=enemy.startup+enemy.active){
        const int remaining=enemy.total()-seen.age-reaction;
        for(Action reply:{Action::kick,Action::punch}){
            Move counter=ownMove(reply);
            if(remaining>=counter.startup+2&&distance<counter.reach+7&&chance(level==0?30:level==1?70:94))
                {act(Tactic::punish,reply==Action::kick?Kick:Punch);return;}
        }
    }
    constexpr float spacing[]={43,61,30,47};
    const float desired=spacing[p.character];
    const bool canBack=(back==Left?p.x>35:p.x<285);
    if(distance>(p.character==2?desired-2:desired+7)){
        // Longer-range Lan uses projectiles; Zhen closes to his throw range.
        const unsigned fireChance=(p.character==1?40:p.character==2?9:23)/(level==0?2:1);
        bool ownShot=false;for(const auto& shot:shots)ownShot|=shot.active&&shot.owner==1;
        if(distance>100&&distance<230&&!ownShot&&!shotDanger&&chance(fireChance))
            {act(Tactic::fire,Skill,18);return;}
        if(distance>80&&distance<148&&p.character!=1&&!imminent&&!shotDanger&&chance(p.character==3?30:12)){
            brain.motion={0,Down,Down,Down|back,Down|back,back|Kick,back,0};
            brain.motionCount=8;brain.motionIndex=1;act(Tactic::rush,0);return;
        }
        if(distance>67&&distance<130&&!imminent&&chance(p.character==3?16:6))
            {act(Tactic::jump,Up|forward,14);return;}
        act(Tactic::approach,forward);return;
    }
    if(distance<desired-10&&canBack&&chance(p.character==1?65:24))
        {act(Tactic::space,back);return;}
    if(distance<ownMove(Action::throwing).reach-1&&seen.y<=0&&seen.stun==0&&
        chance(p.character==2?62:level==0?10:22))
        {act(Tactic::throwing,Punch|Kick,12);return;}
    // Occasionally commit to meter pressure in neutral. This is a prediction
    // with normal startup/recovery, so the player can block or punish it.
    // It is distinct from confirming an attack that has already connected.
    if(p.meter>=100&&!enemyAttack&&seen.stun==0&&seen.y<=0&&
        distance<ownMove(Action::super).reach+4&&chance(level==0?2:level==1?5:8))
        {act(Tactic::poke,Down|Skill,12);return;}
    if(chance(level==0?24:level==1?64:78)){
        uint32_t buttons;
        if(distance<45&&seen.y<=0&&chance(27))buttons=Down|Kick;
        else if(distance<ownMove(Action::punch).reach+7&&chance(p.character==0||p.character==2?72:24))buttons=Punch;
        else if(distance<ownMove(Action::kick).reach+7)buttons=Kick;
        else {act(Tactic::approach,forward);return;}
        act(Tactic::poke,buttons,level==0?16:level==1?9:5);return;
    }
    // Short pauses and spacing steps leave openings even on hard difficulty.
    act(Tactic::space,canBack&&distance<desired?back:0u,cadence+3);
}
} // namespace ash
