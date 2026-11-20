#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include "chapters.hpp"

namespace han {

struct City {
    int owner=3, troops=0, fields=1, fort=1;
    int market=0, order=72;
};
struct Order {
    int from=-1,to=-1;
    constexpr explicit operator bool() const {return from>=0;}
};
struct Veteran {
    int xp=0, fatigue=0, station=-1;
    Gear gear=noGear;
    int level() const {return 1+xp/110;}
};
struct Column {
    Order road;
    int troops=0, leader=-1, eta=0;
    bool blockade=false;
    constexpr explicit operator bool() const {return bool(road);}
};
struct Captive {
    int side=-1, officer=-1, city=-1;
    constexpr explicit operator bool() const {return side>=0;}
};
enum FindKind { noFind, moneyFound, grainFound, gearFound, loreFound, talentFound };
struct Discovery {
    FindKind kind=noFind;
    int value=0;
};
enum class SeasonPlan { regular, relief, drills };
inline constexpr int grainBySeason[]={100,100,125,90};
inline constexpr int reliefOrder=14, drillExperience=45, drillRest=18;
struct SeasonAccounts {
    int gold, grain, goldCost, grainCost;
};

struct Campaign {
    int player=0, chapter=0, turn=1, gold=0, grain=0, actions=0;
    int won=0, commander=0, tours=0, recruited=7;
    bool objectivePaid=false;
    unsigned regionalRewards=0;
    unsigned talentSeen=0, talentHired=0;
    uint64_t surveyed=0;
    std::array<int,3> truce{};
    std::array<int,noGear> stock{};
    std::array<City,cityCount> cities{};
    std::array<Veteran,commanderSlots> generals{};
    std::array<Order,ownerCount> orders{};
    std::array<Column,columnCount> columns{};
    Order defense{};
    Captive prisoner{}, guest{};

    static City initialCity(int city,int scene) {
        const auto& site=sites[city];
        if (const auto* change=opening(scene,city))
            return {change->owner,change->troops,site.yield>=4?2:1,change->fort};
        return {site.owner,site.troops,site.yield>=4?2:1,site.fort};
    }

    void start(int side,int scene=0) {
        *this={};
        player=side;
        chapter=scene;
        gold=side==0?430:side==1?580:490;
        grain=side==0?460:side==1?550:610;
        for (int i=0;i<cityCount;++i) cities[i]=initialCity(i,scene);
        if (scene==0 && side==0) cities[10].fields=3;
        if (scene==1) recruited=15;
        if (scene==2) recruited=31;
        stock[side==0?saddle:side==1?armor:manual]=1;
        actions=actionCapacity();
        makeOrders();
    }
    const Chapter& story() const {return chapters[chapter];}
    const Objective& objective() const {return story().objectives[player];}
    int capital() const {return story().base[player];}
    int season() const {return (story().season+turn-1)%4;}
    int year() const {return story().year+(story().season+turn-1)/4;}

