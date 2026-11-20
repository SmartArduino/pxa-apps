#pragma once
#include "campaign.hpp"

namespace han {

struct Duel {
    bool issued=false, active=false;
    int round=0, poseTicks=0, result=0;
    int health[2]{100,100};
    int recoil=-1;
    DuelMove last[2]{thrust,thrust};

    bool visible() const {return active || poseTicks>0;}
    bool ready() const {return active && poseTicks==0;}
    void begin() {issued=true;active=true;}
    void tick() {if (poseTicks) --poseTicks;}
    bool play(DuelMove move,DuelMove enemy,int valor,int rival) {
        if (!ready()) return false;
        last[0]=move;
        last[1]=enemy;
        recoil=-1;
        if (move==enemy) {
            health[0]=std::max(0,health[0]-11);
            health[1]=std::max(0,health[1]-11);
        } else {
            // Thrust beats feint, feint beats a block, the block deflects a thrust.
            const int winner=(int(move)+2)%3==int(enemy)?0:1;
            recoil=1-winner;
            const int damage=41+std::clamp((valor-rival)*(winner==0?1:-1)/3,-14,14);
            health[recoil]=std::max(0,health[recoil]-damage);
        }
        ++round;
        poseTicks=17;
        if (round==3 || !health[0] || !health[1]) {
            result=(health[0]>health[1])-(health[0]<health[1]);
            active=false;
            poseTicks=35;
        }
        return true;
    }
};

struct Settlement {
    int xp=0,level=0,dispersed=0;
    bool objective=false,captured=false;
};

struct Battle {
    int from=0,to=0,hero=0,enemy=3,commander=0,column=-1;
    int enemyIndex=-1,level=1,shield=0,walls=1;
    const Officer* general=&officers[0][0];
    bool defending=false,blockade=false,usedSkill=false,paused=false;
    int troops[2]{},initial[2]{},morale[2]{100,100};
    int losses[2]{},result=0,round=0,ticks=0,signal=0,effects=0;
    Formation stance=spear,intent=spear;
    Maneuver maneuver=measured;
    int maneuverRounds=0;
    Gear gear=noGear;
    Duel duel{};
    static constexpr int approach=44,period=48,maximumRounds=11;
    static constexpr int commandCost=14,skillCost=46,tacticsCost=18,duelCost=28;

