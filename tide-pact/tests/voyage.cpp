#include "game.hpp"
#include "town.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace tide;

void sail_to(Game& game, int destination) {
    game.s.chart_marks|=place_bit(destination);
    assert(game.depart(destination));
    for (int i=0; i<220 && game.sea(); ++i) {
        if (game.encounter()) {
            const auto weather=game.s.weather;
            int choice=weather==Privateer || weather==Rigging ? 1 : 0;
            if (!game.encounter_requirement(choice).empty()) choice=2;
            if (weather==Privateer && game.s.needs_reload) choice=CloseRange;
            game.encounter_action(choice);
            if (weather==Storm || weather==Rigging) game.s.reefed=0;
        } else game.advance();
    }
    assert(!game.sea());
    assert(game.s.origin==destination);
}

void story_journey(int ending, int choice) {
    Game game;
    game.chapter_choice(choice);
    assert(game.s.chapter==1);
    constexpr int stops[]={madeira,canaries,ribeira,elmina,mpinda,cape,mozambique,kilwa,
                           mozambique,cape,mpinda,elmina,ribeira,canaries,madeira,lisbon};
    for (const auto port : stops) {
        if (game.port()) { game.service(0); game.service(1); }
        sail_to(game,port);
        if (game.s.chapter<Chapters && Story[game.s.chapter].place==port)
            game.chapter_choice(choice);
    }
    assert(game.s.ending==ending);
    assert(game.s.chapter==Chapters);
    assert(game.s.renown>=12);
    assert(game.s.gold>0);
    auto bytes=encode(game.s);
    Game loaded;
    assert(decode(loaded.s,bytes));
    assert(loaded.s.ending==game.s.ending && loaded.s.gold==game.s.gold);
    bytes[17]^=std::byte{1};
    assert(!decode(loaded.s,bytes));
}

void port_tales() {
    Game lamps;
    assert(!lamps.choose_tale(Lamps,true));
    sail_to(lamps,madeira);
    assert(lamps.tale_here()==Lamps);
    assert(lamps.choose_tale(Lamps,true));
    assert(lamps.tale_stop(Lamps)==canaries);
    assert(lamps.depart(canaries));
    lamps.advance();
    assert(lamps.s.weather==FalseBeacon && lamps.s.beacon==BeaconSighted);
    State save;
    assert(decode(save,encode(lamps.s)));
    lamps.s=save;
    lamps.restore();
    assert(lamps.encounter() && !lamps.paused);
    int day=lamps.s.day, stores=lamps.s.stores;
    lamps.encounter_action(0);
    assert(lamps.s.beacon==BeaconCharted && lamps.s.day==day+1 && lamps.s.stores==stores-3);
    auto record=read_sea(lamps.s.sea_log[0]);
    assert(record.kind==FalseBeacon && record.day==lamps.s.day && record.choice==0);
    while (lamps.sea()) lamps.advance();
    assert(lamps.s.origin==canaries);
    day=lamps.s.day;
    assert(lamps.choose_tale(Lamps,true));
    assert(lamps.s.day==day && lamps.s.trust==2 && lamps.s.tales[Lamps]==Shared);
    assert(!lamps.choose_tale(Lamps,false));
    lamps.s.chapter=1;
    lamps.chapter_choice(0);
    assert(lamps.s.day==day+1);
    assert(decode(save,encode(lamps.s)) && save.tales[Lamps]==Shared &&
           save.sea_log==lamps.s.sea_log);

    Game fallback;
    fallback.s.origin=madeira;
    assert(fallback.choose_tale(Lamps,true));
    fallback.s.origin=canaries;
    assert(fallback.choose_tale(Lamps,false));
    assert(fallback.s.gold==740 && fallback.s.day==1 && fallback.s.tales[Lamps]==Sold);

    Game charts;
    charts.s.origin=tangier;
    assert(charts.choose_tale(Chartmakers,true));
    charts.s.origin=tunis;
    assert(!charts.choose_tale(Chartmakers,true));
    charts.s.cargo[Timber]=2;
    charts.s.hull=40;
    const int repair=charts.repair_cost();
    assert(charts.choose_tale(Chartmakers,true));
    assert(charts.s.cargo[Timber]==0 && charts.repair_cost()==repair*3/4);
    const int gold=charts.s.gold;
    charts.service(1);
    assert(charts.s.hull==100 && charts.s.gold==gold-repair*3/4);
    charts.s.origin=lisbon;
    charts.s.hull=40;
    assert(charts.repair_cost()==repair);

    Game monsoon;
    monsoon.s.origin=mozambique;
    assert(monsoon.choose_tale(Monsoon,true));
    monsoon.s.origin=malindi;
    monsoon.s.stores=7;
    assert(!monsoon.choose_tale(Monsoon,true));
    monsoon.s.stores=8;
    monsoon.s.water=8;
    assert(monsoon.choose_tale(Monsoon,true));
    assert(monsoon.s.stores==0 && monsoon.water_price()==18 && monsoon.ration_cost()==18);
}

void sea_events() {
    Game game;
    assert(game.depart(madeira));
    game.s.water=50;
    game.s.weather=ReliefShip;
    game.encounter_action(0);
    assert(game.s.cargo[Salt]==1 && game.s.water==68 && game.s.stores==74);
    game.s.weather=Rigging;
    game.s.hull=93;
    game.encounter_action(0);
    assert(game.s.weather==Rigging && game.s.hull==93);
    game.s.cargo[Timber]=1;
    game.encounter_action(0);
    assert(game.s.hull==98 && game.s.cargo[Timber]==0);
    game.s.weather=Fog;
    game.encounter_action(1);
    assert(game.s.weather==Fog);
    game.s.charts=2;
    game.encounter_action(1);
    assert(!game.encounter() && read_sea(game.s.sea_log[0]).kind==Fog);
    game.s.weather=Castaways;
    game.encounter_action(2);
    assert(read_sea(game.s.sea_log[0]).kind==Castaways);
    assert(read_sea(game.s.sea_log[1]).kind==Fog);
    assert(read_sea(game.s.sea_log[2]).kind==Rigging);
}

void voyage_planning() {
    Game game;
    game.s.day=28;
    game.s.spirits=63;
    game.s.crew=18;
    const auto route=passage(lisbon,canaries);
    const auto estimate=game.forecast(route);
    assert(route.count>1 && estimate.arrival_days[0]<estimate.days &&
           estimate.arrival_days[route.count-1]==estimate.days);
    assert(estimate.provisions==estimate.days*2);
    game.s.stores=estimate.provisions;
    game.s.water=static_cast<std::uint8_t>(estimate.water);
    assert(game.depart(canaries));
    game.s.event_mask=UINT32_MAX;
    for (int i=0;i<estimate.days && game.sea();++i) game.advance();
    assert(!game.sea() && game.s.origin==canaries && game.s.day==28+estimate.days);
    assert(game.s.stores==0 && game.s.water==0 && game.s.hull==100);
    assert(!game.depart(madeira));
    const int gold=game.s.gold;
    assert(game.resupply(9,0));
    assert(game.s.stores==9 && game.s.water==0 && game.s.gold==gold-2);
    assert(game.resupply_cost(6,0)==0 && !game.resupply(6,0));

    Game stranded;
    stranded.s.origin=azores;
    stranded.s.stores=0;
    stranded.s.hull=5;
    stranded.s.gold=17;
    assert(stranded.tow_needed() && stranded.request_tow());
    assert(stranded.port() && stranded.s.hull==30 && stranded.s.stores==12);
    assert(stranded.load()==2 && stranded.s.gold==0 && stranded.s.day==1);
    assert(!stranded.request_tow());

    Game credit;
    credit.s.hull=10;
    credit.s.gold=5;
    credit.service(6);
    assert(credit.s.hull==25 && credit.s.stores==74);
}

void trading_quotes() {
    Game game;
    const auto before=encode(game.s);
    const auto barrel=game.quote(Wine,false);
    const auto batch=game.trade_quote(Wine,false,5);
    assert(batch.units==5 && batch.gold>barrel*5);
    assert(before==encode(game.s));
    game.s.gold=barrel*2+1;
    const int gold=game.s.gold;
    const auto partial=game.trade_quote(Wine,false,5);
    assert(partial.units==2);
    const auto sold=game.trade(Wine,false,5);
    assert(sold.units==partial.units && sold.gold==partial.gold && sold.stock_change==partial.stock_change);
    assert(game.s.gold==gold-partial.gold && game.s.cargo[Wine]==2);
    const auto sell=game.trade_quote(Wine,true,5);
    assert(sell.units==2);
    const int balance=game.s.gold;
    game.trade(Wine,true,5);
    assert(game.s.gold==balance+sell.gold && game.s.cargo[Wine]==0);
    assert(game.price_at(lisbon,Sugar,false)==game.quote(Sugar,false));
    assert(game.price_at(-1,Sugar,false)==0 && game.price_at(madeira,-1,true)==0);
    game.s.stock_delta[Wine]=-48;
    assert(game.trade_quote(Wine,false,5).units==0);
    game.s.cargo[Wine]=1;
    assert(game.trade(Wine,true,1).units==1 && game.stock(Wine)==1);
    game.s.stock_delta[Wine]=48;
    game.s.cargo[Wine]=1;
    assert(!game.trade_quote(Wine,true,1).units && game.s.cargo[Wine]==1);
    game.s.gold=999999;
    assert(game.trade_quote(Salt,true,5).units==0);
}

void town_walks() {
    assert(town_index(lisbon)==0 && town_index(cadiz)>=0 && town_index(azores)<0);
    for (const auto& layout : Towns) {
        const auto dock=layout.doors[TownDock], market=layout.doors[TownMarket];
        assert(!town_street(layout,{998,990}));
        const auto road=town_street(layout,{680,layout.quay+16});
        assert(road && road->y==layout.quay && road->x==680);
        TownWalk walk{.at=dock};
        walk.go(market,layout.quay,TownMarket,0);
        assert(walk.count==3 && walk.update(1)==-1 && walk.facing==1);
        assert(walk.position(20).y<dock.y);
        walk.at=walk.position(20);
        walk.go(*road,layout.quay,-1,20);
        assert(walk.update(2000)==-1 && same_town_point(walk.at,*road));
        walk.go(market,layout.quay,TownMarket,2000);
        assert(walk.update(4000)==TownMarket && same_town_point(walk.at,market));
        assert(walk.update(4100)==-1);
    }
}

void merchant_trade() {
    Game game;
    bool checked_reload=false;
    int buys=0, sales=0, holds=0;
    for (int day=0;day<100;++day) {
        const auto before=game.s;
        game.wait_at_port();
        assert(game.s.day==before.day+1);
        assert(game.s.gold==before.gold && game.s.cargo==before.cargo && game.s.rng==before.rng);
        Game without_merchants;
        without_merchants.s=before;
        for (auto& ship : without_merchants.s.merchants)
            ship=MerchantVoyage{.gold=0,.port=ship.port};
        without_merchants.wait_at_port();
        auto expected=without_merchants.s.stock_delta;
        for (std::size_t i=0;i<game.s.merchants.size();++i) {
            const auto& previous=before.merchants[i];
            const auto& current=game.s.merchants[i];
            if (current.held) {
                assert(current.progress==previous.progress && current.destination==previous.destination);
                ++holds;
            }
            if (previous.destination==NoMerchantPort && current.destination!=NoMerchantPort && current.units) {
                auto& stock=expected[int(MerchantPorts[current.port])*GoodsCount+current.good];
                stock-=current.units;
                ++buys;
            }
            if (previous.destination!=NoMerchantPort && current.destination==NoMerchantPort && current.units) {
                assert(current.port==previous.destination && !current.progress);
                auto& stock=expected[int(MerchantPorts[current.port])*GoodsCount+current.good];
                stock=std::min(48,stock+current.units);
                ++sales;
            }
            if (!checked_reload && current.progress>0) {
                Game copy;
                assert(decode(copy.s,encode(game.s)));
                copy.restore();
                const auto pos=game.merchant_position(i), restored=copy.merchant_position(i);
                assert(pos.x==restored.x && pos.y==restored.y);
                Game next=game;
                next.wait_at_port();
                copy.wait_at_port();
                assert(encode(next.s)==encode(copy.s));
                checked_reload=true;
            }
        }
        assert(expected==game.s.stock_delta);
    }
    assert(buys>=4 && sales>=4 && holds>0 && checked_reload);
    game.s.merchants[0].destination=game.s.merchants[0].port;
    State invalid;
    assert(!decode(invalid,encode(game.s)));
}

