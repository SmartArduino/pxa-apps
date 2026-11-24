#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include "chapters.hpp"
#include "history.hpp"

namespace han {

enum class CityPolicy { manual, livelihoods, garrison };
struct Stewardship {
    int order=0, gold=0, grain=0, troops=0;
};
struct City {
    int owner=3, troops=0, fields=1, fort=1;
    int market=0, order=72;
    int prisoners=0;
    CityPolicy policy=CityPolicy::manual;
};
struct Order {
    int from=-1,to=-1;
    constexpr explicit operator bool() const {return from>=0;}
};
struct MarchRoute {
    std::array<int,cityCount> cities{};
    int count=0;
    explicit operator bool() const {return count>=2;}
    int legs() const {return count?count-1:0;}
    Order endpoints() const {return *this?Order{cities[0],cities[count-1]}:Order{};}
    Order leg(int index) const {return {cities[index],cities[index+1]};}
};
struct Veteran {
    int xp=0, fatigue=0, station=-1;
    std::array<Gear,gearSlotCount> kit{noGear,noGear,noGear,noGear};
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
    int since=0, offered=0;
    constexpr explicit operator bool() const {return side>=0;}
};
struct Detention {
    int city=-1, since=0;
    constexpr explicit operator bool() const {return city>=0;}
};
struct PrisonNews {
    int escaped=0, returned=0, enemiesFled=0, troopsReturned=0;
    constexpr explicit operator bool() const {return escaped || returned || enemiesFled || troopsReturned;}
};
enum class Offer {unavailable, refused, joined};
enum FindKind { noFind, moneyFound, grainFound, gearFound, loreFound, talentFound };
struct SearchOdds {
    int talent=0, localItem=0, equipment=0, money=0, grain=0;
};
struct Discovery {
    FindKind kind=noFind;
    int value=0;
};
enum class SeasonPlan { regular, relief, drills };
enum class Difficulty { story, standard, veteran };
struct DifficultyRules {
    int graceSeasons, assaultMargin, minimumReserve, transferLimit, hostileSupplyPercent;
};
inline constexpr DifficultyRules difficultyRules[]={
    {4,40,210,0,100}, {2,-45,180,145,100}, {1,-65,200,240,120}
};
inline constexpr int grainBySeason[]={100,100,125,90};
inline constexpr int reliefOrder=14, drillExperience=45, drillRest=18;
inline constexpr int truceCost=180, truceSeasons=3, allianceCost=240, allianceSeasons=4;
struct SeasonAccounts {
    int gold, grain, goldCost, grainCost;
};
struct AttackForecast {
    Order road;
    int attackers=0, defenders=0;
};
struct AlliedRequest {
    int side=-1, target=-1;
    explicit operator bool() const {return side>=0;}
};
enum class AlliedOutcome { none, canceled, repelled, captured };
struct AlliedReport {
    AlliedRequest request;
    AlliedOutcome outcome=AlliedOutcome::none;
};
inline constexpr int alliedAttackFood=100;

struct Campaign {
    int player=0, chapter=0, turn=1, gold=0, grain=0, actions=0;
    Difficulty difficulty=Difficulty::standard;
    int won=0, commander=0, tours=0, recruited=7;
    int marchBase=-1;
    bool objectivePaid=false;
    unsigned regionalRewards=0, chronicleDeeds=0;
    unsigned talentSeen=0, talentHired=0;
    unsigned hireOffered=0, talentOffered=0;
    uint64_t surveyed=0;
    std::array<int,3> truce{};
    std::array<int,3> alliance{};
    AlliedRequest alliedRequest{};
    AlliedReport alliedReport{};
    std::array<int,noGear> stock{};
    std::array<City,cityCount> cities{};
    std::array<Veteran,commanderSlots> generals{};
    std::array<Order,ownerCount> orders{};
    std::array<Column,columnCount> columns{};
    Order defense{};
    std::array<Captive,3> prisoners{};
    std::array<Captive,guestCount> guests{};
    std::array<Detention,commanderSlots> detained{};

    static City initialCity(int city,int scene) {
        const auto& site=sites[city];
        if (const auto* change=opening(scene,city))
            return {change->owner,change->troops,site.yield>=4?2:1,change->fort};
        return {site.owner,site.troops,site.yield>=4?2:1,site.fort};
    }

