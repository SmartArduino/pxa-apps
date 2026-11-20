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
                const int score=size-c.cities[b].troops-c.cities[b].fort*70;
                const bool siege=score<130;
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
    for (const auto& city:current.cities) {
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
}

static void checkCivicAndColumns() {
    Campaign c;
    c.start(0);
    const auto gold=c.gold,grain=c.grain;
    assert(c.develop(10,recruit) && c.cities[10].troops==2030 && c.actions==1);
    assert(c.gold==gold-80 && c.grain==grain-50);
    auto found=c.survey(10);
    assert(found.kind==moneyFound && !c.canSurvey(10) && !c.actions);
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

int main() {
    checkMigration();
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