void custom_trade_quantities() {
    Game game;
    const auto buy=game.trade_quote(Wine,false,7);
    assert(buy.units==7 && game.trade_limit(Wine,false)>=7);
    game.s.gold=buy.gold;
    assert(game.trade_limit(Wine,false)==7);
    assert(game.trade(Wine,false,7).gold==buy.gold && game.s.cargo[Wine]==7 && !game.s.gold);
    assert(game.trade_limit(Wine,true)==7);
    const auto sell=game.trade_quote(Wine,true,3);
    assert(game.trade(Wine,true,3).gold==sell.gold && game.s.cargo[Wine]==4 && game.s.gold==sell.gold);

    Game old_deal;
    old_deal.s.bargain_good=Wine+1;
    old_deal.s.bargain_rate=8;
    const auto discounted=old_deal.trade_quote(Wine,false,TradeLot);
    assert(discounted.saved && discounted.units==TradeLot);
    old_deal.s.gold=discounted.gold;
    assert(old_deal.trade_quote(Wine,false,old_deal.capacity()).units<TradeLot);
    assert(old_deal.trade_limit(Wine,false)==TradeLot);
}

void port_market_cycles() {
    Game game;
    for (auto& ship : game.s.merchants) ship.gold=0;
    game.s.day=3;
    assert(!market_notice(lisbon,Timber,game.s.day));
    const int common_stock=game.market_stock(lisbon,Timber);
    const int common_price=game.price_at(lisbon,Timber,true);
    game.wait_at_port();
    assert(game.s.day==4 && market_notice(lisbon,Timber,game.s.day)->remaining(game.s.day)==9);
    assert(game.market_stock(lisbon,Timber)==common_stock-2);
    assert(game.price_at(lisbon,Timber,true)>common_price);
    for (int day=4;day<13;++day) game.wait_at_port();
    assert(!market_notice(lisbon,Timber,game.s.day));
    for (int day=13;day<29;++day) game.wait_at_port();
    assert(game.market_stock(lisbon,Timber)>common_stock-8);

    Game harvest;
    for (auto& ship : harvest.s.merchants) ship.gold=0;
    harvest.s.day=13;
    const int sugar=harvest.market_stock(madeira,Sugar);
    const int sugar_price=harvest.price_at(madeira,Sugar,false);
    harvest.wait_at_port();
    assert(market_notice(madeira,Sugar,14) && market_notice(madeira,Sugar,98));
    assert(harvest.market_stock(madeira,Sugar)==sugar+2);
    assert(harvest.price_at(madeira,Sugar,false)<sugar_price);

    Game scarcity;
    const int reserve=scarcity.stock(Wine);
    scarcity.s.stock_delta[Wine]=13-reserve;
    const int well_stocked=scarcity.quote(Wine,false);
    --scarcity.s.stock_delta[Wine];
    const int ordinary_step=scarcity.quote(Wine,false)-well_stocked;
    scarcity.s.stock_delta[Wine]=6-reserve;
    const int short_stock=scarcity.quote(Wine,false);
    --scarcity.s.stock_delta[Wine];
    assert(scarcity.quote(Wine,false)-short_stock>ordinary_step);
}

void naval_and_duel_combat() {
    Game lead;
    assert(lead.accept_escort() && lead.depart(madeira));
    lead.advance();
    assert(lead.convoy_battle() && lead.s.weather==Privateer && lead.s.range==3);
    assert(!lead.encounter_requirement(BoardShip).empty());

    Game tactics=lead;
    assert(tactics.naval_intent()==EnemyFire);
    tactics.s.range=4;
    assert(tactics.naval_intent()==EnemyClose);
    tactics.s.range=2;
    tactics.s.needs_reload=1;
    assert(tactics.naval_intent()==EnemyClose);
    tactics.s.enemy=24;
    tactics.s.needs_reload=0;
    tactics.s.aim=1;
    assert(tactics.naval_intent()==EnemyEvade);
    tactics.s.range=1;
    assert(tactics.naval_intent()==EnemyFire);
    tactics.s.enemy=13;
    assert(tactics.naval_intent()==EnemyEvade);

    Game guns;
    assert(decode(guns.s,encode(lead.s)));
    guns.restore();
    guns.encounter_action(FireGuns);
    const int enemy_after_shot=guns.s.enemy;
    const int hull_after_shot=guns.s.hull;
    assert(guns.s.needs_reload);
    guns.encounter_action(FireGuns);
    assert(guns.s.enemy==enemy_after_shot && guns.s.hull==hull_after_shot);
    for (int i=0;i<10 && guns.encounter();++i)
        guns.encounter_action(guns.s.needs_reload?CloseRange:FireGuns);
    assert(!guns.encounter() && guns.s.escort==EscortWonGuns);
    while (guns.sea()) guns.advance();
    const int before=guns.s.gold;
    assert(guns.deliver_escort(false) && guns.s.gold==before+340);

    lead.encounter_action(CloseRange);
    lead.encounter_action(CloseRange);
    assert(lead.s.range==1 && lead.encounter_requirement(BoardShip).empty());
    lead.encounter_action(BoardShip);
    assert(lead.s.weather==Boarding && lead.s.vigor==lead.vigor_limit());
    State saved;
    assert(decode(saved,encode(lead.s)));
    lead.s=saved;
    lead.restore();
    const int health=lead.s.vigor;
    lead.encounter_action((lead.duel_intent()+2)%3);
    assert(lead.s.vigor<health);
    while (lead.duel()) lead.encounter_action((lead.duel_intent()+1)%3);
    assert(lead.s.escort==EscortWonDuel && read_sea(lead.s.sea_log[0]).kind==Boarding);
    while (lead.sea()) lead.advance();
    assert(lead.deliver_escort(true) && !lead.deliver_escort(true) && lead.s.trust==3);
    assert(decode(saved,encode(lead.s)) && saved.escort==EscortPublished);

    Game dock;
    dock.s.origin=cadiz;
    assert(dock.start_dock_duel() && !dock.port() && !dock.depart(lisbon));
    assert(decode(saved,encode(dock.s)) && saved.weather==DockDuel && saved.vigor==44);
    dock.encounter_action(Yield);
    assert(dock.port() && dock.s.day==1 && dock.s.gold==495 && dock.s.cadiz_victory==0);
    assert(dock.start_dock_duel());
    while (dock.duel()) dock.encounter_action((dock.duel_intent()+1)%3);
    assert(dock.s.cadiz_victory && !dock.start_dock_duel());
    assert(dock.vigor_limit()==lead.vigor_limit()+8);
}

void new_events_and_investment() {
    Game game;
    const auto before=game.quote(Wine,false);
    const auto elsewhere=game.price_at(madeira,Wine,false);
    const int cost=game.guild_cost();
    assert(game.invest() && game.s.gold==520-cost && game.guild_stake(lisbon));
    assert(!game.invest() && game.quote(Wine,false)<before &&
           game.price_at(madeira,Wine,false)==elsewhere);
    assert(game.depart(madeira));
    game.s.weather=ReversedFlag;
    game.encounter_action(0);
    assert(game.s.weather==Privateer && game.s.range==2 && game.s.aim==1);
    assert(read_sea(game.s.sea_log[0]).kind==ReversedFlag);
    game.s.weather=StrandedPost;
    assert(!game.encounter_requirement(0).empty());
    game.encounter_action(0);
    assert(game.s.weather==StrandedPost);
    game.s.cargo[Timber]=1;
    game.encounter_action(1);
    assert(!game.encounter() && game.s.cargo[Timber]==0 && game.s.trust==1);
    State save;
    assert(decode(save,encode(game.s)) && save.guild_stakes==(1U<<lisbon));
    assert(read_sea(save.sea_log[0]).kind==StrandedPost);
}

void free_helm() {
    auto ocean=[](Point){ return true; };
    Game ship;
    assert(ship.start_helm(madeira) && ship.sea() && ship.s.helm);
    const auto anchor=ship.position();
    assert(ship.nearby_anchorage()==lisbon);
    Game following=ship, beating=ship;
    const auto wind=ship.conditions().direction;
    following.s.heading=wind;
    beating.s.heading=(wind+4)%8;
    assert(following.speed_in_tenths_of_a_knot()>beating.speed_in_tenths_of_a_knot());
    following.helm_step(ocean);
    beating.helm_step(ocean);
    const auto distance=[&](const Game& vessel) {
        const int x=vessel.s.helm_x-anchor.x*16, y=vessel.s.helm_y-anchor.y*16;
        return x*x+y*y;
    };
    assert(distance(following)>distance(beating));
    for (int i=0;i<SailingWatches;++i) ship.helm_step(ocean);
    assert(ship.s.day==1 && ship.s.stores==73 && ship.s.water<75);
    State save;
    assert(decode(save,encode(ship.s)) && save.helm && save.helm_x==ship.s.helm_x);
    ship.s=save;
    ship.restore();
    assert(ship.paused && ship.s.heading==save.heading);

    Game grounded;
    assert(grounded.start_helm(-1));
    grounded.s.heading=2;
    grounded.helm_step([](Point p){return p.x<=Anchor[lisbon].x;});
    assert(grounded.paused && grounded.s.hull==94 && grounded.position().x==Anchor[lisbon].x);
    assert(grounded.dock_helm() && grounded.s.origin==lisbon && !grounded.sea());
    assert(grounded.s.sail_watch==1 && decode(save,encode(grounded.s)) && save.sail_watch==1);
    auto reembarked=grounded;
    assert(reembarked.start_helm(-1));
    for (int i=1;i<SailingWatches;++i) reembarked.helm_step(ocean);
    assert(reembarked.s.day==1 && reembarked.s.sail_watch==0);
    assert(grounded.resupply(80,74) && grounded.s.stores==80 && grounded.s.water==74);
    assert(grounded.resupply(80,85) && grounded.s.stores==80 && grounded.s.water==85);
    assert(grounded.resupply(80,110) && grounded.s.water==100 && grounded.s.stores==80);
    assert(grounded.food_capacity()==80 && !grounded.resupply(81,100));
    assert(grounded.unload_provisions(true));
    assert(grounded.resupply(90,90) && grounded.s.stores+grounded.s.water==180);
    assert(decode(save,encode(grounded.s)));
}

void continuous_helm() {
    const auto ocean=[](Point) { return true; };
    Game ship;
    assert(ship.start_helm(-1));
    const int start=ship.s.helm_x;
    ship.helm_pulses(ocean,1);
    assert(ship.s.helm_x<start && ship.s.sail_watch==0 && ship.s.helm_pulse==1);

    const int first=ship.s.helm_x, y=ship.s.helm_y;
    ship.s.heading=0;
    ship.helm_pulses(ocean,1);
    assert(ship.s.helm_x==first && ship.s.helm_y<y && ship.s.helm_pulse==2);

    State saved;
    assert(decode(saved,encode(ship.s)) && saved.helm_pulse==2);
    ship.s=saved;
    ship.restore();
    assert(ship.paused);
    ship.paused=false;
    ship.helm_pulses(ocean,HelmPulsesPerWatch-2);
    assert(ship.s.helm_pulse==0 && ship.s.sail_watch==1 && ship.s.day==0);

    Game docking;
    assert(docking.start_helm(-1));
    docking.helm_pulses(ocean,1);
    assert(docking.dock_helm(ocean) && decode(saved,encode(docking.s)));
    assert(!saved.helm && saved.helm_pulse==1);
    assert(docking.start_helm(-1));
    docking.helm_pulses(ocean,HelmPulsesPerWatch-1);
    assert(docking.s.sail_watch==1 && docking.s.helm_pulse==0);
}