    void start(int side,int scene=0,Difficulty level=Difficulty::standard) {
        *this={};
        player=side;
        chapter=scene;
        difficulty=level;
        gold=side==0?430:side==1?580:490;
        grain=side==0?460:side==1?550:610;
        if (level==Difficulty::story) {gold+=120;grain+=160;}
        for (int i=0;i<cityCount;++i) cities[i]=initialCity(i,scene);
        if (scene==0 && side==0) cities[10].fields=3;
        if (scene==1) recruited=15;
        if (scene==2) {
            recruited=31;
            if (side==2) talentSeen=talentHired=1u<<14;
        }
        stock[side==0?saddle:side==1?armor:manual]=1;
        if (side==0) {
            generals[rulerSlot].kit[armsSlot]=twinJian;
            generals[1].kit[armsSlot]=dragonBlade;
            generals[2].kit[armsSlot]=snakeSpear;
            generals[3].kit[strategySlot]=featherFan;
        }
        actions=actionCapacity();
        makeOrders();
    }
    const Chapter& story() const {return chapters[chapter];}
    const Objective& objective() const {return story().objectives[player];}
    int capital() const {return story().base[player];}
    int season() const {return (story().season+turn-1)%4;}
    int year() const {return story().year+(story().season+turn-1)/4;}
    const DifficultyRules& rules() const {return difficultyRules[int(difficulty)];}

