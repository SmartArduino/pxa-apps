#include "battle.hpp"
#include "save.hpp"
#include <cassert>
#include <iostream>

using namespace han;

static Formation counter(Formation f) {
    return Formation((f+1)%3);
}

static Settlement resolve(Campaign& campaign, Order order,int column) {
    Battle battle;
    const int guard=campaign.fieldLeader(campaign.commander,order.to);
    battle.start(campaign,order,column,spear,guard);
    for (int i=0;i<1100 && !battle.result;++i) {
        if (battle.duel.ready()) {
            assert(battle.duelOrder(DuelMove((battle.duelIntent()+1)%3)));
        } else if (!battle.duel.visible()) {
            battle.order(counter(battle.intent));
            if (battle.morale[1]<62) battle.rally();
            if (battle.round==1 && battle.morale[1]>42 && battle.signal>=Battle::duelCost)
                battle.challenge();
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

static Campaign play(int side,int chapter) {
    Campaign c;
    c.start(side,chapter);
    assert(c.own(c.capital()) && c.year()==chapters[chapter].year);
    for (int attempt=0;!c.won && attempt<440;++attempt) {
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
    constexpr size_t header=18*2;
    constexpr size_t legacyCities=oldCityCount*6;
    constexpr size_t preRegionalEnd=FifthSave{}.size()-2;
    std::copy_n(newest.begin(),header,third.begin());
    third[2]=std::byte{3};
    std::copy_n(newest.begin()+header+2,legacyCities,third.begin()+header);
    std::copy(newest.begin()+header+2+cityCount*6,newest.begin()+preRegionalEnd,
              third.begin()+header+legacyCities);
    const auto sum=checksum(third,third.size()-2);
    third[third.size()-2]=std::byte(sum&255);
    third.back()=std::byte(sum>>8);
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
    constexpr size_t expandedHeader=header+2;
    std::copy_n(newest.begin(),expandedHeader+expandedCityCount*6,fourth.begin());
    fourth[2]=std::byte{4};
    std::copy(newest.begin()+expandedHeader+cityCount*6,newest.begin()+preRegionalEnd,
              fourth.begin()+expandedHeader+expandedCityCount*6);
    const auto fourthSum=checksum(fourth,fourth.size()-2);
    fourth[fourth.size()-2]=std::byte(fourthSum&255);
    fourth.back()=std::byte(fourthSum>>8);
    assert(migrate(fourth,c) && c.chapter==2 && c.cities[37].troops==432 &&
           c.surveyed==(uint64_t(1)<<37) && c.cities[40].owner==7 && c.own(47));
    c.surveyed|=uint64_t(1)<<47;
    checkSave(c);

    newest=encode(c);
    FifthSave fifth{};
    std::copy_n(newest.begin(),preRegionalEnd,fifth.begin());
    fifth[2]=std::byte{5};
    const auto fifthSum=checksum(fifth,fifth.size()-2);
    fifth[fifth.size()-2]=std::byte(fifthSum&255);
    fifth.back()=std::byte(fifthSum>>8);
    c.regionalRewards=7;
    Save stored{};
    std::copy(fifth.begin(),fifth.end(),stored.begin());
    assert(!restoreSave(stored,fifth.size()-1,c));
    assert(restoreSave(stored,fifth.size(),c) && c.regionalRewards==0 && c.own(47) && c.cities[37].troops==432 &&
           c.surveyed==((uint64_t(1)<<37)|(uint64_t(1)<<47)));

    c.regionalRewards=7;
    newest=encode(c);
    SixthSave sixth{};
    std::copy_n(newest.begin(),sixth.size()-2,sixth.begin());
    sixth[2]=std::byte{6};
    const auto sixthSum=checksum(sixth,sixth.size()-2);
    sixth[sixth.size()-2]=std::byte(sixthSum&255);
    sixth.back()=std::byte(sixthSum>>8);
    std::copy(sixth.begin(),sixth.end(),stored.begin());
    assert(restoreSave(stored,sixth.size(),c) && c.regionalRewards==7 && c.talentHired==0 &&
           c.own(47) && c.cities[37].troops==432);
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
    saved[SixthSave{}.size()-3]=std::byte{1};
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
    b.start(c,c.columns[id].road,id,spear);
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
        b.order(counter(b.intent));
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
    fight.start(arms,arms.columns[column].road,column,horse);
    assert(&fight.officer()==&talents[3].officer && fight.signal==64 && fight.rally());
    assert(fight.losses[1]==talents[3].officer.strength+35);
    arms.cities[11].owner=0;
    auto ordinary=arms;
    assert(ordinary.search(11,0).kind==moneyFound);
    assert(arms.search(11,talentFirst+4).kind==gearFound);
    checkSave(arms);
}

int main() {
    checkMigration();
    checkSearchAndTalents();
    checkSeasonAndRegionalGoals();
    checkCivicAndColumns();
    checkBattleAndCaptive();
    for (int chapter=0;chapter<chapterCount;++chapter)
        for (int side=0;side<3;++side) {
            auto outcome=play(side,chapter);
            std::cout<<chapters[chapter].name<<' '<<factions[side].title<<' '
                     <<outcome.holdings(side)<<" cities, season "<<outcome.turn<<std::endl;
            assert(outcome.won==1 && outcome.holdings(side)==cityCount);
        }
}