void chart_skirmish() {
    const auto ocean=[](Point) { return true; };
    const auto land=[](Point) { return false; };
    Game ship;
    assert(ship.start_helm(-1));
    ship.s.weather=Privateer;
    ship.s.range=3;
    ship.s.enemy=43;
    ship.s.guns=2;
    ship.s.cargo[Timber]=2;
    assert(!ship.begin_skirmish(land));
    assert(ship.begin_skirmish(ocean) && ship.paused);
    const Point rival=ship.s.skirmish.rival;
    assert(!ship.tick_skirmish(ocean) && ship.s.skirmish.rival.x==rival.x);
    assert(!ship.fire_skirmish(-1));

    ship.paused=false;
    ship.s.skirmish.snared=20;
    assert(ship.fire_skirmish(-1) && !ship.fire_skirmish(-1));
    assert(ship.s.enemy==43 && ship.s.skirmish.shots[0].steps>0);
    State reload;
    assert(decode(reload,encode(ship.s)) && reload.skirmish.shots[0].steps>0);
    ship.s=reload;
    ship.restore();
    assert(ship.paused);
    ship.paused=false;
    unsigned effects=0;
    for (int step=0;step<10;++step) effects|=ship.tick_skirmish(ocean);
    assert((effects&SkirmishHit) && ship.s.enemy==14);

    ship.s.skirmish.rival={ship.s.helm_x,ship.s.helm_y+4};
    const int timber=ship.s.cargo[Timber];
    assert(ship.drop_drift_rope() && ship.s.cargo[Timber]==timber-1);
    assert((ship.tick_skirmish(ocean)&SkirmishRope) && ship.s.skirmish.snared);
    assert(ship.can_board_skirmish() && ship.board_skirmish());
    assert(ship.duel() && !ship.s.skirmish.active && decode(reload,encode(ship.s)));

    Game escape;
    assert(escape.start_helm(-1));
    escape.s.weather=Privateer;
    escape.s.range=3;
    escape.s.enemy=42;
    assert(escape.begin_skirmish(ocean));
    escape.s.skirmish.rival={escape.s.helm_x+165,escape.s.helm_y};
    escape.paused=false;
    const int gold=escape.s.gold;
    assert(escape.tick_skirmish(ocean)&SkirmishEnded);
    assert(!escape.encounter() && escape.s.gold==gold &&
           read_sea(escape.s.sea_log[0]).choice==1);
}

void coastal_fishery() {
    const auto ocean=[](Point) { return true; };
    const auto ready=[&](Game& ship,Point area) {
        assert(ship.start_helm(-1));
        ship.s.helm_x=area.x*16;
        ship.s.helm_y=area.y*16;
        while (ship.conditions().weather==Squall) ++ship.s.day;
    };
    Game fishing;
    ready(fishing,chart_point(-11,39));
    fishing.s.stores=50;
    assert(fishing.fishery_available()==SardineFishery && fishing.begin_fishery(ocean));
    auto& net=fishing.s.fishery;
    const auto initial=net.animal;
    fishing.helm_pulses(ocean,3);
    assert(initial.x!=net.animal.x || initial.y!=net.animal.y);
    assert(!fishing.work_fishery());
    fishing.s.reefed=1;
    fishing.s.helm_x=net.animal.x;
    fishing.s.helm_y=net.animal.y;
    assert(fishing.work_fishery() && net.stage==GearSet);
    fishing.helm_pulses(ocean,8);
    assert(net.stage==GearReady);
    const int stores=fishing.s.stores;
    fishing.s.helm_x=net.gear.x;
    fishing.s.helm_y=net.gear.y;
    State saved;
    assert(decode(saved,encode(fishing.s)) && saved.fishery.stage==GearReady);
    assert(fishing.work_fishery() && fishing.s.stores>stores);
    assert(!fishing.fishery_available() && !fishing.work_fishery());

    Game whale;
    ready(whale,chart_point(-6,44.5f));
    assert(whale.fishery_available()==WhaleFishery && whale.begin_fishery(ocean));
    auto& job=whale.s.fishery;
    whale.s.reefed=1;
    whale.s.helm_x=job.animal.x;
    whale.s.helm_y=job.animal.y;
    job.age=21;
    assert(!whale.whale_at_surface() && !whale.work_fishery());
    job.age=0;
    const int load=whale.load();
    assert(whale.work_fishery());
    whale.helm_pulses(ocean,10);
    assert(job.stage==GearReady);
    whale.s.helm_x=job.gear.x;
    whale.s.helm_y=job.gear.y;
    assert(whale.work_fishery() && whale.s.whale_blubber==3 && whale.load()==load+3);
    whale.s.helm_x=Anchor[bordeaux].x*16;
    whale.s.helm_y=Anchor[bordeaux].y*16;
    whale.s.chart_marks|=place_bit(bordeaux);
    assert(whale.dock_helm(ocean));
    const int cost=whale.whale_rendering_cost(), prior_gold=whale.s.gold;
    assert(cost==21 && whale.render_whale_oil() && whale.s.whale_oil==3 &&
           whale.s.gold==prior_gold-cost && !whale.render_whale_oil());
    const int gold=whale.s.gold, quote=whale.whale_oil_quote();
    assert(quote>0 && whale.sell_whale_oil() && whale.s.gold==gold+quote);
    assert(!whale.sell_whale_oil() && whale.load()==load);
}

void island_settlements() {
    Game colony;
    colony.s.origin=madeira;
    colony.s.gold=1100;
    colony.s.renown=3;
    colony.s.trust=1;
    colony.s.cargo[Timber]=20;
    assert(colony.enclave_here()==0 && colony.lease_enclave(0));
    auto& site=colony.s.enclaves[0];
    assert(site.residents==4 && !colony.lease_enclave(0));
    assert(colony.build_enclave(0,EnclaveWharf) && !colony.build_enclave(0,EnclaveHouses));
    colony.wait_enclave_day();
    State saved;
    assert(decode(saved,encode(colony.s)) && saved.enclaves[0].under_construction==EnclaveWharf+1);
    colony.wait_enclave_day();
    assert(site.buildings&building_flag(EnclaveWharf));
    assert(colony.supply_enclave(0,DeliverEnclaveTimber));
    assert(colony.build_enclave(0,EnclaveGardens));
    for (int day=0;day<2;++day) colony.wait_enclave_day();
    assert(colony.supply_enclave(0,DeliverEnclaveTimber));
    assert(colony.build_enclave(0,EnclaveHouses));
    for (int day=0;day<3;++day) colony.wait_enclave_day();
    assert(site.buildings&building_flag(EnclaveHouses));
    colony.s.origin=lisbon;
    assert(colony.take_enclave_migrants(0) && !colony.take_enclave_migrants(0));
    assert(decode(saved,encode(colony.s)) && saved.migrant_site==1);
    colony.s.origin=madeira;
    assert(colony.land_enclave_migrants(0) && site.residents==7);
    assert(colony.hire_enclave_people(0,true) && site.carpenter);
    assert(colony.supply_enclave(0,DeliverEnclaveTimber));
    assert(colony.supply_enclave(0,DeliverEnclaveTimber));
    assert(colony.build_enclave(0,EnclaveWorkshop));
    for (int day=0;day<5;++day) colony.wait_enclave_day();
    const int payday=colony.enclave_export_quote(0);
    assert(payday>0 && colony.sell_enclave_exports(0) && !colony.sell_enclave_exports(0));

    Game siege;
    siege.s.origin=azores;
    siege.s.chart_marks|=place_bit(azores);
    assert(siege.enclave_here()==1 && !siege.siege_enclave(1));
    siege.s.guns=3;
    assert(siege.siege_enclave(1) && siege.s.enclaves[1].status==EnclaveOccupied);
    assert(siege.s.enclaves[1].prisoners==3 && siege.s.court_hold_until[Portugal]>siege.s.day);
    const int before=siege.s.court_hold_until[Portugal];
    assert(decode(saved,encode(siege.s)) && saved.enclaves[1].prisoners==3);
    assert(siege.decide_enclave_prisoners(1,ReleaseEnclavePrisoners));
    assert(siege.s.court_hold_until[Portugal]<before &&
           !siege.decide_enclave_prisoners(1,RansomEnclavePrisoners));
}

void merchant_raids_and_patrols() {
    const auto ocean=[](Point) { return true; };
    Game ship;
    ship.s.day=5;
    ship.s.guns=3;
    ship.s.merchants[0]={.progress=77,.gold=700,.port=0,.destination=1,.good=Timber,.units=4};
    assert(ship.start_helm(-1));
    const auto merchant=ship.merchant_position(0);
    ship.s.helm_x=(merchant.x-3)*16;
    ship.s.helm_y=merchant.y*16;
    ship.restore();
    assert(ship.raid_target(ocean)==0 && ship.begin_merchant_raid(0,ocean));
    assert(ship.s.sea_warrant[Portugal]>ship.s.day && !ship.s.sea_warrant[Castile]);
    State saved;
    assert(decode(saved,encode(ship.s)) && saved.skirmish.opponent==FirstMerchantOpponent);
    ship.paused=false;
    ship.s.heading=6;
    ship.s.enemy=30;
    ship.s.skirmish.rival={ship.s.helm_x,ship.s.helm_y+21};
    ship.s.skirmish.snared=18;
    assert(ship.fire_skirmish(-1) && ship.s.enemy==30);
    const int money=ship.s.gold;
    for (int tick=0;ship.encounter() && tick<12;++tick) ship.tick_skirmish(ocean);
    assert(!ship.encounter() && ship.s.gold==money+52 && ship.s.cargo[Timber]==4);
    assert(ship.s.merchants[0].units==0);

    auto patrol=ship;
    patrol.s.day=patrol.s.patrol_after[Portugal];
    patrol.s.helm_x=(Anchor[lisbon].x-9)*16;
    patrol.s.helm_y=Anchor[lisbon].y*16;
    patrol.s.chart_marks=(1ULL<<PlaceCount)-1;
    while (patrol.conditions().weather==Squall) ++patrol.s.day;
    patrol.s.sail_watch=3;
    patrol.s.helm_pulse=5;
    patrol.helm_pulses(ocean,1);
    assert(patrol.encounter() && patrol.s.skirmish.opponent==PatrolOpponent);
    assert(patrol.begin_skirmish(ocean) && patrol.s.skirmish.opponent==PatrolOpponent);

    ship.s.helm_x=Anchor[lisbon].x*16;
    ship.s.helm_y=Anchor[lisbon].y*16;
    assert(ship.dock_helm(ocean) && ship.market_held() && ship.port_settlement_cost()==150);
    const int payout=ship.s.gold;
    assert(ship.settle_port_case() && ship.s.gold==payout-150 && !ship.market_held());
    assert(!ship.settle_port_case());
}