    bool own(int city) const {return city>=0 && city<cityCount && cities[city].owner==player;}
    int preferredOrigin() const {
        if (own(marchBase)) return marchBase;
        if (own(capital())) return capital();
        for (int city=0;city<cityCount;++city) if (own(city)) return city;
        return -1;
    }
    bool setOrigin(int city) {
        if (!own(city) || marchBase==city) return false;
        marchBase=city;
        return true;
    }
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
        if (slot==rulerSlot) return true;
        if (const int index=guestIndex(slot);index>=0) return bool(guests[index]);
        if (const int id=talentIndex(slot);id>=0) return (talentHired&(1u<<id))!=0;
        return slot>=0 && slot<officerCount && (recruited&(1<<slot));
    }
    const Officer& leader(int slot) const {
        if (slot==rulerSlot) return rulers[player];
        if (auto* talent=talentAtSlot(slot)) return talent->officer;
        if (const int index=guestIndex(slot);index>=0)
            return officers[guests[index].side][guests[index].officer];
        return officers[player][slot];
    }
    bool hasSpecialty(int slot,Specialty specialty) const {
        const auto* talent=talentAtSlot(slot);
        return talent && talent->specialty==specialty;
    }
    bool inField(int slot) const {
        for (const auto& unit:columns) if (unit && unit.leader==slot) return true;
        return false;
    }
    bool held(int slot) const {return slot>=0 && slot<commanderSlots && bool(detained[slot]);}
    bool available(int slot,int defendCity=-1) const {
        if (!unlocked(slot) || held(slot) || inField(slot)) return false;
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
        if (!own(city) || !unlocked(slot) || held(slot) || inField(slot)) return false;
        const int previous=governor(city);
        if (previous>=0) generals[previous].station=-1;
        if (slot!=previous) generals[slot].station=city;
        return true;
    }
    void relieve(int city) {
        const int slot=governor(city);
        if (slot>=0) generals[slot].station=-1;
    }
    bool setPolicy(int city,CityPolicy policy) {
        if (!own(city) || cities[city].policy==policy ||
            (policy!=CityPolicy::manual && governor(city)<0)) return false;
        cities[city].policy=policy;
        return true;
    }
    Stewardship stewardship(int city,int mayor) const {
        if (mayor<0) return {};
        const auto& officer=leader(mayor);
        switch (cities[city].policy) {
            case CityPolicy::livelihoods:return {4+officer.counsel/30,6+officer.counsel/10,10+officer.counsel/7,0};
            case CityPolicy::garrison:return {0,0,0,28+officer.valor/3};
            case CityPolicy::manual:return {};
        }
        return {};
    }
    bool canChangeGear(int slot) const {return unlocked(slot) && !held(slot) && !inField(slot);}
    bool mayEquip(int slot,Gear item) const {
        if (!canChangeGear(slot) || item<0 || item>=noGear || !stock[item]) return false;
        const auto& spec=gearSpecs[item];
        return generals[slot].kit[spec.slot]!=item &&
            (spec.slot!=armsSlot || spec.weapon==longbow || battleRole(leader(slot))!=Role::strategist);
    }
    bool equip(int slot,Gear item) {
        if (!mayEquip(slot,item)) return false;
        auto& old=generals[slot].kit[gearSpecs[item].slot];
        if (old!=noGear) ++stock[old];
        --stock[item];
        old=item;
        return true;
    }
    bool unequip(int slot,GearSlot category) {
        if (!canChangeGear(slot) || category<0 || category>=gearSlotCount) return false;
        auto& old=generals[slot].kit[category];
        if (old==noGear) return false;
        ++stock[old];
        old=noGear;
        return true;
    }
    bool canBuyGear(Gear item) const {
        return item>=0 && item<noGear && !won && actions>0 && stock[item]<24 && gold>=gearSpecs[item].price;
    }
    bool buyGear(Gear item) {
        if (!canBuyGear(item)) return false;
        --actions;
        gold-=gearSpecs[item].price;
        ++stock[item];
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
        if (!unlocked(slot) || held(slot) || inField(slot) || !actions || gold<85 ||
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
            const auto delegated=stewardship(i,mayor);
            sum+=food?delegated.grain:delegated.gold;
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
    int roll(int subject,int purpose) const {
        uint32_t seed=uint32_t(subject+1)*0x9e3779b9u+uint32_t(turn)*0x85ebca6bu+
            uint32_t(1+purpose+8*player+32*chapter)*0xc2b2ae35u;
        seed=(seed^(seed>>16))*0x7feb352du;
        return int((seed^(seed>>15))%100);
    }
    int diplomacy() const {
        return (commander>=0 && !held(commander)?leader(commander).counsel:52)/5;
    }
    int hireChance(int slot) const {return std::clamp(58+3*tours+diplomacy()-3*slot,45,95);}
    bool mayHire(int slot) const {
        return slot>=3 && slot<officerCount && !unlocked(slot) && actions &&
               !(hireOffered&(1u<<slot)) && tours>=hireRenown(slot) && gold>=hireCost(slot);
    }
    Offer hire(int slot) {
        if (!mayHire(slot)) return Offer::unavailable;
        hireOffered|=1u<<slot;
        --actions;
        const bool joined=roll(slot,0)<hireChance(slot);
        gold-=joined?hireCost(slot):hireCost(slot)/4;
        if (joined) recruited|=1<<slot;
        return joined?Offer::joined:Offer::refused;
    }
    int freeGuest() const {
        for (int i=0;i<guestCount;++i) if (!guests[i]) return i;
        return -1;
    }
    bool enemyAvailable(int side,int officer) const {
        for (const auto& p:prisoners) if (p.side==side && p.officer==officer) return false;
        for (const auto& p:guests) if (p.side==side && p.officer==officer) return false;
        return true;
    }
    int capture(int side,int officer,int city) {
        if (side<0 || side>=3 || side==player || officer<0 || officer>=officerCount ||
            !own(city) || !enemyAvailable(side,officer)) return -1;
        for (int i=0;i<int(prisoners.size());++i) if (!prisoners[i]) {
            prisoners[i]={side,officer,city,turn};
            return i;
        }
        return -1;
    }
    int admitChance(int index) const {
        const auto& p=prisoners[index];
        constexpr int loyalty[3][officerCount]={
            {93,99,96,98,82,84,63,69}, {94,80,91,85,66,67,97,84}, {95,76,88,92,91,92,82,80}
        };
        const int base=holdings(p.side)?21:57;
        return std::clamp(base+diplomacy()+(90-loyalty[p.side][p.officer])/2+
                          std::min(26,9*(turn-p.since)),12,95);
    }
    bool mayAdmit(int index) const {
        const auto& p=prisoners[index];
        return p && p.offered!=turn && freeGuest()>=0 && gold>=220;
    }
    Offer admit(int index) {
        if (!mayAdmit(index)) return Offer::unavailable;
        auto& p=prisoners[index];
        p.offered=turn;
        if (roll(p.side*officerCount+p.officer,1)>=admitChance(index)) {
            gold-=55;
            return Offer::refused;
        }
        gold-=220;
        guests[freeGuest()]={p.side,p.officer,-1};
        p={};
        return Offer::joined;
    }
    bool mayRansom(int index) const {
        return prisoners[index] && holdings(prisoners[index].side)>0;
    }
    bool disposeCaptive(int index,bool ransom) {
        auto& p=prisoners[index];
        if (!p || (ransom && !mayRansom(index))) return false;
        if (ransom) gold=std::min(9999,gold+110);
        else cities[p.city].order=std::min(100,cities[p.city].order+12);
        p={};
        return true;
    }
    int enemyOfficer(int side,int city) const {
        const int count=chapter==0?5:officerCount;
        const int start=(city+turn+side*3)%count;
        for (int n=0;n<officerCount;++n) {
            const int idx=n<count?(start+n)%count:n;
            if (enemyAvailable(side,idx)) return idx;
        }
        return -1;
    }
    int escapeChance(int index,int seasonOffset=0) const {
        const auto& p=prisoners[index];
        const auto& camp=cities[p.city];
        return std::clamp(38-camp.fort*5-camp.order/4+std::min(20,(turn+seasonOffset-p.since)*4),5,50);
    }
    void holdGeneral(int slot,int city) {
        detained[slot]={city,turn};
        generals[slot].station=-1;
        if (commander!=slot) return;
        for (int i=0;i<commanderSlots;++i) if (unlocked(i) && !held(i)) {
            commander=i;
            return;
        }
    }
    void freeGeneral(int slot) {
        if (held(commander)) commander=slot;
        detained[slot]={};
    }
    static constexpr int friendlyRansom=160;
    bool ransomGeneral(int slot) {
        if (!held(slot) || gold<friendlyRansom) return false;
        gold-=friendlyRansom;
        freeGeneral(slot);
        return true;
    }
    int freeGeneralsAt(int city) {
        int freed=0;
        for (int slot=0;slot<commanderSlots;++slot) if (detained[slot].city==city) {
            freeGeneral(slot);
            ++freed;
        }
        return freed;
    }
    void losePrisonCamp(int city) {
        for (auto& p:prisoners) if (p.city==city) p={};
    }
    int holdTroops(int city,int count) {
        auto& camp=cities[city];
        const int added=std::min(armyLimit-camp.prisoners,count);
        camp.prisoners+=added;
        return added;
    }
    int enlistCapacity(int city) const {
        if (!actions || !own(city)) return 0;
        const auto& camp=cities[city];
        return std::min({camp.prisoners,std::max(0,armyLimit-camp.troops),180,2*grain});
    }
    bool enlistPrisoners(int city) {
        const int count=enlistCapacity(city);
        if (!count) return false;
        auto& camp=cities[city];
        --actions;
        grain-=(count+1)/2;
        camp.prisoners-=count;
        camp.troops+=count;
        return true;
    }
    bool releasePrisoners(int city) {
        if (!own(city) || !cities[city].prisoners) return false;
        auto& camp=cities[city];
        camp.order=std::min(100,camp.order+std::min(15,4+camp.prisoners/25));
        camp.prisoners=0;
        return true;
    }
    PrisonNews updatePrisoners() {
        PrisonNews news;
        for (int index=0;index<int(prisoners.size());++index) {
            auto& p=prisoners[index];
            if (p && roll(p.side*officerCount+p.officer,2)<escapeChance(index)) {
                p={};
                ++news.escaped;
            }
        }
        for (int slot=0;slot<commanderSlots;++slot) if (detained[slot]) {
            const int age=turn-detained[slot].since;
            const int chance=20+10*age-3*cities[detained[slot].city].fort;
            if (age>=4 || roll(slot,3)<chance) {
                freeGeneral(slot);
                ++news.returned;
            }
        }
        for (int city=0;city<cityCount;++city) {
            auto& camp=cities[city];
            if (!camp.prisoners) continue;
            const int chance=std::clamp(30-4*camp.fort-camp.order/6,5,30);
            if (roll(city,5)>=chance) continue;
            const int fleeing=std::clamp(camp.prisoners/5,1,40);
            if (own(city)) {
                camp.prisoners-=fleeing;
                news.enemiesFled+=fleeing;
            } else {
                const int returned=fleeing-billet(fleeing,city,-1);
                camp.prisoners-=returned;
                news.troopsReturned+=returned;
            }
        }
        return news;
    }

    bool allied(int side) const {return side>=0 && side<3 && side!=player && alliance[side]>0;}
    bool ceasefire(int side) const {return allied(side) || (side>=0 && side<3 && truce[side]>0);}
    bool canNegotiate(int side,int cost) const {
        if (side<0 || side>=3 || side==player || won || defense || !actions || gold<cost ||
            !holdings(side)) return false;
        for (const auto& unit:columns) if (unit && cities[unit.road.to].owner==side) return false;
        return true;
    }
    bool mayTreat(int side) const {return !ceasefire(side) && canNegotiate(side,truceCost);}
    bool treat(int side) {
        if (!mayTreat(side)) return false;
        gold-=truceCost;
        --actions;
        truce[side]=truceSeasons;
        makeOrders();
        return true;
    }
    bool mayAlly(int side) const {
        return canNegotiate(side,allianceCost) && alliance[side]<=1;
    }
    bool ally(int side) {
        if (!mayAlly(side)) return false;
        gold-=allianceCost;
        --actions;
        truce[side]=0;
        alliance[side]=allianceSeasons;
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
    bool canDispatchDeed(int entry) const {
        if (entry<0 || entry>=chronicleCount || won || !actions) return false;
        const auto& story=chronicle[entry];
        const auto& deed=story.deed;
        return deed.bit && !(chronicleDeeds&deed.bit) && own(story.city) &&
            reachedDate(year(),season(),story.year,story.season) &&
            gold>=deed.gold && grain>=deed.grain;
    }
    bool dispatchDeed(int entry) {
        if (!canDispatchDeed(entry)) return false;
        const auto& story=chronicle[entry];
        const auto& deed=story.deed;
        --actions;
        gold-=deed.gold;
        grain=std::min(9999,grain-deed.grain+deed.gainGrain);
        auto& city=cities[story.city];
        city.order=std::min(100,city.order+deed.gainOrder);
        chronicleDeeds|=deed.bit;
        return true;
    }
    static constexpr int searchCost=35;
    int localTalent(int city) const {
        for (int i=0;i<talentCount;++i) if (talents[i].city[chapter]==city) return i;
        return -1;
    }
    bool knowsTalent(int id) const {return id>=0 && (talentSeen&(1u<<id));}
    bool alreadySearched(int city) const {return city>=0 && city<cityCount && (surveyed&(uint64_t(1)<<city));}
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
    Gear commonFind(int city) const {
        const int count=tours>=4?noGear:legacyGearCount;
        auto gear=Gear((city+chapter+turn)%count);
        if (gear==localFinds(city).item) gear=Gear((gear+1)%count);
        return gear;
    }
    SearchOdds searchOdds(int city,int slot) const {
        if (city<0 || city>=cityCount || !available(slot,city)) return {};
        const int quality=searchQuality(slot);
        const auto local=localFinds(city);
        const int candidate=localTalent(city);
        SearchOdds odds;
        if (candidate>=0 && !knowsTalent(candidate)) {
            // The capital lead remains available for the opening recruitment lesson.
            if (candidate==player && city==capital()) return {100};
            odds.talent=std::clamp(local.talentChance+(quality-65)/3+
                                   (cities[city].order-70)/4,12,75);
        }
        if (local.item!=noGear && stock[local.item]<24)
            odds.localItem=std::clamp(local.itemChance+(quality-70)/5,4,28);
        odds.localItem=std::min(odds.localItem,95-odds.talent);
        const int remainder=100-odds.talent-odds.localItem;
        if (stock[commonFind(city)]<24)
            odds.equipment=remainder*std::clamp((quality-40)/2,5,43)/100;
        const int supplies=remainder-odds.equipment;
        const int grainShare=sites[city].terrain==river?52:sites[city].terrain==plain?44:34;
        odds.grain=supplies*grainShare/100;
        odds.money=supplies*37/100;
        return odds;
    }
    Discovery search(int city,int slot) {
        if (!canSearch(city,slot)) return {};
        const auto odds=searchOdds(city,slot);
        const int quality=searchQuality(slot);
        surveyed|=uint64_t(1)<<city;
        --actions;
        gold-=searchCost;
        gainXp(slot,15,8);
        int result=roll(city,6);
        if (result<odds.talent) {
            const int candidate=localTalent(city);
            talentSeen|=1u<<candidate;
            return {talentFound,candidate};
        }
        result-=odds.talent;
        if (result<odds.localItem) {
            const auto gear=localFinds(city).item;
            gainGear(gear);
            return {gearFound,gear};
        }
        result-=odds.localItem;
        if (result<odds.equipment) {
            const auto gear=commonFind(city);
            gainGear(gear);
            return {gearFound,gear};
        }
        if (result<odds.equipment+odds.money) {
            const int amount=std::min(9999-gold,55+quality/2);
            gold+=amount;
            return {moneyFound,amount};
        }
        if (result<odds.equipment+odds.money+odds.grain) {
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
            knowsTalent(id) && !unlocked(talentSlot(id)) && !(talentOffered&(1u<<id)) && gold>=talents[id].fee;
    }
    int welcomeChance(int city,int id) const {
        const int envoy=searchLead(city);
        const int counsel=envoy>=0?searchQuality(envoy):45;
        return std::clamp(41+counsel/4+cities[city].order/5-talents[id].fee/20,50,94);
    }
    Offer welcome(int city,int id) {
        if (!canWelcome(city,id)) return Offer::unavailable;
        talentOffered|=1u<<id;
        const bool joined=roll(id,4)<welcomeChance(city,id);
        gold-=joined?talents[id].fee:talents[id].fee/4;
        if (joined) talentHired|=1u<<id;
        return joined?Offer::joined:Offer::refused;
    }

    static int expeditionSize(int n,DispatchSize size=legion) {
        return std::max(0,std::min(n*levyPercent[size]/100,n-garrison));
    }
    int dispatch(int from,DispatchSize size=legion) const {
        return expeditionSize(cities[from].troops,size);
    }
    int dispatchTo(int from,int to,DispatchSize size=legion) const {
        const int troops=dispatch(from,size);
        return own(to)?std::min(troops,std::max(0,armyLimit-cities[to].troops)):troops;
    }
    MarchRoute marchRoute(int from,int to) const {
        if (!own(from) || to<0 || to>=cityCount || from==to || ceasefire(cities[to].owner))
            return {};
        if (adjacent(from,to)) return {{from,to},2};
        std::array<int,cityCount> previous{},queue{};
        previous.fill(-1);
        previous[from]=from;
        queue[0]=from;
        int tail=1;
        for (int head=0;head<tail && previous[to]<0;++head) {
            const int city=queue[head];
            for (const auto& road:roads) {
                const int next=road.from==city?road.to:road.to==city?road.from:-1;
                if (next<0 || previous[next]>=0 ||
                    (next!=to && !own(next) && !allied(cities[next].owner))) continue;
                previous[next]=city;
                queue[tail++]=next;
            }
        }
        MarchRoute route;
        if (previous[to]<0) return route;
        for (int city=to;city!=from;city=previous[city]) route.cities[route.count++]=city;
        route.cities[route.count++]=from;
        std::reverse(route.cities.begin(),route.cities.begin()+route.count);
        return route;
    }
    int alliedWaypoint(const MarchRoute& route) const {
        for (int i=1;i+1<route.count;++i)
            if (allied(cities[route.cities[i]].owner)) return route.cities[i];
        return -1;
    }
    static constexpr int marchCost(int troops) {return 24+troops/16;}
    int supplyCost(const MarchRoute& route,DispatchSize size,int slot,bool blockade) const {
        if (!route) return 0;
        const auto [from,to]=route.endpoints();
        const int base=marchCost(dispatchTo(from,to,size));
        int supply=base+(blockade?45:0);
        if (own(to)) supply/=2;
        else supply=(supply*rules().hostileSupplyPercent+99)/100;
        supply+=(route.legs()-1)*(base/2);
        const auto mount=generals[slot].kit[mountSlot];
        if (mount==saddle || mount==whiteHorse) supply=supply*3/4;
        else if (mount==redHare) supply=supply*4/5;
        return std::max(1,supply-(hasSpecialty(slot,Specialty::convoy)?22:0));
    }
    int supplyCost(int from,int to,DispatchSize size,int slot,bool blockade) const {
        return supplyCost(marchRoute(from,to),size,slot,blockade);
    }
    int readyColumn() const {
        for (int i=0;i<columnCount;++i) if (columns[i] && !columns[i].eta) return i;
        return -1;
    }
    int freeColumn() const {
        for (int i=0;i<columnCount;++i) if (!columns[i]) return i;
        return -1;
    }
    bool canSend(const MarchRoute& route,DispatchSize size,int lead,bool blockade=false) const {
        if (!route || won || defense || !actions || !available(lead) || freeColumn()<0)
            return false;
        const auto [from,to]=route.endpoints();
        if (dispatchTo(from,to,size)<180) return false;
        for (const auto& unit:columns) if (unit && unit.road.to==to) return false;
        if (own(to) && blockade) return false;
        return grain>=supplyCost(route,size,lead,blockade);
    }
    bool canSend(int from,int to,DispatchSize size,int lead,bool blockade=false) const {
        return canSend(marchRoute(from,to),size,lead,blockade);
    }
    int routeLeader(const MarchRoute& route) const {
        if (!route) return -1;
        if (canSend(route,vanguard,commander)) return commander;
        for (int i=0;i<commanderSlots;++i)
            if (i!=commander && canSend(route,vanguard,i)) return i;
        return -1;
    }
    int routeLeader(int from,int to) const {return routeLeader(marchRoute(from,to));}
    bool canMarch(int from,int to) const {return routeLeader(from,to)>=0;}
    int send(int from,int to,DispatchSize size,int lead,bool blockade) {
        const auto route=marchRoute(from,to);
        if (!canSend(route,size,lead,blockade)) return -1;
        const int id=freeColumn();
        const int food=supplyCost(route,size,lead,blockade);
        const int troops=dispatchTo(from,to,size);
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
            const int added=std::min(troops,std::max(0,armyLimit-cities[city].troops));
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
    Order nextAttack() const {
        for (int side=0;side<ownerCount;++side) {
            const auto order=orders[side];
            if (side!=player && own(order.to) && validOrder(order,side)) return order;
        }
        return {};
    }
    int orderAfterSeason(int city,SeasonPlan plan) const {
        const int mayor=governor(city);
        const int civicBonus=hasSpecialty(mayor,Specialty::envoy)?6:0;
        return std::min(100,cities[city].order+(mayor>=0?8:3)+civicBonus+
                            stewardship(city,mayor).order+(plan==SeasonPlan::relief?reliefOrder:0));
    }
    int troopsAfterSeason(int city,int order) const {
        const auto& c=cities[city];
        if (own(city)) {
            const int mayor=governor(city);
            return std::min(armyLimit,c.troops+25+order/7+(mayor>=0?18:0)+stewardship(city,mayor).troops);
        }
        const int restore=(c.owner<3?63:33)+c.fields*13;
        const bool seat=c.owner<3 && story().base[c.owner]==city;
        // Reinforcements can put a city above the limit for local recruitment.
        const int capacity=std::min(armyLimit,(c.owner<3?1680:1280)+
            110*sites[city].yield+70*c.fort+(seat?220:0));
        return c.troops+std::clamp(capacity-c.troops,0,restore);
    }
    AttackForecast forecastAttack(SeasonPlan plan) const {
        const auto road=nextAttack();
        if (!road) return {};
        return {road,expeditionSize(troopsAfterSeason(road.from,cities[road.from].order)),
                troopsAfterSeason(road.to,orderAfterSeason(road.to,plan))};
    }
    std::array<int,cityCount> borderPressure() const {
        std::array<int,cityCount> pressure{};
        auto inspect=[&](int from,int to) {
            const int a=cities[from].owner,b=cities[to].owner;
            if (a==b || (a==player && ceasefire(b)) || (b==player && ceasefire(a))) return;
            pressure[to]=std::max(pressure[to],dispatch(from));
        };
        for (const auto& road:roads) {
            inspect(road.from,road.to);
            inspect(road.to,road.from);
        }
        for (const auto& unit:columns) if (unit && !own(unit.road.to) &&
                                             !ceasefire(cities[unit.road.to].owner))
            pressure[unit.road.to]=std::max(pressure[unit.road.to],unit.troops);
        return pressure;
    }
    bool hostileCity(int target) const {
        return target>=0 && target<cityCount && !own(target) && !ceasefire(cities[target].owner);
    }
    bool canAskAlly(int target) const {
        return hostileCity(target) && std::any_of(alliance.begin(),alliance.end(),[](int n){return n>0;});
    }
    AttackForecast alliedForecast(int side,int target,const std::array<int,cityCount>& pressure) const {
        if (!allied(side) || !hostileCity(target)) return {};
        const auto& goal=cities[target];
        const int defenders=troopsAfterSeason(target,goal.order);
        const int defenseValue=defenders+70*goal.fort;
        AttackForecast best{{},0,defenders};
        int bestScore=0;
        for (const auto& road:roads) {
            const int from=road.from==target?road.to:road.to==target?road.from:-1;
            if (from<0 || !validOrder({from,target},side)) continue;
            const auto& origin=cities[from];
            const int troops=troopsAfterSeason(from,origin.order);
            const int force=expeditionSize(troops);
            const int remaining=troops-force;
            const int risk=std::max(0,pressure[from]-remaining-55*origin.fort);
            if (force<260 || force*4<defenseValue*3 || remaining<rules().minimumReserve ||
                risk>remaining) continue;
            const int score=force-risk;
            if (!best.road || score>bestScore) {
                best={{from,target},force,defenders};
                bestScore=score;
            }
        }
        return best;
    }
    bool requestAlliedAttack(int side,int target) {
        if (won || defense || !actions || grain<alliedAttackFood || alliedRequest ||
            !alliedForecast(side,target,borderPressure()).road) return false;
        --actions;
        grain-=alliedAttackFood;
        alliedRequest={side,target};
        alliedReport={};
        makeOrders();
        return true;
    }
    bool cancelAlliedAttack() {
        if (!alliedRequest) return false;
        alliedRequest={};
        grain=std::min(9999,grain+alliedAttackFood);
        makeOrders();
        return true;
    }
    Order alliedOrder() const {
        if (!alliedRequest) return {};
        const auto order=orders[alliedRequest.side];
        return order.to==alliedRequest.target?order:Order{};
    }
    void makeOrders() {
        const auto pressure=borderPressure();
        const auto& policy=rules();
        bool playerTargeted=false;
        std::array<bool,cityCount> targeted{};
        orders={};
        if (alliedRequest) {
            const auto plan=alliedForecast(alliedRequest.side,alliedRequest.target,pressure);
            orders[alliedRequest.side]=plan.road;
            if (plan.road) targeted[plan.road.to]=true;
        }
        std::array<int,ownerCount> territories{};
        for (const auto& city:cities) ++territories[city.owner];
        for (int i=0;i<ownerCount;++i) {
            const int side=(i+turn)%ownerCount;
            if (side==player || side==alliedRequest.side || (side>=3 && (side+turn)%3)) continue;
            int best=side<3?policy.assaultMargin:45;
            auto consider=[&](int from,int to) {
                if (!validOrder({from,to},side) || targeted[to]) return;
                const auto& origin=cities[from];
                const auto& target=cities[to];
                const bool playerCity=target.owner==player;
                if (playerCity && (playerTargeted || turn<=policy.graceSeasons)) return;
                const int force=dispatch(from);
                const int reserve=origin.troops-force;
                if (target.troops>force*2 || reserve<policy.minimumReserve) return;
                if (difficulty==Difficulty::veteran && !playerCity &&
                    force<target.troops+70*target.fort+45) return;
                const int risk=std::max(0,pressure[from]-reserve-55*origin.fort);
                const int center=target.owner<3 && story().base[target.owner]==to?75:0;
                const int retaliation=playerCity && difficulty==Difficulty::veteran?105:0;
                const int decisive=difficulty==Difficulty::veteran && !playerCity && target.owner<3 &&
                    territories[target.owner]==1?75:0;
                const int score=force-target.troops-65*target.fort+
                    (target.owner>=3?170:45)+27*sites[to].yield+center+retaliation+decisive-
                    risk/3+((to*19+turn*31+side*17)%41);
                if (score>best) {best=score;orders[side]={from,to};}
            };
            for (const auto& r:roads) {consider(r.from,r.to);consider(r.to,r.from);}
            if (orders[side]) {
                targeted[orders[side].to]=true;
                if (own(orders[side].to)) playerTargeted=true;
            }
        }
    }
    void reinforceEnemyBorders(const std::array<Order,ownerCount>& incoming) {
        const int limit=rules().transferLimit;
        if (!limit) return;
        const auto pressure=borderPressure();
        std::array<bool,cityCount> committed{};
        for (const auto& order:incoming) if (order) {
            committed[order.from]=true;
            committed[order.to]=true;
        }
        for (int side=0;side<3;++side) {
            if (side==player) continue;
            int best=0,amount=0;
            Order transfer;
            auto consider=[&](int from,int to) {
                if (cities[from].owner!=side || cities[to].owner!=side || committed[from] || committed[to]) return;
                const int need=pressure[to]+130-cities[to].troops-40*cities[to].fort;
                const int surplus=cities[from].troops-std::max(330,pressure[from]+150);
                const int troops=std::min({limit,need,surplus,armyLimit-cities[to].troops});
                const int score=troops+need/4;
                if (troops>=75 && pressure[to]>0 && score>best) {
                    best=score;
                    amount=troops;
                    transfer={from,to};
                }
            };
            for (const auto& road:roads) {
                consider(road.from,road.to);
                consider(road.to,road.from);
            }
            if (transfer) {
                cities[transfer.from].troops-=amount;
                cities[transfer.to].troops+=amount;
            }
        }
    }
    PrisonNews advance(SeasonPlan plan=SeasonPlan::regular) {
        if (!canAdvance(plan)) return {};
        auto incoming=orders;
        const auto invitation=alliedRequest;
        if (invitation)
            incoming[invitation.side]=alliedForecast(invitation.side,invitation.target,borderPressure()).road;
        alliedRequest={};
        alliedReport=invitation?AlliedReport{invitation,AlliedOutcome::canceled}:AlliedReport{};
        const auto next=seasonAccounts(plan);
        ++turn;
        const auto prisonNews=updatePrisoners();
        gold=std::min(9999,gold-next.goldCost+next.gold);
        grain=std::min(9999,grain-next.grainCost+next.grain);
        actions=actionCapacity();
        surveyed=0;
        hireOffered=talentOffered=0;
        for (int i=0;i<commanderSlots;++i) {
            const bool away=inField(i);
            const bool drills=plan==SeasonPlan::drills && !away && unlocked(i) && !held(i);
            generals[i].fatigue=std::max(0,generals[i].fatigue-(away?12:26)-(drills?drillRest:0));
            if (drills) gainXp(i,drillExperience);
        }
        for (auto& unit:columns) if (unit && unit.eta) --unit.eta;
        for (int i=0;i<cityCount;++i) {
            auto& c=cities[i];
            if (own(i)) c.order=orderAfterSeason(i,plan);
            c.troops=troopsAfterSeason(i,c.order);
        }
        int approaching=-1;
        for (int side=0;side<ownerCount;++side) {
            auto o=incoming[side];
            const bool requested=side==invitation.side;
            if (!validOrder(o,side) || (requested && !hostileCity(o.to))) continue;
            auto& a=cities[o.from];
            auto& d=cities[o.to];
            if (own(o.to)) {
                if (!defense) {defense=o;approaching=side;}
                continue;
            }
            const int n=dispatch(o.from);
            if (n>d.troops+d.fort*70) {
                a.troops-=n;
                d={side,std::max(garrison,n-d.troops*2/3),d.fields,std::max(1,d.fort-1),d.market,56,d.prisoners};
                if (requested) alliedReport.outcome=AlliedOutcome::captured;
            } else {
                a.troops-=std::min(n*2/3,d.troops/2);
                d.troops=std::max(garrison,d.troops-n/3);
                if (requested) alliedReport.outcome=AlliedOutcome::repelled;
            }
        }
        if (alliedReport.outcome==AlliedOutcome::canceled) grain=std::min(9999,grain+alliedAttackFood);
        reinforceEnemyBorders(incoming);
        orders={};
        if (defense && !validOrder(defense,approaching)) defense={};
        for (int& remaining:truce) if (remaining>0) --remaining;
        for (int& remaining:alliance) if (remaining>0) --remaining;
        if (!defense) afterTurn();
        return prisonNews;
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
