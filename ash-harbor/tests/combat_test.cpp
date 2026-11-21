// SPDX-License-Identifier: MIT
#include "../combat.hpp"
#include <cassert>
#include <cstdio>
using namespace ash;
Arena fresh(){Arena a;a.reset();a.ai=false;a.phase=Phase::fight;a.f[0].x=131;a.f[1].x=167;return a;}
void steps(Arena& a,int n){for(int i=0;i<n;++i)a.step();}
void press(Arena& a,uint32_t buttons){a.feed(0,buttons);a.step();}
void qcf(Arena& a,bool fire=true){press(a,Down);press(a,Down|Right);press(a,Right|(fire?Punch:0u));}
int main(){
    int tests=0;
    for(int player=0;player<4;++player)for(int first=0;first<4;++first){
        bool seen[4]{};
        for(int stage=0;stage<3;++stage){int opponent=tourOpponent(player,first,stage);assert(opponent!=player&&!seen[opponent]);seen[opponent]=true;}
        if(first!=player)assert(tourOpponent(player,first,0)==first);
        ++tests;
    }
    for(int character=0;character<4;++character){auto a=fresh();a.f[0].character=character;a.f[1].x=290;
        press(a,Up);float x=a.f[0].x;press(a,Right);steps(a,6);
        assert(a.f[0].y>0&&a.f[0].x>x&&a.f[0].vx>0);
        press(a,Left);steps(a,10);assert(a.f[0].y>0&&a.f[0].vx<0);++tests;
    }
    {auto a=fresh();a.f[1].x=290;press(a,Up|Right|Kick);
     assert(a.f[0].action==Action::air&&a.f[0].y>0);float x=a.f[0].x;steps(a,6);
     assert(a.f[0].x>x+6);++tests;}
    {auto a=fresh();press(a,Left|Evade);float x=a.f[0].x;press(a,0);steps(a,3);
     assert(a.f[0].action==Action::dodge&&a.f[0].x<x);++tests;}
    {auto a=fresh();a.f[0].character=2;a.f[1].x=a.f[0].x+30;press(a,Punch|Kick);
     assert(a.f[0].action==Action::throwing);++tests;}
    {auto a=fresh();a.f[0].y=12;a.f[0].vy=-1;a.f[0].stun=1;a.f[0].action=Action::hurt;
     a.feed(0,Up);a.step();assert(a.f[0].action==Action::jump&&a.f[0].y<12&&a.f[0].vy<0);++tests;}
    {auto a=fresh();qcf(a);assert(a.f[0].action==Action::fire);steps(a,14);
     bool found=false;for(auto& s:a.shots)found|=s.active;assert(found||a.damageEvents);++tests;}
    {auto a=fresh();press(a,Right);press(a,Down);press(a,Down|Right|Punch);
     assert(a.f[0].action==Action::rise);assert(a.f[0].invulnerable);++tests;}
    {auto a=fresh();a.f[0].meter=100;qcf(a,false);qcf(a);
     assert(a.f[0].action==Action::super&&a.f[0].meter==0);
     steps(a,20);assert(a.f[0].meter==0);++tests;}
    {auto a=fresh();a.f[0].x=205;a.f[1].x=100;a.step();assert(a.f[0].face==-1);
     press(a,Down);press(a,Down|Left);press(a,Left|Punch);
     assert(a.f[0].action==Action::fire);++tests;}
    {auto a=fresh();qcf(a);steps(a,65);press(a,0);press(a,Punch);
     assert(a.f[0].action==Action::punch);++tests;}
    {auto a=fresh();a.feed(1,Right);press(a,Punch);steps(a,6);
     assert(a.f[1].hp==100&&a.blocks==1);++tests;}
    {auto a=fresh();a.feed(1,Right);press(a,Down|Kick);steps(a,9);
     assert(a.f[1].hp<100&&a.blocks==0);++tests;}
    {auto a=fresh();a.feed(1,Right|Down);press(a,Down|Kick);steps(a,9);
     assert(a.f[1].hp==100&&a.blocks==1);++tests;}
    {auto a=fresh();a.feed(1,Evade);press(a,Punch);steps(a,6);
     assert(a.f[1].hp==100);++tests;}
    {auto a=fresh();press(a,Punch);steps(a,4);assert(a.f[0].connected);
     press(a,0);press(a,Kick);while(a.freeze)a.step();a.step();
     assert(a.f[0].action==Action::kick);steps(a,15);
     assert(a.f[0].combo==2);assert(a.f[0].comboDamage==17);++tests;}
    {auto a=fresh();a.feed(0,Punch);a.feed(1,Punch);steps(a,5);
     assert(a.f[0].hp==93&&a.f[1].hp==93);++tests;}
    {auto a=fresh();a.f[1].hp=1;press(a,Punch);steps(a,6);
     assert(a.phase==Phase::round_end&&a.wins[0]==1);steps(a,140);
     assert(a.phase==Phase::intro&&a.round==2&&a.f[0].hp==100&&a.wins[0]==1);++tests;}
    {CommandBuffer c;c.push(2,0);c.push(4,1);c.push(3,2);c.push(6,3);
     assert(!c.match({2,3,6},3));c.clear();c.push(2,10);c.push(3,11);c.push(6,12);
     assert(!c.match({2,3,6},30));++tests;}
    {auto a=fresh();press(a,Up);assert(a.f[0].y>0);press(a,0);steps(a,60);
     assert(a.f[0].grounded()&&a.f[0].action==Action::idle);++tests;}
    {Arena a;a.reset(true);a.phase=Phase::fight;a.f[0].hp=1;a.f[0].meter=0;a.step();
     assert(a.f[0].hp==100&&a.f[0].meter==100&&a.phase==Phase::fight);++tests;}
    {Arena a;a.reset();steps(a,90);for(int i=0;i<20000;++i){
        if(a.phase==Phase::match_end)a.reset();
        unsigned r=a.random();a.feed(0,r&0x33f);a.step();
        for(auto& p:a.f){assert(p.hp>=0&&p.hp<=100&&p.meter>=0&&p.meter<=100);
            assert(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=19&&p.x<=301);
            assert(a.sprite(p)>=0&&a.sprite(p)<48);}
     }++tests;}
    {auto a=fresh();a.f[1].x=a.f[0].x+24;press(a,Punch);press(a,Punch|Kick);
     assert(a.f[0].action==Action::throwing);++tests;}
    {Fighter p;p.action=Action::kick;Move jin=moveFor(p);p.character=1;Move lan=moveFor(p);
     assert(lan.reach>jin.reach&&lan.startup>jin.startup&&lan.damage>jin.damage);++tests;}
    {auto a=fresh();press(a,Up);steps(a,3);press(a,Up|Skill);
     assert(a.f[0].action==Action::rise);++tests;}
    {Arena a;a.reset(true);a.ai=false;a.phase=Phase::fight;
     a.f[0].x=30;a.f[1].x=290;
     bool frames[8]={};a.feed(0,Right);
     for(int i=0;i<64;++i){a.step();frames[a.f[0].walkFrame()]=true;}
     for(bool seen:frames)assert(seen);
     float gait=a.f[0].walkDistance;a.freeze=5;steps(a,5);
     assert(a.f[0].walkDistance==gait);++tests;}
    {auto a=fresh();a.f[0].x=301;a.f[1].x=100;a.feed(0,Right);a.step();
     float gait=a.f[0].walkDistance;steps(a,30);
     assert(a.f[0].walkDistance==gait&&a.sprite(a.f[0])==40);++tests;}
    {Fighter p;p.walkDistance=10;assert(p.walkFrame()==1);
     p.walkDistance=-10;assert(p.walkFrame()==7);p.walkDistance=79.99f;
     assert(p.walkFrame()==7);++tests;}
    {Fighter p;p.action=Action::throwing;auto normal=moveFor(p);p.character=2;auto heavy=moveFor(p);
     assert(heavy.damage>normal.damage&&heavy.reach>normal.reach);++tests;}
    {Arena slow,fast;slow.reset(true,2);fast.reset(true,3);slow.phase=fast.phase=Phase::fight;
     slow.feed(0,Right);fast.feed(0,Right);steps(slow,30);steps(fast,30);
     assert(fast.f[0].x>slow.f[0].x+10);++tests;}
    {auto a=fresh();press(a,Punch);steps(a,4);steps(a,22);
     assert(!a.f[1].stun&&a.f[1].comboTimer>0);press(a,0);press(a,Punch);steps(a,5);
     assert(a.damageEvents==2&&a.f[0].combo==1&&a.f[0].comboDamage==7);++tests;}
    {auto a=fresh();a.f[1].hp=2;press(a,Punch);steps(a,5);
     assert(a.f[0].comboDamage==2&&a.bestDamage==2);++tests;}
    {Arena a;a.reset(true);a.phase=Phase::fight;a.f[0].x=131;a.f[1].x=167;a.practice=Practice::guard;
     press(a,Down|Kick);steps(a,12);assert(a.blocks==1&&a.damageEvents==0);
     steps(a,40);press(a,0);press(a,Punch);steps(a,8);assert(a.blocks==2);++tests;}
    {Arena a;a.reset(true);a.phase=Phase::fight;a.f[0].x=131;a.f[1].x=167;a.practice=Practice::after_hit;
     press(a,Punch);steps(a,6);assert(a.practiceHit&&a.damageEvents==1);
     steps(a,60);press(a,0);press(a,Punch);steps(a,8);assert(a.blocks==1&&a.damageEvents==1);++tests;}
    {Arena a;a.reset(true);a.phase=Phase::fight;
     for(int i=0;i<37;++i)a.feed(0,i%2?Down:Right|Punch);
     assert(a.inputCount==37&&a.inputs[(a.inputCount-1)%5]==(Right|Punch));
     a.feed(0,Right|Punch);assert(a.inputCount==37);++tests;}
    bool aiSpentMeter=false;
    for(int level=0;level<3;++level)for(int character=0;character<characterCount;++character){
        Arena a;a.reset(true,0,character);a.practice=Practice::spar;a.difficulty=Difficulty(level);a.phase=Phase::fight;
        a.f[0].x=131;a.f[1].x=167;bool attacked=false,spent=false;
        for(int frame=0;frame<1800;++frame){a.step();attacked|=attack(a.f[1].action);spent|=a.f[1].action==Action::super;
            assert(a.phase==Phase::fight&&a.f[0].hp>=0&&a.f[1].hp>=0);}
        assert(attacked);aiSpentMeter|=spent;++tests;
    }
    assert(aiSpentMeter); // Do not require a speculative super in every easy match.
    for(int player=0;player<characterCount;++player)for(int opponent=0;opponent<characterCount;++opponent){
        Arena a;a.reset(true,player,opponent);a.phase=Phase::fight;a.ai=false;
        for(int frame=0;frame<2400;++frame){
            if(frame%8==0)for(int id=0;id<2;++id)a.feed(id,a.random()&(Up|Down|Left|Right|Punch|Kick|Evade|Skill));
            a.step();
            for(int id=0;id<2;++id){const auto& p=a.f[id];assert(p.character==(id?opponent:player));
                assert(p.hp>=0&&p.hp<=100&&p.meter>=0&&p.meter<=100);
                assert(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=19&&p.x<=301);
                assert(a.sprite(p)>=0&&a.sprite(p)<48);}
        }
        ++tests;
    }
    std::printf("%d combat tests passed, including 80,000 deterministic stress frames, all 16 matchups and all AI difficulties.\n",tests);
}
