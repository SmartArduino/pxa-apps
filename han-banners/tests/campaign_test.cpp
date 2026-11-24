#include "battle.hpp"
#include "save.hpp"
#include <string_view>
#include <cassert>
#include <iostream>

using namespace han;

static BattlePlan planFor(const Campaign& c,Order order,int column) {
    const int enemy=c.cities[column<0?order.from:order.to].owner;
    const int foe=enemy<3?c.enemyOfficer(enemy,order.to):-1;
    const Officer* lead=foe>=0?&officers[enemy][foe]:nullptr;
    const auto core=Battle::enemyUnit(order.to,enemy,lead);
    const auto foes=Battle::enemyDeployment(order.to,c.cities[order.to].fort,column<0,core,c.difficulty);
    BattlePlan plan{spear,wedge,longbow};
    for (int lane=0;lane<Battle::rankCount;++lane) plan.lineup[lane]=counterTroop(foes[lane]);
    return plan;
}

static Settlement resolve(Campaign& campaign, Order order,int column) {
    Battle battle;
    const int guard=campaign.fieldLeader(campaign.commander,order.to);
    battle.start(campaign,order,column,planFor(campaign,order,column),guard);
    for (int i=0;i<1100 && !battle.result;++i) {
        if (battle.duel.ready()) {
            assert(battle.duelOrder(DuelMove((battle.duelIntent()+1)%3)));
        } else if (!battle.duel.visible()) {
            if (battle.morale[1]<62) battle.rally();
            if (battle.round==1 && battle.morale[1]>42 && battle.mayDuel())
                battle.challenge();
            else if (battle.signal>58) battle.strike();
        }
        battle.step();
    }
    assert(battle.result);
    return battle.settle(campaign);
}

static void checkSave(const Campaign& c) {
    auto data=encode(c);
    Campaign restored;
    assert(decode(data,restored));
    assert(data==encode(restored));
    data.back()^=std::byte{1};
    assert(!decode(data,restored));
}

static void fightUntilRound(Battle& battle,int next) {
    for (int i=0;i<Battle::fieldLength && !battle.result && battle.round<next;++i) battle.step();
    assert(battle.round==next || battle.result);
}

static Campaign play(int side,int chapter,Difficulty difficulty=Difficulty::standard) {
    Campaign c;
    c.start(side,chapter,difficulty);
    assert(c.own(c.capital()) && c.year()==chapters[chapter].year);
    for (int attempt=0;!c.won && attempt<24*lastTurn;++attempt) {
        for (int index=0;index<int(c.prisoners.size());++index) if (c.prisoners[index]) {
            if (c.mayAdmit(index) && c.admit(index)==Offer::joined) continue;
            c.disposeCaptive(index,c.mayRansom(index));
        }
        for (int slot=0;slot<commanderSlots;++slot)
            if (c.gold>=280 && c.held(slot)) assert(c.ransomGeneral(slot));
        if (c.defense) {
            resolve(c,c.defense,-1);
            continue;
        }
        const int next=c.readyColumn();
        if (next>=0) {
            if (c.own(c.columns[next].road.to)) c.arriveFriendly(next);
            else resolve(c,c.columns[next].road,next);
            checkSave(c);
            continue;
        }

        int lead=c.fieldLeader(c.commander), best=-650, from=-1, to=-1;
        bool prepareSiege=false;
        int recruitCity=-1,recruitScore=-10000;
        for (int slot=0;slot<commanderSlots;++slot) if (c.available(slot) && lead>=0) {
            if (c.generals[slot].fatigue+18<c.generals[lead].fatigue) lead=slot;
        }
        for (int a=0;a<cityCount;++a) if (c.own(a)) {
            for (int b=0;b<cityCount;++b) if (adjacent(a,b) && !c.own(b)) {
                const int size=c.dispatch(a);
                const int difficulty=size-c.cities[b].troops-c.cities[b].fort*70;
                const bool siege=difficulty<130;
                const int score=difficulty+(siege?std::min(c.cities[b].fort,2)*70:0);
                if (score>best && (!siege || size>=1450) && c.canSend(a,b,legion,lead,siege)) {
                    best=score;from=a;to=b;prepareSiege=siege;
                }
                if (score>recruitScore && c.canWork(a,recruit)) {recruitScore=score;recruitCity=a;}
            }
        }
        if (from>=0) {
            if (prepareSiege && c.stock[sword]) c.equip(lead,sword);
            assert(c.send(from,to,legion,lead,prepareSiege)>=0);
        }
        else if (recruitCity>=0) {assert(c.develop(recruitCity,recruit));c.makeOrders();}
        else c.advance();
        checkSave(c);
    }
    return c;
}

static void writeLegacy(const Campaign& fixture,auto& older,int version,int cities) {
    constexpr size_t baseHeader=18*2;
    constexpr size_t newHeader=baseHeader+4;
    constexpr size_t prisonersAt=2*(ninthSaveWords-1+3*(newTalentFirst-legacyCommanders)+2);
    constexpr size_t kitsBegin=2*(twelfthSaveWords-1-commanderSlots*gearSlotCount);
    constexpr size_t growth=(cityCount-previousCityCount)*6;
    const auto newest=encode(fixture);
    FifteenthSave expanded{};
    std::copy_n(newest.begin(),newHeader,expanded.begin());
    auto word=[&](size_t at,int value) {
        expanded[at]=std::byte(value&255);
        expanded[at+1]=std::byte(value>>8);
    };
    for (int i=0;i<previousCityCount;++i) {
        const auto& city=fixture.cities[i];
        const auto at=newHeader+6*i;
        word(at,city.owner|(city.fields<<4)|(city.fort<<8));
        word(at+2,city.troops);
        word(at+4,city.order|(city.market<<8));
        word(prisonersAt+2*i,city.prisoners);
    }
    constexpr size_t mapEnd=newHeader+previousCityCount*6;
    std::copy_n(newest.begin()+mapEnd+growth,prisonersAt-mapEnd,expanded.begin()+mapEnd);
    std::copy_n(newest.begin()+prisonersAt+growth,kitsBegin-prisonersAt-2*previousCityCount,
                expanded.begin()+prisonersAt+2*previousCityCount);
    constexpr size_t packedKits=kitsBegin+growth-2*previousCityCount;
    for (int slot=0;slot<commanderSlots;++slot) {
        const auto packed=packedKits+2*slot;
        const int kit=int(newest[packed])|(int(newest[packed+1])<<8);
        for (int category=0;category<gearSlotCount;++category)
            word(kitsBegin+2*(slot*gearSlotCount+category),(kit>>(4*category))&15);
    }
    constexpr size_t endOfKits=kitsBegin+2*commanderSlots*gearSlotCount;
    constexpr size_t endOfPacked=packedKits+2*commanderSlots;
    constexpr size_t endOfBase=2*(fifteenthSaveWords-cityPolicyWords-1);
    std::copy_n(newest.begin()+endOfPacked,endOfBase-endOfKits,expanded.begin()+endOfKits);
    for (int first=0;first<previousCityCount;first+=8) {
        int policies=0;
        for (int i=0;i<8;++i) policies|=int(fixture.cities[first+i].policy)<<(2*i);
        word(endOfBase+2*(first/8),policies);
    }
    const size_t header=version>=8?newHeader:baseHeader+(version>=4?2:0);
    std::copy_n(expanded.begin(),header,older.begin());
    older[2]=std::byte(version);
    std::copy_n(expanded.begin()+newHeader,cities*6,older.begin()+header);
    const auto body=header+cities*6;
    if (version==16) {
        std::copy(expanded.begin()+mapEnd,expanded.begin()+kitsBegin,older.begin()+body);
        std::copy_n(newest.begin()+packedKits,2*commanderSlots,older.begin()+kitsBegin);
        std::copy(expanded.begin()+endOfKits,expanded.end()-2,older.begin()+kitsBegin+2*commanderSlots);
    } else {
        std::copy_n(expanded.begin()+mapEnd,older.size()-body-2,older.begin()+body);
    }
    const auto sum=checksum(older,older.size()-2);
    older[older.size()-2]=std::byte(sum&255);
    older.back()=std::byte(sum>>8);
}

