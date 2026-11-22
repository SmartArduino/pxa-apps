#pragma once
#include <cstdlib>
#include <optional>
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
    int xp=0,level=0,dispersed=0,enemyReturned=0,enemyDispersed=0;
    int captive=-1,ourCaptive=-1,enemyPrisoners=0,ourPrisoners=0;
    int rescuedGenerals=0,rescuedTroops=0;
    bool objective=false;
};

struct RankOrder {
    Maneuver maneuver=measured;
    int rounds=0;
};

struct RankAid {
    int source=-1,target=-1,rounds=0;
};

struct Battle {
    static constexpr int rankCount=laneCount;
    using Ranks=std::array<int,rankCount>;
    using Clash=std::array<Ranks,2>;
    struct OrderPreview {
        Clash losses;
        int cost;
    };
    int from=0,to=0,hero=0,enemy=3,commander=0,column=-1;
    int enemyIndex=-1,level=1,shield=0,walls=1;
    const Officer* general=&officers[0][0];
    bool defending=false,blockade=false,usedSkill=false,paused=false;
    bool awaitingOrders=false,anchored=false;
    int troops[2]{},initial[2]{},morale[2]{100,100};
    Ranks ranks[2]{},fronts[2]{},rankLosses[2]{};
    std::array<bool,rankCount> routed[2]{};
    std::array<bool,rankCount> walking[2]{};
    int fled[2]{},routCue[2]{};
    std::array<RankOrder,rankCount> rankOrders{};
    RankAid aid{},enemyAid{};
    int enemyRank=-1,lastSupport=-1,lastStrikeLane=-1;
    int losses[2]{},result=0,round=0,ticks=0,roundClock=0,signal=0,effects=0;
    BattlePlan plan{};
    Lineup enemyLineup{spear,spear,spear};
    Formation enemyFormation=square;
    Maneuver enemyManeuver=measured;
    Difficulty difficulty=Difficulty::standard;
    int enemySignal=0,nextEnemyOrder=0,enemySkillTicks=0;
    bool enemySkillQueued=false,enemySkillUsed=false,enemySiegeSortie=false;
    int enemyRetreatCity=-1;
    bool enemyWithdrawalQueued=false,enemyWithdrew=false;
    int generalLane=1,generalHealth=100,lastStrike=-1,strikeTicks=0;
    bool atFront=false;
    Maneuver maneuver=measured;
    int maneuverRounds=0;
    Gear armament=noGear, bodyArmor=noGear, mount=noGear, strategy=noGear;
    int wallPoints=0,engineShots=0,lastShot=-1,siegeTicks=0;
    int suppressRounds=0,fireLane=-1,fireRounds=0;
    Duel duel{};
    static constexpr int fieldLength=960,campFront=114,fieldMargin=50;
    static constexpr int period=48,maximumRounds=11,skillFrames=38;
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
        static constexpr Officer cityGuard{"城中守备","","",spear,guard,0,0,50,50};
        general=commander>=0?&c.leader(commander):&cityGuard;
        enemyIndex=enemy<3?c.enemyOfficer(enemy,to):-1;
        std::copy(initial,initial+2,troops);
        plan=deployment;
        signal=52;
        if (commander>=0) {
            auto veteran=c.generals[commander];
            level=veteran.level();
            armament=veteran.kit[armsSlot];
            bodyArmor=veteran.kit[armorSlot];
            mount=veteran.kit[mountSlot];
            strategy=veteran.kit[strategySlot];
            if (armament!=noGear && (role()!=Role::strategist || gearSpecs[armament].weapon==longbow))
                plan.weapon=gearSpecs[armament].weapon;
            signal+=level*3-3+(strategy==manual?18:strategy==featherFan?12:0)+
                (c.hasSpecialty(commander,Specialty::command)?12:0);
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
    Role role() const {return battleRole(officer());}
    void prepare() {awaitingOrders=true;}
    bool launch(bool stand) {
        if (!awaitingOrders || paused || result) return false;
        awaitingOrders=false;
        anchored=stand;
        return true;
    }
    bool anchor() {
        if (result || paused || awaitingOrders || duel.visible() || anchored) return false;
        anchored=true;
        maneuver=measured;
        maneuverRounds=0;
        rankOrders={};
        return true;
    }
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
        if (!canMoveGeneral() || lane<0 || lane>=rankCount || !ranks[0][lane] || routed[0][lane] ||
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
        if (armament==featherBow) distance+=42;
        if (armament==snakeSpear) distance+=27;
        for (int lane=0;lane<rankCount;++lane) {
            if (!ranks[1][lane]) continue;
            const int gap=generalGap(lane);
            if (gap<distance) {best=lane;distance=gap;}
        }
        return best;
    }
    bool mayStrike() const {
        return !awaitingOrders && canMoveGeneral() && lastStrike!=round && !routed[0][generalLane] && ranks[0][generalLane] &&
            (role()!=Role::strategist || plan.weapon==longbow) &&
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
        if (role()==Role::strategist) damage=damage*3/5;
        if (armament==sword) damage+=12;
        else if (armament==twinJian) damage+=stance(0,generalLane)==press?38:24;
        else if (armament==snakeSpear && mounted(troopFor(1,lastStrikeLane))) damage+=24;
        rankLosses[1]={};
        losses[1]=hurtRank(1,lastStrikeLane,damage);
        if (armament==dragonBlade) {
            for (int lane=std::max(0,lastStrikeLane-1);lane<=std::min(rankCount-1,lastStrikeLane+1);++lane)
                if (lane!=lastStrikeLane && generalGap(lane)<=112)
                    losses[1]+=hurtRank(1,lane,damage/3);
        }
        evaluate();
        return true;
    }
    bool mayManeuver(Maneuver next) const {
        if (result || paused || awaitingOrders || duel.visible()) return false;
        if (next==measured) return anchored || maneuver!=measured ||
            std::any_of(rankOrders.begin(),rankOrders.end(),[](const RankOrder& order) {return order.rounds>0;});
        return signal>=tacticsCost &&
            (next!=maneuver || maneuverRounds<2) && (next!=flank || sites[to].terrain!=river);
    }
    bool direct(Maneuver next) {
        if (!mayManeuver(next)) return false;
        if (next!=measured) signal-=tacticsCost;
        else anchored=false;
        maneuver=next;
        maneuverRounds=next==measured?0:2;
        rankOrders={};
        return true;
    }
    Maneuver stance(int side,int lane) const {
        if (routed[side][lane]) return fallBack;
        if (side==1) return enemyRank<0 || enemyRank==lane?enemyManeuver:measured;
        return rankOrders[lane].rounds?rankOrders[lane].maneuver:maneuver;
    }
    bool mayDirectRank(int lane,Maneuver next) const {
        return lane>=0 && lane<rankCount && ranks[0][lane]>0 && !routed[0][lane] &&
            !awaitingOrders && !result && !paused &&
            !duel.visible() && signal>=rankOrderCost && (next!=flank || sites[to].terrain!=river) &&
            (next!=stance(0,lane) || (rankOrders[lane].rounds==1));
    }
    bool directRank(int lane,Maneuver next) {
        if (!mayDirectRank(lane,next)) return false;
        signal-=rankOrderCost;
        rankOrders[lane]={next,2};
        return true;
    }
    bool issueOrder(int lane,Maneuver next) {
        return lane==-1?direct(next):directRank(lane,next);
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
            if (source!=lane && !routed[0][source] && (!ranks[0][lane] || (plan.lineup[source]==plan.lineup[lane] &&
                 std::abs(fronts[0][source]-fronts[0][lane])<=164)) &&
                (best<0 || ranks[0][source]>ranks[0][best])) best=source;
        return best;
    }
    int supportSize(int lane) const {
        if (lane<0 || lane>=rankCount || routed[0][lane] || awaitingOrders || paused || result || duel.visible() ||
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
    const RankAid& coordination(int side) const {return side?enemyAid:aid;}
    bool canCoordinate(int side,int source,int target) const {
        if (source<0 || source>=rankCount || target<0 || target>=rankCount ||
            std::abs(source-target)!=1 || ranks[side][source]<90 || !ranks[side][target] ||
            routed[side][source] || routed[side][target]) return false;
        const auto unit=troopFor(side,source);
        const int reach=ranged(unit)?166:unit==shields?100:80;
        return std::abs(fronts[side][source]-fronts[side][target])<=reach;
    }
    bool mayAid(int source,int target) const {
        return !awaitingOrders && !paused && !result && !duel.visible() && signal>=supportCost &&
            lastSupport!=round && canCoordinate(0,source,target);
    }
    bool directAid(int source,int target) {
        if (!mayAid(source,target)) return false;
        signal-=supportCost;
        lastSupport=round;
        aid={source,target,2};
        return true;
    }
    bool coordinating(int side=0) const {
        const auto& order=coordination(side);
        return order.rounds && canCoordinate(side,order.source,order.target);
    }
    bool shieldCover(int side=0) const {
        return coordinating(side) && troopFor(side,coordination(side).source)==shields;
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
        return canMoveGeneral() && role()!=Role::strategist && !routed[0][generalLane] &&
            round>0 && !duel.issued && signal>=duelCost;
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
    int personalSkillCost() const {return skillCost-(strategy==featherFan?6:0);}
    static bool supportPower(Power power) {
        return power==guard || power==inspire || power==rallyGuard || power==rallyOath ||
            power==eightArray || power==tigerBanner || power==ironCommand || power==eyeValor;
    }
    bool mayRally() const {
        if (awaitingOrders || !canMoveGeneral() || usedSkill || signal<personalSkillCost()) return false;
        if (supportPower(officer().power)) return true;
        if (officer().power==fireArrow)
            return ranks[1][generalLane] && laneGap(0,generalLane,generalLane)<=skillRange+48;
        if (officer().power==markVolley)
            return ranks[1][generalLane] && generalGap(generalLane)<=280;
        if (officer().power==crescentStrike || officer().power==whiteSpear ||
            officer().power==suddenRaid || officer().power==brushFire)
            return ranks[1][generalLane] && generalGap(generalLane)<=skillRange;
        for (int lane=0;lane<rankCount;++lane)
            if (ranks[1][lane] && generalGap(lane)<=skillRange) return true;
        return false;
    }
    bool rally() {
        if (!mayRally()) return false;
        usedSkill=true;
        signal-=personalSkillCost();
        effects=skillFrames;
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
            case breakSupport:
                enemyAid={};
                suppressRounds=2;
                signal=std::min(100,signal+20);
                break;
            case rallyGuard:
                morale[0]=std::min(100,std::max(morale[0],43)+12);
                shield=2;
                break;
            case rallyOath:
                morale[0]=std::min(100,std::max(42,morale[0]+lead.strength));
                signal=std::min(100,signal+14);
                shield=1;
                break;
            case crescentStrike:
            case whiteSpear:
                rankLosses[1]={};
                losses[1]=hurtRank(1,generalLane,lead.strength+10*(level-1)+
                    (lead.power==whiteSpear && mounted(troopFor(0,generalLane))?45:0));
                if (lead.power==crescentStrike) {
                    for (int lane=std::max(0,generalLane-1);lane<=std::min(rankCount-1,generalLane+1);++lane)
                        if (lane!=generalLane && generalGap(lane)<=skillRange)
                            losses[1]+=hurtRank(1,lane,lead.strength/3);
                    morale[1]=std::max(0,morale[1]-9);
                } else morale[0]=std::min(100,morale[0]+16);
                evaluate();
                return true;
            case bridgeRoar:
                morale[1]=std::max(0,morale[1]-lead.strength);
                enemyAid={};
                suppressRounds=1;
                break;
            case eightArray:
                shield=2;
                enemyAid={};
                nextEnemyOrder=std::max(nextEnemyOrder,round+2);
                signal=std::min(100,signal+47);
                break;
            case tigerBanner:
                morale[0]=std::min(100,morale[0]+lead.strength);
                shield=2;
                break;
            case ironCommand:
                signal=100;
                shield=1;
                break;
            case eyeValor:
                generalHealth=std::min(100,generalHealth+32);
                morale[0]=std::min(100,morale[0]+lead.strength);
                shield=1;
                break;
            case suddenRaid:
            case markVolley:
                rankLosses[1]={};
                losses[1]=hurtRank(1,generalLane,lead.strength+10*(level-1)+
                    (lead.power==markVolley && ranged(troopFor(0,generalLane))?30:0));
                if (lead.power==suddenRaid) {
                    rankOrders[generalLane]={press,2};
                    morale[1]=std::max(0,morale[1]-18);
                }
                evaluate();
                return true;
            case fireArrow:
            case brushFire:
                fireLane=generalLane;
                fireRounds=2;
                if (lead.power==brushFire) enemySignal=std::max(0,enemySignal-25);
                rankLosses[1]={};
                losses[1]=hurtRank(1,fireLane,lead.strength+10*(level-1)+(sites[to].terrain==river?55:0));
                morale[1]=std::max(0,morale[1]-10);
                evaluate();
                return true;
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
    int supportedTarget(int side,int lane) const {
        const auto& support=coordination(side);
        if (lane!=support.source || !coordinating(side) || troopFor(side,lane)==shields ||
            !ranks[1-side][support.target]) return -1;
        return support.target;
    }
    int attackTarget(int side,int lane) const {
        if (routed[side][lane]) return -1;
        const int assisted=supportedTarget(side,lane);
        if (assisted>=0 && laneGap(side,lane,assisted)<=range(side,lane)) return assisted;
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
            if (routed[side][lane]) {
                next[side][lane]=std::clamp(fronts[side][lane]+(side?4:-4),fieldMargin,fieldLength-fieldMargin);
                continue;
            }
            if (side==0 && anchored && move==measured) continue;
            const int assisted=supportedTarget(side,lane);
            const int target=assisted>=0?assisted:targetRank(side,lane);
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
            const bool cavalryLead=side==0 && mount==redHare && atFront && generalLane==lane && move==press;
            const int speed=std::max(1,2+(troop==horse && plainfield)-(troop==heavyHorse && !plainfield)+
                cavalryLead+
                (move==press || move==flank)-(side==1 && fireRounds && lane==fireLane));
            const int step=std::clamp(std::abs(foe-x)-stop,0,speed);
            next[side][lane]=std::clamp(x+direction*step,fieldMargin,fieldLength-fieldMargin);
            if (side==1 && plan.engine!=noEngine && wallPoints && move==measured && direction<0 &&
                (round || ticks<460))
                next[side][lane]=std::max(next[side][lane],std::min(x,gatePosition-68));
        }
        for (int side=0;side<2;++side) {
            for (int lane=0;lane<rankCount;++lane) walking[side][lane]=fronts[side][lane]!=next[side][lane];
            fronts[side]=next[side];
        }
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
            int pressure=modifier;
            if (side==1 && ranged(troopFor(side,lane))) {
                if (suppressRounds) pressure=pressure*65/100;
                if (bodyArmor==scaleArmor) pressure=pressure*82/100;
            }
            attacks[target]+=(power*formationPower(side,lane)/100+bonuses[lane])*
                int64_t(pressure)*attack*guard.exposure/1000000;
        }
        if (shieldCover(1-side)) {
            const int lane=coordination(1-side).target;
            attacks[lane]=attacks[lane]*74/100;
        }
        for (int lane=0;lane<rankCount;++lane) attacks[lane]=std::min(attacks[lane],ranks[1-side][lane]);
        return attacks;
    }
    void steadyRanks(int side) {
        int weighted=0;
        for (int i=0;i<rankCount;++i) weighted+=ranks[side][i]*tactics(side,i).spirit;
        morale[side]=std::min(100,morale[side]+weighted/std::max(1,troops[side]));
    }
    bool canSortie() const {
        if (plan.engine==noEngine || !engineShots || !wallPoints || engineGap()>engineRange()+60 ||
            morale[1]<58) return false;
        const int lane=plan.engineLane;
        if (!ranks[0][lane] || ranks[1][lane]<120 || laneGap(1,lane,lane)>range(1,lane)+92) return false;
        const auto escort=troopFor(0,lane),sortie=troopFor(1,lane);
        return 4*ranks[1][lane]*advantage(sortie,escort)>=3*ranks[0][lane]*advantage(escort,sortie);
    }
    bool planEnemyAid() {
        if (suppressRounds || enemySignal<supportCost || coordinating(1)) return false;
        // Project movement once. Candidates are compared at the same positions,
        // then the chosen order takes effect during the next round's real march.
        auto estimate=*this;
        estimate.enemyAid={};
        for (int tick=0;tick<period;++tick) {
            ++estimate.ticks;
            estimate.marchRanks();
        }
        const auto baseline=estimate.forecastClash();
        int bestGain=18;
        RankAid choice{};
        for (int source=0;source<rankCount;++source)
            for (int lane=std::max(0,source-1);lane<=std::min(rankCount-1,source+1);++lane) {
                if (!canCoordinate(1,source,lane) || !estimate.canCoordinate(1,source,lane)) continue;
                estimate.enemyAid={source,lane,2};
                const auto supported=estimate.forecastClash();
                int gain=0;
                for (int rank=0;rank<rankCount;++rank) {
                    const int pressure=ranks[0][rank]<=190?4:2;
                    const int survival=ranks[1][rank]<=190?4:2;
                    gain+=(supported[0][rank]-baseline[0][rank])*pressure+
                         (baseline[1][rank]-supported[1][rank])*survival;
                }
                if (gain>bestGain) {
                    bestGain=gain;
                    choice=estimate.enemyAid;
                }
            }
        if (!choice.rounds) return false;
        enemyAid=choice;
        enemySignal-=supportCost;
        nextEnemyOrder=round+2;
        return true;
    }
    void planEnemyOrder() {
        enemyManeuver=measured;
        enemyRank=-1;
        enemySkillQueued=false;
        enemyWithdrawalQueued=false;
        enemySiegeSortie=false;
        if (difficulty==Difficulty::story || result) return;
        const bool veteran=difficulty==Difficulty::veteran;
        if (enemyRetreatCity>=0 && round>=(veteran?3:4) && troops[1]>=180 &&
            troops[0]>troops[1]+150 && morale[0]>40 &&
            (morale[1]<=(veteran?43:35) || troops[1]*3<=initial[1])) {
            enemyWithdrawalQueued=true;
            enemyManeuver=fallBack;
            enemyAid={};
            return;
        }
        enemySignal=std::min(100,enemySignal+(veteran?22:16));
        if (round<nextEnemyOrder || round<(veteran?1:2)) return;
        const auto* lead=opponent();
        const bool strained=morale[1]<62 || losses[1]>losses[0]*4/3;
        const bool skillWindow=lead && (supportPower(lead->power)?strained:
            round>=3 && troops[1]>initial[1]/4);
        if (skillWindow && !enemySkillUsed && enemySignal>=skillCost) {
            enemySkillQueued=true;
            enemySignal-=skillCost;
            nextEnemyOrder=round+2;
            return;
        }
        if (veteran && enemySignal>=tacticsCost && canSortie()) {
            enemyManeuver=press;
            enemyRank=plan.engineLane;
            enemySiegeSortie=true;
            enemySignal-=tacticsCost;
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
        } else if (veteran) planEnemyAid();
    }
    int formationPower(int side,int lane) const {
        const bool covering=coordination(side).source==lane && shieldCover(side);
        if (side) {
            int rate=enemyFormation==wedge?111:enemyFormation==wings?107:100;
            if (bodyArmor==armor) rate=rate*85/100;
            else if (bodyArmor==brightArmor) rate=rate*92/100;
            if (plan.formation==square) rate=rate*89/100;
            if (plan.formation==wedge || (plan.formation==wings && mounted(troopFor(1,lane))))
                rate=rate*110/100;
            return covering?rate*76/100:rate;
        }
        int rate=(troopFor(0,lane)==officer().favored?112:100)+(level-1)*4+(armament==sword?12:0);
        if (plan.formation==wedge)
            rate=rate*(generalHealth && atFront && generalLane==1?128:116)/100;
        if (plan.formation==wings)
            rate=rate*(stance(0,lane)==flank || (generalHealth && atFront && generalLane!=1)?126:108)/100;
        if (covering) rate=rate*76/100;
        if (enemyFormation==square && difficulty!=Difficulty::story) rate=rate*94/100;
        if (enemyFormation==wedge) rate=rate*106/100;
        return rate;
    }
    int enemySkillDamage() const {
        if (!enemySkillQueued) return 0;
        switch (opponent()->power) {
            case charge:return std::min(95,opponent()->strength/2+14);
            case crescentStrike:case whiteSpear:case suddenRaid:case markVolley:
                return std::min(95,opponent()->strength/2+15);
            case inspire:return 25;
            case wildfire:return sites[to].terrain==river?82:42;
            case brushFire:return sites[to].terrain==river?70:42;
            case volley:return ranged(troopFor(1,1))?81:49;
            default:return 0;
        }
    }
    Clash forecastClash() const {
        const int enemyGuard=enemySkillQueued && supportPower(opponent()->power) &&
            opponent()->power!=inspire?54:100;
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
    std::optional<OrderPreview> previewOrder(int lane,Maneuver next) const {
        auto projection=*this;
        if (!projection.issueOrder(lane,next)) return std::nullopt;
        return OrderPreview{projection.previewClash(),signal-projection.signal};
    }
    void step() {
        if (paused || awaitingOrders) return;
        if (duel.visible()) {duel.tick();return;}
        if (effects) --effects;
        if (strikeTicks) --strikeTicks;
        if (siegeTicks) --siegeTicks;
        if (enemySkillTicks) --enemySkillTicks;
        for (int& cue:routCue) if (cue) --cue;
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
                case bridgeRoar:morale[0]=std::max(0,morale[0]-19);aid={};break;
                case suddenRaid:morale[0]=std::max(0,morale[0]-15);break;
                case eightArray:aid={};break;
                case wildfire:morale[0]=std::max(0,morale[0]-7);break;
                case inspire:morale[1]=std::min(100,morale[1]+19);break;
                case eyeValor:morale[1]=std::min(100,morale[1]+24);break;
                default:break;
            }
        }
        steadyRanks(0);
        steadyRanks(1);
        if (shield>0) --shield;
        if (suppressRounds>0) --suppressRounds;
        if (fireRounds>0) --fireRounds;
        if (commander>=0 && generalHealth) {
            const auto position=stance(0,generalLane);
            if (atFront && incoming[0][generalLane]) {
                const int risk=10+(role()==Role::strategist?8:0)+std::clamp(incoming[0][generalLane]/3,0,21)+
                    (position==press?5:0)-(plan.formation==square?5:0);
                const int protection=(bodyArmor==brightArmor?8:bodyArmor==armor?4:0)+(mount==whiteHorse?5:0);
                generalHealth=std::max(0,generalHealth-std::max(2,risk-protection));
                if (!generalHealth) {
                    atFront=false;
                    morale[0]=std::max(0,morale[0]-12);
                }
            } else if (!atFront) generalHealth=std::min(100,generalHealth+(position==fallBack?15:8));
        }
        if (maneuverRounds && !--maneuverRounds) maneuver=measured;
        for (auto& command:rankOrders) if (command.rounds) --command.rounds;
        for (auto* support:{&aid,&enemyAid})
            if (support->rounds && !--support->rounds) *support={};
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
    void resolveRouts(int side) {
        if (morale[side]>=42) {
            routed[side]={};
            return;
        }
        const auto* lead=side?opponent():general;
        const int threshold=lead && battleRole(*lead)==Role::marshal?19:24;
        if (morale[side]>threshold) return;
        const auto opening=deployRanks(initial[side],side?enemyFormation:plan.formation);
        for (int lane=0;lane<rankCount;++lane) {
            const int remaining=ranks[side][lane];
            if (routed[side][lane] || remaining<8 ||
                (remaining*2>opening[lane] && rankLosses[side][lane]<30)) continue;
            const int deserters=std::max(1,remaining/7);
            routed[side][lane]=true;
            ranks[side][lane]-=deserters;
            troops[side]-=deserters;
            fled[side]+=deserters;
            routCue[side]=28;
            if (side==0 && lane==generalLane) atFront=false;
        }
    }
    void evaluate() {
        resolveRouts(0);
        resolveRouts(1);
        const bool left=!troops[0] || !morale[0], right=!troops[1] || !morale[1];
        if (left || right) result=(right&&!left)||(right&&left&&defending)?1:-1;
        else if (round>=maximumRounds) {
            const int a=troops[0]+3*morale[0],b=troops[1]+3*morale[1];
            result=a>b+(defending?-110:110)?1:-1;
        }
    }
    bool exposed(int side) const {
        const bool defeated=side==0?duel.result<0 || generalHealth<=30:duel.result>0;
        return defeated || morale[side]<=28 || 3*troops[side]<=initial[side];
    }
    Settlement settle(Campaign& c) const {
        Settlement aftermath{};
        if (!result) return aftermath;
        auto& a=c.cities[from];
        auto& d=c.cities[to];
        int enemySurvivors=troops[1],ourSurvivors=troops[0];
        const bool captureEnemy=result>0 && !enemyWithdrew && exposed(1);
        const bool captureOurs=result<0 && exposed(0);
        const int gained=result>0?92+std::min(70,(initial[1]-troops[1]-fled[1])/8):38;
        if (commander>=0) {
            const int before=c.generals[commander].level();
            c.gainXp(commander,gained,18+std::min(38,(initial[0]-troops[0])/12));
            aftermath.xp=gained;
            aftermath.level=c.generals[commander].level()>before?c.generals[commander].level():0;
        }
        if (defending) {
            a.troops-=initial[1];
            if (result>0) {
                if (captureEnemy) {
                    aftermath.enemyPrisoners=c.holdTroops(to,enemySurvivors/2);
                    enemySurvivors-=aftermath.enemyPrisoners;
                }
                a.troops+=enemySurvivors+fled[1];
                d.troops=std::max(garrison,ourSurvivors);
                aftermath.enemyReturned=enemySurvivors;
            } else {
                c.relieve(to);
                c.losePrisonCamp(to);
                d={a.owner,std::max(garrison,enemySurvivors),d.fields,std::max(1,d.fort-1),d.market,54};
                a.troops+=fled[1];
            }
        } else {
            c.columns[column]={};
            if (result>0) {
                const int formerPrisoners=d.prisoners;
                d={hero,std::max(garrison,ourSurvivors),d.fields,std::max(1,d.fort-1),d.market,49};
                aftermath.rescuedGenerals=c.freeGeneralsAt(to);
                aftermath.rescuedTroops=formerPrisoners-c.billet(formerPrisoners,to,from);
                if (captureEnemy) {
                    aftermath.enemyPrisoners=c.holdTroops(to,enemySurvivors/2);
                    enemySurvivors-=aftermath.enemyPrisoners;
                }
                if (enemyRetreatCity>=0 && c.cities[enemyRetreatCity].owner==enemy) {
                    auto& refuge=c.cities[enemyRetreatCity];
                    const int room=std::max(0,armyLimit-refuge.troops);
                    aftermath.enemyReturned=std::min(enemySurvivors,room);
                    refuge.troops+=aftermath.enemyReturned+std::min(fled[1],room-aftermath.enemyReturned);
                }
                aftermath.enemyDispersed=enemySurvivors-aftermath.enemyReturned;
                ++c.tours;
                c.gold=std::min(9999,c.gold+115);
                c.grain=std::min(9999,c.grain+95);
                if (c.tours%3==0) c.gainGear(Gear((to+c.tours)%noGear));
                aftermath.objective=c.rewardObjective();
            } else {
                d.troops=std::max(garrison,enemySurvivors+fled[1]);
            }
        }
        if (captureEnemy && enemyIndex>=0)
            aftermath.captive=c.capture(enemy,enemyIndex,to);
        if (result<0) {
            if (captureOurs) {
                aftermath.ourPrisoners=c.holdTroops(to,ourSurvivors/2);
                ourSurvivors-=aftermath.ourPrisoners;
                if (commander>=0) {
                    c.holdGeneral(commander,to);
                    aftermath.ourCaptive=commander;
                }
            }
            aftermath.dispersed=c.billet(ourSurvivors,defending?to:from,-1);
        }
        if (fled[0]) (void)c.billet(fled[0],defending || result>0?to:from,defending?-1:from);
        if (defending) c.afterTurn();
        else {
            c.checkEnd(false);
            if (!c.won) c.makeOrders();
        }
        return aftermath;
    }
};

}