void sailing_units() {
    const Game at_port;
    assert(at_port.forecast(passage(lisbon,cadiz)).days==2);
    assert(at_port.forecast(passage(lisbon,madeira)).days==5);
    assert(at_port.forecast(passage(lisbon,canaries)).days==8);
    for (const int latitude : {0,60}) {
        Game ship;
        assert(ship.start_helm(-1));
        const int x=(122+LegacyMapX)*16, y=(ChartNorth-latitude)*ChartLatitudeScale*16;
        ship.s.helm_x=x;
        ship.s.helm_y=y;
        ship.s.heading=2;
        const float indicated=ship.speed_in_tenths_of_a_knot()/10.f;
        ship.helm_step([](Point){return true;});
        const float longitude=(ship.s.helm_x-x)/(16.f*ChartLongitudeScale);
        const float nautical_miles=60.f*longitude*std::cos(latitude*0.01745329252f);
        assert(std::abs(indicated-nautical_miles/HoursPerWatch)<0.12f);
        assert(ship.s.sail_watch==1 && ship.s.day==0);
    }
}

void weather_forecasts() {
    Game ship;
    ship.s.day=5;
    const auto path=passage(lisbon,canaries);
    const auto plan=ship.forecast(path);
    assert(path.count>1 && plan.outlook[0].day==6 && plan.outlook[2].day==8);
    auto waiting=ship;
    waiting.wait_at_port();
    assert(waiting.forecast(path).outlook[0].day==7);
    assert(waiting.s.stores==ship.s.stores && waiting.s.water==ship.s.water);

    assert(ship.depart(canaries));
    assert(ship.remaining_forecast().days==plan.days);
    assert(ship.sailing_target()==canaries);
    ship.s.event_mask=(1U<<path.count)-1;
    const int food=ship.s.stores, water=ship.s.water;
    for (int elapsed=0;elapsed<plan.days;++elapsed) {
        assert(ship.sea());
        const auto remaining=ship.remaining_forecast();
        const auto& first=remaining.outlook[0];
        assert(remaining.days==plan.days-elapsed);
        assert(first.day==ship.s.day+1 && first.speed_tenths==ship.speed_in_tenths_of_a_knot());
        const int before=ship.s.water;
        ship.advance();
        assert(!ship.encounter() && before-ship.s.water==first.water);
    }
    assert(!ship.sea() && ship.s.origin==canaries && ship.sailing_target()==-1);
    assert(food-ship.s.stores==plan.provisions && water-ship.s.water==plan.water);

    assert(ship.depart(lisbon));
    ship.s.event_mask=(1U<<ship.route.count)-1;
    ship.advance();
    assert(ship.sea() && ship.s.progress>0);
    const auto through=ship.remaining_forecast();
    ship.s.stop_next=1;
    const auto stop=ship.remaining_forecast();
    assert(ship.sailing_target()==madeira && stop.days==through.arrival_days[ship.s.leg]);
    assert(stop.days<through.days && stop.provisions<through.provisions && stop.water<through.water);
    for (int i=0;i<stop.days;++i) ship.advance();
    assert(!ship.sea() && ship.s.origin==madeira);

    assert(ship.start_helm(-1));
    const auto local=ship.local_forecast();
    assert(!ship.remaining_forecast().days && ship.sailing_target()==-1);
    for (int i=0;i<ForecastDays;++i) {
        const auto& entry=local[i];
        assert(entry.day==ship.s.day+i+1 && !entry.speed_tenths);
        assert(entry.sea.weather==conditions_at(ship.position(),entry.day).weather);
        assert(entry.water==ship.daily_water(entry.sea.weather));
    }

    Game at_port;
    int squall_loss=0, fair_loss=0;
    for (int day=0;day<120 && (!squall_loss || !fair_loss);++day) {
        at_port.s.day=day;
        const auto full=at_port.forecast(path).outlook[0];
        at_port.s.reefed=1;
        const auto reefed=at_port.forecast(path).outlook[0];
        at_port.s.reefed=0;
        if (full.sea.weather==Squall) squall_loss=full.speed_tenths-reefed.speed_tenths;
        if (full.sea.weather==Fair) fair_loss=full.speed_tenths-reefed.speed_tenths;
    }
    assert(squall_loss>0 && squall_loss<fair_loss);
}

void sailing_rations() {
    Game ship;
    const auto path=passage(lisbon,madeira);
    const auto normal=ship.forecast(path);
    ship.toggle_ration(SaveFood);
    assert(ship.rationing(SaveFood) && !ship.rationing(SaveWater));
    auto use=ship.sailing_ration(1,Fair);
    assert(use.food==0 && use.water==1 && use.morale_cost==3);
    ship.toggle_ration(SaveWater);
    use=ship.sailing_ration(1,Heat);
    assert(use.food==0 && use.water==2 && use.morale_cost==3);
    use=ship.sailing_ration(1,Fair);
    assert(use.food==0 && use.water==0 && use.morale_cost==7);
    use=ship.sailing_ration(2,Fair);
    assert(use.food==1 && use.water==1 && use.morale_cost==0);

    const auto reduced=ship.forecast(path);
    assert(reduced.provisions<normal.provisions && reduced.water<normal.water);
    assert(reduced.outlook[0].food==0 && reduced.outlook[0].water==0);
    assert(reduced.outlook[0].speed_tenths<normal.outlook[0].speed_tenths);
    assert(ship.depart(madeira));
    ship.s.event_mask=(1U<<ship.route.count)-1;
    for (int elapsed=0;elapsed<reduced.days;++elapsed) {
        const auto remaining=ship.remaining_forecast();
        const int food_before=ship.s.stores, water_before=ship.s.water;
        assert(remaining.days==reduced.days-elapsed);
        assert(remaining.outlook[0].speed_tenths==ship.speed_in_tenths_of_a_knot());
        ship.advance();
        assert(food_before-ship.s.stores==remaining.outlook[0].food);
        assert(water_before-ship.s.water==remaining.outlook[0].water);
    }
    assert(ship.s.origin==madeira && !ship.sea() && ship.s.spirits<78);
    assert(74-ship.s.stores==reduced.provisions && 74-ship.s.water==reduced.water);
    State saved;
    assert(decode(saved,encode(ship.s)) && saved.ration_flags==(SaveFood|SaveWater));
    assert(decode(saved,encode(Game{}.s)) && !saved.ration_flags);

    Game helm;
    helm.toggle_ration(SaveFood);
    assert(helm.start_helm(-1));
    helm.s.heading=6;
    helm.s.sail_watch=SailingWatches-1;
    helm.s.stores=0;
    assert(helm.local_forecast()[0].food==0);
    helm.helm_step([](Point){return true;});
    assert(helm.s.day==1 && helm.s.stores==0 && helm.s.spirits==75);
    assert(helm.local_forecast()[0].food==1);
    helm.toggle_ration(SaveFood);
    assert(!helm.rationing(SaveFood));
}

void sea_stories() {
    const auto ocean=[](Point){return true;};
    auto cast_off=[] {
        Game ship;
        ship.s.origin=canaries;
        ship.s.chart_marks|=place_bit(ribeira);
        assert(ship.depart(ribeira));
        ship.s.progress=180;
        return ship;
    };
    auto shoal=cast_off();
    for (unsigned seed=1;shoal.s.weather!=ShoalLantern && seed<600;++seed) {
        shoal=cast_off();
        shoal.s.rng=seed;
        shoal.advance();
    }
    assert(shoal.s.weather==ShoalLantern && !shoal.heard_rumor(ArguinRumor));
    assert(!shoal.encounter_requirement(0).empty() && !shoal.encounter_requirement(1).empty());
    shoal.encounter_action(0);
    assert(shoal.s.weather==ShoalLantern && !shoal.heard_rumor(ArguinRumor));
    auto surveyor=shoal;
    surveyor.s.surveyed|=place_bit(arguin);
    ++surveyor.s.charts;
    const int supplies=surveyor.s.stores, day=surveyor.s.day;
    surveyor.encounter_action(1);
    assert(surveyor.heard_rumor(ArguinRumor) && !surveyor.encounter() && surveyor.s.day==day+1);
    assert(surveyor.s.stores==supplies-1 && surveyor.s.renown==1);
    auto leaving=shoal;
    leaving.encounter_action(2);
    assert(!leaving.heard_rumor(ArguinRumor) && leaving.s.day==day);
    shoal.s.cargo[Timber]=1;
    shoal.encounter_action(0);
    assert(!shoal.s.cargo[Timber] && shoal.heard_rumor(ArguinRumor) && !shoal.charted(arguin));
    assert(shoal.s.trust==1 && shoal.s.day==day && !shoal.encounter());
    State reloaded;
    assert(decode(reloaded,encode(shoal.s)) && read_sea(reloaded.sea_log[0]).kind==ShoalLantern);

    Game equatorial;
    assert(equatorial.start_helm(-1));
    equatorial.s.heading=6;
    equatorial.s.helm_x=(Anchor[equator].x-30)*16;
    equatorial.s.helm_y=Anchor[equator].y*16;
    equatorial.s.sail_watch=SailingWatches-1;
    equatorial.s.water=30;
    auto calm=equatorial;
    for (int day=1;calm.s.weather!=Doldrums && day<120;day+=2) {
        for (unsigned seed=1;calm.s.weather!=Doldrums && seed<36;++seed) {
            calm=equatorial;
            calm.s.day=day;
            calm.s.rng=seed;
            calm.helm_step(ocean);
        }
    }
    assert(calm.s.weather==Doldrums && calm.s.helm);
    assert(decode(reloaded,encode(calm.s)) && reloaded.weather==Doldrums);
    assert(!calm.encounter_requirement(0).empty() && !calm.encounter_requirement(1).empty());
    auto drifting=calm;
    drifting.encounter_action(2);
    assert(drifting.s.day==calm.s.day+1 && drifting.s.water<calm.s.water && !drifting.encounter());
    auto pilot=calm;
    pilot.s.first_mate=Pilot;
    pilot.encounter_action(1);
    assert(pilot.s.water==calm.s.water && pilot.s.day==calm.s.day && !pilot.encounter());
    calm.s.cargo[Cloth]=1;
    const int water=calm.s.water;
    calm.encounter_action(0);
    assert(calm.s.day==drifting.s.day && !calm.encounter() && !calm.s.cargo[Cloth]);
    assert(calm.s.water>water && calm.s.water==drifting.s.water+10);
    assert(decode(reloaded,encode(calm.s)) && read_sea(reloaded.sea_log[0]).kind==Doldrums);
}

void expeditions() {
    for (int id=0;id<ExpeditionCount;++id) {
        const auto& study=Expeditions[id];
        Game game;
        game.s.origin=study.archive;
        assert(game.archive_here()==id && game.expedition_here()<0);
        assert(!game.launch_expedition(id,true) && !game.consult_witness(id));
        if (!game.understands(BookLanguage[id]) || !game.understands(WitnessLanguage[id]))
            assert(game.hire_interpreter());
        assert(game.read_expedition(id) && !game.read_expedition(id));
        assert(game.consult_witness(id) && !game.consult_witness(id));
        const bool funded=id%2==0;
        const int gold_before=game.s.gold;
        assert(game.launch_expedition(id,funded) && !game.launch_expedition(id,funded));
        assert(game.s.gold==gold_before+(funded?study.advance:0));
        sail_to(game,study.site);
        assert(game.expedition_here()==id && !game.file_expedition(id));
        if (id==BirdsAtLowWater) {
            game.s.day=game.s.expeditions[id].rival_day+4;
            game.s.day+=game.s.day%2==0;
            const int before=game.s.day;
            assert(!game.observation_block(id).empty() && !game.observe(id,study.method));
            assert(game.s.day==before && game.wait_to_observe(id));
        }
        for (int days=0;days<6 && !game.observation_block(id).empty();++days)
            assert(game.wait_to_observe(id));
        assert(game.observation_block(id).empty());
        const int stores=game.s.stores;
        assert(!game.observe(id,1-study.method));
        assert(game.s.stores==stores-game.daily_rations());
        for (int days=0;days<6 && !game.observation_block(id).empty();++days)
            assert(game.wait_to_observe(id));
        assert(game.observe(id,study.method) && game.expedition_here()<0);
        assert(!game.observe(id,study.method));
        const bool won=game.s.day<=game.s.expeditions[id].rival_day;
        assert(won==(id!=BirdsAtLowWater));
        const int purse=(won?study.purse:study.purse/2)-(funded?study.advance*3/2:0);
        assert(game.expedition_purse(id)==purse);
        State restored;
        assert(decode(restored,encode(game.s)));
        assert(restored.expeditions[id].stage==HasEvidence &&
               restored.expeditions[id].flags==(HeardWitness|(won?FirstObserver:0)|(funded?Sponsored:0)));
        game.s=restored;
        sail_to(game,study.archive);
        const int gold=game.s.gold, fame=game.s.renown;
        assert(game.file_expedition(id) && !game.file_expedition(id));
        assert(game.s.gold==gold+purse && game.s.renown==fame+(won?4:2));
        assert(decode(restored,encode(game.s)) && restored.expeditions[id].stage==FiledReport);
    }
}