static void checkMigration() {
    LegacySave old{};
    size_t offset=0;
    auto put=[&](int n) {old[offset++]=std::byte(n&255);old[offset++]=std::byte(n>>8);};
    for (int n:{0x4842,1,0,3,583,480,0,1}) put(n);
    for (int i=0;i<10;++i) {
        const int owner=i==2||i==3||i==7?0:3;
        put(owner|(2<<4)|(1<<8));
        put(i==7?1017:650);
    }
    for (int i=0;i<8;++i) put(0);
    put(checksum(old,offset));
    assert(offset==74);
    Campaign c;
    assert(migrate(old,c) && c.turn==3 && c.gold==583 && c.grain==480);
    assert(c.cities[7].owner==0 && c.cities[7].troops==1017 && c.holdings(0)==4);
    old[13]^=std::byte{1};
    assert(!migrate(old,c));

    SecondSave second{};
    auto current=Campaign{};
    current.start(2);
    current.commander=3;
    current.recruited=15;
    current.truce[1]=3;
    offset=0;
    auto write=[&](int n) {second[offset++]=std::byte(n&255);second[offset++]=std::byte(n>>8);};
    for (int n:{0x4842,2,2,14,234,689,2,1,3,15,5,0,3,0}) write(n);
    for (int i=0;i<oldCityCount;++i) {
        const auto& city=current.cities[i];
        write(city.owner|(city.fields<<4)|(city.fort<<8));
        write(city.troops);
    }
    for (int i=0;i<2*(ownerCount+1);++i) write(0);
    write(checksum(second,offset));
    assert(offset==second.size());
    assert(migrate(second,c) && c.own(9) && c.turn==14 && c.player==2 && c.commander==3 &&
           c.chapter==0 && c.truce[1]==3);
    second[11]^=std::byte{1};
    assert(!migrate(second,c));

    current.start(0,2);
    current.surveyed=uint64_t(1)<<31;
    auto fixture=current;
    ThirdSave third{};
    FourteenthSave fourteenth{};
    writeLegacy(fixture,fourteenth,14,previousCityCount);
    assert(migrate(fourteenth,c) && c.preferredOrigin()==current.capital() &&
           c.cities[current.capital()].policy==CityPolicy::manual);
    {
        Campaign geared=current;
        geared.generals[rulerSlot].kit={twinJian,scaleArmor,whiteHorse,featherFan};
        const auto previous=fixture;
        fixture=geared;
        FifteenthSave fifteenth{};
        writeLegacy(fixture,fifteenth,15,previousCityCount);
        assert(restoreSave(fifteenth,fifteenth.size(),c));
        assert(c.generals[rulerSlot].kit==geared.generals[rulerSlot].kit && encode(c)==encode(fixture));
        fixture=previous;
    }
    ThirteenthSave thirteenth{};
    current.alliance[2]=allianceSeasons;
    fixture=current;
    writeLegacy(fixture,thirteenth,13,previousCityCount);
    assert(migrate(thirteenth,c) && c.allied(2) && !c.alliedRequest && !c.alliedReport.request);
    current.alliance[2]=0;
    fixture=current;
    TwelfthSave twelfth{};
    writeLegacy(fixture,twelfth,12,previousCityCount);
    assert(migrate(twelfth,c) && c.chapter==2 && (c.alliance==std::array<int,3>{}));
    writeLegacy(fixture,third,3,oldCityCount);
    assert(migrate(third,c) && c.chapter==2 && c.own(32) && c.cities[37].owner==7 &&
           c.surveyed==(uint64_t(1)<<31));
    assert(c.canSearch(32,0));
    c.search(32,0);
    assert(!c.canSearch(32,0) && (c.surveyed>>32)==1);
    checkSave(c);

    current.surveyed=uint64_t(1)<<37;
    current.cities[37].troops=432;
    fixture=current;
    FourthSave fourth{};
    writeLegacy(fixture,fourth,4,expandedCityCount);
    assert(migrate(fourth,c) && c.chapter==2 && c.cities[37].troops==432 &&
           c.surveyed==(uint64_t(1)<<37) && c.cities[40].owner==7 && c.own(47));
    c.surveyed|=uint64_t(1)<<47;
    checkSave(c);

    c.regionalRewards=7;
    fixture=c;
    FifthSave fifth{};
    writeLegacy(fixture,fifth,5,seventhCityCount);
    Save stored{};
    std::copy(fifth.begin(),fifth.end(),stored.begin());
    assert(!restoreSave(stored,fifth.size()-1,c));
    assert(restoreSave(stored,fifth.size(),c) && c.regionalRewards==0 && c.own(47) && c.cities[37].troops==432 &&
           c.surveyed==((uint64_t(1)<<37)|(uint64_t(1)<<47)));

    NinthSave ninth{};
    writeLegacy(fixture,ninth,9,previousCityCount);
    assert(migrate(ninth,c) && c.difficulty==Difficulty::standard && c.own(47));
    assert(!c.held(c.commander) && !c.prisoners[0] && !c.cities[c.capital()].prisoners);
    const auto priorFixture=fixture;
    current.guests[1]={1,2,-1};
    current.commander=guestCommand(1);
    current.holdTroops(current.capital(),47);
    fixture=current;
    TenthSave tenth{};
    writeLegacy(fixture,tenth,10,previousCityCount);
    assert(migrate(tenth,c) && c.commander==guestCommand(1) && &c.leader(c.commander)==&officers[1][2]);
    assert(c.cities[c.capital()].prisoners==47 && !c.unlocked(talentSlot(previousTalentCount)));
    current.generals[0].kit[armorSlot]=armor;
    fixture=current;
    EleventhSave eleventh{};
    writeLegacy(fixture,eleventh,11,previousCityCount);
    assert(migrate(eleventh,c) && c.unlocked(rulerSlot) && c.generals[0].kit[armorSlot]==armor);
    assert(c.generals[0].kit[mountSlot]==noGear && c.generals[rulerSlot].kit[armsSlot]==twinJian);
    fixture=priorFixture;

    SixthSave sixth{};
    writeLegacy(fixture,sixth,6,seventhCityCount);
    std::copy(sixth.begin(),sixth.end(),stored.begin());
    assert(restoreSave(stored,sixth.size(),c) && c.regionalRewards==7 && c.talentHired==0 &&
           c.own(47) && c.cities[37].troops==432);
    SeventhSave seventh{};
    writeLegacy(fixture,seventh,7,seventhCityCount);
    std::copy(seventh.begin(),seventh.end(),stored.begin());
    assert(restoreSave(stored,seventh.size(),c) && c.own(47) && c.cities[37].troops==432);
    for (int i=seventhCityCount;i<cityCount;++i) {
        const auto original=Campaign::initialCity(i,c.chapter);
        assert(c.cities[i].owner==original.owner && c.cities[i].troops==original.troops);
        assert(std::any_of(std::begin(roads),std::end(roads),[&](auto& r){return r.from==i||r.to==i;}));
    }
    SixteenthSave sixteenth{};
    fixture=current;
    fixture.cities[2].policy=CityPolicy::garrison;
    writeLegacy(fixture,sixteenth,16,previousCityCount);
    assert(restoreSave(sixteenth,sixteenth.size(),c) && c.cities[2].policy==CityPolicy::garrison &&
           c.cities[c.capital()].prisoners==47 && c.own(shangyong) && !c.chronicleDeeds);
    c.surveyed|=uint64_t(1)<<(cityCount-1);
    checkSave(c);
}

static void checkSeasonAndRegionalGoals() {
    Campaign c;
    c.start(0);
    const auto fall=c.seasonAccounts(SeasonPlan::regular);
    auto winter=c;
    ++winter.turn;
    assert(fall.grain>winter.income(true) && fall.gold==winter.income(false));

    constexpr int id=2;
    const auto& goal=regionalGoals[id];
    for (int city:goal.cities) c.cities[city].owner=0;
    c.orders={};
    c.cities[7].order=50;
    c.cities[43].order=64;
    assert(c.regionalProgress(id)==1 && !c.claimRegional(id));
    assert(!c.claimRegional(-1) && !c.claimRegional(regionalGoalCount));
    const auto relief=c.seasonAccounts(SeasonPlan::relief);
    const int gold=c.gold;
    const int grain=c.grain;
    auto poor=c;
    poor.gold=relief.goldCost-1;
    assert(!poor.canAdvance(SeasonPlan::relief));
    poor.advance(SeasonPlan::relief);
    assert(poor.turn==1 && poor.cities[7].order==50 && poor.gold==relief.goldCost-1);
    auto regular=c;
    regular.advance();
    assert(regular.cities[7].order==53 && !regular.canClaimRegional(id));
    c.advance(SeasonPlan::relief);
    assert(c.turn==2 && c.gold==gold+relief.gold-relief.goldCost && c.grain==grain+relief.grain);
    assert(c.cities[7].order==67 && c.cities[10].order==89 && c.cities[43].order==81);
    assert(c.canClaimRegional(id));
    const auto rewardGold=c.gold+goal.gold;
    const auto rewardFood=c.grain+goal.grain;
    const auto gear=c.stock[goal.gear]+1;
    assert(c.claimRegional(id) && !c.claimRegional(id) && c.regionalPaid(id));
    assert(c.gold==rewardGold && c.grain==rewardFood && c.stock[goal.gear]==gear);
    auto saved=encode(c);
    Campaign restored;
    assert(decode(saved,restored) && restored.regionalPaid(id) && !restored.claimRegional(id));
    saved[43]|=std::byte{0xf0}; // Invalid packed city policy.
    const auto sum=checksum(saved,saved.size()-2);
    saved[saved.size()-2]=std::byte(sum&255);
    saved.back()=std::byte(sum>>8);
    assert(!decode(saved,restored));

    Campaign drilled;
    drilled.start(0);
    assert(drilled.assign(10,2));
    const auto column=drilled.send(10,15,division,0,true);
    assert(column>=0);
    for (int slot=0;slot<4;++slot) drilled.generals[slot].fatigue=80;
    drilled.generals[2].xp=530;
    const auto estimate=drilled.seasonAccounts(SeasonPlan::drills);
    const auto food=drilled.grain;
    drilled.advance(SeasonPlan::drills);
    assert(drilled.grain==food+estimate.grain-estimate.grainCost && drilled.readyColumn()==column);
    assert(drilled.generals[0].fatigue==68 && drilled.generals[0].xp==0);
    assert(drilled.generals[1].fatigue==36 && drilled.generals[1].xp==45);
    assert(drilled.generals[2].fatigue==36 && drilled.generals[2].xp==549);
    assert(drilled.generals[3].xp==0 && !drilled.canAdvance(SeasonPlan::regular));
}