    bool own(int city) const {return city>=0 && city<cityCount && cities[city].owner==player;}
    int holdings(int side) const {
        int total=0;
        for (const auto& c:cities) total+=c.owner==side;
        return total;
    }
    int actionCapacity() const {
        const int owned=holdings(player);
        return owned<5?2:owned<13?3:4;
    }
    int governor(int city) const {
        if (city<0 || city>=cityCount) return -1;
        for (int i=0;i<commanderSlots;++i) if (generals[i].station==city) return i;
        return -1;
    }
    bool unlocked(int slot) const {
        if (slot==guestSlot) return bool(guest);
        if (talentAtSlot(slot)) return (talentHired&(1u<<(slot-talentFirst)))!=0;
        return slot>=0 && slot<officerCount && (recruited&(1<<slot));
    }
    const Officer& leader(int slot) const {
        if (auto* talent=talentAtSlot(slot)) return talent->officer;
        return slot==guestSlot?officers[guest.side][guest.officer]:officers[player][slot];
    }
    bool hasSpecialty(int slot,Specialty specialty) const {
        const auto* talent=talentAtSlot(slot);
        return talent && talent->specialty==specialty;
    }
    bool inField(int slot) const {
        for (const auto& unit:columns) if (unit && unit.leader==slot) return true;
        return false;
    }
    bool available(int slot,int defendCity=-1) const {
        if (!unlocked(slot) || inField(slot)) return false;
        const int city=generals[slot].station;
        return city<0 || city==defendCity;
    }
    int fieldLeader(int preferred,int defendCity=-1) const {
        if (available(preferred,defendCity)) return preferred;
        for (int slot=0;slot<commanderSlots;++slot)
            if (available(slot,defendCity)) return slot;
        // If every officer is occupied, a city can still mount a defense.
        return -1;
    }
    bool assign(int city,int slot) {
        if (!own(city) || !unlocked(slot) || inField(slot)) return false;
        const int previous=governor(city);
        if (previous>=0) generals[previous].station=-1;
        if (slot!=previous) generals[slot].station=city;
        return true;
    }
    void relieve(int city) {
        const int slot=governor(city);
        if (slot>=0) generals[slot].station=-1;
    }
    bool equip(int slot,Gear type) {
        if (!unlocked(slot) || inField(slot) || type<0 || type>noGear) return false;
        auto& kit=generals[slot].gear;
        if (type==kit || (type!=noGear && !stock[type])) return false;
        if (kit!=noGear) ++stock[kit];
        if (type!=noGear) --stock[type];
        kit=type;
        return true;
    }
    void gainGear(Gear type) {
        if (stock[type]<24) ++stock[type];
        else gold=std::min(9999,gold+65);
    }
    void gainXp(int slot,int effort,int strain=0) {
        if (!unlocked(slot)) return;
        auto& g=generals[slot];
        g.xp=std::min(549,g.xp+effort);
        g.fatigue=std::min(100,g.fatigue+strain);
    }
    bool drill(int slot) {
        if (!unlocked(slot) || inField(slot) || !actions || gold<85 ||
            (generals[slot].level()==5 && !generals[slot].fatigue)) return false;
        --actions;
        gold-=85;
        gainXp(slot,68);
        generals[slot].fatigue=std::max(0,generals[slot].fatigue-20);
        return true;
    }
    int income(bool food) const {
        int sum=food?35:30;
        for (int i=0;i<cityCount;++i) {
            const auto& c=cities[i];
            if (c.owner!=player) continue;
            const int base=food?18+17*c.fields:12+9*sites[i].yield+21*c.market;
            const int mayor=governor(i);
            const int orderFactor=75+c.order/3;
            sum+=base*orderFactor/100+(mayor>=0?(leader(mayor).counsel-35)/5:0);
            if (food && hasSpecialty(mayor,Specialty::granary)) sum+=28;
            if (!food && hasSpecialty(mayor,Specialty::treasury)) sum+=22;
        }
        return food?sum*grainBySeason[season()]/100:sum;
    }
    SeasonAccounts seasonAccounts(SeasonPlan plan) const {
        const int reliefCost=plan==SeasonPlan::relief?55+4*holdings(player):0;
        const int drillCost=plan==SeasonPlan::drills?135:0;
        return {income(false),income(true),reliefCost,drillCost};
    }
    bool canAdvance(SeasonPlan plan) const {
        if (won || defense || readyColumn()>=0) return false;
        const auto next=seasonAccounts(plan);
        return gold>=next.goldCost && grain>=next.grainCost;
    }

    static int hireCost(int slot) {return slot==3?240:380+80*(slot-4);}
    static int hireRenown(int slot) {return slot-2;}
    bool mayHire(int slot) const {
        return slot>=3 && slot<officerCount && !unlocked(slot) && actions &&
               tours>=hireRenown(slot) && gold>=hireCost(slot);
    }
    bool hire(int slot) {
        if (!mayHire(slot)) return false;
        recruited|=1<<slot;
        gold-=hireCost(slot);
        --actions;
        return true;
    }
    bool mayAdmit() const {return prisoner && !guest && gold>=220;}
    bool admit() {
        if (!mayAdmit()) return false;
        gold-=220;
        guest=prisoner;
        guest.city=-1;
        prisoner={};
        return true;
    }
    void disposeCaptive(bool ransom) {
        if (!prisoner) return;
        if (ransom) gold=std::min(9999,gold+110);
        else if (own(prisoner.city)) {
            auto& c=cities[prisoner.city];
            c.order=std::min(100,c.order+19);
        }
        prisoner={};
    }
    int enemyOfficer(int side,int city) const {
        const int count=chapter==0?5:officerCount;
        int idx=(city+turn+side*3)%count;
        if (guest && guest.side==side && guest.officer==idx) idx=(idx+1)%count;
        return idx;
    }