void port_languages() {
    Game ship;
    assert(ship.fluency(Portuguese)==Fluent && !ship.can_study(Arabic));
    assert(ship.hire_officer(Pilot) && ship.assign_officer(NoOfficer));
    ship.s.origin=bordeaux;
    assert(ship.crew_interpreter(French)==Pilot && ship.hear_rumor(1));
    assert(!ship.take_lesson(Swahili) && !ship.take_lesson(Portuguese));

    Game reader;
    reader.s.origin=cadiz;
    assert(!reader.read_expedition(TwoShores) && !reader.hear_rumor(3));
    assert(reader.take_lesson(Arabic) && reader.fluency(Arabic)==Phrases);
    assert(reader.s.day==1 && reader.s.gold==502 && !reader.read_expedition(TwoShores));
    assert(reader.take_lesson(Arabic) && reader.fluency(Arabic)==Fluent);
    assert(reader.s.day==3 && reader.s.gold==464 && reader.read_expedition(TwoShores));
    assert(!reader.consult_witness(TwoShores));
    assert(reader.hire_interpreter() && !reader.hire_interpreter());
    assert(reader.consult_witness(TwoShores) && reader.hear_rumor(3));
    State saved;
    assert(decode(saved,encode(reader.s)));
    reader.s=saved;
    assert(reader.local_interpreter(Castilian) && reader.fluency(Arabic)==Fluent);
    assert(reader.depart(lisbon) && reader.s.interpreter_port==-1);
    assert(decode(saved,encode(reader.s)) && saved.interpreter_port==-1);

    Game ledger;
    assert(!ledger.tide_ledger_here());
    ledger.s.origin=tangier;
    assert(ledger.tide_ledger_here() && !ledger.copy_tide_ledger());
    assert(ledger.hire_interpreter() && ledger.copy_tide_ledger() && !ledger.copy_tide_ledger());
    assert(ledger.start_helm(-1) && ledger.s.interpreter_port==-1);
    assert(decode(saved,encode(ledger.s)) && saved.tide_ledger==LedgerCopied);
    ledger.s=State{};
    ledger.s.origin=cadiz;
    ledger.s.tide_ledger=saved.tide_ledger;
    assert(ledger.tide_ledger_here() && !ledger.decide_tide_ledger(true));
    assert(ledger.hire_interpreter());
    auto returned=ledger;
    assert(ledger.decide_tide_ledger(true) && ledger.s.trust==2 && ledger.charted(gibraltar));
    assert(!ledger.decide_tide_ledger(false));
    const int before=returned.s.gold;
    assert(returned.decide_tide_ledger(false) && returned.s.gold==before+100 && returned.charted(gibraltar));
    assert(decode(saved,encode(ledger.s)) && saved.tide_ledger==LedgerPublished);
    ledger.s.language_skills=3U<<(2*Arabic);
    assert(!decode(saved,encode(ledger.s)));
}

void dockside_bargaining() {
    Game ship;
    ship.s.origin=cadiz;
    assert(!ship.bargain_block(Wine,false).empty() && ship.bargain_offer(Wine,false,8).reply==InvalidOffer);
    assert(ship.take_lesson(Castilian) && ship.negotiation_rate()==4);
    const int day=ship.s.day;
    const int before=ship.s.gold;
    auto old_save=ship;
    old_save.s.bargain_good=Wine+1;
    old_save.s.bargain_rate=4;
    State restored;
    assert(decode(restored,encode(old_save.s)));
    assert(old_save.trade_quote(Wine,false,TradeLot).saved>0 && old_save.trade_quote(Wine,false,1).saved==0);
    const auto reply=ship.bargain_offer(Wine,false,8);
    assert(reply.reply==CounterOffer && reply.counter>0 && ship.s.bargain_warnings==1 && ship.s.gold==before);
    assert(decode(restored,encode(ship.s)) && restored.bargain_warnings==1);
    const auto counter=ship.bargain_quote(Wine,false,reply.counter);
    assert(counter.saved>0);
    assert(ship.bargain_offer(Wine,false,reply.counter).reply==DealAgreed && ship.s.day==day);
    assert(ship.s.gold==before-counter.gold && ship.s.cargo[Wine]==TradeLot && ship.s.bargain_good==BargainUsed);
    assert(ship.depart(lisbon) && !ship.s.bargain_good && !ship.s.bargain_warnings);

    Game refusing;
    refusing.s.origin=cadiz;
    refusing.s.trust=1;
    assert(refusing.hire_interpreter() && refusing.negotiation_rate()==8);
    refusing.s.bargain_warnings=2;
    assert(refusing.bargain_offer(Wine,false,18).reply==TalksEnded);
    assert(!refusing.s.trust && refusing.s.bargain_good==BargainUsed);
    assert(refusing.bargain_offer(Wine,false,2).reply==InvalidOffer && refusing.trade(Wine,false,1).units);
    assert(refusing.start_helm(-1) && !refusing.s.bargain_warnings);

    Game seller;
    seller.s.day=4;
    seller.s.cargo[Timber]=TradeLot;
    const int stock=seller.stock(Timber);
    const auto price=seller.bargain_quote(Timber,true,8);
    assert(price.units==TradeLot && price.saved>0);
    assert(seller.bargain_offer(Timber,true,8).reply==DealAgreed);
    assert(!seller.s.cargo[Timber] && seller.stock(Timber)==stock+TradeLot);
    assert(seller.s.gold==520+price.gold && seller.trade_quote(Timber,false,TradeLot).gold>=price.gold);
    Game poor;
    poor.s.gold=0;
    assert(poor.bargain_offer(Wine,false,2).reply==InvalidOffer && !poor.s.bargain_warnings);
}

void riverside_timber() {
    Game ship;
    assert(!ship.resolve_timber_trail(true) && ship.inspect_timber_trail() && !ship.inspect_timber_trail());
    auto reward=ship;
    assert(reward.resolve_timber_trail(false) && reward.s.gold==575 && !reward.resolve_timber_trail(true));
    assert(ship.resolve_timber_trail(true) && ship.s.gold==520 && ship.s.trust==1);
    State saved;
    assert(decode(saved,encode(ship.s)) && saved.timber_trail==TimberReturned);
    assert(reward.bargain_offer(Timber,false,6).reply==CounterOffer);
    assert(ship.bargain_offer(Timber,false,6).reply==DealAgreed);
    const auto spot=town_street(OutskirtsQuay,OutskirtsSites,OutskirtsSites[1]);
    assert(spot && same_town_point(*spot,OutskirtsSites[1]));
    TownWalk walk{.at=OutskirtsSites[0]};
    walk.go(OutskirtsSites[2],OutskirtsQuay,TimberShedTarget,100);
    assert(walk.moving() && walk.update(1500)==TimberShedTarget && same_town_point(walk.at,OutskirtsSites[2]));
}

void inland_survey() {
    Game limited;
    limited.s.stores=3;
    limited.s.water=3;
    assert(!limited.explore_land(FollowStream));
    assert(limited.s.day==0 && limited.s.stores==3 && limited.s.water==3);
    limited.s.stores=4;
    assert(limited.explore_land(FollowStream));
    assert(limited.land_away() && limited.s.day==2 && limited.s.stores==1 && limited.s.water==1);
    assert(!limited.explore_land(CopyMilestone) && !limited.explore_land(SurveyLookout));
    assert(!limited.port() && !limited.depart(madeira) && !limited.start_helm(-1));
    assert(!limited.trade(Wine,false,1).units && !limited.file_land_report());
    State saved;
    assert(decode(saved,encode(limited.s)) && saved.land_stage==LandRidge);
    limited.s=saved;
    assert(limited.explore_land(ReturnToCity) && limited.port() && limited.s.land_stage==LandUnvisited);
    assert(limited.s.day==3 && !limited.s.stores && !limited.s.water);

    Game survey;
    assert(!survey.explore_land(CrossPass) && survey.s.land_stage==LandUnvisited);
    assert(survey.inspect_timber_trail() && survey.knows_land_pass());
    const int food=survey.s.stores, water=survey.s.water;
    assert(survey.explore_land(CrossPass) && survey.s.day==1);
    assert(survey.s.stores==food-2 && survey.s.water==water-3);
    Game quick=survey;
    assert(quick.explore_land(CopyMilestone) && !quick.explore_land(SurveyLookout));
    assert(quick.explore_land(ReturnToCity) && quick.s.land_stage==LandNotes);
    assert(quick.file_land_report() && quick.s.gold==670 && quick.s.renown==1);
    assert(decode(saved,encode(quick.s)) && saved.land_stage==LandFiled && saved.timber_trail==TrailFound);
    assert(survey.explore_land(SurveyLookout) && survey.explore_land(ReturnToCity));
    assert(decode(saved,encode(survey.s)) && saved.land_stage==LandFullNotes);
    assert(survey.s.day==3 && survey.s.stores==food-5 && survey.s.water==water-6);
    assert(survey.file_land_report() && survey.s.gold==735 && survey.s.renown==2);
    assert(survey.s.land_stage==LandFullFiled && !survey.file_land_report() && !survey.explore_land(FollowStream));

    Game with_pilot;
    assert(with_pilot.hire_officer(Pilot));
    assert(with_pilot.knows_land_pass() && with_pilot.explore_land(CrossPass));
}