static void checkAlliedTransit() {
    constexpr int xiakou=10, chaisang=9, hefei=23, panyu=29, wu=2;
    Campaign campaign;
    campaign.start(0,0,Difficulty::story);
    assert(adjacent(xiakou,chaisang) && adjacent(chaisang,hefei) && !adjacent(xiakou,hefei));
    assert(!campaign.canMarch(xiakou,hefei));
    auto truce=campaign;
    assert(truce.treat(wu) && truce.ceasefire(wu) && !truce.allied(wu));
    assert(!truce.canMarch(xiakou,hefei) && !truce.canMarch(xiakou,chaisang));
    truce.gold=allianceCost;
    assert(truce.ally(wu) && !truce.truce[wu] && truce.alliance[wu]==allianceSeasons);

    auto besieger=campaign;
    assert(besieger.send(xiakou,chaisang,vanguard,0,true)>=0 && !besieger.mayAlly(wu));
    const int gold=campaign.gold, orders=campaign.actions;
    assert(campaign.ally(wu) && campaign.gold==gold-allianceCost && campaign.actions==orders-1);
    assert(!campaign.mayAlly(wu) && !campaign.mayTreat(wu) && !campaign.validOrder({chaisang,xiakou},wu));
    assert(!campaign.canMarch(chaisang,hefei) && !campaign.canMarch(xiakou,chaisang));
    const auto route=campaign.marchRoute(xiakou,hefei);
    assert(route.legs()==2 && route.cities[1]==chaisang && campaign.alliedWaypoint(route)==chaisang);
    const auto longer=campaign.marchRoute(xiakou,panyu);
    assert(longer.legs()==3);
    for (int i=0;i<longer.legs();++i)
        assert(adjacent(longer.leg(i).from,longer.leg(i).to));
    const int standing=campaign.cities[chaisang].troops;
    const int food=campaign.grain, cost=campaign.supplyCost(route,vanguard,0,false);
    const int base=campaign.marchCost(campaign.dispatch(xiakou,vanguard));
    assert(cost==base+base/2);
    campaign.grain=cost-1;
    assert(!campaign.canSend(xiakou,hefei,vanguard,0));
    campaign.grain=food;

    auto transfer=campaign;
    transfer.cities[hefei].owner=transfer.player;
    assert(!transfer.canSend(xiakou,hefei,vanguard,0,true));
    const int atDestination=transfer.cities[hefei].troops;
    const int reinforcements=transfer.dispatch(xiakou,vanguard);
    const int moving=transfer.send(xiakou,hefei,vanguard,0,false);
    assert(moving>=0 && !transfer.canSend(xiakou,hefei,vanguard,1,false));
    transfer.arriveFriendly(moving);
    assert(transfer.cities[hefei].troops==atDestination+reinforcements &&
           transfer.cities[chaisang].owner==wu && transfer.cities[chaisang].troops==standing);

    const int marching=campaign.send(xiakou,hefei,vanguard,0,true);
    assert(marching>=0 && campaign.cities[chaisang].troops==standing);
    checkSave(campaign);
    Campaign restored;
    assert(decode(encode(campaign),restored) && restored.alliedWaypoint(restored.marchRoute(xiakou,hefei))==chaisang);
    restored.gold=allianceCost;
    restored.alliance[wu]=1;
    restored.actions=1;
    auto renewed=restored;
    assert(renewed.ally(wu) && renewed.alliance[wu]==allianceSeasons);
    restored.advance();
    assert(!restored.allied(wu) && restored.readyColumn()==marching && !restored.marchRoute(xiakou,hefei));
    checkSave(restored);
    assert(restored.recall(marching) && !restored.columns[marching] && restored.cities[chaisang].owner==wu);
}

static void checkAlliedOffensive() {
    constexpr int chaisang=9, hefei=23, wu=2;
    Campaign campaign;
    campaign.start(0,0,Difficulty::story);
    assert(!campaign.requestAlliedAttack(wu,hefei));
    assert(campaign.ally(wu) && !campaign.requestAlliedAttack(wu,chaisang));
    const auto forecast=campaign.alliedForecast(wu,hefei,campaign.borderPressure());
    assert(forecast.road.from==chaisang && forecast.road.to==hefei && forecast.attackers>forecast.defenders);
    auto fronts=campaign.borderPressure();
    fronts.fill(armyLimit);
    assert(!campaign.alliedForecast(wu,hefei,fronts).road);
    const int troops=campaign.cities[chaisang].troops;
    const int food=campaign.grain, orders=campaign.actions;
    assert(campaign.requestAlliedAttack(wu,hefei) && !campaign.requestAlliedAttack(wu,hefei));
    assert(campaign.grain==food-alliedAttackFood && campaign.actions==orders-1 &&
           campaign.cities[chaisang].troops==troops && campaign.alliedOrder().to==hefei);
    for (int side=0;side<ownerCount;++side)
        if (side!=wu) assert(campaign.orders[side].to!=hefei);
    checkSave(campaign);

    auto canceled=campaign;
    assert(canceled.cancelAlliedAttack() && !canceled.cancelAlliedAttack());
    assert(canceled.grain==food && canceled.actions==orders-1 && !canceled.alliedOrder());

    auto blocked=campaign;
    blocked.cities[hefei].owner=blocked.player;
    blocked.makeOrders();
    assert(!blocked.alliedOrder());
    checkSave(blocked);
    const int before=blocked.grain;
    const int income=blocked.seasonAccounts(SeasonPlan::regular).grain;
    blocked.advance();
    assert(blocked.own(hefei) && !blocked.alliedRequest && blocked.alliedReport.outcome==AlliedOutcome::canceled);
    assert(blocked.grain==before+income+alliedAttackFood);
    checkSave(blocked);

    auto repelled=campaign;
    repelled.orders={};
    for (auto& city:repelled.cities)
        if (city.owner!=repelled.player) city.troops=120;
    repelled.cities[chaisang].troops=1000;
    repelled.cities[chaisang].fort=3;
    repelled.cities[hefei].troops=680;
    const auto risky=repelled.alliedForecast(wu,hefei,repelled.borderPressure());
    assert(risky.road && risky.attackers<=risky.defenders+140);
    repelled.advance();
    assert(repelled.alliedReport.outcome==AlliedOutcome::repelled && repelled.cities[hefei].owner!=wu);

    Campaign restored;
    assert(decode(encode(campaign),restored) && restored.alliedOrder().from==chaisang);
    restored.advance();
    assert(restored.cities[hefei].owner==wu && !restored.alliedRequest &&
           restored.alliedReport.outcome==AlliedOutcome::captured && restored.alliedReport.request.target==hefei);
    checkSave(restored);
}

static void checkDecisiveAssault() {
    Campaign game;
    game.start(2,0,Difficulty::veteran);
    for (auto& city:game.cities) city={3,2000,2,4};
    game.turn=6;
    game.cities[30]={2,1300,2,2};
    game.cities[9]={0,1600,1,2};
    game.cities[10]={1,490,1,2};
    game.cities[23]={3,550,1,2};
    game.makeOrders();
    assert(game.orders[0].from==9 && game.orders[0].to==10);
    game.cities[55].owner=1;
    game.makeOrders();
    assert(game.orders[0].from==9 && game.orders[0].to==23);
}

static void checkCivicAndColumns() {
    Campaign c;
    c.start(0);
    const auto gold=c.gold,grain=c.grain;
    assert(c.develop(10,recruit) && c.cities[10].troops==2030 && c.actions==1);
    assert(c.gold==gold-80 && c.grain==grain-50);
    auto found=c.search(10,1);
    assert(found.kind==talentFound && !c.canSearch(10,1) && !c.actions);
    assert(c.assign(10,2) && c.governor(10)==2 && !c.available(2));
    assert(c.available(2,10) && c.equip(0,saddle));
    checkSave(c);
    c.advance();
    assert(c.actions==2);
    assert(c.treat(2) && c.truce[2]==3 && !c.canSend(10,9,legion,0));
    const int total=c.cities[10].troops;
    const int supply=c.supplyCost(10,15,division,0,true);
    const int troops=c.dispatch(10,division);
    const int food=c.grain;
    const int column=c.send(10,15,division,0,true);
    assert(column==0 && c.columns[column].troops==troops && c.columns[column].eta==1);
    assert(c.cities[10].troops+troops==total && c.grain==food-supply && !c.available(0));
    assert(c.readyColumn()==-1 && !c.assign(10,0));
    checkSave(c);
    c.advance();
    assert(c.readyColumn()==column);
    const auto r=resolve(c,c.columns[column].road,column);
    assert(c.own(15) && r.xp>=90 && c.generals[0].fatigue>0 && !c.columns[column]);
    assert(c.governor(10)==2);
    checkSave(c);

    Campaign wide;
    wide.start(1,2);
    assert(wide.send(7,17,division,0,true)==-1);
    const int first=wide.send(6,10,division,0,true);
    const int next=wide.send(23,8,division,1,true);
    assert(first>=0 && next>=0 && first!=next && wide.freeColumn()>=0 &&
           !wide.available(0) && !wide.available(1));
    assert(!wide.mayTreat(2));
    checkSave(wide);
    const auto before=wide.cities[23].troops;
    const auto troopsBefore=wide.columns[next].troops;
    assert(wide.recall(next) && wide.cities[23].troops==before+troopsBefore);
    assert(wide.available(1) && !wide.available(0));
}

