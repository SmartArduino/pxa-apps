#include "battle.hpp"
#include "save.hpp"
#include <cassert>
#include <iostream>

using namespace han;

static BattlePlan planFor(const Campaign& c,Order order,int column) {
    const int enemy=c.cities[column<0?order.from:order.to].owner;
    const Officer* lead=enemy<3?&officers[enemy][c.enemyOfficer(enemy,order.to)]:nullptr;
    const auto troop=counterTroop(Battle::enemyUnit(order.to,enemy,lead));
    return {troop,wedge,longbow};
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

static Campaign play(int side,int chapter,Difficulty difficulty=Difficulty::standard) {
    Campaign c;
    c.start(side,chapter,difficulty);
    assert(c.own(c.capital()) && c.year()==chapters[chapter].year);
    for (int attempt=0;!c.won && attempt<24*lastTurn;++attempt) {
        if (c.prisoner) {
            if (c.mayAdmit()) assert(c.admit());
            else c.disposeCaptive(true);
        }
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
    auto newest=encode(current);
    ThirdSave third{};
    const auto writeLegacy=[&](auto& older,int version,int cities) {
        constexpr size_t baseHeader=18*2;
        constexpr size_t newHeader=baseHeader+4;
        const size_t header=baseHeader+(version>=4?2:0);
        std::copy_n(newest.begin(),header,older.begin());
        older[2]=std::byte(version);
        std::copy_n(newest.begin()+newHeader,cities*6,older.begin()+header);
        const auto body=header+cities*6;
        std::copy_n(newest.begin()+newHeader+cityCount*6,older.size()-body-2,older.begin()+body);
        const auto sum=checksum(older,older.size()-2);
        older[older.size()-2]=std::byte(sum&255);
        older.back()=std::byte(sum>>8);
    };
    writeLegacy(third,3,oldCityCount);
    assert(migrate(third,c) && c.chapter==2 && c.own(32) && c.cities[37].owner==7 &&
           c.surveyed==(uint64_t(1)<<31));
    assert(c.canSearch(32,0));
    c.search(32,0);
    assert(!c.canSearch(32,0) && (c.surveyed>>32)==1);
    checkSave(c);

    current.surveyed=uint64_t(1)<<37;
    current.cities[37].troops=432;
    newest=encode(current);
    FourthSave fourth{};
    writeLegacy(fourth,4,expandedCityCount);
    assert(migrate(fourth,c) && c.chapter==2 && c.cities[37].troops==432 &&
           c.surveyed==(uint64_t(1)<<37) && c.cities[40].owner==7 && c.own(47));
    c.surveyed|=uint64_t(1)<<47;
    checkSave(c);

    c.regionalRewards=7;
    newest=encode(c);
    FifthSave fifth{};
    writeLegacy(fifth,5,seventhCityCount);
    Save stored{};
    std::copy(fifth.begin(),fifth.end(),stored.begin());
    assert(!restoreSave(stored,fifth.size()-1,c));
    assert(restoreSave(stored,fifth.size(),c) && c.regionalRewards==0 && c.own(47) && c.cities[37].troops==432 &&
           c.surveyed==((uint64_t(1)<<37)|(uint64_t(1)<<47)));

    SixthSave sixth{};
    writeLegacy(sixth,6,seventhCityCount);
    std::copy(sixth.begin(),sixth.end(),stored.begin());
    assert(restoreSave(stored,sixth.size(),c) && c.regionalRewards==7 && c.talentHired==0 &&
           c.own(47) && c.cities[37].troops==432);
    SeventhSave seventh{};
    writeLegacy(seventh,7,seventhCityCount);
    std::copy(seventh.begin(),seventh.end(),stored.begin());
    assert(restoreSave(stored,seventh.size(),c) && c.own(47) && c.cities[37].troops==432);
    for (int i=seventhCityCount;i<cityCount;++i) {
        const auto original=Campaign::initialCity(i,c.chapter);
        assert(c.cities[i].owner==original.owner && c.cities[i].troops==original.troops);
        assert(std::any_of(std::begin(roads),std::end(roads),[&](auto& r){return r.from==i||r.to==i;}));
    }
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
    saved[SixthSave{}.size()+6*(cityCount-seventhCityCount)-1]=std::byte{1};
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
    for (int i=0;i<=Battle::approach;++i) b.step();
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
    assert(c.own(7) && r.captured && c.prisoner && c.prisoner.side==1);
    assert(c.admit() && c.unlocked(guestSlot) && c.available(guestSlot));
    c.commander=guestSlot;
    assert(c.leader(guestSlot).name==officers[c.guest.side][c.guest.officer].name);
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
            assert(restored.welcome(city,side) && !restored.welcome(city,side));
            assert(restored.gold==opening.gold-talents[side].fee && restored.actions==opening.actions);
            assert(restored.unlocked(talentFirst+side));
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
    assert(&fight.officer()==&talents[3].officer && fight.signal==64 && fight.rally());
    assert(fight.losses[1]==talents[3].officer.strength+35);
    arms.cities[11].owner=0;
    auto ordinary=arms;
    assert(ordinary.search(11,0).kind==moneyFound);
    assert(arms.search(11,talentFirst+4).kind==gearFound);
    checkSave(arms);
}

static void checkBattleOrders() {
    Campaign c;
    c.start(0);
    const int id=c.send(10,7,legion,0,false);
    assert(id>=0);
    Battle b;
    b.start(c,c.columns[id].road,id,{horse,wedge,saber});
    assert(!b.mayStrike() && b.moveGeneral(0,true) && !b.moveGeneral(3,true));
    assert(b.order(wings) && !b.order(wings) && b.plan.troop==horse);
    while (!b.round) b.step();
    assert(b.generalHealth<100 && b.moveGeneral(1,false) && !b.mayStrike());
    assert(b.moveGeneral(1,true));
    const int before=b.troops[1], signal=b.signal;
    assert(b.strike() && b.troops[1]<before && b.signal==signal-Battle::strikeCost && !b.strike());
    assert(b.moveGeneral(1,false) && b.direct(fallBack));
    const int health=b.generalHealth;
    while (b.round<2 && !b.result) b.step();
    assert(b.rankAdvance[0][1]<0 && b.generalHealth>health);
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
    auto stored=encode(veteran);
    EighthSave previous{};
    std::copy_n(stored.begin(),previous.size()-2,previous.begin());
    previous[2]=std::byte{8};
    const auto sum=checksum(previous,previous.size()-2);
    previous[previous.size()-2]=std::byte(sum&255);
    previous.back()=std::byte(sum>>8);
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
    b.ticks=Battle::approach+3*Battle::period-1;
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
        battle.ticks=Battle::approach+4*Battle::period-1;
        battle.step();
        assert(battle.enemyWithdrew && battle.result>0 && battle.troops[1]>0);
    };
    lastExchange(defense);
    const int attackingCity=frontier.cities[43].troops;
    const auto defenders=defense.settle(frontier);
    assert(!frontier.defense && frontier.cities[43].troops==attackingCity-defense.initial[1]+defense.troops[1]);
    assert(defenders.enemyReturned==defense.troops[1] && !frontier.prisoner);

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
    assert(siege.own(43) && !siege.prisoner && !siege.columns[column]);
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
    const auto forecast=guarded.forecastClash();
    for (int i=0;i<Battle::approach;++i) {normal.step();guarded.step();}
    assert(guarded.rankLosses[0]==forecast[0] && guarded.rankLosses[1]==forecast[1]);
    assert(guarded.rankLosses[0][0]<normal.rankLosses[0][0]);
    assert(guarded.rankLosses[0][1]==normal.rankLosses[0][1]);
    assert(guarded.rankAdvance[0][0]<0 && guarded.rankAdvance[0][1]==0);
    assert(guarded.rankOrders[0].rounds==1);
    for (int i=0;i<Battle::period;++i) guarded.step();
    assert(guarded.stance(0,0)==measured);

    const auto ranks=guarded.ranks[0];
    const int donor=guarded.supportSource(0);
    const int sent=guarded.supportSize(0);
    const int total=guarded.troops[0];
    assert(sent>0 && donor==1 && guarded.supportRank(0));
    assert(guarded.ranks[0][donor]==ranks[donor]-sent &&
           guarded.ranks[0][0]==ranks[0]+sent && guarded.troops[0]==total);
    assert(!guarded.supportRank(2));
    assert(guarded.order(wedge) && guarded.ranks[0]==Battle::deployRanks(total,wedge));
    assert(guarded.directRank(2,press) && guarded.direct(hold) && guarded.stance(0,2)==hold);

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

int main() {
    checkMigration();
    checkSearchAndTalents();
    checkSeasonAndRegionalGoals();
    checkCivicAndColumns();
    checkBattleAndCaptive();
    checkBattleOrders();
    checkDifficultyAndAI();
    checkScoutingAndWithdrawal();
    checkRankCommands();
    checkBreakthrough();
    for (auto difficulty:{Difficulty::story,Difficulty::standard,Difficulty::veteran})
        for (int chapter=0;chapter<chapterCount;++chapter)
            for (int side=0;side<3;++side) {
                auto outcome=play(side,chapter,difficulty);
                std::cout<<int(difficulty)<<' '<<chapters[chapter].name<<' '<<factions[side].title<<' '
                         <<outcome.holdings(side)<<" cities, season "<<outcome.turn<<std::endl;
                assert(outcome.won==1 && outcome.holdings(side)==cityCount);
            }
}