    bool ceasefire(int side) const {return side>=0 && side<3 && truce[side]>0;}
    bool mayTreat(int side) const {
        if (side<0 || side>=3 || side==player || !actions || gold<180 ||
            ceasefire(side) || !holdings(side)) return false;
        for (const auto& unit:columns) if (unit && cities[unit.road.to].owner==side) return false;
        return true;
    }
    bool treat(int side) {
        if (!mayTreat(side)) return false;
        gold-=180;
        --actions;
        truce[side]=3;
        makeOrders();
        return true;
    }
    bool border(int city) const {
        for (const auto& r:roads) {
            if (r.from==city && !own(r.to)) return true;
            if (r.to==city && !own(r.from)) return true;
        }
        return false;
    }
    bool market() {
        if (!actions || gold<120) return false;
        gold-=120;
        grain=std::min(9999,grain+330);
        --actions;
        return true;
    }
    bool militia() {
        if (!actions || gold<150 || grain<95) return false;
        gold-=150;
        grain-=95;
        --actions;
        for (int i=0;i<cityCount;++i) if (own(i) && border(i))
            cities[i].troops=std::min(armyLimit,cities[i].troops+105);
        makeOrders();
        return true;
    }
    static constexpr int cost(Work work) {
        constexpr int costs[]={80,95,105,125,25};
        return costs[work];
    }
    bool canWork(int city,Work work) const {
        if (!actions || !own(city) || gold<cost(work)) return false;
        const auto& c=cities[city];
        switch (work) {
            case recruit:return grain>=50 && c.troops<armyLimit;
            case farm:return c.fields<4;
            case walls:return c.fort<4;
            case commerce:return c.market<3;
            case pacify:return grain>=65 && c.order<100;
        }
        return false;
    }
    bool develop(int city,Work work) {
        if (!canWork(city,work)) return false;
        auto& c=cities[city];
        gold-=cost(work);
        --actions;
        switch (work) {
            case recruit:
                grain-=50;
                c.troops=std::min(armyLimit,c.troops+240+c.order*90/72);
                break;
            case farm:++c.fields;break;
            case walls:++c.fort;break;
            case commerce:++c.market;break;
            case pacify:grain-=65;c.order=std::min(100,c.order+26);break;
        }
        return true;
    }
    static constexpr int searchCost=35;
    int localTalent(int city) const {
        for (int i=0;i<talentCount;++i) if (talents[i].city[chapter]==city) return i;
        return -1;
    }
    bool knowsTalent(int id) const {return id>=0 && (talentSeen&(1u<<id));}
    bool alreadySearched(int city) const {return surveyed&(uint64_t(1)<<city);}
    int searchLead(int city) const {
        int lead=-1;
        for (int slot=0;slot<commanderSlots;++slot) {
            if (!available(slot,city)) continue;
            if (lead<0 || searchQuality(slot)>searchQuality(lead)) lead=slot;
        }
        return lead;
    }
    int searchQuality(int slot) const {
        return leader(slot).counsel-generals[slot].fatigue/4+
            (hasSpecialty(slot,Specialty::scout)?30:0);
    }
    bool canSearch(int city,int slot) const {
        return !won && actions && own(city) && !alreadySearched(city) && gold>=searchCost &&
            available(slot,city);
    }
    Discovery search(int city,int slot) {
        if (!canSearch(city,slot)) return {};
        surveyed|=uint64_t(1)<<city;
        --actions;
        gold-=searchCost;
        const int quality=searchQuality(slot);
        gainXp(slot,15,8);
        const int candidate=localTalent(city);
        if (candidate>=0 && !knowsTalent(candidate)) {
            talentSeen|=1u<<candidate;
            return {talentFound,candidate};
        }
        const int roll=(city*17+turn*37+chapter*23+player*13)%100;
        const int gearChance=std::clamp((quality-40)/2,8,43);
        if (roll<gearChance) {
            const auto gear=Gear((city+chapter+turn)%noGear);
            if (stock[gear]<24) {
                gainGear(gear);
                return {gearFound,gear};
            }
        }
        if (roll<gearChance || roll<55) {
            const int amount=std::min(9999-gold,55+quality/2);
            gold+=amount;
            return {moneyFound,amount};
        }
        if (roll<88) {
            const int amount=std::min(9999-grain,85+quality);
            grain+=amount;
            return {grainFound,amount};
        }
        const int extra=std::min(549-generals[slot].xp,55+quality/3);
        gainXp(slot,extra);
        return {loreFound,extra};
    }
    bool canWelcome(int city,int id) const {
        return !won && own(city) && id>=0 && id<talentCount && talents[id].city[chapter]==city &&
            knowsTalent(id) && !unlocked(talentFirst+id) && gold>=talents[id].fee;
    }
    bool welcome(int city,int id) {
        if (!canWelcome(city,id)) return false;
        gold-=talents[id].fee;
        talentHired|=1u<<id;
        return true;
    }