static void checkFiefManagement() {
    constexpr int xiakou=10, jiangling=7;
    Campaign campaign;
    campaign.start(0,0,Difficulty::story);
    campaign.cities[jiangling].owner=campaign.player;
    assert(campaign.preferredOrigin()==xiakou && !campaign.setOrigin(9));
    assert(campaign.setOrigin(jiangling) && campaign.preferredOrigin()==jiangling);
    assert(!campaign.setPolicy(xiakou,CityPolicy::livelihoods));
    assert(campaign.assign(xiakou,0) && campaign.assign(jiangling,1));
    auto manual=campaign;
    assert(campaign.setPolicy(xiakou,CityPolicy::livelihoods) &&
           campaign.setPolicy(jiangling,CityPolicy::garrison) && campaign.actions==manual.actions);
    const auto civilian=campaign.stewardship(xiakou,0);
    const auto defensive=campaign.stewardship(jiangling,1);
    assert(civilian.order>0 && civilian.gold>0 && civilian.grain>0 && !civilian.troops);
    assert(defensive.troops>0 && !defensive.order && !defensive.gold && !defensive.grain);
    assert(campaign.income(false)==manual.income(false)+civilian.gold);
    assert(campaign.income(true)>manual.income(true));
    const int targetOrder=campaign.orderAfterSeason(xiakou,SeasonPlan::regular);
    const int targetTroops=campaign.troopsAfterSeason(jiangling,campaign.cities[jiangling].order);
    assert(targetOrder==manual.orderAfterSeason(xiakou,SeasonPlan::regular)+civilian.order);
    assert(targetTroops==manual.troopsAfterSeason(jiangling,campaign.cities[jiangling].order)+defensive.troops);

    auto paused=campaign;
    paused.relieve(jiangling);
    assert(paused.cities[jiangling].policy==CityPolicy::garrison &&
           !paused.stewardship(jiangling,paused.governor(jiangling)).troops);
    checkSave(paused);
    assert(paused.assign(jiangling,1) && paused.stewardship(jiangling,1).troops==defensive.troops);
    auto lost=campaign;
    lost.cities[jiangling].owner=1;
    assert(lost.preferredOrigin()==xiakou);
    checkSave(campaign);
    campaign.advance();
    assert(campaign.cities[xiakou].order==targetOrder && campaign.cities[jiangling].troops>=targetTroops);

    campaign.cities[jiangling].troops=armyLimit-150;
    assert(!campaign.canSend(xiakou,jiangling,legion,2));
    campaign.cities[jiangling].troops=armyLimit-320;
    const int originBefore=campaign.cities[xiakou].troops;
    const int food=campaign.grain;
    const int cost=campaign.supplyCost(xiakou,jiangling,legion,2,false);
    const int transfer=campaign.send(xiakou,jiangling,legion,2,false);
    assert(transfer>=0 && campaign.columns[transfer].troops==320 && campaign.grain==food-cost);
    checkSave(campaign);
    campaign.arriveFriendly(transfer);
    assert(campaign.cities[xiakou].troops==originBefore-320 && campaign.cities[jiangling].troops==armyLimit);

    for (int owned:{1,4,5,12,13,cityCount}) {
        for (int i=0;i<cityCount;++i) campaign.cities[i].owner=i<owned?campaign.player:1;
        assert(campaign.actionCapacity()==(owned<5?2:owned<13?3:4));
    }
}

static void checkBattleAndCaptive() {
    assert(Battle::advantage(spear,horse)>100 && Battle::advantage(horse,bow)>100 &&
           Battle::advantage(bow,spear)>100);
    Campaign c;
    c.start(0);
    const int id=c.send(10,7,legion,0,false);
    assert(id>=0);
    Battle b;
    b.start(c,c.columns[id].road,id,planFor(c,c.columns[id].road,id));
    assert(!b.mayManeuver(flank) && b.direct(hold) && b.maneuverRounds==2);
    fightUntilRound(b,1);
    assert(b.round==1 && b.maneuverRounds==1 && b.challenge());
    assert(!b.challenge());
    const int morale=b.morale[1];
    while (b.duel.active) {
        assert(b.duelOrder(DuelMove((b.duelIntent()+1)%3)));
        while (b.duel.poseTicks) b.step();
    }
    assert(b.duel.result>0 && b.morale[1]==std::max(0,morale-24));
    for (int i=0;i<650 && !b.result;++i) {
        b.strike();
        b.rally();
        b.step();
    }
    assert(b.result==1);
    auto r=b.settle(c);
    assert(c.own(7) && r.captive==0 && c.prisoners[0].side==1);
    assert(c.enemyOfficer(1,7)!=b.enemyIndex);
    while (c.roll(8+b.enemyIndex,1)>=c.admitChance(0)) ++c.turn;
    assert(c.admit(0)==Offer::joined && c.unlocked(guestSlot) && c.available(guestSlot));
    c.commander=guestSlot;
    assert(&c.leader(guestSlot)==&officers[1][b.enemyIndex]);
    checkSave(c);

    Campaign defended;
    defended.start(0);
    defended.orders={};
    defended.orders[1]={7,10};
    defended.advance();
    assert(defended.defense.from==7 && defended.defense.to==10);
    checkSave(defended);
    resolve(defended,defended.defense,-1);
    assert(!defended.defense);
}

static void checkSearchAndTalents() {
    for (int chapter=0;chapter<chapterCount;++chapter) {
        for (int side=0;side<3;++side) {
            Campaign opening;
            opening.start(side,chapter);
            const int city=opening.capital();
            const int lead=opening.searchLead(city);
            const int gold=opening.gold, actions=opening.actions;
            const auto found=opening.search(city,lead);
            assert(found.kind==talentFound && found.value==side && opening.knowsTalent(side));
            assert(!opening.unlocked(talentFirst+side) && opening.alreadySearched(city));
            assert(opening.gold==gold-Campaign::searchCost && opening.actions==actions-1);
            assert(opening.generals[lead].xp==15 && opening.generals[lead].fatigue==8);
            assert(opening.search(city,lead).kind==noFind);
            checkSave(opening);
            Campaign restored;
            assert(decode(encode(opening),restored) && restored.canWelcome(city,side));
            const auto result=restored.welcome(city,side);
            assert(result!=Offer::unavailable && restored.welcome(city,side)==Offer::unavailable);
            const bool joined=result==Offer::joined;
            assert(restored.gold==opening.gold-talents[side].fee/(joined?1:4) && restored.actions==opening.actions);
            assert(restored.unlocked(talentFirst+side)==joined);
            checkSave(restored);
            const auto nextTurn=restored.turn+1;
            restored.advance();
            assert(restored.turn==nextTurn && !restored.alreadySearched(city));
            const auto loot=restored.search(city,lead);
            assert(loot.kind!=talentFound && loot.kind!=noFind);
        }
    }

    Campaign office;
    office.start(0);
    office.cities[4].owner=0;
    office.cities[36].owner=0;
    office.talentSeen=office.talentHired=(1u<<0)|(1u<<1)|(1u<<5);
    assert(office.assign(10,talentFirst));
    assert(office.assign(4,talentFirst+1));
    assert(office.assign(36,talentFirst+5));
    assert(!office.available(talentFirst+1,36) && office.available(talentFirst+1,4));
    auto without=office;
    without.relieve(4);
    without.relieve(36);
    assert(office.income(false)-without.income(false)==22+(96-35)/5+(91-35)/5);
    assert(office.income(true)>without.income(true)+40);
    office.orders={};
    office.advance();
    assert(office.cities[10].order==86);
    checkSave(office);

    Campaign arms;
    arms.start(0);
    arms.talentSeen=arms.talentHired=(1u<<2)|(1u<<3)|(1u<<4);
    assert(arms.supplyCost(10,7,legion,talentFirst+2,false)==arms.supplyCost(10,7,legion,0,false)-22);
    const int column=arms.send(10,7,legion,talentFirst+3,false);
    assert(column>=0 && !arms.available(talentFirst+3,10));
    checkSave(arms);
    Battle fight;
    fight.start(arms,arms.columns[column].road,column,{horse,square,saber});
    assert(&fight.officer()==&talents[3].officer && fight.signal==64 && !fight.mayRally());
    for (int i=0;i<Battle::fieldLength && !fight.mayRally();++i) fight.step();
    assert(fight.rally());
    assert(fight.losses[1]==talents[3].officer.strength+35);
    arms.cities[11].owner=0;
    const auto ordinary=arms.searchOdds(11,0);
    const auto trained=arms.searchOdds(11,talentFirst+4);
    assert(trained.talent>ordinary.talent);
    const int market=linzi;
    arms.cities[market].owner=0;
    assert(arms.searchOdds(market,talentFirst+4).localItem>arms.searchOdds(market,0).localItem);
    checkSave(arms);
}

static void checkBattleOrders() {
    Campaign c;
    c.start(0);
    const int id=c.send(10,7,legion,0,false);
    assert(id>=0);
    Battle b;
    b.start(c,c.columns[id].road,id,{horse,wedge,saber});
    assert(!b.mayStrike() && b.moveGeneral(1,true) && !b.moveGeneral(3,true));
    assert(b.order(wings) && !b.order(wings) && b.plan.lineup[1]==horse);
    fightUntilRound(b,1);
    assert(b.generalHealth<100 && b.moveGeneral(1,false) && !b.mayStrike());
    assert(b.moveGeneral(1,true));
    for (int i=0;i<Battle::period && !b.mayStrike();++i) b.step();
    const int before=b.troops[1], signal=b.signal;
    assert(b.strike() && b.troops[1]<before && b.signal==signal-Battle::strikeCost && !b.strike());
    assert(b.moveGeneral(1,false) && b.direct(fallBack));
    const int health=b.generalHealth;
    const int position=b.fronts[0][1];
    fightUntilRound(b,b.round+1);
    assert(b.fronts[0][1]<position && b.generalHealth>health);
}