void nature_observation() {
    Game woods;
    woods.s.origin=madeira;
    assert(woods.nature_site()==NoNatureSite && !woods.record_nature(IslandLaurel));
    assert(woods.explore_land(FollowStream) && woods.nature_site()==LaurelWoods);
    woods.s.sail_watch=2;
    assert(!woods.record_nature(Trocaz) && !woods.nature_wait().empty());
    const int day=woods.s.day, water=woods.s.water;
    assert(woods.wait_for_nature() && woods.s.day==day+1 && woods.s.sail_watch==1);
    assert(woods.record_nature(Trocaz) && woods.record_nature(IslandLaurel));
    assert(!woods.record_nature(Trocaz) && !woods.record_nature(Saltwort));
    assert(woods.s.renown==2 && woods.s.water==water-2 && woods.s.sail_watch==3);
    assert(woods.explore_land(SurveyLookout) && woods.explore_land(ReturnToCity) && woods.file_land_report());
    assert(woods.explore_land(FollowStream) && woods.s.nature_hike && woods.land_away());
    assert(!woods.explore_land(FollowStream) && !woods.explore_land(SurveyLookout) && !woods.start_helm(-1));
    State saved;
    assert(decode(saved,encode(woods.s)) && saved.nature_hike && saved.nature_records==woods.s.nature_records);
    woods.s=saved;
    woods.restore();
    assert(woods.explore_land(ReturnToCity) && woods.port() && woods.land_stage()==LandFullFiled);

    Game marsh;
    marsh.s.origin=cadiz;
    assert(marsh.explore_land(FollowStream) && marsh.nature_site()==SaltMarsh);
    while (marsh.cadiz_low_tide()) ++marsh.s.day;
    assert(!marsh.record_nature(Flamingo));
    assert(marsh.wait_for_nature() && marsh.cadiz_low_tide());
    marsh.s.water=1;
    assert(!marsh.record_nature(Flamingo) && marsh.s.water==1);
    marsh.s.water=3;
    assert(marsh.record_nature(Flamingo) && marsh.record_nature(Saltwort));
    assert(marsh.s.water==1 && marsh.explore_land(ReturnToCity));

    Game sea;
    assert(sea.start_helm(-1));
    const auto move=[&sea](float lon,float lat) {
        const auto point=chart_point(lon,lat);
        sea.s.helm_x=point.x*16;
        sea.s.helm_y=point.y*16;
        while (sea.conditions().weather==Squall) ++sea.s.day;
    };
    move(-16.5f,35);
    assert(sea.nature_site()==MadeiraWaters && !sea.record_nature(CommonDolphin));
    sea.paused=true;
    sea.s.sail_watch=SailingWatches-1;
    const int stores=sea.s.stores;
    sea.s.stores=0;
    assert(!sea.record_nature(CommonDolphin));
    sea.s.stores=stores;
    const int sea_day=sea.s.day;
    assert(sea.record_nature(CommonDolphin) && sea.s.day==sea_day+1 && sea.s.stores<stores);
    assert(sea.s.sail_watch==0 && sea.paused);
    move(-24.4f,15.3f);
    assert(sea.nature_site()==CaboVerdeWaters && !sea.record_nature(Loggerhead));
    sea.s.reefed=1;
    assert(sea.record_nature(Loggerhead) && !sea.record_nature(Loggerhead));
    assert(decode(saved,encode(sea.s)) && saved.nature_records==sea.s.nature_records);
    move(-30,22);
    assert(sea.nature_site()==NoNatureSite);
}

void charter_and_reserve_vessel() {
    Game game;
    game.s.gold=4500;
    assert(!game.open_berth() && game.invest() && game.open_berth());
    assert(game.at_berth() && !game.open_berth());
    game.s.hull=73;
    game.s.guns=2;
    assert(game.repair_cost()==44 && game.purchase_reserve(2) && !game.purchase_reserve(1));
    assert(game.swap_vessel() && game.s.ship==2 && game.s.hull==100 && game.s.guns==0);
    assert(game.s.berth.vessel==1 && game.s.berth.hull==73 && game.s.berth.guns==2);
    game.s.crew=26;
    game.s.cargo[Timber]=32;
    assert(!game.swap_vessel());
    game.s.cargo[Timber]=12;
    game.s.water=130;
    assert(!game.swap_vessel());
    game.s.water=70;
    assert(game.swap_vessel() && game.s.crew==18 && game.s.berth.sailors==8 && game.s.hull==73);
    State saved;
    assert(decode(saved,encode(game.s)) && saved.berth.sailors==8 && saved.berth.guns==0);
    game.s=saved;
    assert(game.swap_vessel() && game.s.crew==26 && game.s.berth.sailors==0);
    game.s.origin=tunis;
    ++game.s.trust;
    assert(game.invest() && !game.open_berth() && !game.swap_vessel() && !game.sell_reserve());
    game.s.origin=lisbon;
    const int price=game.reserve_value(), gold=game.s.gold;
    assert(game.sell_reserve() && game.s.gold==gold+price && !game.s.berth.vessel);
    game.s.origin=tunis;
    assert(!game.open_berth() && game.hire_interpreter() && game.open_berth());
    assert(game.berth_port()==tunis && !game.s.berth.vessel);
}

void aden_merchant_and_colors() {
    Game ship;
    ship.s.cargo[Wine]=2;
    ship.s.chart_marks=place_bit(PlaceCount)-1;
    assert(ship.start_helm(-1));
    const auto shore=Anchor[calicut];
    ship.s.heading=6;
    for (int day=0;day<20 && !ship.encounter();++day) {
        ship.s.day=day;
        ship.s.helm_x=(shore.x-24)*16;
        ship.s.helm_y=shore.y*16;
        ship.s.sail_watch=SailingWatches-1;
        ship.paused=false;
        ship.helm_step([](Point) { return true; });
    }
    assert(ship.s.weather==AdenMerchant);
    State saved;
    assert(decode(saved,encode(ship.s)) && saved.weather==AdenMerchant && !saved.met_aden_ship);
    assert(!ship.encounter_requirement(0).empty() && !ship.encounter_requirement(1).empty());
    ship.s.language_skills|=std::uint32_t(Fluent)<<(2*Arabic);
    Game honest=ship, leave=ship;
    const int water=honest.s.water, gold=honest.s.gold;
    honest.encounter_action(0);
    assert(honest.s.met_aden_ship && !honest.s.calicut_hold_until && honest.s.trust==1);
    assert(honest.s.gold==gold-Game::AdenWaterPrice && honest.s.water==water+12);
    leave.encounter_action(2);
    assert(leave.s.met_aden_ship && !leave.s.calicut_hold_until);
    ship.encounter_action(1);
    assert(ship.s.met_aden_ship && ship.s.calicut_hold_until==ship.s.day+12);
    assert(decode(saved,encode(ship.s)) && saved.calicut_hold_until==ship.s.calicut_hold_until);
    assert(read_sea(saved.sea_log[0]).kind==AdenMerchant && read_sea(saved.sea_log[0]).choice==1);
    ship.s.helm_x=shore.x*16;
    ship.s.helm_y=shore.y*16;
    assert(ship.dock_helm([](Point) { return true; }) && ship.s.origin==calicut);
    assert(ship.market_held() && !ship.trade_limit(Wine,true) && !ship.trade(Wine,false,1).units);
    assert(!ship.invest() && !ship.lease_requirement().empty());
    Game expiry=ship;
    expiry.s.day=expiry.s.calicut_hold_until;
    assert(!expiry.market_held());
    ship.s.origin=malindi;
    assert(!ship.market_held() && !ship.settle_port_case());
    ship.s.origin=calicut;
    assert(ship.settle_port_case() && !ship.market_held() && ship.s.gold==gold-Game::PortSettlement);
    assert(ship.trade_limit(Wine,true)>0);
}

void voyages_in_company() {
    Game flotilla;
    flotilla.s.gold=8000;
    flotilla.s.crew=16;
    assert(flotilla.invest() && flotilla.open_berth() && flotilla.purchase_reserve(1));
    const int single_space=flotilla.capacity(), single_food=flotilla.daily_rations();
    const auto lane=passage(lisbon,madeira);
    const auto alone_forecast=flotilla.forecast(lane);
    assert(flotilla.change_company() && flotilla.s.in_company);
    assert(flotilla.capacity()==single_space+48 && flotilla.daily_rations()==single_food+1);
    assert(flotilla.s.crew==10 && flotilla.s.berth.sailors==6);
    assert(flotilla.forecast(lane).provisions>alone_forecast.provisions);
    assert(!flotilla.sell_reserve() && !flotilla.swap_vessel());
    flotilla.s.cargo[Timber]=40;
    assert(!flotilla.change_company());
    State persisted;
    assert(decode(persisted,encode(flotilla.s)) && persisted.in_company && persisted.berth.sailors==6);
    flotilla.s=persisted;
    sail_to(flotilla,madeira);
    assert(flotilla.s.in_company && !flotilla.change_company() && flotilla.berth_port()==lisbon);
    flotilla.s.berth.hull=9;
    assert(!flotilla.start_helm(-1));
    const int quote=flotilla.repair_cost(), gold=flotilla.s.gold;
    flotilla.service(1);
    assert(flotilla.s.berth.hull==100 && flotilla.s.gold==gold-quote);
    flotilla.s.origin=lisbon;
    flotilla.s.cargo[Timber]=12;
    assert(flotilla.change_company() && !flotilla.s.in_company && flotilla.s.crew>=10);
    assert(flotilla.capacity()==single_space);
}

void royal_colors_and_port_inspections() {
    Game ship;
    ship.s.gold=900;
    assert(ship.display_colors(CastilianColors));
    assert(!ship.receive_royal_pass());
    sail_to(ship,madeira);
    assert(ship.s.colors==MerchantColors && ship.market_held());
    const int deadline=ship.port_hold_until();
    assert(deadline==ship.s.day+8 && ship.s.court_hold_until[Castile]==0);
    State saved;
    assert(decode(saved,encode(ship.s)) && saved.court_hold_until[Portugal]==deadline);
    ship.s.origin=cadiz;
    assert(!ship.market_held() && !ship.settle_port_case());
    ship.s.origin=lisbon;
    assert(ship.market_held() && ship.settle_port_case());
    ship.s.renown=4;
    ship.s.trust=2;
    assert(ship.invest() && ship.receive_royal_pass() && !ship.receive_royal_pass());
    assert(ship.has_royal_pass(Portugal) && ship.display_colors(PortugueseColors));
    const int discount=ship.resupply_cost(100,100);
    assert(ship.display_colors(FurledColors) && discount<ship.resupply_cost(100,100));
    assert(ship.display_colors(PortugueseColors));
    sail_to(ship,madeira);
    assert(!ship.market_held() && ship.s.colors==PortugueseColors);
}

void shore_assignments() {
    Game gold;
    gold.s.origin=elmina;
    assert(gold.shore_task_here()==GoldWash && !gold.advance_shore_task(GoldWash));
    assert(gold.hire_interpreter() && gold.advance_shore_task(GoldWash));
    const int stores=gold.s.stores, day=gold.s.day, price=gold.s.gold;
    assert(gold.advance_shore_task(GoldWash) && gold.s.day==day+2 && gold.s.stores==stores-4);
    assert(gold.advance_shore_task(GoldWash) && gold.s.gold==price+110 && !gold.advance_shore_task(GoldWash));

    Game documents;
    documents.s.origin=kilwa;
    assert(documents.hire_interpreter() && documents.advance_shore_task(KilwaPorcelain));
    assert(!documents.advance_shore_task(KilwaPorcelain));
    documents.s.cargo[Cloth]=1;
    assert(documents.advance_shore_task(KilwaPorcelain) && documents.charted(calicut));
    assert(!documents.advance_shore_task(KilwaPorcelain));
    documents.s.origin=calicut;
    assert(documents.shore_task_here()==KilwaPorcelain && !documents.advance_shore_task(KilwaPorcelain));
    assert(documents.hire_interpreter() && documents.advance_shore_task(KilwaPorcelain));
    State saved;
    assert(decode(saved,encode(documents.s)) && saved.shore_tasks[KilwaPorcelain]==3);
    documents.s.origin=ribeira;
    assert(documents.advance_shore_task(IslandTestimony) && documents.advance_shore_task(IslandTestimony));
    documents.s.stores=3;
    assert(!documents.advance_shore_task(IslandTestimony));
    documents.s.stores=4;
    assert(documents.advance_shore_task(IslandTestimony) && documents.s.stores==0 && documents.s.trust==2);
}