    int dispatch(int from,DispatchSize size=legion) const {
        const int n=cities[from].troops;
        return std::max(0,std::min(n*levyPercent[size]/100,n-garrison));
    }
    static constexpr int marchCost(int troops) {return 24+troops/16;}
    int supplyCost(int from,int to,DispatchSize size,int slot,bool blockade) const {
        int supply=marchCost(dispatch(from,size))+ (blockade?45:0);
        if (own(to)) supply/=2;
        if (generals[slot].gear==saddle) supply=supply*3/4;
        return std::max(1,supply-(hasSpecialty(slot,Specialty::convoy)?22:0));
    }
    int readyColumn() const {
        for (int i=0;i<columnCount;++i) if (columns[i] && !columns[i].eta) return i;
        return -1;
    }
    int freeColumn() const {
        for (int i=0;i<columnCount;++i) if (!columns[i]) return i;
        return -1;
    }
    bool canSend(int from,int to,DispatchSize size,int lead,bool blockade=false) const {
        if (won || defense || !actions || !own(from) || to<0 || to>=cityCount ||
            !adjacent(from,to) || !available(lead) || freeColumn()<0 || dispatch(from,size)<180)
            return false;
        for (const auto& unit:columns) if (unit && unit.road.to==to) return false;
        if (own(to) && (blockade || cities[to].troops>=armyLimit)) return false;
        return !ceasefire(cities[to].owner) && grain>=supplyCost(from,to,size,lead,blockade);
    }
    int routeLeader(int from,int to) const {
        if (canSend(from,to,vanguard,commander)) return commander;
        for (int i=0;i<commanderSlots;++i) if (canSend(from,to,vanguard,i)) return i;
        return -1;
    }
    bool canMarch(int from,int to) const {return routeLeader(from,to)>=0;}
    int send(int from,int to,DispatchSize size,int lead,bool blockade) {
        if (!canSend(from,to,size,lead,blockade)) return -1;
        const int id=freeColumn();
        const int food=supplyCost(from,to,size,lead,blockade);
        const int troops=dispatch(from,size);
        cities[from].troops-=troops;
        grain-=food;
        --actions;
        columns[id]={{from,to},troops,lead,blockade?1:0,blockade};
        makeOrders();
        return id;
    }
    int billet(int troops,int desired,int fallback) {
        auto add=[&](int city) {
            if (!own(city)) return;
            const int added=std::min(troops,armyLimit-cities[city].troops);
            cities[city].troops+=added;
            troops-=added;
        };
        add(desired);
        add(fallback);
        for (int city=0;troops && city<cityCount;++city)
            if (adjacent(desired,city) || adjacent(fallback,city)) add(city);
        return troops;
    }
    bool recall(int id) {
        if (id<0 || id>=columnCount || !columns[id]) return false;
        const auto unit=columns[id];
        columns[id]={};
        (void)billet(unit.troops,unit.road.from,unit.road.to);
        gainXp(unit.leader,0,8);
        makeOrders();
        return true;
    }
    void arriveFriendly(int id) {
        const auto unit=columns[id];
        columns[id]={};
        (void)billet(unit.troops,unit.road.to,unit.road.from);
        gainXp(unit.leader,20,6);
        makeOrders();
    }