static void checkRetreatFacing() {
    Battle battle;
    battle.ranks[0]={100,100,100};
    battle.ranks[1]={100,100,100};
    battle.fronts[0]={410,410,410};
    battle.fronts[1]={450,450,450};
    battle.rankOrders[1]={fallBack,2};
    battle.enemyRank=1;
    battle.enemyManeuver=fallBack;
    assert(battle.rankFacing(0,0)==1 && battle.rankFacing(1,0)==-1);
    assert(battle.rankFacing(0,1)==-1 && battle.rankFacing(1,1)==1);
    battle.marchRanks();
    assert(battle.rankMotion[0][1]==-1 && battle.rankMotion[1][1]==1);

    battle.rankOrders[1]={};
    battle.enemyManeuver=measured;
    battle.marchRanks();
    assert(battle.rankFacing(0,1)==1 && battle.rankFacing(1,1)==-1);

    battle.routed[0][2]=battle.routed[1][2]=true;
    battle.marchRanks();
    assert(battle.rankMotion[0][2]==-1 && battle.rankMotion[1][2]==1);
    battle.fronts[0][2]=Battle::fieldMargin;
    battle.fronts[1][2]=Battle::fieldLength-Battle::fieldMargin;
    battle.marchRanks();
    assert(!battle.rankMotion[0][2] && !battle.rankMotion[1][2]);
    assert(battle.rankFacing(0,2)==-1 && battle.rankFacing(1,2)==1);

    battle.fronts[0][0]=540;
    battle.fronts[1][0]=490;
    battle.marchRanks();
    assert(battle.rankFacing(0,0)==-1 && battle.rankFacing(1,0)==1);
}

static void checkOrderPreview() {
    Campaign campaign;
    campaign.start(0,0,Difficulty::story);
    const int column=campaign.send(10,7,legion,0,false);
    assert(column>=0);
    Battle battle;
    battle.start(campaign,campaign.columns[column].road,column,{spear,square,longspear});
    const int initialSignal=battle.signal;
    const auto initialFronts=battle.fronts[0];
    const auto baseline=battle.previewClash();
    const auto global=battle.previewOrder(-1,press);
    const auto local=battle.previewOrder(0,hold);
    assert(global && global->cost==Battle::tacticsCost);
    assert(local && local->cost==Battle::rankOrderCost);
    assert(!battle.previewOrder(0,flank));
    assert(!battle.previewOrder(Battle::rankCount,press));
    assert(battle.signal==initialSignal && battle.fronts[0]==initialFronts && !battle.ticks);
    assert(battle.stance(0,0)==measured && battle.previewClash()==baseline);

    auto other=battle;
    assert(other.issueOrder(-1,press) && other.previewClash()==global->losses);
    assert(battle.issueOrder(0,hold) && battle.signal==initialSignal-local->cost);
    fightUntilRound(battle,1);
    assert(battle.rankLosses[0]==local->losses[0] && battle.rankLosses[1]==local->losses[1]);
    battle.paused=true;
    assert(!battle.previewOrder(-1,fallBack) && !battle.previewOrder(1,press));
}

static void checkDifficultyAndAI() {
    Campaign story;
    story.start(0,0,Difficulty::story);
    Campaign ordinary;
    ordinary.start(0);
    assert(story.gold>ordinary.gold && story.grain>ordinary.grain);
    auto localRecruitment=ordinary;
    localRecruitment.cities[42]={7,1670,1,1};
    localRecruitment.orders={};
    localRecruitment.advance();
    assert(localRecruitment.cities[42].troops==1680);
    localRecruitment.cities[42].troops=1900;
    localRecruitment.orders={};
    localRecruitment.advance();
    assert(localRecruitment.cities[42].troops==1900);
    for (const auto difficulty:{Difficulty::story,Difficulty::standard,Difficulty::veteran}) {
        Campaign state;
        state.start(0,0,difficulty);
        for (auto& city:state.cities) city={1,120,1,1};
        state.cities[10]={0,180,1,1};
        state.cities[43]={1,2000,1,1};
        state.turn=state.rules().graceSeasons;
        state.makeOrders();
        assert(!state.orders[1]);
        ++state.turn;
        state.makeOrders();
        assert(state.orders[1].from==43 && state.orders[1].to==10);
        state.truce[1]=1;
        state.makeOrders();
        assert(!state.orders[1]);

        state.truce[1]=0;
        state.cities[10].troops=1900;
        state.cities[43].troops=250;
        state.cities[9].troops=2050;
        const int transfer=state.rules().transferLimit;
        state.reinforceEnemyBorders({});
        assert(state.cities[43].troops==250+transfer);
        assert(state.cities[9].troops==2050-transfer);
        std::array<Order,ownerCount> pending{};
        pending[1]={9,10};
        state.reinforceEnemyBorders(pending);
        assert(state.cities[9].troops==2050-transfer && state.cities[43].troops==250+transfer);
    }

    Campaign veteran;
    veteran.start(0,0,Difficulty::veteran);
    checkSave(veteran);
    EighthSave previous{};
    writeLegacy(veteran,previous,8,previousCityCount);
    assert(migrate(previous,ordinary) && ordinary.difficulty==Difficulty::standard);
    assert(ordinary.cities[10].troops==veteran.cities[10].troops && ordinary.grain==veteran.grain);

    const int column=veteran.send(10,43,legion,0,false);
    assert(column>=0);
    Battle b;
    b.start(veteran,veteran.columns[column].road,column,{bow,square,longbow});
    assert(b.enemyFormation==square && b.difficulty==Difficulty::veteran);
    b.round=1;
    b.losses[1]=180;
    b.losses[0]=60;
    b.planEnemyOrder();
    assert(b.enemyManeuver==hold && !b.enemySkillQueued);
    b.round=3;
    b.enemySignal=70;
    b.planEnemyOrder();
    assert(b.enemySkillQueued && b.enemySignal<70);
    b.roundClock=Battle::period-1;
    b.step();
    assert(b.enemySkillUsed && b.enemySkillTicks>0 && !b.enemySkillQueued);
    b.round=9;
    b.enemySignal=100;
    b.planEnemyOrder();
    assert(!b.enemySkillQueued);
}

static void checkScoutingAndWithdrawal() {
    Campaign frontier;
    frontier.start(0,0,Difficulty::veteran);
    frontier.turn=3;
    frontier.cities[10].troops=650;
    frontier.cities[10].order=48;
    frontier.cities[43].troops=1100;
    frontier.orders={};
    frontier.orders[2]={43,10};
    const auto normal=frontier.forecastAttack(SeasonPlan::regular);
    const auto relief=frontier.forecastAttack(SeasonPlan::relief);
    assert(relief.road.from==43 && relief.road.to==10);
    assert(relief.attackers==normal.attackers && relief.defenders>normal.defenders);
    frontier.truce[2]=1;
    assert(!frontier.forecastAttack(SeasonPlan::regular).road);
    frontier.truce[2]=0;
    frontier.advance(SeasonPlan::relief);
    assert(frontier.defense.from==43 && frontier.defense.to==10);
    Battle defense;
    defense.start(frontier,frontier.defense,-1,{bow,square,longbow},0);
    assert(defense.initial[0]==relief.defenders && defense.initial[1]==relief.attackers);
    assert(defense.enemyRetreatCity==43);

    auto lastExchange=[](Battle& battle) {
        battle.round=4;
        battle.troops[1]=280;
        battle.formRanks(1);
        battle.morale[1]=35;
        battle.planEnemyOrder();
        assert(battle.enemyWithdrawalQueued && battle.enemyManeuver==fallBack);
        battle.roundClock=Battle::period-1;
        battle.step();
        assert(battle.enemyWithdrew && battle.result>0 && battle.troops[1]>0);
    };
    lastExchange(defense);
    const int attackingCity=frontier.cities[43].troops;
    const auto defenders=defense.settle(frontier);
    assert(!frontier.defense && frontier.cities[43].troops==attackingCity-defense.initial[1]+defense.troops[1]);
    assert(defenders.enemyReturned==defense.troops[1] && defenders.captive<0);

    Campaign siege;
    siege.start(0,0,Difficulty::veteran);
    siege.cities[43].troops=1000;
    siege.cities[9].troops=340;
    const int column=siege.send(10,43,legion,0,false);
    assert(column>=0);
    Battle attack;
    attack.start(siege,siege.columns[column].road,column,{bow,square,longbow});
    assert(attack.enemyRetreatCity==9);
    auto cutOff=siege;
    cutOff.cities[9].troops=armyLimit;
    Battle trapped;
    trapped.start(cutOff,cutOff.columns[column].road,column,{bow,square,longbow});
    assert(trapped.enemyRetreatCity<0);
    trapped.round=4;
    trapped.troops[1]=280;
    trapped.formRanks(1);
    trapped.morale[1]=35;
    trapped.planEnemyOrder();
    assert(!trapped.enemyWithdrawalQueued);
    lastExchange(attack);
    const auto captured=attack.settle(siege);
    assert(siege.own(43) && captured.captive<0 && !siege.columns[column]);
    assert(captured.enemyReturned==attack.troops[1] && siege.cities[9].troops==340+attack.troops[1]);
    checkSave(siege);
}