void road_freight() {
    Game game;
    assert(!game.dispatch_offer_here(RoadFreight) && !game.accept_dispatch(RoadFreight));
    assert(game.explore_land(FollowStream) && game.explore_land(SurveyLookout) && game.explore_land(ReturnToCity));
    const int original_stock=game.stock(Timber);
    const int original_quote=game.quote(Timber,false);
    assert(!game.dispatch_offer_here(RoadFreight) && game.file_land_report());
    assert(game.road_timber_bonus()==6 && game.stock(Timber)==original_stock+6);
    assert(game.quote(Timber,false)<original_quote && game.dispatch_offer_here(RoadFreight));

    State legacy;
    assert(decode(legacy,encode(game.s)) && !legacy.road_delivery && legacy.land_stage==LandFullFiled);
    assert(game.accept_dispatch(RoadFreight) && game.dispatch_space()==6 && game.dispatch_target()==madeira);
    assert(decode(legacy,encode(game.s)) && legacy.dispatch.number==RoadFreight+1);
    assert(!game.deliver_dispatch() && game.cancel_dispatch() && game.dispatch_offer_here(RoadFreight));
    assert(game.accept_dispatch(RoadFreight));
    sail_to(game,madeira);
    const int timber=game.stock(Timber);
    assert(timber<=59 && game.deliver_dispatch());
    assert(game.stock(Timber)==timber+6 && game.s.road_delivery && !game.dispatch_space());
    assert(decode(legacy,encode(game.s)) && legacy.road_delivery);
    sail_to(game,lisbon);
    assert(!game.dispatch_offer_here(RoadFreight) && !game.accept_dispatch(RoadFreight));
    assert(game.accept_dispatch(0) && game.cancel_dispatch());
    assert(decode(legacy,encode(game.s)) && legacy.road_delivery && legacy.dispatch.last==1);
    assert(!game.dispatch_offer_here(RoadFreight));
}


void search_for_driftwood() {
    Game ship;
    assert(!ship.take_drift_lead());
    ship.s.origin=madeira;
    assert(ship.take_drift_lead() && !ship.take_drift_lead());
    assert(!ship.sees_drift() && !ship.near_drift());
    ship.s.cargo[Timber]=1;
    assert(ship.start_helm(-1));
    const auto initial=ship.position();
    ship.s.heading=direction_to({DriftGround.x-initial.x,DriftGround.y-initial.y});
    for (int i=0;i<16 && !ship.paused;++i) {
        if (ship.sees_drift()) {
            const auto current=ship.position(), target=ship.drift_position();
            ship.s.heading=direction_to({target.x-current.x,target.y-current.y});
        }
        ship.helm_step([](Point){ return true; });
    }
    assert(ship.paused && ship.near_drift() && ship.sees_drift());
    State saved;
    assert(decode(saved,encode(ship.s)) && saved.drift_stage==DriftSearching);
    auto waiting=ship;
    const int day=waiting.s.day, food=waiting.s.stores, water=waiting.s.water;
    assert(waiting.wait_for_drift() && waiting.s.day==day+1);
    assert(waiting.s.stores==food-waiting.daily_rations() && waiting.s.water<water);
    waiting.s.stores=0;
    assert(!waiting.wait_for_drift() && waiting.s.day==day+1);
    auto without_timber=ship;
    without_timber.s.cargo[Timber]=0;
    assert(!without_timber.recover_drift(RigHook));
    assert(ship.recover_drift(RigHook) && !ship.s.cargo[Timber] && !ship.recover_drift(RigHook));
    assert(decode(saved,encode(ship.s)) && saved.drift_stage==DriftRecovered);

    auto small_boat=waiting;
    bool saw_calm=false, saw_rough=false;
    for (int d=0;d<60 && (!saw_calm || !saw_rough);++d) {
        small_boat.s.day=d;
        const auto drift=small_boat.drift_position();
        small_boat.s.helm_x=drift.x*16;
        small_boat.s.helm_y=drift.y*16;
        const auto conditions=small_boat.conditions();
        if (conditions.weather==Squall || conditions.strength>=4) {
            assert(!small_boat.recover_drift(LaunchBoat));
            saw_rough=true;
        } else {
            auto attempt=small_boat;
            assert(attempt.recover_drift(LaunchBoat) && attempt.s.cargo[Timber]==1);
            saw_calm=true;
        }
    }
    assert(saw_calm && saw_rough);
    Game shore;
    shore.s.origin=madeira;
    shore.s.drift_stage=saved.drift_stage;
    assert(!shore.finish_drift(true));
    sail_to(shore,cadiz);
    shore.s.hull=80;
    assert(!shore.finish_drift(true) && shore.hire_interpreter());
    auto filed=shore;
    assert(shore.finish_drift(true) && shore.repair_cost()==30 && shore.s.trust>=3);
    assert(!shore.finish_drift(false) && decode(saved,encode(shore.s)) && saved.drift_stage==DriftReturned);
    const int gold=filed.s.gold;
    assert(filed.finish_drift(false) && filed.s.gold==gold+180 && filed.repair_cost()==40);
}

void ship_officers() {
    Game ship;
    assert(!ship.hire_officer(Quartermaster) && !ship.assign_officer(Quartermaster));
    const auto original=ship;
    assert(ship.hire_officer(Pilot) && !ship.hire_officer(Pilot));
    assert(ship.s.first_mate==Pilot && ship.s.gold==520-Officers[Pilot].fee);
    Game regular=original;
    assert(regular.depart(bordeaux) && ship.depart(bordeaux));
    assert(!ship.assign_officer(NoOfficer));
    regular.advance();
    ship.advance();
    assert(ship.s.progress==regular.s.progress+12);
    ship=original;
    assert(ship.hire_officer(Pilot) && ship.start_helm(-1));
    ship.s.heading=2;
    ship.helm_step([](Point p){return p.x<=Anchor[lisbon].x;});
    assert(ship.paused && ship.s.hull==97);
    assert(ship.dock_helm());

    ship.s.origin=madeira;
    ship.s.crew=18;
    const auto route=passage(madeira,canaries);
    assert(ship.hire_officer(Quartermaster) && ship.s.first_mate==Pilot);
    assert(ship.assign_officer(NoOfficer));
    const auto forecast=ship.forecast(route);
    assert(ship.assign_officer(Quartermaster) && !ship.assign_officer(Quartermaster));
    const auto lower=ship.forecast(route);
    assert(forecast.days==lower.days && forecast.provisions==lower.provisions);
    assert(lower.water==forecast.water-forecast.days);
    ship.s.water=1;
    assert(ship.depart(canaries));
    ship.advance();
    assert(!ship.s.water && ship.s.hull==97);

    Game combat;
    combat.s.origin=cadiz;
    combat.s.cadiz_victory=1;
    assert(combat.hiring_fee(Boatswain)==85 && combat.hire_officer(Boatswain));
    const auto setup_battle=[](Game& g) {
        assert(g.depart(lisbon));
        g.s.weather=Privateer;
        g.s.enemy=95;
        g.s.range=1;
    };
    auto no_guard=combat;
    assert(no_guard.assign_officer(NoOfficer));
    setup_battle(combat);
    setup_battle(no_guard);
    combat.encounter_action(FireGuns);
    no_guard.encounter_action(FireGuns);
    assert(combat.s.hull==no_guard.s.hull+3);
    combat.encounter_action(BoardShip);
    no_guard.encounter_action(BoardShip);
    assert(combat.duel() && combat.s.vigor==no_guard.s.vigor+6);
    State loaded;
    assert(decode(loaded,encode(combat.s)) && loaded.first_mate==Boatswain &&
           loaded.officer_roster==combat.s.officer_roster && loaded.vigor==combat.s.vigor);
}

void unknown_waters() {
    Game game;
    constexpr int rumor=0;
    const auto site=Rumors[rumor].site;
    const auto shore=Anchor[site];
    const auto exposed=[&] { return game.chart_cell(shore.x/ChartCell,shore.y/ChartCell); };
    assert(game.charted(lisbon) && game.charted(madeira));
    assert(!game.charted(site) && !exposed() && !game.depart(site));
    assert(game.hear_rumor(rumor) && !game.hear_rumor(rumor) && !game.hear_rumor(1));
    assert(game.heard_rumor(rumor) && !game.charted(site) && !exposed() && !game.depart(site));
    assert(game.start_helm(-1) && game.s.destination==-1);
    game.s.helm_x=(shore.x+23)*16;
    game.s.helm_y=shore.y*16;
    for (int i=0;i<10 && !game.paused;++i)
        game.helm_step([](Point) { return true; });
    const auto visited=game.s.visited;
    assert(game.charted(site) && exposed() && game.paused &&
           game.visible_landmark()==site && !(visited&place_bit(site)));
    State loaded;
    assert(decode(loaded,encode(game.s)));
    const auto coast=game.s.coast;
    game.s=loaded;
    game.restore();
    assert(game.charted(site) && game.heard_rumor(rumor) && game.paused && game.s.coast==coast);
    game.paused=false;
    for (int steps=0;steps<12 && game.nearby_anchorage()<0;++steps)
        game.helm_step([](Point) { return true; });
    assert(game.dock_helm() && game.s.origin==site && (game.s.visited&place_bit(site)));
    assert(game.depart(lisbon));

    Game official;
    official.s.origin=madeira;
    assert(official.read_expedition(WesternReturn)==false);
    official.s.origin=lisbon;
    assert(official.read_expedition(WesternReturn));
    assert(!official.charted(azores) && official.consult_witness(WesternReturn) && official.charted(azores));
    assert(official.depart(azores));
}

void harbor_navigation() {
    // A narrow strip of land separates the vessel from a charted Azores anchorage.
    const auto water=[](Point p) {
        return p.x>=0 && p.x<ChartWidth && p.y>=0 && p.y<ChartHeight &&
               !(p.x>=137+LegacyMapX && p.x<=140+LegacyMapX && p.y>=272+LegacyMapY && p.y<=274+LegacyMapY);
    };
    Game ship;
    ship.s.chart_marks|=place_bit(azores);
    assert(ship.start_helm(-1));
    ship.s.helm_x=(140+LegacyMapX)*16;
    ship.s.helm_y=(270+LegacyMapY)*16;
    const auto channel=ship.harbor_approach(azores,water);
    assert(channel.count>2 && !channel.direct);
    assert(channel.points[0].x==140+LegacyMapX && channel.points[0].y==270+LegacyMapY);
    const auto berth=channel.points[channel.count-1];
    assert(berth.x==Anchor[azores].x && berth.y==Anchor[azores].y);
    for (int i=1;i<channel.count;++i) {
        const auto a=channel.points[i-1], b=channel.points[i];
        assert(water(a) && water(b));
        const int dx=b.x-a.x, dy=b.y-a.y;
        if (dx && dy) assert(water({b.x,a.y}) && water({a.x,b.y}));
    }

    ship.s.helm_x=(148+LegacyMapX)*16;
    ship.s.helm_y=(276+LegacyMapY)*16;
    assert(ship.harbor_approach(azores,water).direct);
    assert(ship.nearby_anchorage(water)<0);
    for (int i=0;i<8 && !ship.paused;++i) ship.helm_step(water);
    assert(ship.paused && ship.nearby_anchorage(water)==azores && ship.s.hull==100);
    assert(ship.dock_helm(water) && ship.s.origin==azores && !ship.sea());
    const auto end=ship.wake_point(ship.s.wake_count-1);
    assert(end.x==berth.x && end.y==berth.y);
    State saved;
    assert(decode(saved,encode(ship.s)));
    assert(saved.wake_count==ship.s.wake_count && saved.wake_head==ship.s.wake_head);
    assert(ship.start_helm(-1) && ship.s.wake_count==1);

    const auto open_atlantic=chart_point(-40.f,55.f);
    ship.s.helm_x=open_atlantic.x*16;
    ship.s.helm_y=open_atlantic.y*16;
    ship.s.heading=2;
    ship.s.day=99999;
    for (int i=0;i<80;++i) ship.helm_step(water);
    assert(ship.s.wake_count==WakeCapacity);
    int previous=-1;
    for (int i=0;i<ship.s.wake_count;++i) {
        const auto point=ship.wake_point(i);
        assert(point.x>previous && point.y==open_atlantic.y);
        previous=point.x;
    }
    assert(decode(saved,encode(ship.s)) && saved.wake_count==WakeCapacity);
}

