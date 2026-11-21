#pragma once
#include <cstdlib>
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
    int xp=0,level=0,dispersed=0,enemyReturned=0;
    bool objective=false,captured=false;
};

struct RankOrder {
    Maneuver maneuver=measured;
    int rounds=0;
};

struct Battle {
    static constexpr int rankCount=3;
    using Ranks=std::array<int,rankCount>;
    using Clash=std::array<Ranks,2>;
    int from=0,to=0,hero=0,enemy=3,commander=0,column=-1;
    int enemyIndex=-1,level=1,shield=0,walls=1;
    const Officer* general=&officers[0][0];
    bool defending=false,blockade=false,usedSkill=false,paused=false;
    int troops[2]{},initial[2]{},morale[2]{100,100};
    Ranks ranks[2]{},rankAdvance[2]{},rankLosses[2]{};
    std::array<RankOrder,rankCount> rankOrders{};
    int enemyRank=-1,lastSupport=-1,lastStrikeLane=-1;
    int losses[2]{},result=0,round=0,ticks=0,signal=0,effects=0;
    BattlePlan plan{};
    Troop enemyTroop=spear;
    Formation enemyFormation=square;
    Maneuver enemyManeuver=measured;
    Difficulty difficulty=Difficulty::standard;
    int enemySignal=0,nextEnemyOrder=0,enemySkillTicks=0;
    bool enemySkillQueued=false,enemySkillUsed=false;
    int enemyRetreatCity=-1;
    bool enemyWithdrawalQueued=false,enemyWithdrew=false;
    int generalLane=1,generalHealth=100,lastStrike=-1,strikeTicks=0;
    bool atFront=false;
    Maneuver maneuver=measured;
    int maneuverRounds=0;
    Gear gear=noGear;
    Duel duel{};
    static constexpr int approach=44,period=48,maximumRounds=11;
    static constexpr int commandCost=14,skillCost=46,tacticsCost=18,duelCost=28,strikeCost=12;
    static constexpr int rankOrderCost=9,supportCost=8;

    static Ranks deployRanks(int force,Formation formation) {
        constexpr int wingShare[]={31,24,39};
        const int wing=force*wingShare[formation]/100;
        return {wing,force-2*wing,wing};
    }
    void formRanks(int side) {
        ranks[side]=deployRanks(troops[side],side?enemyFormation:plan.formation);
    }