static void checkRankCommands() {
    Campaign campaign;
    campaign.start(0);
    campaign.cities[10].troops=2150;
    campaign.cities[43].troops=1600;
    const int column=campaign.send(10,43,legion,0,false);
    assert(column>=0);
    Battle normal;
    normal.start(campaign,campaign.columns[column].road,column,{bow,square,longbow});
    auto guarded=normal;
    assert(!guarded.directRank(0,flank));
    const int before=guarded.signal;
    assert(guarded.directRank(0,hold) && guarded.signal==before-Battle::rankOrderCost);
    assert(!guarded.directRank(0,hold) && guarded.stance(0,1)==measured);
    const auto forecast=guarded.previewClash();
    fightUntilRound(normal,1);
    fightUntilRound(guarded,1);
    assert(guarded.rankLosses[0]==forecast[0] && guarded.rankLosses[1]==forecast[1]);
    assert(guarded.rankLosses[0][0]<normal.rankLosses[0][0]);
    assert(guarded.rankLosses[0][1]==normal.rankLosses[0][1]);
    assert(guarded.fronts[0][0]==Battle::campFront && guarded.fronts[0][1]>Battle::campFront);
    assert(guarded.rankOrders[0].rounds==1);
    for (int i=0;i<Battle::period;++i) guarded.step();
    assert(guarded.stance(0,0)==measured);

    assert(!guarded.supportSize(0));
    auto reserves=normal;
    const auto ranks=reserves.ranks[0];
    const int donor=reserves.supportSource(0);
    const int sent=reserves.supportSize(0);
    const int total=reserves.troops[0];
    assert(sent>0 && donor==1 && reserves.supportRank(0));
    assert(reserves.ranks[0][donor]==ranks[donor]-sent &&
           reserves.ranks[0][0]==ranks[0]+sent && reserves.troops[0]==total);
    assert(!reserves.supportRank(2));
    assert(reserves.order(wedge) && reserves.ranks[0]==Battle::deployRanks(total,wedge));
    assert(reserves.directRank(2,press) && reserves.direct(hold) && reserves.stance(0,2)==hold);

    auto armed=normal;
    assert(armed.moveGeneral(2,false));
    const auto enemies=armed.ranks[1];
    assert(armed.strike());
    assert(armed.lastStrikeLane==2 && armed.ranks[1][2]<enemies[2]);
    assert(armed.ranks[1][0]==enemies[0] && armed.ranks[1][1]==enemies[1]);
    for (int side=0;side<2;++side) {
        const auto& force=armed.ranks[side];
        assert(force[0]+force[1]+force[2]==armed.troops[side]);
    }
}

static void checkBreakthrough() {
    Battle battle;
    battle.to=1;
    battle.difficulty=Difficulty::story;
    battle.morale[0]=62;
    battle.ranks[0]={100,100,100};
    battle.ranks[1]={1,100,100};
    battle.troops[0]=300;
    battle.troops[1]=201;
    const auto exchange=battle.rankAttacks(0,150);
    assert((exchange==Battle::Ranks{1,50,50}));
    --battle.troops[1];
    battle.ranks[1][0]=0;
    const auto exposed=battle.rankAttacks(0,150);
    assert((exposed==Battle::Ranks{0,100,50}));
    const auto flank=battle.rankAttacks(0,120);
    assert((flank==Battle::Ranks{0,86,40}));

    battle.difficulty=Difficulty::veteran;
    battle.ranks[0]={190,100,100};
    battle.ranks[1]={100,250,100};
    battle.troops[0]=390;
    battle.troops[1]=450;
    battle.round=2;
    battle.rankOrders[0]={press,1};
    battle.planEnemyOrder();
    assert(battle.enemyRank==0 && battle.stance(1,0)==hold);
    assert(battle.stance(1,1)==measured);
}

static void checkLongField() {
    Campaign campaign;
    campaign.start(0,0,Difficulty::story);
    const int column=campaign.send(10,7,legion,0,false);
    assert(column>=0);
    Battle battle;
    battle.start(campaign,campaign.columns[column].road,column,{spear,square,longspear});
    assert(battle.closestGap()>650 && battle.troopFor(1,1)==bow && !battle.inContact());
    assert(battle.directRank(0,press) && battle.directRank(2,hold));
    const auto opening=battle.ranks[0];
    for (int i=0;i<60;++i) battle.step();
    assert(!battle.round && battle.ranks[0]==opening);
    assert(battle.fronts[0][0]>battle.fronts[0][1] && battle.fronts[0][2]==Battle::campFront);
    assert(!battle.mayStrike());
    const auto preview=battle.previewClash();
    fightUntilRound(battle,1);
    assert(battle.ticks>=110 && battle.ticks<=170);
    assert(battle.rankLosses[0]==preview[0] && battle.rankLosses[1]==preview[1]);
    assert(battle.losses[0]>0 && battle.losses[1]==0);
    assert(battle.canReach(1,0) && !battle.canReach(0,0));
    assert(!battle.canReach(0,2) && !battle.canReach(1,2));
    const auto second=battle.previewClash();
    fightUntilRound(battle,2);
    assert(battle.rankLosses[0]==second[0] && battle.rankLosses[1]==second[1]);
    assert(battle.losses[1]>0);

    assert(battle.direct(hold));
    const int signal=battle.signal;
    assert(battle.direct(measured) && battle.signal==signal && !battle.maneuverRounds);
    assert(battle.stance(0,2)==measured && !battle.mayManeuver(measured));

    battle.round=0;
    battle.enemyLineup.fill(horse);
    battle.ranks[0]={100,0,0};
    battle.ranks[1]={0,0,100};
    battle.troops[0]=battle.troops[1]=100;
    const auto isolated=battle.previewClash();
    assert(isolated[0][0]+isolated[1][2]>0);
    fightUntilRound(battle,1);
}

static void checkMixedForcesAndSiege() {
    Campaign campaign;
    campaign.start(0,0,Difficulty::story);
    const int column=campaign.send(10,7,legion,0,false);
    assert(column>=0);
    const auto order=campaign.columns[column].road;
    BattlePlan plan{spear,square,longbow};
    plan.lineup={shields,crossbow,heavyHorse};
    plan.engine=ram;
    plan.engineLane=0;
    Battle battle;
    battle.start(campaign,order,column,plan);
    assert(battle.range(0,1)>battle.range(0,0) && battle.troopFor(0,2)==heavyHorse);
    assert(Battle::advantage(bow,shields)<Battle::advantage(bow,spear));
    assert(battle.engineShots==2 && battle.wallPoints==4 && !battle.mayFireEngine());
    const auto original=battle.ranks[0];
    assert(battle.order(wedge) && battle.ranks[0]==original);
    assert(!battle.supportSize(0));
    auto reserves=battle;
    reserves.ranks[0][0]=0;
    reserves.troops[0]-=original[0];
    assert(reserves.supportRank(0) && reserves.troopFor(0,0)==crossbow);
    assert(reserves.fronts[0][0]==reserves.fronts[0][1]);

    for (int i=0;i<Battle::fieldLength && !battle.result && !battle.mayFireEngine();++i) battle.step();
    const int troops=battle.troops[1];
    assert(battle.fireEngine() && battle.engineShots==1 && battle.wallPoints==2 &&
           battle.troops[1]==troops && !battle.fireEngine());
    for (int i=0;i<Battle::fieldLength && !battle.result && !battle.mayFireEngine();++i) battle.step();
    assert(battle.fireEngine() && !battle.wallPoints && !battle.engineShots && !battle.mayFireEngine());

    plan.engine=stoneThrower;
    plan.engineLane=1;
    Battle stones;
    stones.start(campaign,order,column,plan);
    for (int i=0;i<Battle::fieldLength && !stones.mayFireEngine();++i) stones.step();
    const int middle=stones.ranks[1][1];
    assert(stones.fireEngine() && stones.wallPoints==3 && stones.engineShots==2);
    assert(stones.ranks[1][1]<middle && !stones.fireEngine());

    Battle supported=stones;
    supported.fronts[0][1]=500;
    supported.fronts[1]={800,800,605};
    assert(supported.attackTarget(0,1)==2);
    assert(supported.forecastClash()[1][2]>0);
    assert(!Battle::mayBringEngine(1,false,false) && !Battle::mayBringEngine(3,true,false));
    assert(!Battle::mayBringEngine(2,false,true));
}

static void checkCoordinateLanes() {
    Battle battle;
    battle.to=1;
    battle.signal=52;
    battle.plan.lineup={shields,crossbow,horse};
    battle.enemyLineup={bow,bow,bow};
    battle.fronts[0]={400,400,400};
    battle.fronts[1]={447,447,447};
    battle.ranks[0]={300,300,300};
    battle.ranks[1]={300,300,300};
    battle.troops[0]=battle.initial[0]=900;
    battle.troops[1]=battle.initial[1]=900;
    const auto ordinary=battle.forecastClash();

    auto covered=battle;
    assert(covered.directAid(0,1) && covered.shieldCover() && covered.aid.rounds==2);
    assert(covered.signal==52-Battle::supportCost && covered.ranks[0]==battle.ranks[0]);
    assert(!covered.mayAid(2,1) && !covered.supportRank(1));
    const auto guarded=covered.forecastClash();
    assert(guarded[0][1]<ordinary[0][1] && guarded[0][0]==ordinary[0][0]);
    assert(guarded[1][0]<ordinary[1][0]);
    const auto projected=covered.previewClash();
    fightUntilRound(covered,1);
    assert(covered.rankLosses[0]==projected[0] && covered.aid.rounds==1);
    fightUntilRound(covered,2);
    assert(!covered.coordinating() && covered.aid.source==-1);

    assert(!battle.mayAid(0,2) && !battle.mayAid(1,1));
    assert(battle.attackTarget(0,1)==1 && battle.directAid(1,0));
    assert(battle.attackTarget(0,1)==0);
    const auto focused=battle.forecastClash();
    assert(focused[1][0]>ordinary[1][0] && focused[1][1]<ordinary[1][1]);
    battle.fronts[0][0]=140;
    assert(!battle.coordinating() && battle.attackTarget(0,1)==1);
}