void port_stories() {
    Game game;
    assert(!game.meet_port_story(LastSugar) && !game.resolve_port_story(TideSkiff,1));
    assert(game.meet_port_story(TideSkiff) && !game.meet_port_story(TideSkiff));
    assert(!game.resolve_port_story(TideSkiff,0));
    game.s.cargo[Timber]=1;
    assert(game.resolve_port_story(TideSkiff,0));
    assert(!game.s.cargo[Timber] && game.s.gold==574 && game.s.renown==2);
    assert(!game.resolve_port_story(TideSkiff,1));

    game.s.origin=madeira;
    assert(game.meet_port_story(LastSugar));
    game.s.cargo[Wine]=game.capacity()-game.load()-1;
    assert(!game.resolve_port_story(LastSugar,0) && game.s.gold==574);
    game.s.cargo[Wine]=0;
    assert(game.resolve_port_story(LastSugar,0));
    assert(game.s.cargo[Sugar]==2 && game.s.gold==554);

    game.s.origin=canaries;
    assert(game.meet_port_story(TornChart));
    Game with_chart=game;
    assert(with_chart.resolve_port_story(TornChart,0));
    assert(with_chart.charted(ribeira) && with_chart.s.gold==526);
    assert(game.resolve_port_story(TornChart,1));
    assert(game.heard_rumor(RibeiraRumor) && !game.charted(ribeira) && game.s.gold==554);

    game.s.origin=cadiz;
    assert(game.meet_port_story(TwoLetters) && !game.resolve_port_story(TwoLetters,1));
    assert(game.hire_interpreter() && game.resolve_port_story(TwoLetters,1));
    assert(game.s.renown==6 && game.s.trust==2);
    State restored;
    assert(decode(restored,encode(game.s)) && restored.port_stories==game.s.port_stories);
    assert(decode(restored,encode(State{})) && !restored.port_stories);
}

void port_dispatches() {
    for (int i=0;i<DispatchCount;++i) {
        const auto& order=DispatchOrders[i];
        Game game;
        game.s.origin=order.from;
        assert(game.dispatch_board_here());
        assert(game.dispatch_quote(i).days>0 && game.dispatch_quote(i).fee>0);
    }

    Game courier;
    courier.take_contract();
    const int starting_gold=courier.s.gold, cargo=courier.load();
    const auto offer=courier.dispatch_quote(0);
    assert(courier.accept_dispatch(0) && !courier.accept_dispatch(1));
    assert(courier.s.gold==starting_gold-18 && courier.load()==cargo+1);
    assert(courier.s.contract==madeira && courier.dispatch_target()==cadiz);
    assert(!courier.deliver_dispatch());
    sail_to(courier,cadiz);
    assert(courier.s.day<=courier.s.dispatch.due);
    const int arrival_gold=courier.s.gold;
    assert(courier.deliver_dispatch() && !courier.deliver_dispatch());
    assert(courier.s.gold==arrival_gold+offer.fee+18 && courier.load()==cargo &&
           courier.s.dispatch.outcome==DispatchOnTime);

    Game freighter;
    freighter.s.cargo[Wine]=freighter.capacity()-freighter.load()-5;
    assert(!freighter.accept_dispatch(1));
    --freighter.s.cargo[Wine];
    assert(freighter.accept_dispatch(1) && freighter.load()==freighter.capacity());
    assert(!freighter.trade_quote(Cloth,false,1).units);
    assert(freighter.cancel_dispatch() && !freighter.cancel_dispatch());
    assert(freighter.s.gold==520-45 && freighter.s.dispatch.outcome==DispatchCancelled);

    Game passenger;
    assert(passenger.accept_dispatch(2));
    sail_to(passenger,canaries);
    assert(passenger.dispatch_target()==madeira && !passenger.deliver_dispatch());
    sail_to(passenger,madeira);
    assert(passenger.s.dispatch.stopped && passenger.dispatch_target()==canaries);
    while (passenger.s.day<=passenger.s.dispatch.due) passenger.wait_at_port();
    assert(passenger.depart(canaries));
    Game resumed;
    assert(decode(resumed.s,encode(passenger.s)));
    resumed.restore();
    assert(resumed.paused && resumed.s.dispatch.stopped && resumed.dispatch_space()==4);
    resumed.paused=false;
    while (resumed.sea() && !resumed.encounter()) resumed.advance();
    assert(resumed.port() && resumed.s.origin==canaries);
    const int late_gold=resumed.s.gold;
    const int late_renown=resumed.s.renown;
    assert(resumed.deliver_dispatch() && resumed.s.dispatch.outcome==DispatchLate);
    assert(resumed.s.gold==late_gold+35+resumed.dispatch_quote(2).fee/2 && resumed.s.renown==late_renown);
    State impossible=freighter.s;
    impossible.dispatch={.due=5,.number=2};
    assert(decode(impossible,encode(impossible)));
    ++impossible.cargo[Wine];
    assert(!decode(impossible,encode(impossible)));

    Game rescue;
    assert(rescue.accept_dispatch(0) && rescue.depart(cadiz));
    rescue.s.stores=0;
    rescue.s.water=0;
    rescue.s.hull=1;
    rescue.advance();
    assert(rescue.port() && !rescue.dispatch_space() && rescue.s.dispatch.outcome==DispatchLost);

    Game visited;
    visited.s.origin=canaries;
    assert(!visited.accept_dispatch(7));
    visited.s.visited|=place_bit(ribeira);
    assert(visited.accept_dispatch(7) && decode(visited.s,encode(visited.s)));
}

void expanded_world_and_land() {
    Game ship;
    State saved;
    assert(!ship.charted(calicut) && !ship.charted(santo_domingo) && town_index(calicut)>=0);
    ship.s.chart_marks|=place_bit(mandeb)|place_bit(calicut);
    ship.s.surveyed|=place_bit(mandeb);
    assert(decode(saved,encode(ship.s)) && saved.chart_marks==ship.s.chart_marks &&
           saved.surveyed==ship.s.surveyed);
    for (const int port : {bergen,vardo,muscat,hormuz,calicut,santo_domingo}) {
        const auto route=passage(lisbon,port);
        assert(route.miles>300 && route.count>0 && route.count<=32);
    }

    Game summer, winter;
    summer.s.origin=winter.s.origin=malindi;
    summer.s.day=150;
    const auto indian_ocean=passage(malindi,calicut);
    assert(summer.forecast(indian_ocean).days<winter.forecast(indian_ocean).days);

    ship.s.origin=madeira;
    assert(ship.hire_officer(Quartermaster) && ship.land_cost(CrossPass).water==2);
    assert(ship.explore_land(CrossPass) && ship.explore_land(SurveyLookout));
    assert(ship.land_away() && !ship.start_helm(-1));
    assert(decode(saved,encode(ship.s)) && saved.inland_stages[0]==LandLookout);
    const int purse=ship.s.gold, trust=ship.s.trust;
    assert(ship.explore_land(ReturnToCity) && ship.file_land_report() && !ship.file_land_report());
    assert(ship.s.gold==purse+245 && ship.s.trust==trust+1 && ship.s.land_stage==LandUnvisited);

    Game salt;
    salt.s.origin=cadiz;
    salt.s.sail_watch=1;
    assert(!salt.cadiz_low_tide() && !salt.explore_land(CrossPass));
    const int rations=salt.s.stores, water=salt.s.water;
    assert(salt.wait_land_tide() && salt.cadiz_low_tide() && salt.s.stores==rations && salt.s.water==water);
    assert(salt.explore_land(CrossPass) && salt.explore_land(CopyMilestone));
    assert(salt.explore_land(ReturnToCity) && salt.file_land_report());
    assert(salt.s.gold==680 && decode(saved,encode(salt.s)) && saved.inland_stages[1]==LandFiled);
}

int main() {
    expanded_world_and_land();
    for (int from=0; from<PlaceCount; ++from)
        for (int to=0; to<PlaceCount; ++to) {
            const auto route=passage(from,to);
            assert(from==to ? route.count==0 : route.count>0 && route.count<=32);
            if (from==to) continue;
            const auto a=vessel_at(route,0,0), b=vessel_at(route,route.count-1,1);
            assert(a.x==Anchor[from].x && a.y==Anchor[from].y);
            assert(b.x==Anchor[to].x && b.y==Anchor[to].y);
        }
    Game merchant;
    int gold=merchant.s.gold;
    assert(merchant.stock(Wine)>3);
    merchant.take_contract();
    assert(merchant.s.contract==madeira && merchant.s.contract_good==Wine);
    const int initial_price=merchant.quote(Wine,false);
    merchant.trade(Wine,false,5);
    assert(merchant.s.cargo[Wine]==5 && merchant.s.gold<gold);
    assert(merchant.quote(Wine,false)>initial_price);
    sail_to(merchant,madeira);
    merchant.deliver();
    assert(merchant.s.contract==-1 && merchant.s.cargo[Wine]==2);
    merchant.trade(Wine,true,2);
    assert(merchant.s.cargo[Wine]==0 && merchant.s.gold>gold);
    merchant.service(5);
    assert(merchant.s.guns==1);
    assert(merchant.depart(canaries));
    merchant.advance();
    assert(merchant.sea());
    const auto bytes=encode(merchant.s);
    Game loaded;
    assert(decode(loaded.s,bytes));
    loaded.restore();
    assert(loaded.paused && loaded.s.progress==merchant.s.progress);
    loaded.turn_back();
    for (int i=0;i<20 && loaded.sea();++i) loaded.advance();
    assert(loaded.s.origin==madeira && !loaded.sea());

    Game explorer;
    sail_to(explorer,gibraltar);
    int reward=explorer.s.gold;
    explorer.survey();
    assert(explorer.s.charts==1 && explorer.s.gold>reward);
    reward=explorer.s.gold;
    explorer.survey();
    assert(explorer.s.charts==1 && explorer.s.gold==reward);
    sail_to(explorer,tunis);
    explorer.navigator();
    assert(explorer.s.allies==1 && explorer.s.cargo[Salt]==0);

    Game combat;
    assert(combat.depart(madeira));
    combat.s.weather=2;
    combat.s.enemy=46;
    combat.s.range=3;
    combat.encounter_action(0);
    assert(combat.s.range==2 && combat.s.aim==1 && combat.s.hull<100);
    int hull=combat.s.hull;
    combat.encounter_action(1);
    assert(combat.s.enemy<46 && combat.s.aim==0 && combat.s.hull<hull);
    while (combat.encounter())
        combat.encounter_action(combat.s.needs_reload ? CloseRange : FireGuns);
    assert(combat.s.renown>=2);
    port_tales();
    sea_events();
    voyage_planning();
    trading_quotes();
    merchant_trade();
    port_market_cycles();
    custom_trade_quantities();
    town_walks();
    port_stories();
    port_dispatches();
    naval_and_duel_combat();
    new_events_and_investment();
    free_helm();
    continuous_helm();
    chart_skirmish();
    coastal_fishery();
    island_settlements();
    merchant_raids_and_patrols();
    sailing_units();
    weather_forecasts();
    sailing_rations();
    sea_stories();
    expeditions();
    port_languages();
    dockside_bargaining();
    riverside_timber();
    inland_survey();
    nature_observation();
    charter_and_reserve_vessel();
    aden_merchant_and_colors();
    voyages_in_company();
    royal_colors_and_port_inspections();
    shore_assignments();
    road_freight();
    search_for_driftwood();
    ship_officers();
    unknown_waters();
    harbor_navigation();
    story_journey(1,0);
    story_journey(2,1);
    std::puts("Routes, markets, combat, expeditions, exploration, navigation and saves passed.");
}
