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
    static constexpr int rankCount=laneCount;
    using Ranks=std::array<int,rankCount>;
    using Clash=std::array<Ranks,2>;
    int from=0,to=0,hero=0,enemy=3,commander=0,column=-1;
    int enemyIndex=-1,level=1,shield=0,walls=1;
    const Officer* general=&officers[0][0];
    bool defending=false,blockade=false,usedSkill=false,paused=false;
    int troops[2]{},initial[2]{},morale[2]{100,100};
    Ranks ranks[2]{},fronts[2]{},rankLosses[2]{};
    std::array<RankOrder,rankCount> rankOrders{};
    int enemyRank=-1,lastSupport=-1,lastStrikeLane=-1;
    int losses[2]{},result=0,round=0,ticks=0,roundClock=0,signal=0,effects=0;
    BattlePlan plan{};
    Lineup enemyLineup{spear,spear,spear};
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
    int wallPoints=0,engineShots=0,lastShot=-1,siegeTicks=0;
    Duel duel{};
    static constexpr int fieldLength=960,campFront=114,fieldMargin=50;
    static constexpr int period=48,maximumRounds=11;
    static constexpr int commandCost=14,skillCost=46,tacticsCost=18,duelCost=28,strikeCost=12;
    static constexpr int rankOrderCost=9,supportCost=8,skillRange=196;
    static constexpr int gatePosition=710,engineCost=14;

    static Ranks deployRanks(int force,Formation formation) {
        constexpr int wingShare[]={31,24,39};
        const int wing=force*wingShare[formation]/100;
        return {wing,force-2*wing,wing};
    }
    void formRanks(int side) {
        ranks[side]=deployRanks(troops[side],side?enemyFormation:plan.formation);
    }
    static int formationDepth(Formation formation,int lane) {
        constexpr Ranks depth[]={{0,0,0},{-12,17,-12},{13,-13,13}};
        return depth[formation][lane];
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
        const auto core=enemyUnit(to,enemy,opponent());
        enemyLineup=enemyDeployment(to,walls,defending,core,difficulty);
        enemyFormation=enemyFormationFor(to,walls,defending,core,difficulty);
        if (!mayBringEngine(walls,defending,blockade)) plan.engine=noEngine;
        wallPoints=2*std::max(0,walls-(blockade?2:0));
        engineShots=plan.engine==ram?2:plan.engine==stoneThrower?3:0;
        formRanks(0);
        formRanks(1);
        for (int lane=0;lane<rankCount;++lane) {
            const int lead=campFront+formationDepth(plan.formation,lane);
            const int rival=fieldLength-campFront-formationDepth(enemyFormation,lane);
            fronts[0][lane]=lead;
            fronts[1][lane]=rival;
        }
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
    static Lineup enemyDeployment(int city,int fort,bool advancing,Troop core,Difficulty skill) {
        if (skill==Difficulty::story) return {core,core,core};
        switch (sites[city].terrain) {
            case river: return {bow,core,skill==Difficulty::veteran?crossbow:spear};
            case hills: return {shields,core,crossbow};
            case plain: return {skill==Difficulty::veteran?heavyHorse:horse,
                                !advancing && fort>=3?shields:core,bow};
        }
        return {core,core,core};
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
        const auto former=ranks[0];
        plan.formation=next;
        const auto weights=deployRanks(troops[0],next);
        // A change of formation can redistribute units of the same kind.
        // Each troop pool retains its current strength across a change of shape.
        std::array<int,troopCount> available{},weight{};
        for (int lane=0;lane<rankCount;++lane) {
            available[plan.lineup[lane]]+=former[lane];
            weight[plan.lineup[lane]]+=std::max(1,weights[lane]);
        }
        for (int lane=0;lane<rankCount;++lane) {
            const auto troop=plan.lineup[lane];
            const int portion=std::max(1,weights[lane]);
            ranks[0][lane]=available[troop]*portion/weight[troop];
            available[troop]-=ranks[0][lane];
            weight[troop]-=portion;
        }
        for (int lane=0;lane<rankCount;++lane) if (!former[lane] && ranks[0][lane]) {
            const int source=nearestRank(former,lane);
            if (source>=0) fronts[0][lane]=fronts[0][source];
        }
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
    int generalPosition(int lane,bool front) const {
        return std::clamp(fronts[0][lane]+(front?12:-48),20,fieldLength-20);
    }
    int generalGap(int lane) const {
        return std::abs(fronts[1][lane]-generalPosition(generalLane,atFront))+
            26*std::abs(lane-generalLane);
    }
    int strikeTarget() const {
        int best=-1, distance=plan.weapon==longbow?224:79;
        for (int lane=0;lane<rankCount;++lane) {
            if (!ranks[1][lane]) continue;
            const int gap=generalGap(lane);
            if (gap<distance) {best=lane;distance=gap;}
        }
        return best;
    }
    bool mayStrike() const {
        return canMoveGeneral() && lastStrike!=round && ranks[0][generalLane] &&
            signal>=strikeCost && (atFront || plan.weapon==longbow) && strikeTarget()>=0;
    }
    bool strike() {
        if (!mayStrike()) return false;
        signal-=strikeCost;
        lastStrike=round;
        strikeTicks=15;
        lastStrikeLane=strikeTarget();
        int damage=30+officer().valor/3+(level-1)*5;
        switch (plan.weapon) {
            case longspear:
                damage+=mounted(troopFor(1,lastStrikeLane))?38:12;
                break;
            case saber:
                damage+=generalLane==1?36:19;
                morale[1]=std::max(0,morale[1]-4);
                break;
            case longbow:
                damage+=troopFor(1,lastStrikeLane)==spear?22:6;
                if (!atFront) damage=damage*4/5;
                break;
        }
        if (gear==sword) damage+=12;
        rankLosses[1]={};
        losses[1]=hurtRank(1,lastStrikeLane,damage);
        evaluate();
        return true;
    }
    bool mayManeuver(Maneuver next) const {
        if (result || paused || duel.visible()) return false;
        if (next==measured) return maneuver!=measured ||
            std::any_of(rankOrders.begin(),rankOrders.end(),[](const RankOrder& order) {return order.rounds>0;});
        return signal>=tacticsCost &&
            (next!=maneuver || maneuverRounds<2) && (next!=flank || sites[to].terrain!=river);
    }
    bool direct(Maneuver next) {
        if (!mayManeuver(next)) return false;
        if (next!=measured) signal-=tacticsCost;
        maneuver=next;
        maneuverRounds=next==measured?0:2;
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
            if (source!=lane && (!ranks[0][lane] || (plan.lineup[source]==plan.lineup[lane] &&
                 std::abs(fronts[0][source]-fronts[0][lane])<=164)) &&
                (best<0 || ranks[0][source]>ranks[0][best])) best=source;
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
        const int source=supportSource(lane);
        if (!ranks[0][lane]) {
            fronts[0][lane]=fronts[0][source];
            plan.lineup[lane]=plan.lineup[source];
        }
        ranks[0][source]-=count;
        ranks[0][lane]+=count;
        lastSupport=round;
        signal-=supportCost;
        return true;
    }
    static bool mayBringEngine(int fort,bool defense,bool siege) {
        return !defense && fort>=2 && fort>(siege?2:0);
    }
    int enginePosition() const {
        return fronts[0][plan.engineLane]+(plan.engine==ram?16:-49);
    }
    int engineRange() const {return plan.engine==ram?151:298;}
    int engineGap() const {return std::abs(gatePosition-enginePosition());}
    int effectiveCover() const {return (wallPoints+1)/2;}
    bool mayFireEngine() const {
        return plan.engine!=noEngine && engineShots>0 && wallPoints>0 && !result && !paused &&
            !duel.visible() && ranks[0][plan.engineLane] && lastShot!=round &&
            signal>=engineCost && engineGap()<=engineRange();
    }
    bool fireEngine() {
        if (!mayFireEngine()) return false;
        signal-=engineCost;
        --engineShots;
        lastShot=round;
        siegeTicks=23;
        wallPoints=std::max(0,wallPoints-(plan.engine==ram?2:1));
        const int lane=plan.engineLane;
        if (plan.engine==stoneThrower && std::abs(fronts[1][lane]-gatePosition)<=95) {
            rankLosses[1]={};
            const int hit=hurtRank(1,lane,44);
            if (hit) morale[1]=std::max(0,morale[1]-4);
            evaluate();
        }
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
    bool mayRally() const {
        if (!canMoveGeneral() || usedSkill || signal<skillCost) return false;
        if (officer().power==guard || officer().power==inspire) return true;
        for (int lane=0;lane<rankCount;++lane)
            if (ranks[1][lane] && generalGap(lane)<=skillRange) return true;
        return false;
    }
    bool rally() {
        if (!mayRally()) return false;
        usedSkill=true;
        signal-=skillCost;
        effects=25;
        const auto& lead=officer();
        int strike=0;
        switch (lead.power) {
            case charge:
                strike=lead.strength+(troopFor(0,generalLane)==lead.favored?35:0);
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
                strike=lead.strength+(ranged(troopFor(0,generalLane))?85:0);
                break;
        }
        if (strike) strike+=10*(level-1);
        rankLosses[1]={};
        losses[1]=0;
        Ranks targets{};
        int count=0;
        for (int lane=0;lane<rankCount;++lane) if (generalGap(lane)<=skillRange) {
            targets[lane]=ranks[1][lane];
            count+=targets[lane];
        }
        const auto portions=splitByRank(strike,targets,count);
        for (int lane=0;lane<rankCount;++lane)
            losses[1]+=hurtRank(1,lane,portions[lane]);
        evaluate();
        return true;
    }
    static int advantage(Troop a,Troop b) {
        constexpr int match[troopCount][troopCount]={
            {100, 69,145, 85, 94,151},
            {145,100, 69, 64, 82, 65},
            { 69,145,100,117,138, 88},
            {105,124, 85,100,122, 68},
            {127,117, 72, 86,100,116},
            { 64,135,120,145,113,100}
        };
        return match[a][b];
    }
    int lanePower(int side,int lane,int target) const {
        const auto troop=troopFor(side,lane);
        const auto opponent=troopFor(1-side,target);
        int strength=advantage(troop,opponent)+(morale[side]-62)/3;
        const auto terrain=sites[to].terrain;
        if (mounted(troop)) strength+=terrain==plain?17:-21;
        if (troop==bow && terrain==river) strength+=20;
        if (troop==spear && terrain==hills) strength+=12;
        if (side==int(defending)) {
            const int cover=effectiveCover()-(stance(side,lane)==flank?1:0);
            strength-=8*std::max(0,cover);
        }
        return std::max(32,strength);
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
        const int lost=std::min(damage,ranks[side][lane]);
        ranks[side][lane]-=lost;
        rankLosses[side][lane]+=lost;
        troops[side]-=lost;
        return lost;
    }
    struct Tactics {int attack,exposure,spirit;};
    Tactics tactics(int side,int lane) const {
        const auto move=stance(side,lane);
        if (side) {
            constexpr Tactics enemyOrders[]={{100,100,0},{121,112,0},{76,72,7},{132,88,0},{51,62,13}};
            return enemyOrders[move];
        }
        switch (move) {
            case measured:return {100,100,0};
            case press:return {135,122,0};
            case hold:return {75,56,8};
            case flank:return {ranged(troopFor(1,lane))?149:120,mounted(troopFor(1,lane))?114:86,0};
            case fallBack:return {43,42,14};
        }
        return {100,100,0};
    }
    Troop troopFor(int side,int lane) const {return side?enemyLineup[lane]:plan.lineup[lane];}
    int targetRank(int side,int lane) const {
        const auto& opposing=ranks[1-side];
        int target=nearestRank(opposing,lane);
        if (target>=0 && stance(side,lane)==flank)
            for (int n=std::max(0,lane-1);n<=std::min(rankCount-1,lane+1);++n)
                if (opposing[n] && opposing[n]<opposing[target]) target=n;
        return target;
    }
    int laneGap(int side,int lane,int target) const {
        return std::abs(fronts[side][lane]-fronts[1-side][target])+
            24*std::abs(lane-target);
    }
    int range(int side) const {
        int furthest=0;
        for (int lane=0;lane<rankCount;++lane) if (ranks[side][lane])
            furthest=std::max(furthest,range(side,lane));
        return furthest;
    }
    int range(int side,int lane) const {
        constexpr int reach[troopCount]={64,144,60,56,168,62};
        return reach[troopFor(side,lane)];
    }
    int attackTarget(int side,int lane) const {
        const int target=targetRank(side,lane);
        if (target<0 || laneGap(side,lane,target)<=range(side,lane)) return target;
        // Archers can cover an adjacent engaged line while their own target is too far off.
        if (ranged(troopFor(side,lane))) {
            int best=-1,gap=range(side,lane);
            for (int n=std::max(0,lane-1);n<=std::min(rankCount-1,lane+1);++n)
                if (ranks[1-side][n] && laneGap(side,lane,n)<=gap) {
                    best=n;
                    gap=laneGap(side,lane,n);
                }
            return best;
        }
        return -1;
    }
    bool canReach(int side,int lane) const {
        if (!ranks[side][lane]) return false;
        return attackTarget(side,lane)>=0;
    }
    bool inContact() const {
        for (int side=0;side<2;++side) for (int lane=0;lane<rankCount;++lane)
            if (canReach(side,lane)) return true;
        return false;
    }
    int closestGap() const {
        int gap=fieldLength;
        for (int lane=0;lane<rankCount;++lane) {
            if (!ranks[0][lane]) continue;
            for (int foe=0;foe<rankCount;++foe)
                if (ranks[1][foe]) gap=std::min(gap,laneGap(0,lane,foe));
        }
        return gap;
    }
    void marchRanks() {
        Ranks next[2]={fronts[0],fronts[1]};
        for (int side=0;side<2;++side) for (int lane=0;lane<rankCount;++lane) {
            if (!ranks[side][lane]) continue;
            const auto move=stance(side,lane);
            const int target=targetRank(side,lane);
            if (target<0 || move==hold) continue;
            const int x=fronts[side][lane], foe=fronts[1-side][target];
            const int direction=x==foe?(side?-1:1):foe>x?1:-1;
            if (move==fallBack) {
                if (laneGap(side,lane,target)<range(side,lane)+88)
                    next[side][lane]=std::clamp(x-direction*2,fieldMargin,fieldLength-fieldMargin);
                continue;
            }
            const int lateral=24*std::abs(lane-target);
            const int reach=range(side,lane);
            const int stop=std::max(6,(move==press?reach*2/3:reach-6)-lateral);
            const auto troop=troopFor(side,lane);
            const bool plainfield=sites[to].terrain==plain;
            const int speed=2+(troop==horse && plainfield)-(troop==heavyHorse && !plainfield)+
                (move==press || move==flank);
            const int step=std::clamp(std::abs(foe-x)-stop,0,speed);
            next[side][lane]=std::clamp(x+direction*step,fieldMargin,fieldLength-fieldMargin);
            if (side==1 && plan.engine!=noEngine && wallPoints && move==measured && direction<0 &&
                (round || ticks<460))
                next[side][lane]=std::max(next[side][lane],std::min(x,gatePosition-68));
        }
        fronts[0]=next[0];
        fronts[1]=next[1];
        if (round) ++roundClock;
    }
    bool clashReady() const {return round?roundClock>=period:inContact();}
    Ranks rankAttacks(int side,int base,int extra=0,int modifier=100) const {
        const auto shares=splitByRank(base,ranks[side],troops[side]);
        const auto bonuses=splitByRank(extra,ranks[side],troops[side]);
        Ranks attacks{};
        for (int lane=0;lane<rankCount;++lane) {
            if (!shares[lane]) continue;
            const auto& opposing=ranks[1-side];
            const int target=attackTarget(side,lane);
            if (target<0) continue;
            const auto source=tactics(side,lane);
            const auto guard=tactics(1-side,target);
            // An open lane can hit the side of its neighbor, but never cross two lanes for a bonus.
            const int attack=source.attack+(!opposing[lane] && std::abs(lane-target)==1?15:0);
            const int power=std::max(1,shares[lane]*lanePower(side,lane,target)/100);
            attacks[target]+=(power*formationPower(side,lane)/100+bonuses[lane])*
                int64_t(modifier)*attack*guard.exposure/1000000;
        }
        for (int lane=0;lane<rankCount;++lane)
            attacks[lane]=std::min(attacks[lane],ranks[1-side][lane]);
        return attacks;
    }
    void steadyRanks(int side) {
        int weighted=0;
        for (int i=0;i<rankCount;++i) weighted+=ranks[side][i]*tactics(side,i).spirit;
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
        int balance=0;
        bool rangedThreat=false;
        for (int lane=0;lane<rankCount;++lane) {
            const auto own=troopFor(0,lane),foe=troopFor(1,lane);
            balance+=ranks[1][lane]*advantage(foe,own)-ranks[0][lane]*advantage(own,foe);
            rangedThreat|=ranks[0][lane]>0 && ranged(own) && mounted(foe);
        }
        if (veteran && morale[1]<40 && troops[1]>initial[1]/5) enemyManeuver=fallBack;
        else if (strained || maneuver==press) enemyManeuver=hold;
        else if (veteran && rangedThreat &&
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
    int formationPower(int side,int lane) const {
        if (side) {
            int rate=enemyFormation==wedge?111:enemyFormation==wings?107:100;
            if (gear==armor) rate=rate*85/100;
            if (plan.formation==square) rate=rate*89/100;
            if (plan.formation==wedge || (plan.formation==wings && mounted(troopFor(1,lane))))
                rate=rate*110/100;
            return rate;
        }
        int rate=(troopFor(0,lane)==officer().favored?112:100)+(level-1)*4+(gear==sword?12:0);
        if (plan.formation==wedge)
            rate=rate*(generalHealth && atFront && generalLane==1?128:116)/100;
        if (plan.formation==wings)
            rate=rate*(stance(0,lane)==flank || (generalHealth && atFront && generalLane!=1)?126:108)/100;
        if (enemyFormation==square && difficulty!=Difficulty::story) rate=rate*94/100;
        if (enemyFormation==wedge) rate=rate*106/100;
        return rate;
    }
    int enemySkillDamage() const {
        if (!enemySkillQueued) return 0;
        switch (opponent()->power) {
            case charge:return std::min(95,opponent()->strength/2+14);
            case inspire:return 25;
            case wildfire:return sites[to].terrain==river?82:42;
            case volley:return ranged(troopFor(1,1))?81:49;
            default:return 0;
        }
    }
    Clash forecastClash() const {
        const int enemyGuard=enemySkillQueued && opponent()->power==guard?54:100;
        return {rankAttacks(1,troops[1]/10+27,enemySkillDamage(),shield?33:100),
                rankAttacks(0,troops[0]/10+27,0,enemyGuard)};
    }
    Clash previewClash() const {
        auto projection=*this;
        // During the opening the enemy keeps advancing even while our side holds.
        const int limit=round?period:fieldLength;
        for (int i=0;i<limit;++i) {
            ++projection.ticks;
            projection.marchRanks();
            if (projection.clashReady()) return projection.forecastClash();
        }
        return {};
    }
    void step() {
        if (paused) return;
        if (duel.visible()) {duel.tick();return;}
        if (effects) --effects;
        if (strikeTicks) --strikeTicks;
        if (siegeTicks) --siegeTicks;
        if (enemySkillTicks) --enemySkillTicks;
        if (result) return;
        ++ticks;
        marchRanks();
        // Give time to issue opening orders without filling the command gauge for free.
        if (ticks%(round?5:14)==0) signal=std::min(100,signal+2);
        if (!clashReady()) return;
        roundClock=0;
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
            if (atFront && incoming[0][generalLane]) {
                const int risk=10+std::clamp(incoming[0][generalLane]/3,0,21)+
                    (position==press?5:0)-(plan.formation==square?5:0);
                generalHealth=std::max(0,generalHealth-risk);
                if (!generalHealth) {
                    atFront=false;
                    morale[0]=std::max(0,morale[0]-12);
                }
            } else if (!atFront) generalHealth=std::min(100,generalHealth+(position==fallBack?15:8));
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
        }
        if (losses[0] || losses[1]) for (int side=0;side<2;++side)
            morale[side]=std::max(0,morale[side]-4-105*losses[side]/std::max(240,initial[side]));
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