static void checkEnemyCoordinationAndSortie() {
    Battle field;
    field.to=1;
    field.round=1;
    field.plan.lineup={bow,horse,spear};
    field.enemyLineup={shields,bow,spear};
    field.fronts[0]={400,400,400};
    field.fronts[1]={447,447,447};
    field.ranks[0]={300,300,300};
    field.ranks[1]={350,150,300};
    field.troops[0]=field.initial[0]=900;
    field.troops[1]=field.initial[1]=800;
    auto novice=field;
    novice.difficulty=Difficulty::story;
    novice.planEnemyOrder();
    assert(!novice.enemyAid.rounds && novice.enemyManeuver==measured);

    field.difficulty=Difficulty::veteran;
    const auto before=field.previewClash();
    field.planEnemyOrder();
    assert(field.shieldCover(1) && field.enemyAid.source==0 && field.enemyAid.target==1);
    assert(field.enemySignal==22-Battle::supportCost && field.nextEnemyOrder==3);
    const auto covered=field.previewClash();
    assert(covered[1][1]<before[1][1] && covered[1][2]==before[1][2]);
    fightUntilRound(field,2);
    assert(field.rankLosses[1]==covered[1] && field.enemyAid.rounds==1);

    auto siege=novice;
    siege.difficulty=Difficulty::veteran;
    siege.plan.engine=stoneThrower;
    siege.plan.engineLane=1;
    siege.plan.lineup[1]=spear;
    siege.enemyLineup[1]=spear;
    siege.engineShots=3;
    siege.wallPoints=4;
    siege.ranks[1]={250,300,250};
    siege.fronts[0]={570,570,570};
    siege.fronts[1]={642,642,642};
    auto ordinary=siege;
    ordinary.difficulty=Difficulty::standard;
    ordinary.planEnemyOrder();
    assert(!ordinary.enemySiegeSortie);
    auto depleted=siege;
    depleted.engineShots=0;
    depleted.planEnemyOrder();
    assert(!depleted.enemySiegeSortie);
    siege.planEnemyOrder();
    assert(siege.enemySiegeSortie && siege.enemyRank==1);
    assert(siege.stance(1,1)==press && siege.stance(1,0)==measured);
    const int position=siege.fronts[1][1];
    siege.step();
    assert(siege.fronts[1][1]<position);
}

static void checkPrisonCamps() {
    Campaign c;
    c.start(0);
    c.gold=c.grain=1000;
    c.tours=3;
    const int source=c.capital(), target=7;
    const int deployed=c.send(source,target,legion,0,false);
    assert(deployed>=0);
    Battle lost;
    lost.start(c,{source,target},deployed,{});
    lost.result=-1;
    auto safeRetreat=c;
    auto limited=lost.settle(safeRetreat);
    assert(!limited.ourPrisoners && limited.ourCaptive<0 && !limited.dispersed);
    lost.troops[0]=320;
    lost.morale[0]=10;
    const auto defeat=lost.settle(c);
    assert(defeat.ourPrisoners==160 && defeat.ourCaptive==0 && !defeat.dispersed);
    assert(c.cities[target].prisoners==160 && c.held(0) && c.commander!=0);
    assert(!c.available(0) && !c.assign(source,0) && !c.drill(0));
    checkSave(c);

    auto bailed=c;
    const int fee=bailed.gold;
    assert(bailed.ransomGeneral(0) && !bailed.held(0));
    assert(bailed.gold==fee-Campaign::friendlyRansom && bailed.cities[target].prisoners==160);
    checkSave(bailed);

    auto breakout=c;
    breakout.turn+=4;
    assert(breakout.updatePrisoners().returned==1 && !breakout.held(0));
    auto fleeing=c;
    while (fleeing.roll(target,5)>=5) ++fleeing.turn;
    const auto troopNews=fleeing.updatePrisoners();
    assert(troopNews.troopsReturned==32 && fleeing.cities[target].prisoners==128);
    const int relief=c.send(source,target,legion,c.commander,false);
    assert(relief>=0);
    Battle won;
    won.start(c,{source,target},relief,{});
    won.result=1;
    won.troops[1]=240;
    won.morale[1]=0;
    const auto victory=won.settle(c);
    assert(victory.rescuedGenerals==1 && victory.rescuedTroops==160 && !c.held(0));
    assert(victory.enemyPrisoners==120 && victory.captive==0 && c.cities[target].prisoners==120);
    const int second=c.capture(1,(won.enemyIndex+1)%officerCount,target);
    assert(second==1 && c.capture(1,won.enemyIndex,target)<0);
    checkSave(c);
    c.actions=2;
    const int troops=c.cities[target].troops, grain=c.grain;
    assert(c.enlistPrisoners(target));
    assert(c.cities[target].troops==troops+120 && !c.cities[target].prisoners && c.grain==grain-60);
    assert(c.holdTroops(target,67)==67 && c.releasePrisoners(target) && !c.cities[target].prisoners);

    while (c.roll(8+won.enemyIndex,1)<c.admitChance(0)) ++c.turn;
    const int offerGold=c.gold;
    assert(c.admit(0)==Offer::refused && c.gold==offerGold-55 && c.prisoners[0]);
    assert(c.admit(0)==Offer::unavailable);
    Campaign restored;
    assert(decode(encode(c),restored) && !restored.mayAdmit(0));
    auto weak=c,strong=c;
    weak.cities[target].fort=1;
    weak.cities[target].order=30;
    strong.cities[target].fort=4;
    strong.cities[target].order=100;
    assert(weak.escapeChance(0)>strong.escapeChance(0));
    while (weak.roll(8+won.enemyIndex,2)>=weak.escapeChance(0)) ++weak.turn;
    assert(weak.updatePrisoners().escaped>=1 && !weak.prisoners[0]);
    for (int n=0;n<guestCount;++n) {
        const int index=n<2?n:c.capture(2,0,target);
        auto& captive=c.prisoners[index];
        while (captive.offered==c.turn || c.roll(captive.side*officerCount+captive.officer,1)>=c.admitChance(index))
            ++c.turn;
        assert(c.admit(index)==Offer::joined && c.unlocked(guestCommand(n)));
    }
    assert(c.freeGuest()==-1);
    checkSave(c);

    Campaign scouts;
    scouts.start(0);
    scouts.tours=2;
    scouts.gold=2000;
    const int home=talents[legacyTalentCount].city[0];
    scouts.cities[home].owner=0;
    while (scouts.roll(home,6)>=scouts.searchOdds(home,0).talent) ++scouts.turn;
    const auto discovery=scouts.search(home,0);
    assert(discovery.kind==talentFound && discovery.value==legacyTalentCount);
    const int id=legacyTalentCount;
    while (scouts.roll(id,4)<scouts.welcomeChance(home,id)) ++scouts.turn;
    assert(scouts.welcome(home,id)==Offer::refused && !scouts.canWelcome(home,id));
    while (scouts.roll(3,0)<scouts.hireChance(3)) ++scouts.turn;
    assert(scouts.hire(3)==Offer::refused && !scouts.mayHire(3));
    checkSave(scouts);
    scouts.orders={};
    scouts.advance();
    assert(scouts.canWelcome(home,id) && scouts.mayHire(3));
    while (scouts.roll(id,4)>=scouts.welcomeChance(home,id)) ++scouts.turn;
    assert(scouts.welcome(home,id)==Offer::joined && scouts.available(talentFirst+id));
    checkSave(scouts);
}

static void checkOpeningAndRoles() {
    Campaign c;
    c.start(0);
    c.talentSeen=c.talentHired=7u<<previousTalentCount;
    const int strategistSlot=talentSlot(12), guardSlot=talentSlot(13), archerSlot=talentSlot(14);
    assert(battleRole(c.leader(strategistSlot))==Role::strategist);
    assert(battleRole(c.leader(guardSlot))==Role::marshal);
    assert(battleRole(c.leader(archerSlot))==Role::vanguard);
    assert(guestIndex(strategistSlot)<0 && talentAtSlot(guestCommand(2))==nullptr);
    c.commander=archerSlot;
    c.generals[archerSlot].xp=135;
    checkSave(c);
    const int column=c.send(10,7,legion,strategistSlot,false);
    assert(column>=0);
    Battle opening;
    opening.start(c,{10,7},column,{bow,square,longbow});
    opening.prepare();
    const auto ours=opening.fronts[0], theirs=opening.fronts[1];
    const auto initialSignal=opening.signal;
    for (int i=0;i<120;++i) opening.step();
    assert(!opening.ticks && !opening.round && opening.fronts[0]==ours &&
           opening.fronts[1]==theirs && opening.signal==initialSignal && !opening.mayRally());
    assert(opening.launch(true) && !opening.launch(false));
    for (int i=0;i<40;++i) opening.step();
    assert(opening.fronts[0]==ours && opening.fronts[1][1]<theirs[1]);
    assert(opening.direct(measured) && !opening.anchored);
    opening.step();
    assert(opening.fronts[0][1]>ours[1] && opening.anchor());

    opening.fronts[0]={410,410,410};
    opening.fronts[1]={502,502,502};
    opening.enemyLineup={bow,bow,bow};
    opening.enemyAid={1,0,2};
    opening.round=2;
    opening.signal=100;
    opening.atFront=true;
    auto original=opening;
    opening.plan.weapon=saber;
    assert(!opening.mayDuel() && !opening.mayStrike());
    opening.plan.weapon=longbow;
    assert(opening.mayStrike() && opening.rally() && opening.suppressRounds==2 && !opening.enemyAid.rounds);
    const auto weakened=opening.forecastClash()[0];
    const auto previous=original.forecastClash()[0];
    assert(weakened[0]+weakened[1]+weakened[2]<previous[0]+previous[1]+previous[2]);

    opening.general=&c.leader(guardSlot);
    opening.usedSkill=false;
    opening.morale[0]=17;
    opening.ranks[0]={70,400,410};
    opening.troops[0]=880;
    opening.resolveRouts(0);
    assert(opening.routed[0][0] && !opening.routed[0][1] &&
           opening.fled[0]==10 && opening.troops[0]==870);
    assert(opening.attackTarget(0,0)<0 && !opening.mayDirectRank(0,press));
    const int wing=opening.fronts[0][0];
    opening.marchRanks();
    assert(opening.fronts[0][0]<wing);
    assert(opening.rally() && !opening.routed[0][0] && opening.morale[0]>=42 && opening.shield==2);
    assert(opening.fled[0]==10 && opening.troops[0]==870);

    opening.general=&c.leader(archerSlot);
    opening.usedSkill=false;
    opening.signal=100;
    opening.generalLane=2;
    const auto before=opening.ranks[1];
    assert(opening.rally() && opening.fireLane==2 && opening.fireRounds==2);
    assert(opening.ranks[1][0]==before[0] && opening.ranks[1][1]==before[1] &&
           before[2]-opening.ranks[1][2]==170);
}

