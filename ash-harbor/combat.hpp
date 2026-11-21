// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include "roster.hpp"

namespace ash {
// Fixed 60 Hz simulation. All durations are simulation frames, never render ticks.
enum Key : uint32_t {Up=1,Down=2,Left=4,Right=8,Punch=16,Kick=32,Evade=256,Skill=512};
enum class Action {idle,walk,crouch,jump,punch,kick,low,air,fire,rise,rush,super,
                   dodge,guard,hurt,down,win,throwing};
enum class Phase {intro,fight,round_end,match_end};
enum class Difficulty {easy,normal,hard};
enum class Practice {stand,guard,after_hit,spar};
struct Direction {int value=5;uint32_t tick=0,serial=0;};
struct CommandBuffer {
    std::array<Direction,48> data{};
    unsigned count=0;
    uint32_t serial=0,used=0;
    void clear(){count=0;used=serial;}
    void push(int dir,uint32_t tick){
        if(count&&data[(count-1)%data.size()].value==dir)return;
        data[count%data.size()]={dir,tick,++serial};++count;
    }
    bool match(std::initializer_list<int> command,uint32_t tick,unsigned window=26)const{
        if(!count)return false;
        int want=int(command.size())-1,seen=0;
        const int* sequence=command.begin();
        for(unsigned n=0;n<std::min<unsigned>(count,data.size());++n){
            const auto& d=data[(count-1-n)%data.size()];
            if(d.serial<=used||tick-d.tick>window)return false;
            if(d.value==5)continue;
            if(!seen&&tick-d.tick>8)return false;
            if(d.value==sequence[want]){seen=1;if(--want<0)return true;}
            else if(seen)return false; // No arbitrary opposite directions inside motions.
            else return false;
        }
        return false;
    }
    void consume(){used=serial;}
    void pause(){for(unsigned n=0;n<std::min<unsigned>(count,data.size());++n)++data[(count-1-n)%data.size()].tick;}
};
struct Move {
    int startup=0,active=0,recovery=0,damage=0,stun=0;
    float reach=0,low=0,high=0;
    int hitstop=0;
    bool low_attack=false,overhead=false,cancel=false;
    int total()const{return startup+active+recovery;}
};
inline Move move(Action a){
    switch(a){
    case Action::punch:return {5,3,11,7,16,37,22,70,5,false,false,true};
    case Action::kick:return {9,4,17,12,23,52,34,77,7,false,false,true};
    case Action::low:return {7,4,15,9,18,44,0,31,6,true,false,true};
    case Action::air:return {4,10,10,10,20,44,4,66,6,false,true,false};
    case Action::fire:return {12,3,22,12,22,0,0,0,7};
    case Action::rise:return {5,12,29,18,29,39,12,116,9};
    case Action::rush:return {10,6,21,16,26,44,18,70,8};
    case Action::super:return {18,8,32,31,38,61,0,110,12};
    case Action::throwing:return {3,2,28,16,32,27,0,72,8};
    default:return {};
    }
}
inline bool attack(Action a){return move(a).active>0;}
struct Fighter {
    int character=0,face=1,hp=100,meter=0,age=0,stun=0,invulnerable=0;
    int combo=0,comboDamage=0,comboTimer=0;
    float x=90,y=0,vx=0,vy=0,walkDistance=0;
    uint32_t held=0,buffer=0,bufferAt=0,last=0,bufferHeld=0,throwPressedAt=0;
    int throwProtect=0;
    bool throwPressed=false;
    CommandBuffer command;
    Action action=Action::idle;
    bool connected=false,spawned=false;
    void set(Action a){action=a;age=0;connected=spawned=false;if(a==Action::walk)walkDistance=0;}
    int walkFrame()const{int step=int(std::floor(walkDistance/10.f));return (step%8+8)%8;}
    bool grounded()const{return y<=0&&vy<=0;}
    bool free()const{return action==Action::idle||action==Action::walk||action==Action::crouch||action==Action::guard;}
    int direction()const{
        int horizontal=((held&Right)?1:0)-((held&Left)?1:0);
        int vertical=((held&Up)?1:0)-((held&Down)?1:0);
        return 5+horizontal*face+vertical*3;
    }
    bool back()const{return face>0?(held&Left)&&!(held&Right):(held&Right)&&!(held&Left);}
    bool crouching()const{return grounded()&&(held&Down);}
};
inline Move moveFor(const Fighter& p){
    Move m=move(p.action);
    if(p.character==1){
        if(p.action==Action::kick){m.reach+=6;m.startup+=2;m.damage+=1;}
        if(p.action==Action::fire){m.startup+=2;m.damage+=2;}
        if(p.action==Action::rise){m.startup+=2;m.damage+=2;}
        if(p.action==Action::rush){m.reach+=8;m.recovery+=3;}
    }else if(p.character==2){
        if(p.action==Action::punch){m.damage+=2;m.startup+=2;}
        if(p.action==Action::kick){m.damage+=3;m.startup+=3;m.recovery+=3;}
        if(p.action==Action::throwing){m.damage+=6;m.reach+=4;m.recovery+=5;}
        if(p.action==Action::fire){m.damage+=4;m.startup+=4;}
        if(p.action==Action::rise){m.damage+=3;m.recovery+=5;}
        if(p.action==Action::super){m.damage+=7;m.startup+=4;}
    }else if(p.character==3){
        if(p.action==Action::punch){m.startup-=1;m.damage-=1;}
        if(p.action==Action::kick){m.startup-=2;m.recovery-=4;m.damage-=1;m.reach+=5;}
        if(p.action==Action::air){m.damage+=2;m.active+=2;}
        if(p.action==Action::rush){m.startup-=2;m.recovery-=4;m.damage-=2;m.reach+=4;}
        if(p.action==Action::throwing)m.damage-=2;
    }
    return m;
}
struct Projectile {
    bool active=false;int owner=0,damage=0,life=0;float x=0,y=0,speed=0;int age=0;
};
struct WorldPoint {float x,y;};
inline WorldPoint fireOrigin(const Fighter& p){
    const auto pixel=firePalms[p.character];
    return {p.x+p.face*(pixel.x+.5f-fighterHalf),p.y+fighterBaseline-(pixel.y+.5f)};
}
inline int projectileFrame(const Projectile& p){return (std::max(0,p.age-1)/4)%4;}
struct HitFx {int age=100;float x=0,y=0;bool block=false;};
// Only visible animation/position and hit feedback enter the delayed AI view.
// Opponent buttons, command buffers and future choices are deliberately absent.
struct ComputerView {
    float x=0,y=0,vx=0,vy=0,shotX=0,shotY=0,shotSpeed=0;
    Action action=Action::idle,selfAction=Action::idle;
    int age=0,character=0,stun=0,hp=100,selfAge=0,stop=0;
    bool shot=false,confirmed=false;
};
enum class Tactic {wait,approach,space,guard,anti_air,punish,confirm,throwing,fire,jump,poke,rush};
struct ComputerState {
    std::array<ComputerView,32> history{};
    std::array<uint32_t,8> motion{};
    unsigned samples=0,motionIndex=0,motionCount=0;
    int think=0,threatAge=0;
    uint32_t intent=0;
    Action threat=Action::idle;
    bool guardAttack=false,attacking=false,escapeDecided=false;
    Tactic tactic=Tactic::wait;
};
struct Arena {
    std::array<Fighter,2> f{};
    std::array<Projectile,8> shots{};
    HitFx hit;
    Phase phase=Phase::intro;
    uint32_t tick=0,rng=0xA51F823u;
    int phaseAge=0,round=1,wins[2]{},time=60*60,freeze=0,flash=0;
    int winner=-1,lastMove=0,lastMoveAge=0,damageEvents=0,blocks=0;
    int hitOwner=0;Action hitAction=Action::punch;
    static constexpr int throwWindow=12;
    int grabOwner=-1,grabAge=0,escapeOwner=-1,escapeAge=100;
    uint32_t escapes=0;
    bool training=false,ai=true;
    Difficulty difficulty=Difficulty::normal;
    Practice practice=Practice::stand;
    ComputerState brain;
    int aiStage=0,bestCombo=0,bestDamage=0;
    bool practiceHit=false;
    std::array<uint32_t,5> inputs{};unsigned inputCount=0;
    uint32_t random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
    void reset(bool practice=false,int player=0,int opponent=-1){
        *this=Arena{};training=practice;ai=!practice;
        player=std::clamp(player,0,characterCount-1);
        f[0].character=player;f[1].character=opponent<0?(player+1)%characterCount:std::clamp(opponent,0,characterCount-1);newRound();
    }
    void newRound(){
        int c0=f[0].character,c1=f[1].character;
        f={Fighter{},Fighter{}};f[0].character=c0;f[1].character=c1;
        f[0].x=91;f[1].x=229;f[1].face=-1;
        if(training){f[0].meter=100;f[1].meter=100;}
        shots={};hit={};brain={};grabOwner=escapeOwner=-1;grabAge=0;escapeAge=100;
        phase=Phase::intro;phaseAge=0;freeze=flash=0;
        time=3600;winner=-1;
    }
    void feed(int id,uint32_t buttons){
        auto& p=f[id];uint32_t edges=buttons&~p.last;p.last=p.held=buttons;
        if(training&&id==0&&(!inputCount||inputs[(inputCount-1)%inputs.size()]!=buttons))inputs[inputCount++%inputs.size()]=buttons;
        p.command.push(p.direction(),tick);
        if(edges&(Punch|Kick|Evade|Skill)){
            p.buffer=edges&(Punch|Kick|Evade|Skill);p.bufferAt=tick;p.bufferHeld=buttons;
        }
        if((buttons&(Punch|Kick))==(Punch|Kick)&&(edges&(Punch|Kick))&&!(buttons&(Skill|Evade))){
            p.throwPressed=true;p.throwPressedAt=tick;
        }
    }
    void clearInput(){for(auto& p:f){p.held=p.last=p.buffer=p.bufferHeld=0;p.throwPressed=false;p.command.clear();}}
    void beginMove(int i,Action action){
        auto& p=f[i];
        if(action==Action::super){if(p.meter<100)return;p.meter-=100;flash=20;freeze=10;}
        p.set(action);p.buffer=0;
        if(action==Action::rise){p.vy=4.6f;p.invulnerable=9;}
        if(action==Action::dodge)p.invulnerable=12;
        if(action==Action::dodge){bool back=p.face>0?(p.bufferHeld&Left)&&!(p.bufferHeld&Right):(p.bufferHeld&Right)&&!(p.bufferHeld&Left);
            p.vx=p.face*(back?-2.3f:2.3f);}
        if(i==0){lastMove=int(action);lastMoveAge=70;}
    }
    void choose(int i){
        auto& p=f[i];auto& q=f[1-i];
        if(grabOwner>=0)return;
        if(!p.buffer||tick-p.bufferAt>8){p.buffer=0;return;}
        if(p.stun||p.action==Action::down||p.action==Action::win)return;
        // A diagonal jump + attack in one sample must take off before attacking.
        const uint32_t intent=p.bufferHeld;
        if(p.free()&&p.grounded()&&(intent&Up)&&!(intent&Down)&&
           (p.buffer&(Punch|Kick))&&!(p.buffer&Skill)){
            p.set(Action::jump);p.vy=roster[p.character].jump;
            p.vx=(((intent&Right)?1:0)-((intent&Left)?1:0))*roster[p.character].air;
        }
        bool cancel=attack(p.action)&&p.connected&&move(p.action).cancel;
        bool superCancel=attack(p.action)&&p.connected&&p.action!=Action::super;
        bool throwChord=(intent&(Punch|Kick))==(Punch|Kick)&&p.grounded()&&
            (p.free()||((p.action==Action::punch||p.action==Action::kick)&&p.age<=3));
        if(!p.free()&&p.action!=Action::jump&&!cancel&&!superCancel&&!throwChord)return;
        Action selected=Action::idle;
        bool motion=false;
        if((p.buffer&Punch)&&p.grounded()&&p.meter>=100&&p.command.match({2,3,6,2,3,6},tick,44)){
            selected=Action::super;motion=true;
        }else if((p.buffer&Punch)&&p.grounded()&&p.command.match({6,2,3},tick)){
            selected=Action::rise;motion=true;
        }else if((p.buffer&Punch)&&p.grounded()&&p.command.match({2,3,6},tick)){
            selected=Action::fire;motion=true;
        }else if((p.buffer&Kick)&&p.grounded()&&p.command.match({2,1,4},tick)){
            selected=Action::rush;motion=true;
        }else if(p.buffer&Skill){
            if(p.grounded()||(p.action==Action::jump&&p.age<=6&&(intent&Up))){selected=(intent&Down)?(p.meter>=100?Action::super:Action::rise):
                (intent&Up)?Action::rise:Action::fire;}
        }else if(throwChord&&
                 std::abs(p.x-q.x)<move(Action::throwing).reach+(p.character==2?4:0)&&q.grounded()&&!q.stun&&!q.invulnerable&&!q.throwProtect){
            selected=Action::throwing;
        }else if((p.buffer&Evade)&&p.grounded()&&p.free())selected=Action::dodge;
        else if(p.buffer&(Punch|Kick)){
            selected=!p.grounded()?Action::air:(intent&Down)?Action::low:
                (p.buffer&Kick)?Action::kick:Action::punch;
        }
        if(selected==Action::idle)return;
        bool chain=p.action==Action::punch&&(selected==Action::kick||selected==Action::low);
        if(cancel&&!chain&&selected!=Action::fire&&selected!=Action::rise&&selected!=Action::rush&&selected!=Action::super)return;
        if(!p.free()&&p.action!=Action::jump&&!cancel&&!(throwChord&&selected==Action::throwing)&&!(superCancel&&selected==Action::super))return;
        if(motion)p.command.consume();
        beginMove(i,selected);
    }
    void computer();
    bool freshThrow(const Fighter& p)const{return p.throwPressed&&tick-p.throwPressedAt<=6;}
    void releaseGrab(){
        if(grabOwner<0)return;
        auto& victim=f[1-grabOwner];victim.stun=0;victim.vx=0;
        if(victim.action==Action::hurt)victim.set(Action::idle);
        victim.throwProtect=std::max(victim.throwProtect,12);grabOwner=-1;grabAge=0;
    }
    void escapeThrow(int defender){
        grabOwner=-1;grabAge=0;escapeOwner=defender;escapeAge=0;++escapes;
        for(int i=0;i<2;++i){auto& p=f[i];p.set(Action::guard);p.stun=10;
            p.vx=p.x<f[1-i].x?-2.2f:2.2f;p.throwProtect=20;
            p.buffer=0;p.throwPressed=false;p.command.clear();}
        hit={0,(f[0].x+f[1].x)*.5f,42,true};hitOwner=1-defender;hitAction=Action::throwing;
        freeze=std::max(freeze,4);
    }
    void updateGrab(){
        if(grabOwner<0)return;
        int owner=grabOwner;auto& victim=f[1-owner];
        if(freshThrow(victim)){escapeThrow(1-owner);return;}
        if(++grabAge>=throwWindow){
            grabOwner=-1;grabAge=0;victim.stun=0;
            // A throw is damage only after the escape window, never on catch.
            damage(owner,moveFor(f[owner]));
            if(!training&&(!f[0].hp||!f[1].hp))endRound();
            return;
        }
        victim.stun=2;victim.vx=victim.vy=0;
    }
    void damage(int owner,const Move& m,bool projectile=false){
        auto& a=f[owner];auto& b=f[1-owner];
        if(b.invulnerable||b.hp<=0||b.action==Action::down)return;
        releaseGrab(); // A projectile can interrupt either side of a grab.
        bool blocked=m.damage&&(projectile||a.action!=Action::throwing)&&b.grounded()&&
            (b.free()||(b.action==Action::guard))&&b.back()&&
            (m.low_attack?b.crouching():m.overhead?!b.crouching():true);
        bool linked=b.stun>0&&b.action==Action::hurt;
        hit={0,(a.x+b.x)*.5f,blocked?38.f:48.f,blocked};
        hitOwner=owner;hitAction=projectile?Action::fire:a.action;
        freeze=std::max(freeze,blocked?3:m.hitstop);
        b.stun=blocked?9:m.stun;
        b.vx=float(a.face)*(blocked?.6f:b.character==2?1.05f:1.4f);
        b.set(blocked?Action::guard:Action::hurt);
        a.connected=true;
        if(blocked){
            b.comboTimer=0;
            if(projectile||a.action==Action::super)b.hp=std::max(1,b.hp-2);
            b.meter=std::min(100,b.meter+4);++blocks;
        }else{
            if(!linked){a.combo=0;a.comboDamage=0;}
            int amount=std::max(3,m.damage*(100-std::min(45,a.combo*12))/100);
            amount=std::min(b.hp,amount);b.hp-=amount;a.combo++;a.comboDamage+=amount;b.comboTimer=m.stun+18;
            if(owner==0){bestCombo=std::max(bestCombo,a.combo);bestDamage=std::max(bestDamage,a.comboDamage);if(training)practiceHit=true;}
            a.meter=std::min(100,a.meter+(a.action==Action::super?0:10));
            b.meter=std::min(100,b.meter+6);++damageEvents;
            if(a.action==Action::rise||a.action==Action::super||a.action==Action::throwing){
                b.vy=3.6f;b.y=std::max(1.f,b.y);b.stun=std::max(28,b.stun);
            }
        }
    }
    void advance(int i){
        auto& p=f[i];auto& q=f[1-i];++p.age;
        if(p.invulnerable)--p.invulnerable;
        if(p.throwProtect)--p.throwProtect;
        if(p.comboTimer)--p.comboTimer;
        if(p.stun){--p.stun;p.x+=p.vx;p.vx*=.87f;if(!p.stun){
            p.throwProtect=std::max(p.throwProtect,12);p.set(!p.hp?Action::down:p.grounded()?Action::idle:Action::jump);}}
        else if(p.free()){
            int newFace=p.x<=q.x?1:-1;
            if(newFace!=p.face){p.face=newFace;p.command.clear();}
            bool down=p.held&Down;int horizontal=training&&i==1&&practice!=Practice::spar?0:((p.held&Right)?1:0)-((p.held&Left)?1:0);
            Action a=down?Action::crouch:horizontal?Action::walk:Action::idle;
            if(p.action!=a)p.set(a);
            if(!down)p.x+=horizontal*(p.back()?roster[p.character].back:roster[p.character].walk);
            if((p.held&Up)&&!(p.held&Down)){p.set(Action::jump);p.vy=roster[p.character].jump;p.vx=horizontal*roster[p.character].air;}
        }
        if(p.action==Action::jump||p.action==Action::air){
            int steer=((p.held&Right)?1:0)-((p.held&Left)?1:0);
            if(steer&&!p.stun){float target=steer*roster[p.character].air;
                p.vx=std::clamp(target,p.vx-.18f,p.vx+.18f);}
            p.x+=p.vx;
        }
        if(p.action==Action::dodge){p.x+=p.vx;if(p.age>=21)p.set(Action::idle);}
        if(p.action==Action::rush&&p.age>7&&p.age<20)p.x+=p.face*roster[p.character].rush;
        if(p.action==Action::super&&p.age>18&&p.age<25)p.x+=p.face*1.4f;
        if(p.y>0||p.vy>0){p.y=std::max(0.f,p.y+p.vy);p.vy-=.25f;
            if(p.y<=0){p.vy=0;p.throwProtect=std::max(p.throwProtect,8);if(p.action==Action::jump||p.action==Action::air)p.set(Action::idle);}}
        if(attack(p.action)){
            Move m=moveFor(p);
            if(p.age>=m.total())p.set(p.grounded()?Action::idle:Action::jump);
        }
        p.x=std::clamp(p.x,19.f,301.f);
    }
    void resolve(int i){
        auto& p=f[i];auto& q=f[1-i];Move m=moveFor(p);
        if(grabOwner>=0)return;
        if(!m.active||p.connected||p.age<m.startup||p.age>=m.startup+m.active)return;
        float d=(q.x-p.x)*p.face;
        if(d>=-5&&d<m.reach+12&&q.y<m.high+p.y&&q.y+(q.crouching()?34:72)>m.low+p.y){
            if(p.action==Action::throwing){
                if(!p.grounded()||!q.grounded()||q.stun||q.invulnerable||q.throwProtect)return;
                if(freshThrow(q)){escapeThrow(1-i);return;}
                grabOwner=i;grabAge=0;p.connected=true;q.set(Action::hurt);q.stun=2;q.vx=q.vy=0;
            }else damage(i,m);
        }
    }
    void endRound(){
        winner=f[0].hp==f[1].hp?-1:f[0].hp>f[1].hp?0:1;
        if(winner>=0){++wins[winner];f[winner].set(Action::win);f[1-winner].set(Action::down);}
        else for(auto& p:f)p.set(Action::down);
        grabOwner=-1;grabAge=0;phase=Phase::round_end;phaseAge=0;clearInput();
    }
    void step(){
        ++tick;++phaseAge;if(hit.age<100)++hit.age;if(escapeAge<100)++escapeAge;if(flash)--flash;if(lastMoveAge)--lastMoveAge;
        if(phase==Phase::intro){if(phaseAge>85){phase=Phase::fight;phaseAge=0;clearInput();}return;}
        if(phase==Phase::round_end){for(auto& p:f)++p.age;if(phaseAge>135){
            if(wins[0]>=2||wins[1]>=2){phase=Phase::match_end;phaseAge=0;}
            else{++round;newRound();}}return;}
        if(phase!=Phase::fight)return;
        computer();
        for(auto& p:f)p.command.push(p.direction(),tick);
        if(freeze){for(auto& p:f){if(p.buffer)++p.bufferAt;p.command.pause();}--freeze;return;}
        updateGrab();if(freeze)return;
        choose(0);choose(1);
        // Super flash is a simulation pause; neither fighter advances through it.
        if(freeze)return;
        float previousX[]={f[0].x,f[1].x};
        advance(0);advance(1);
        if(f[0].grounded()&&f[1].grounded()&&f[0].action!=Action::dodge&&f[1].action!=Action::dodge){
            float delta=f[1].x-f[0].x;float dist=std::abs(delta);
            if(dist<23){float s=delta>=0?1.f:-1.f,push=(23-dist)*.5f;
                f[0].x=std::clamp(f[0].x-s*push,19.f,301.f);
                f[1].x=std::clamp(f[1].x+s*push,19.f,301.f);}
        }
        // Drive footfalls by ground actually travelled, including backwards.
        // A wall, pushbox or hitstop must not make planted feet slide in place.
        for(unsigned i=0;i<2;++i)if(f[i].action==Action::walk)
            f[i].walkDistance=std::fmod(f[i].walkDistance+(f[i].x-previousX[i])*f[i].face,80.f);
        // Emit after wall/pushbox correction so the origin matches the drawn palm.
        for(int i=0;i<2;++i){auto& p=f[i];
            if(p.action==Action::fire&&p.age>=moveFor(p).startup&&!p.spawned){
                auto origin=fireOrigin(p);
                for(auto& shot:shots)if(!shot.active){shot={true,i,moveFor(p).damage,135,origin.x,origin.y,p.face*roster[p.character].projectile};break;}
                p.spawned=true;
            }
        }
        // Ordinary throws take priority over strikes for either side. Two fresh
        // throw commands escape together; strike trades still use a snapshot.
        auto before=f;
        auto oldEscapes=escapes;
        if(before[0].action==Action::throwing)resolve(0);
        if(grabOwner<0&&escapes==oldEscapes&&before[1].action==Action::throwing)resolve(1);
        if(grabOwner<0&&escapes==oldEscapes){
            if(before[0].action!=Action::throwing)resolve(0);
            Fighter saved=f[1];f[1].action=before[1].action;f[1].age=before[1].age;f[1].connected=before[1].connected;
            if(before[1].action!=Action::throwing)resolve(1);
            f[1].action=saved.action;f[1].age=saved.age;
            f[1].connected=saved.connected||f[1].connected;
        }
        for(auto& s:shots)if(s.active){
            if(s.age++>0)s.x+=s.speed; // The first visible frame stays at the palm.
            if(--s.life<=0||s.x<-30||s.x>350){s.active=false;continue;}
            for(auto& other:shots)if(other.active&&other.owner!=s.owner&&std::abs(other.x-s.x)<15&&std::abs(other.y-s.y)<16){
                other.active=s.active=false;hit={0,s.x,s.y,false};break;}
            auto& q=f[1-s.owner];
            if(s.active&&std::abs(s.x-q.x)<16&&q.y<s.y+10&&q.y+(q.crouching()?34:72)>s.y-8&&!q.invulnerable){
                damage(s.owner,{0,1,0,s.damage,22,0,0,0,7},true);
                hit.x=s.x;hit.y=s.y;s.active=false;}
        }
        if(training){time=3600;for(auto& p:f){p.meter=100;if(p.hp<=0||(p.hp<100&&!p.stun&&!p.comboTimer))p.hp=100;}}
        else if(--time<=0||!f[0].hp||!f[1].hp)endRound();
    }
    int sprite(const Fighter& p)const{
        if(p.action==Action::down)return p.age<13?38:39;
        if(p.action==Action::win)return 31;
        if(p.action==Action::guard)return p.crouching()?17:18;
        if(p.action==Action::hurt)return 19;
        if(p.action==Action::walk)return 40+p.walkFrame();
        if(p.action==Action::crouch)return 16;
        if(p.action==Action::jump)return p.vy>2?20:p.vy>-.5f?21:23;
        if(p.action==Action::dodge)return 37;
        if(attack(p.action)){
            Move m=moveFor(p);int stage=p.age<m.startup?0:p.age<m.startup+m.active?1:p.age<m.total()-5?2:3;
            if(p.action==Action::throwing)return 36;
            if(p.action==Action::punch)return 8+stage;
            if(p.action==Action::low)return 32+stage;
            if(p.action==Action::kick)return 12+stage;
            if(p.action==Action::air)return 22;
            if(p.action==Action::fire)return p.age<m.startup?24:fireReleaseFrame;
            if(p.action==Action::rise)return p.age<m.startup?26:27;
            if(p.action==Action::rush)return 28;
            if(p.action==Action::super)return p.age<m.startup?29:30;
        }
        return (p.age/9)%4;
    }
};
} // namespace ash
#include "computer.hpp"