    void start(const Campaign& c,Order order,int expedition,Formation formation,int defender=-1) {
        *this={};
        from=order.from;
        to=order.to;
        hero=c.player;
        column=expedition;
        defending=column<0;
        walls=c.cities[to].fort;
        if (defending) {
            commander=c.available(defender,to)?defender:-1;
            enemy=c.cities[from].owner;
            initial[0]=c.cities[to].troops;
            initial[1]=c.dispatch(from);
        } else {
            const auto& unit=c.columns[column];
            commander=unit.leader;
            blockade=unit.blockade;
            enemy=c.cities[to].owner;
            initial[0]=unit.troops;
            initial[1]=c.cities[to].troops;
        }
        general=&c.leader(std::max(0,commander));
        enemyIndex=enemy<3?c.enemyOfficer(enemy,to):-1;
        std::copy(initial,initial+2,troops);
        stance=formation;
        signal=52;
        if (commander>=0) {
            auto veteran=c.generals[commander];
            level=veteran.level();
            gear=veteran.gear;
            signal+=level*3-3+(gear==manual?18:0)+(c.hasSpecialty(commander,Specialty::command)?12:0);
            morale[0]=100-veteran.fatigue/3;
        } else {
            signal=28;
            morale[0]=76;
        }
        if (blockade) signal=std::min(100,signal+10);
        intent=predict();
    }
    const Officer& officer() const {return *general;}
    const Officer* opponent() const {return enemyIndex>=0?&officers[enemy][enemyIndex]:nullptr;}
    Formation predict() const {
        if (round==0) {
            if (sites[to].terrain==river) return bow;
            if (sites[to].terrain==hills) return spear;
            if (auto* lead=opponent()) return lead->favored;
        }
        return Formation((to+enemy+round*2+round/3)%3);
    }
    bool order(Formation next) {
        if (result || paused || duel.visible() || stance==next || signal<commandCost) return false;
        stance=next;
        signal-=commandCost;
        return true;
    }
    bool mayManeuver(Maneuver next) const {
        return !result && !paused && !duel.visible() && signal>=tacticsCost &&
            (next!=maneuver || maneuverRounds<2) && (next!=flank || sites[to].terrain!=river);
    }
    bool direct(Maneuver next) {
        if (!mayManeuver(next)) return false;
        signal-=tacticsCost;
        maneuver=next;
        maneuverRounds=2;
        return true;
    }
    bool mayDuel() const {
        return commander>=0 && round>0 && !paused && !result && !duel.issued && signal>=duelCost;
    }
    bool challenge() {
        if (!mayDuel()) return false;
        signal-=duelCost;
        duel.begin();
        return true;
    }
    DuelMove duelIntent() const {
        return DuelMove((to+std::max(0,enemyIndex)+duel.round*2+(enemy==3?1:0))%3);
    }
    bool duelOrder(DuelMove move) {
        if (paused || !duel.play(move,duelIntent(),officer().valor+level*2,
                               opponent()?opponent()->valor:75)) return false;
        if (!duel.active) {
            if (duel.result>0) morale[1]=std::max(0,morale[1]-24);
            else if (duel.result<0) morale[0]=std::max(0,morale[0]-19);
            else {
                morale[0]=std::max(0,morale[0]-6);
                morale[1]=std::max(0,morale[1]-6);
            }
            evaluate();
        }
        return true;
    }
    bool rally() {
        if (result || paused || duel.visible() || commander<0 || usedSkill || signal<skillCost) return false;
        usedSkill=true;
        signal-=skillCost;
        effects=25;
        const auto& lead=officer();
        int strike=0;
        switch (lead.power) {
            case charge:
                strike=lead.strength+(stance==lead.favored?35:0);
                morale[0]=std::min(100,morale[0]+16);
                break;
            case guard:shield=2;signal=std::min(100,signal+30+lead.strength);break;
            case guile:strike=55;morale[1]=std::max(0,morale[1]-lead.strength);break;
            case inspire:
                strike=lead.strength;
                morale[0]=std::min(100,morale[0]+42);
                signal=std::min(100,signal+26);
                break;
            case wildfire:
                strike=lead.strength+(sites[to].terrain==river?100:0);
                morale[1]=std::max(0,morale[1]-23);
                break;
            case volley:
                strike=lead.strength+(stance==bow?85:0);
                break;
        }
        if (strike) strike+=10*(level-1);
        losses[1]=std::min(strike,troops[1]);
        troops[1]-=losses[1];
        evaluate();
        return true;
    }
    static int advantage(Formation a,Formation b) {
        return a==b?100:(int(a)+2)%3==int(b)?145:69;
    }
    int hit(int attackers,Formation a,Formation b,int spirit,int fort) const {
        int strength=advantage(a,b);
        auto terrain=sites[to].terrain;
        if (a==horse) strength+=terrain==plain?17:-21;
        if (a==bow && terrain==river) strength+=20;
        if (a==spear && terrain==hills) strength+=12;
        strength+=(spirit-62)/3-8*fort;
        return std::max(18,(attackers/10+27)*std::max(32,strength)/100);
    }
    void step() {
        if (paused) return;
        if (duel.visible()) {duel.tick();return;}
        if (result) return;
        ++ticks;
        if (effects) --effects;
        if (ticks%5==0) signal=std::min(100,signal+2);
        if (ticks<approach || (ticks-approach)%period) return;
        const int effectiveWall=std::max(0,walls-(blockade?2:0)-(maneuver==flank?1:0));
        int taken=hit(troops[1],intent,stance,morale[1],defending?walls:0);
        int dealt=hit(troops[0],stance,intent,morale[0],defending?0:effectiveWall);
        int prowess=(stance==officer().favored?112:100)+(level-1)*4+(gear==sword?12:0);
        dealt=dealt*prowess/100;
        if (gear==armor) taken=taken*85/100;
        switch (maneuver) {
            case measured:break;
            case press:dealt=dealt*135/100;taken=taken*122/100;break;
            case hold:dealt=dealt*75/100;taken=taken*56/100;morale[0]=std::min(100,morale[0]+8);break;
            case flank:
                dealt=dealt*(intent==bow?149:120)/100;
                taken=taken*(intent==horse?114:86)/100;
                break;
        }
        if (maneuverRounds && !--maneuverRounds) maneuver=measured;
        if (shield>0) {taken/=3;--shield;}
        losses[0]=std::min(troops[0],taken);
        losses[1]=std::min(troops[1],dealt);
        for (int side=0;side<2;++side) {
            troops[side]-=losses[side];
            morale[side]=std::max(0,morale[side]-4-105*losses[side]/std::max(240,initial[side]));
        }
        ++round;
        evaluate();
    }
    void evaluate() {
        const bool left=!troops[0] || !morale[0], right=!troops[1] || !morale[1];
        if (left || right) result=(right&&!left)||(right&&left&&defending)?1:-1;
        else if (round>=maximumRounds) {
            const int a=troops[0]+3*morale[0],b=troops[1]+3*morale[1];
            result=a>b+(defending?-110:110)?1:-1;
        }
        if (!result) intent=predict();
    }
    Settlement settle(Campaign& c) const {
        Settlement aftermath{};
        if (!result) return aftermath;
        auto& a=c.cities[from];
        auto& d=c.cities[to];
        const int gained=result>0?92+std::min(70,(initial[1]-troops[1])/8):38;
        if (commander>=0) {
            const int before=c.generals[commander].level();
            c.gainXp(commander,gained,18+std::min(38,(initial[0]-troops[0])/12));
            aftermath.xp=gained;
            aftermath.level=c.generals[commander].level()>before?c.generals[commander].level():0;
        }
        if (defending) {
            a.troops-=initial[1];
            if (result>0) {
                a.troops+=troops[1];
                d.troops=std::max(garrison,troops[0]);
            } else {
                c.relieve(to);
                d={a.owner,std::max(garrison,troops[1]),d.fields,std::max(1,d.fort-1),d.market,54};
                aftermath.dispersed=c.billet(troops[0],to,-1);
            }
            c.afterTurn();
        } else {
            c.columns[column]={};
            if (result>0) {
                d={hero,std::max(garrison,troops[0]),d.fields,std::max(1,d.fort-1),d.market,49};
                ++c.tours;
                c.gold=std::min(9999,c.gold+115);
                c.grain=std::min(9999,c.grain+95);
                if (c.tours%3==0) c.gainGear(Gear((to+c.tours)%noGear));
                aftermath.objective=c.rewardObjective();
            } else {
                d.troops=std::max(garrison,troops[1]);
                aftermath.dispersed=c.billet(troops[0],from,-1);
            }
            c.checkEnd(false);
            if (!c.won) c.makeOrders();
        }
        if (result>0 && enemyIndex>=0 && !c.prisoner && !c.won &&
            (duel.result>0 || troops[1]*3<=initial[1] || !morale[1])) {
            c.prisoner={enemy,enemyIndex,to};
            aftermath.captured=true;
        }
        return aftermath;
    }
};

}