static void checkSignatureEquipment() {
    Campaign c;
    c.start(0);
    assert(c.unlocked(rulerSlot) && &c.leader(rulerSlot)==&rulers[0]);
    assert(c.generals[1].kit[armsSlot]==dragonBlade && c.generals[2].kit[armsSlot]==snakeSpear);
    c.gold=1800;
    c.actions=4;
    const int total=c.gold;
    assert(c.buyGear(scaleArmor) && c.buyGear(redHare) && c.buyGear(featherFan));
    assert(c.actions==1 && c.gold==total-gearSpecs[scaleArmor].price-gearSpecs[redHare].price-
           gearSpecs[featherFan].price);
    for (const auto item:{scaleArmor,redHare,featherFan}) assert(c.equip(rulerSlot,item));
    const auto& kit=c.generals[rulerSlot].kit;
    assert(kit[armsSlot]==twinJian && kit[armorSlot]==scaleArmor &&
           kit[mountSlot]==redHare && kit[strategySlot]==featherFan);
    auto removed=c;
    assert(removed.unequip(rulerSlot,armorSlot) && removed.stock[scaleArmor]==1 &&
           removed.generals[rulerSlot].kit[armsSlot]==twinJian);
    assert(c.supplyCost(10,7,legion,rulerSlot,false)<removed.supplyCost(10,7,legion,0,false));
    c.commander=rulerSlot;
    checkSave(c);
    const int column=c.send(10,7,legion,rulerSlot,false);
    assert(column>=0 && !c.unequip(rulerSlot,mountSlot));
    Battle oath;
    oath.start(c,{10,7},column,{bow,square,longbow});
    assert(oath.plan.weapon==saber && oath.personalSkillCost()==40 && oath.signal==64);
    oath.morale[0]=19;
    oath.routed[0][0]=true;
    assert(oath.rally() && oath.morale[0]==46 && !oath.routed[0][0] && oath.shield==1);
    oath.fronts[0]={370,370,370};
    oath.fronts[1]={505,505,505};
    oath.enemyLineup={bow,bow,bow};
    oath.shield=0;
    const auto protectedRanks=oath.forecastClash()[0];
    oath.bodyArmor=noGear;
    const auto unprotected=oath.forecastClash()[0];
    assert(protectedRanks[1]<unprotected[1]);
    oath.atFront=true;
    oath.signal=100;
    assert(oath.direct(press));
    auto onFoot=oath;
    onFoot.mount=noGear;
    onFoot.marchRanks();
    oath.marchRanks();
    assert(oath.fronts[0][1]==onFoot.fronts[0][1]+1 && oath.fronts[0][0]==onFoot.fronts[0][0]);

    oath.general=&officers[0][1];
    oath.usedSkill=false;
    oath.signal=100;
    oath.fronts[0]={449,449,449};
    oath.fronts[1]={503,503,503};
    const auto enemy=oath.ranks[1];
    assert(oath.rally() && oath.ranks[1][0]<enemy[0] &&
           oath.ranks[1][1]<enemy[1] && oath.ranks[1][2]<enemy[2]);
    oath.general=&officers[0][3];
    oath.usedSkill=false;
    oath.signal=70;
    oath.enemyAid={1,0,2};
    assert(oath.rally() && oath.shield==2 && !oath.enemyAid.rounds && oath.nextEnemyOrder>=2);
}

static void checkRetreatAccounting() {
    Campaign campaign;
    campaign.start(0);
    constexpr int from=10,to=7,refuge=9;
    const int column=campaign.send(from,to,legion,0,false);
    assert(column>=0);
    Battle battle;
    battle.start(campaign,{from,to},column,{});
    battle.result=1;
    battle.troops[1]=340;
    battle.fled[1]=75;
    const auto cutOff=battle;
    battle.enemyRetreatCity=refuge;

    auto emptyCamp=campaign;
    emptyCamp.cities[refuge].owner=battle.enemy;
    emptyCamp.cities[refuge].troops=armyLimit-370;
    const auto ample=battle.settle(emptyCamp);
    assert(ample.enemyReturned==340 && !ample.enemyDispersed && emptyCamp.cities[refuge].troops==armyLimit);

    campaign.cities[refuge].owner=battle.enemy;
    campaign.cities[refuge].troops=armyLimit-120;
    auto noRefuge=campaign;
    noRefuge.cities[refuge].owner=campaign.player;
    auto isolated=cutOff;
    isolated.enemyRetreatCity=-1;
    const auto isolatedResult=isolated.settle(noRefuge);
    assert(!isolatedResult.enemyReturned && isolatedResult.enemyDispersed==340);

    const auto crowded=battle.settle(campaign);
    assert(crowded.enemyReturned==120 && crowded.enemyDispersed==220);
    assert(campaign.cities[refuge].troops==armyLimit);
}

static void checkHistoricalExpansion() {
    const auto name=[](const char* value) {return std::string_view(value);};
    assert(cityCount==64 && name(cityLabel(8,211))=="秣陵" && name(cityLabel(8,212))=="建业");
    assert(name(cityLabel(43,221,0))=="鄂县" && name(cityLabel(43,221,1))=="武昌");
    assert(name(cityLabel(52,222,0))=="白帝城" && name(cityLabel(52,222,1))=="永安");
    assert(name(cityDistrict(shangyong,219,2))=="上庸郡" && name(cityDistrict(shangyong,221))=="新城郡");
    assert(name(historicalEra(1,220,1)->name)=="延康" && name(historicalEra(1,220,3)->name)=="黄初");
    assert(!historicalEra(0,221,0) && name(historicalEra(0,221,1)->name)=="章武");
    assert(name(historicalEra(0,223,1)->name)=="建兴" && name(historicalEra(2,229,1)->name)=="黄龙");
    assert(adjacent(6,fancheng) && adjacent(shangyong,zigui) && adjacent(23,ruxu));
    for (int i=1;i<chronicleCount;++i)
        assert(reachedDate(chronicle[i].year,chronicle[i].season,chronicle[i-1].year,chronicle[i-1].season));
    assert(latestChronicle(208,2)<0 && latestChronicle(228,0,qishan)>=0);

    Campaign c;
    c.start(0,2);
    const int entry=latestChronicle(c.year(),c.season(),shangyong);
    assert(c.own(shangyong) && entry>=0);
    const int gold=c.gold, actions=c.actions, order=c.cities[shangyong].order;
    const auto deed=chronicle[entry].deed;
    assert(c.dispatchDeed(entry) && !c.dispatchDeed(entry));
    assert(c.gold==gold-deed.gold && c.actions==actions-1 &&
           c.cities[shangyong].order==order+deed.gainOrder);
    Campaign restored;
    assert(decode(encode(c),restored) && !restored.canDispatchDeed(entry));

    c.cities[qishan].owner=0;
    c.actions=2;
    assert(c.canSearch(qishan,0));
    const int specialty=c.searchOdds(qishan,0).localItem;
    assert(specialty>0 && c.searchOdds(2,0).localItem!=specialty);
    while (c.roll(qishan,6)>=specialty) ++c.turn;
    const auto found=c.search(qishan,0);
    assert(found.kind==gearFound && found.value==manual);
    assert(c.alreadySearched(qishan) && !c.canSearch(qishan,0));
    checkSave(c);
}

int main() {
    checkMigration();
    checkHistoricalExpansion();
    checkSearchAndTalents();
    checkSeasonAndRegionalGoals();
    checkAlliedTransit();
    checkAlliedOffensive();
    checkDecisiveAssault();
    checkCivicAndColumns();
    checkFiefManagement();
    checkBattleAndCaptive();
    checkPrisonCamps();
    checkBattleOrders();
    checkRetreatFacing();
    checkOrderPreview();
    checkDifficultyAndAI();
    checkScoutingAndWithdrawal();
    checkRankCommands();
    checkBreakthrough();
    checkLongField();
    checkMixedForcesAndSiege();
    checkCoordinateLanes();
    checkEnemyCoordinationAndSortie();
    checkOpeningAndRoles();
    checkSignatureEquipment();
    checkRetreatAccounting();
    for (auto difficulty:{Difficulty::story,Difficulty::standard,Difficulty::veteran})
        for (int chapter=0;chapter<chapterCount;++chapter)
            for (int side=0;side<3;++side) {
                auto outcome=play(side,chapter,difficulty);
                std::cout<<int(difficulty)<<' '<<chapters[chapter].name<<' '<<factions[side].title<<' '
                         <<outcome.holdings(side)<<" cities, season "<<outcome.turn<<std::endl;
                assert(outcome.won==1 && outcome.holdings(side)==cityCount);
            }
}