    void start(const Campaign& c,Order order,int expedition,BattlePlan deployment,int defender=-1) {
        *this={};
        from=order.from;
        to=order.to;
        hero=c.player;
        column=expedition;
        defending=column<0;
        walls=c.cities[to].fort;
        difficulty=c.difficulty;
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
        plan=deployment;
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
        enemyTroop=enemyUnit(to,enemy,opponent());
        enemyFormation=enemyFormationFor(to,walls,defending,enemyTroop,difficulty);
        formRanks(0);
        formRanks(1);
        enemySignal=difficulty==Difficulty::veteran?32:18;
        if (difficulty!=Difficulty::story) {
            enemyRetreatCity=defending?from:retreatDestination(c);
        }
    }
    const Officer& officer() const {return *general;}
    const Officer* opponent() const {return enemyIndex>=0?&officers[enemy][enemyIndex]:nullptr;}
    static Troop enemyUnit(int city,int enemy,const Officer* lead) {
        if (sites[city].terrain==river) return bow;
        if (sites[city].terrain==hills) return spear;
        return lead?lead->favored:Troop((city+enemy)%3);
    }
    static Formation enemyFormationFor(int city,int fort,bool attacking,Troop troop,Difficulty level) {
        if (level==Difficulty::story || (!attacking && fort>=2)) return square;
        if (sites[city].terrain==plain && (troop==horse || attacking)) return wedge;
        return troop==bow && sites[city].terrain!=hills?wings:square;
    }
    int retreatDestination(const Campaign& c) const {
        const auto pressure=c.borderPressure();
        int destination=-1,best=0;
        for (const auto& road:roads) {
            const int candidate=road.from==to?road.to:road.to==to?road.from:-1;
            if (candidate<0 || c.cities[candidate].owner!=enemy) continue;
            const auto& refuge=c.cities[candidate];
            const int room=armyLimit-refuge.troops;
            const int score=room+refuge.fort*90-pressure[candidate]/2;
            if (room>=180 && score>best) {best=score;destination=candidate;}
        }
        return destination;
    }
    bool order(Formation next) {
        if (result || paused || duel.visible() || plan.formation==next || signal<commandCost) return false;
        plan.formation=next;
        formRanks(0);
        signal-=commandCost;
        return true;
    }
    bool canMoveGeneral() const {
        return commander>=0 && generalHealth && !paused && !result && !duel.visible();
    }
    bool moveGeneral(int lane,bool front) {
        if (!canMoveGeneral() || lane<0 || lane>=rankCount || !ranks[0][lane] ||
            (lane==generalLane && front==atFront)) return false;
        generalLane=lane;
        atFront=front;
        return true;
    }
    bool mayStrike() const {
        return canMoveGeneral() && ticks>=approach && lastStrike!=round &&
            ranks[0][generalLane] && signal>=strikeCost &&
            (atFront || plan.weapon==longbow);
    }
    bool strike() {
        if (!mayStrike()) return false;
        signal-=strikeCost;
        lastStrike=round;
        strikeTicks=15;
        int damage=30+officer().valor/3+(level-1)*5;
        switch (plan.weapon) {
            case longspear:
                damage+=enemyTroop==horse?38:12;
                break;
            case saber:
                damage+=generalLane==1?36:19;
                morale[1]=std::max(0,morale[1]-4);
                break;
            case longbow:
                damage+=enemyTroop==spear?22:6;
                if (!atFront) damage=damage*4/5;
                break;
        }
        if (gear==sword) damage+=12;
        lastStrikeLane=nearestRank(ranks[1],generalLane);
        rankLosses[1]={};
        losses[1]=hurtRank(1,lastStrikeLane,damage);
        evaluate();
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
        rankOrders={};
        return true;
    }
    Maneuver stance(int side,int lane) const {
        if (side==1) return enemyRank<0 || enemyRank==lane?enemyManeuver:measured;
        return rankOrders[lane].rounds?rankOrders[lane].maneuver:maneuver;
    }
    bool mayDirectRank(int lane,Maneuver next) const {
        return lane>=0 && lane<rankCount && ranks[0][lane]>0 && !result && !paused &&
            !duel.visible() && signal>=rankOrderCost && (next!=flank || sites[to].terrain!=river) &&
            (next!=stance(0,lane) || (rankOrders[lane].rounds==1));
    }
    bool directRank(int lane,Maneuver next) {
        if (!mayDirectRank(lane,next)) return false;
        signal-=rankOrderCost;
        rankOrders[lane]={next,2};
        return true;
    }
    bool clearRankOrder(int lane) {
        if (lane<0 || lane>=rankCount || !rankOrders[lane].rounds ||
            paused || result || duel.visible()) return false;
        rankOrders[lane]={};
        return true;
    }
    int supportSource(int lane) const {
        int best=-1;
        for (int source=std::max(0,lane-1);source<=std::min(rankCount-1,lane+1);++source)
            if (source!=lane && (best<0 || ranks[0][source]>ranks[0][best])) best=source;
        return best;
    }
    int supportSize(int lane) const {
        if (lane<0 || lane>=rankCount || paused || result || duel.visible() ||
            signal<supportCost || lastSupport==round) return 0;
        const int from=supportSource(lane);
        if (from<0 || ranks[0][from]<90) return 0;
        return std::min(200,ranks[0][from]/3);
    }
    bool supportRank(int lane) {
        const int count=supportSize(lane);
        if (!count) return false;
        ranks[0][supportSource(lane)]-=count;
        ranks[0][lane]+=count;
        lastSupport=round;
        signal-=supportCost;
        return true;
    }
    bool mayDuel() const {
        return canMoveGeneral() && round>0 && !duel.issued && signal>=duelCost;
    }
    bool challenge() {
        if (!mayDuel()) return false;
        signal-=duelCost;
        duel.begin();
        return true;
    }
    DuelMove duelIntent() const {
        if (difficulty==Difficulty::story)
            return DuelMove((to+std::max(0,enemyIndex)+duel.round*2+(enemy==3?1:0))%3);
        uint32_t seed=uint32_t(from+1)*0x9e3779b9u^uint32_t(to+1)*0x85ebca6bu^
                      uint32_t(enemyIndex+2)*0x7feb352du^uint32_t(duel.round+1)*0xc2b2ae35u;
        seed^=seed>>16;
        seed*=0x7feb352du;
        seed^=seed>>15;
        if (difficulty==Difficulty::veteran && duel.round && seed%4==0)
            return DuelMove((int(duel.last[0])+1)%3);
        return DuelMove((seed>>2)%3);
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
        if (result || paused || duel.visible() || commander<0 || !generalHealth || usedSkill || signal<skillCost) return false;
        usedSkill=true;
        signal-=skillCost;
        effects=25;
        const auto& lead=officer();
        int strike=0;
        switch (lead.power) {
            case charge:
                strike=lead.strength+(plan.troop==lead.favored?35:0);
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
                strike=lead.strength+(plan.troop==bow?85:0);
                break;
        }
        if (strike) strike+=10*(level-1);
        rankLosses[1]={};
        losses[1]=0;
        const auto portions=splitByRank(strike,ranks[1],troops[1]);
        for (int lane=0;lane<rankCount;++lane)
            losses[1]+=hurtRank(1,lane,portions[lane]);
        evaluate();
        return true;
    }
    static int advantage(Troop a,Troop b) {
        return a==b?100:(int(a)+2)%3==int(b)?145:69;
    }
    int hit(int attackers,Troop a,Troop b,int spirit,int fort) const {
        int strength=advantage(a,b);
        auto terrain=sites[to].terrain;
        if (a==horse) strength+=terrain==plain?17:-21;
        if (a==bow && terrain==river) strength+=20;
        if (a==spear && terrain==hills) strength+=12;
        strength+=(spirit-62)/3-8*fort;
        return std::max(18,(attackers/10+27)*std::max(32,strength)/100);
    }
    static int nearestRank(const Ranks& force,int lane) {
        int best=-1;
        for (int i=0;i<rankCount;++i)
            if (force[i] && (best<0 || std::abs(lane-i)<std::abs(lane-best) ||
                (std::abs(lane-i)==std::abs(lane-best) && force[i]>force[best]))) best=i;
        return best;
    }
    static Ranks splitByRank(int total,const Ranks& force,int count) {
        Ranks shares{};
        if (!count) return shares;
        int weight=0,split=0;
        for (int i=0;i<rankCount;++i) {
            weight+=force[i];
            shares[i]=total*weight/count-split;
            split+=shares[i];
        }
        return shares;
    }
    int hurtRank(int side,int lane,int damage) {
        int lost=0;
        while (damage>0) {
            const int target=nearestRank(ranks[side],lane);
            if (target<0) break;
            const int hit=std::min(damage,ranks[side][target]);
            ranks[side][target]-=hit;
            rankLosses[side][target]+=hit;
            damage-=hit;
            lost+=hit;
        }
        troops[side]-=lost;
        return lost;
    }
    struct Tactics {int attack,exposure,spirit;};
    Tactics tactics(int side,Maneuver move) const {
        if (side) {
            constexpr Tactics enemyOrders[]={{100,100,0},{121,112,0},{76,72,7},{132,88,0},{51,62,13}};
            return enemyOrders[move];
        }
        switch (move) {
            case measured:return {100,100,0};
            case press:return {135,122,0};
            case hold:return {75,56,8};
            case flank:return {enemyTroop==bow?149:120,enemyTroop==horse?114:86,0};
            case fallBack:return {43,42,14};
        }
        return {100,100,0};
    }
    Ranks rankAttacks(int side,int base) const {
        const auto shares=splitByRank(base,ranks[side],troops[side]);
        Ranks attacks{};
        for (int lane=0;lane<rankCount;++lane) {
            if (!shares[lane]) continue;
            const auto move=stance(side,lane);
            const auto& opposing=ranks[1-side];
            int target=nearestRank(opposing,lane);
            if (target<0) break;
            if (move==flank) {
                for (int n=std::max(0,lane-1);n<=std::min(rankCount-1,lane+1);++n)
                    if (opposing[n] && opposing[n]<opposing[target]) target=n;
            }
            const auto source=tactics(side,move);
            const auto guard=tactics(1-side,stance(1-side,target));
            // An open lane can hit the side of its neighbor, but never cross two lanes for a bonus.
            const int attack=source.attack+(!opposing[lane] && std::abs(lane-target)==1?15:0);
            attacks[target]+=shares[lane]*attack*guard.exposure/10000;
        }
        for (int lane=0;lane<rankCount;++lane)
            attacks[lane]=std::min(attacks[lane],ranks[1-side][lane]);
        return attacks;
    }
    void steadyRanks(int side) {
        int weighted=0;
        for (int i=0;i<rankCount;++i) weighted+=ranks[side][i]*tactics(side,stance(side,i)).spirit;
        morale[side]=std::min(100,morale[side]+weighted/std::max(1,troops[side]));
    }
    void planEnemyOrder() {
        enemyManeuver=measured;
        enemyRank=-1;
        enemySkillQueued=false;
        enemyWithdrawalQueued=false;
        if (difficulty==Difficulty::story || result) return;
        const bool veteran=difficulty==Difficulty::veteran;
        if (enemyRetreatCity>=0 && round>=(veteran?3:4) && troops[1]>=180 &&
            troops[0]>troops[1]+150 && morale[0]>40 &&
            (morale[1]<=(veteran?43:35) || troops[1]*3<=initial[1])) {
            enemyWithdrawalQueued=true;
            enemyManeuver=fallBack;
            return;
        }
        enemySignal=std::min(100,enemySignal+(veteran?22:16));
        if (round<nextEnemyOrder || round<(veteran?1:2)) return;
        const auto* lead=opponent();
        const bool strained=morale[1]<62 || losses[1]>losses[0]*4/3;
        const bool skillWindow=lead && (lead->power==guard || lead->power==inspire?strained:
            round>=3 && troops[1]>initial[1]/4);
        if (skillWindow && !enemySkillUsed && enemySignal>=skillCost) {
            enemySkillQueued=true;
            enemySignal-=skillCost;
            nextEnemyOrder=round+2;
            return;
        }
        if (enemySignal<tacticsCost) return;
        const int balance=troops[1]*advantage(enemyTroop,plan.troop)-
                          troops[0]*advantage(plan.troop,enemyTroop);
        if (veteran && morale[1]<40 && troops[1]>initial[1]/5) enemyManeuver=fallBack;
        else if (strained || maneuver==press) enemyManeuver=hold;
        else if (veteran && plan.troop==bow && enemyTroop!=bow &&
                 sites[to].terrain!=river && balance>=-30000) enemyManeuver=flank;
        else if (balance>15000 && morale[1]>=65) enemyManeuver=press;
        if (veteran && (enemyManeuver==press || enemyManeuver==flank)) {
            int weakness=80;
            for (int i=0;i<rankCount;++i) {
                const int advantage=ranks[1][i]-ranks[0][i];
                if (ranks[0][i]>0 && advantage>weakness) {weakness=advantage;enemyRank=i;}
            }
        }
        if (veteran && enemyManeuver==measured) {
            int pressure=60;
            for (int lane=0;lane<rankCount;++lane) {
                const int threat=ranks[0][lane]-ranks[1][lane];
                if (ranks[1][lane] && stance(0,lane)==press && threat>pressure) {
                    pressure=threat;
                    enemyRank=lane;
                }
            }
            if (enemyRank>=0) enemyManeuver=hold;
        }
        if (enemyManeuver!=measured) {
            enemySignal-=tacticsCost;
            nextEnemyOrder=round+(veteran?2:3);
        }
    }
    void enemyDamageModifiers(int& taken,int& dealt) const {
        switch (enemyFormation) {
            case square: if (difficulty!=Difficulty::story) dealt=dealt*94/100; break;
            case wedge:taken=taken*111/100;dealt=dealt*106/100;break;
            case wings:taken=taken*107/100;break;
        }
        if (!enemySkillQueued) return;
        switch (opponent()->power) {
            case charge:taken+=std::min(95,opponent()->strength/2+14);break;
            case guard:dealt=dealt*54/100;break;
            case guile:break;
            case inspire:taken+=25;break;
            case wildfire:taken+=sites[to].terrain==river?82:42;break;
            case volley:taken+=enemyTroop==bow?81:49;break;
        }
    }
    Clash forecastClash() const {
        const int effectiveWall=std::max(0,walls-(blockade?2:0)-(maneuver==flank?1:0));
        int taken=hit(troops[1],enemyTroop,plan.troop,morale[1],defending?walls:0);
        int dealt=hit(troops[0],plan.troop,enemyTroop,morale[0],defending?0:effectiveWall);
        int prowess=(plan.troop==officer().favored?112:100)+(level-1)*4+(gear==sword?12:0);
        dealt=dealt*prowess/100;
        if (gear==armor) taken=taken*85/100;
        switch (plan.formation) {
            case square:taken=taken*89/100;break;
            case wedge:
                dealt=dealt*(generalHealth && atFront && generalLane==1?128:116)/100;
                taken=taken*110/100;
                break;
            case wings:
                dealt=dealt*(maneuver==flank || (generalHealth && atFront && generalLane!=1)?126:108)/100;
                if (enemyTroop==horse) taken=taken*112/100;
                break;
        }
        enemyDamageModifiers(taken,dealt);
        if (shield>0) taken/=3;
        return {rankAttacks(1,taken),rankAttacks(0,dealt)};
    }
    void step() {
        if (paused) return;
        if (duel.visible()) {duel.tick();return;}
        if (effects) --effects;
        if (strikeTicks) --strikeTicks;
        if (enemySkillTicks) --enemySkillTicks;
        if (result) return;
        ++ticks;
        constexpr int offsets[]={0,14,-12,4,-26};
        for (int side=0;side<2;++side) for (int lane=0;lane<rankCount;++lane) {
            auto& position=rankAdvance[side][lane];
            position+=std::clamp(offsets[stance(side,lane)]-position,-2,2);
        }
        if (ticks%5==0) signal=std::min(100,signal+2);
        if (ticks<approach || (ticks-approach)%period) return;
        const auto incoming=forecastClash();
        if (enemySkillQueued) {
            enemySkillUsed=true;
            enemySkillTicks=22;
            switch (opponent()->power) {
                case guile:morale[0]=std::max(0,morale[0]-12);break;
                case wildfire:morale[0]=std::max(0,morale[0]-7);break;
                case inspire:morale[1]=std::min(100,morale[1]+19);break;
                default:break;
            }
        }
        steadyRanks(0);
        steadyRanks(1);
        if (shield>0) --shield;
        if (commander>=0 && generalHealth) {
            const auto position=stance(0,generalLane);
            if (atFront) {
                const int risk=10+std::clamp(incoming[0][generalLane]/3,0,21)+
                    (position==press?5:0)-(plan.formation==square?5:0);
                generalHealth=std::max(0,generalHealth-risk);
                if (!generalHealth) {
                    atFront=false;
                    morale[0]=std::max(0,morale[0]-12);
                }
            } else generalHealth=std::min(100,generalHealth+(position==fallBack?15:8));
        }
        if (maneuverRounds && !--maneuverRounds) maneuver=measured;
        for (auto& command:rankOrders) if (command.rounds) --command.rounds;
        for (int side=0;side<2;++side) {
            losses[side]=0;
            rankLosses[side]=incoming[side];
            for (int lane=0;lane<rankCount;++lane) {
                ranks[side][lane]-=incoming[side][lane];
                losses[side]+=incoming[side][lane];
            }
            troops[side]-=losses[side];
            morale[side]=std::max(0,morale[side]-4-105*losses[side]/std::max(240,initial[side]));
        }
        if (!ranks[0][generalLane] && troops[0]) {
            generalLane=nearestRank(ranks[0],generalLane);
            atFront=false;
        }
        ++round;
        evaluate();
        if (enemyWithdrawalQueued && result>=0 && troops[1]>0 && morale[1]>0) {
            enemyWithdrew=true;
            result=1;
        }
        planEnemyOrder();
    }
    void evaluate() {
        const bool left=!troops[0] || !morale[0], right=!troops[1] || !morale[1];
        if (left || right) result=(right&&!left)||(right&&left&&defending)?1:-1;
        else if (round>=maximumRounds) {
            const int a=troops[0]+3*morale[0],b=troops[1]+3*morale[1];
            result=a>b+(defending?-110:110)?1:-1;
        }
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
                if (enemyWithdrew) aftermath.enemyReturned=troops[1];
            } else {
                c.relieve(to);
                d={a.owner,std::max(garrison,troops[1]),d.fields,std::max(1,d.fort-1),d.market,54};
                aftermath.dispersed=c.billet(troops[0],to,-1);
            }
            c.afterTurn();
        } else {
            c.columns[column]={};
            if (result>0) {
                if (enemyWithdrew && enemyRetreatCity>=0 && c.cities[enemyRetreatCity].owner==enemy) {
                    auto& refuge=c.cities[enemyRetreatCity];
                    aftermath.enemyReturned=std::min(troops[1],armyLimit-refuge.troops);
                    refuge.troops+=aftermath.enemyReturned;
                }
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
        if (result>0 && !enemyWithdrew && enemyIndex>=0 && !c.prisoner && !c.won &&
            (duel.result>0 || troops[1]*3<=initial[1] || !morale[1])) {
            c.prisoner={enemy,enemyIndex,to};
            aftermath.captured=true;
        }
        return aftermath;
    }
};

}