    bool validOrder(Order o,int side) const {
        return o && adjacent(o.from,o.to) && cities[o.from].owner==side &&
            cities[o.to].owner!=side && dispatch(o.from)>=180 &&
            !(own(o.to) && ceasefire(side));
    }
    void makeOrders() {
        bool playerTargeted=false;
        std::array<bool,cityCount> targeted{};
        orders={};
        for (int side=0;side<ownerCount;++side) {
            if (side==player || (side>=3 && (side+turn)%3)) continue;
            int best=side<3?-45:30;
            auto consider=[&](int from,int to) {
                if (!validOrder({from,to},side) || targeted[to]) return;
                const auto& target=cities[to];
                if (target.owner==player && (playerTargeted || turn<=2)) return;
                const int score=dispatch(from)-target.troops-65*target.fort+
                    (target.owner>=3?175:45)+25*sites[to].yield+
                    ((to*19+turn*31+side*17)%93);
                if (score>best) {best=score;orders[side]={from,to};}
            };
            for (const auto& r:roads) {consider(r.from,r.to);consider(r.to,r.from);}
            if (orders[side]) {
                targeted[orders[side].to]=true;
                if (own(orders[side].to)) playerTargeted=true;
            }
        }
    }
    void advance(SeasonPlan plan=SeasonPlan::regular) {
        if (!canAdvance(plan)) return;
        const auto incoming=orders;
        const auto next=seasonAccounts(plan);
        ++turn;
        gold=std::min(9999,gold-next.goldCost+next.gold);
        grain=std::min(9999,grain-next.grainCost+next.grain);
        actions=actionCapacity();
        surveyed=0;
        for (int i=0;i<commanderSlots;++i) {
            const bool away=inField(i);
            const bool drills=plan==SeasonPlan::drills && !away && unlocked(i);
            generals[i].fatigue=std::max(0,generals[i].fatigue-(away?12:26)-(drills?drillRest:0));
            if (drills) gainXp(i,drillExperience);
        }
        for (auto& unit:columns) if (unit && unit.eta) --unit.eta;
        for (int i=0;i<cityCount;++i) {
            auto& c=cities[i];
            const int mayor=governor(i);
            if (own(i)) {
                const int civicBonus=hasSpecialty(mayor,Specialty::envoy)?6:0;
                c.order=std::min(100,c.order+(mayor>=0?8:3)+civicBonus+(plan==SeasonPlan::relief?reliefOrder:0));
                c.troops=std::min(armyLimit,c.troops+25+c.order/7+(mayor>=0?18:0));
            } else {
                const int restore=(c.owner<3?63:33)+c.fields*13;
                c.troops=std::min(armyLimit,c.troops+restore);
            }
        }
        int approaching=-1;
        for (int side=0;side<ownerCount;++side) {
            auto o=incoming[side];
            if (!validOrder(o,side)) continue;
            auto& a=cities[o.from];
            auto& d=cities[o.to];
            if (own(o.to)) {defense=o;approaching=side;continue;}
            const int n=dispatch(o.from);
            if (n>d.troops+d.fort*70) {
                a.troops-=n;
                d={side,std::max(garrison,n-d.troops*2/3),d.fields,std::max(1,d.fort-1),d.market,56};
            } else {
                a.troops-=std::min(n*2/3,d.troops/2);
                d.troops=std::max(garrison,d.troops-n/3);
            }
        }
        orders={};
        if (defense && !validOrder(defense,approaching)) defense={};
        for (int& remaining:truce) if (remaining>0) --remaining;
        if (!defense) afterTurn();
    }

    void checkEnd(bool seasonEnded) {
        const int owned=holdings(player);
        if (owned==cityCount) won=1;
        else if (!owned || (seasonEnded && turn>lastTurn)) won=-1;
    }
    void afterTurn() {
        defense={};
        actions=std::min(actions,actionCapacity());
        checkEnd(true);
        if (!won) makeOrders();
    }
    bool rewardObjective() {
        auto goal=objective();
        if (objectivePaid || !own(goal.a) || !own(goal.b)) return false;
        objectivePaid=true;
        gold=std::min(9999,gold+250);
        grain=std::min(9999,grain+220);
        gainGear(goal.reward);
        return true;
    }
    bool regionalPaid(int id) const {return regionalRewards&(1u<<id);}
    bool regionalCityReady(int id,int city) const {
        return own(city) && cities[city].order>=regionalGoals[id].minOrder;
    }
    int regionalProgress(int id) const {
        int complete=0;
        for (int city:regionalGoals[id].cities) complete+=regionalCityReady(id,city);
        return complete;
    }
    bool canClaimRegional(int id) const {
        return !won && id>=0 && id<regionalGoalCount && !regionalPaid(id) &&
            regionalProgress(id)==int(std::size(regionalGoals[id].cities));
    }
    bool claimRegional(int id) {
        if (!canClaimRegional(id)) return false;
        const auto& goal=regionalGoals[id];
        regionalRewards|=1u<<id;
        gold=std::min(9999,gold+goal.gold);
        grain=std::min(9999,grain+goal.grain);
        gainGear(goal.gear);
        return true;
    }
};

}
