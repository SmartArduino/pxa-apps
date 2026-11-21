#include "game.hpp"

#include <cassert>
#include <cstdio>

using namespace tide;

void sail_to(Game& game, int destination) {
    game.s.chart_marks|=1U<<destination;
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
    game.s.gold=999999;
    assert(game.trade_quote(Salt,true,5).units==0);
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
    following.helm_step(ocean);
    beating.helm_step(ocean);
    const auto distance=[&](Point p) {
        const int x=p.x-anchor.x, y=p.y-anchor.y;
        return x*x+y*y;
    };
    assert(distance(following.position())>distance(beating.position()));
    for (int i=0;i<4;++i) ship.helm_step(ocean);
    assert(ship.s.day==1 && ship.s.stores==73 && ship.s.water<75);
    State save;
    assert(decode(save,encode(ship.s)) && save.helm && save.helm_x==ship.s.helm_x);
    ship.s=save;
    ship.restore();
    assert(ship.paused && ship.s.heading==save.heading);

    Game grounded;
    assert(grounded.start_helm(-1));
    grounded.s.heading=2;
    grounded.helm_step([](Point p){return p.x<=248;});
    assert(grounded.paused && grounded.s.hull==94 && grounded.position().x==248);
    assert(grounded.dock_helm() && grounded.s.origin==lisbon && !grounded.sea());
    assert(grounded.resupply(80,74) && grounded.s.stores==80 && grounded.s.water==74);
    assert(grounded.resupply(80,85) && grounded.s.stores==80 && grounded.s.water==85);
    assert(grounded.resupply(80,110) && grounded.s.water==100 && grounded.s.stores==80);
    assert(grounded.food_capacity()==80 && !grounded.resupply(81,100));
    assert(grounded.unload_provisions(true));
    assert(grounded.resupply(90,90) && grounded.s.stores+grounded.s.water==180);
    assert(decode(save,encode(grounded.s)));
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
    assert(!ship.can_negotiate(Wine) && !ship.negotiate(Wine));
    assert(ship.take_lesson(Castilian) && ship.negotiation_rate()==4);
    const int day=ship.s.day;
    assert(ship.negotiate(Wine) && ship.s.day==day+1);
    assert(!ship.negotiate(Sugar));
    State restored;
    assert(decode(restored,encode(ship.s)));
    ship.s=restored;
    Game usual=ship;
    usual.s.bargain_good=usual.s.bargain_rate=0;
    const auto regular=usual.trade_quote(Wine,false,TradeLot);
    const auto lot=ship.trade_quote(Wine,false,TradeLot);
    assert(lot.units==TradeLot && lot.saved==regular.gold*4/100 &&
           lot.gold==regular.gold-lot.saved && lot.stock_change==regular.stock_change);
    assert(!ship.trade_quote(Wine,true,TradeLot).saved && !ship.trade_quote(Wine,false,1).saved &&
           !ship.trade_quote(Sugar,false,TradeLot).saved);
    auto short_order=ship;
    short_order.s.gold=ship.quote(Wine,false)*2;
    const auto partial=short_order.trade(Wine,false,TradeLot);
    assert(partial.units>0 && partial.units<TradeLot && !partial.saved && short_order.s.bargain_good==Wine+1);
    ship.s.gold=lot.gold;
    assert(ship.trade(Wine,false,TradeLot).gold==lot.gold && ship.s.gold==0 &&
           ship.s.bargain_good==BargainUsed && !ship.negotiate(Wine));
    assert(ship.depart(lisbon) && !ship.s.bargain_good && !ship.s.bargain_rate);

    Game interpreter;
    interpreter.s.origin=cadiz;
    assert(interpreter.hire_interpreter() && interpreter.negotiation_rate()==8);
    assert(interpreter.negotiate(Wine) && interpreter.s.bargain_rate==8);
    assert(interpreter.start_helm(-1) && !interpreter.s.bargain_good);
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
    assert(ship.s.progress==regular.s.progress+20);
    ship=original;
    assert(ship.hire_officer(Pilot) && ship.start_helm(-1));
    ship.s.heading=2;
    ship.helm_step([](Point p){return p.x<=248;});
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
           game.visible_landmark()==site && !(visited&(1U<<site)));
    State loaded;
    assert(decode(loaded,encode(game.s)));
    const auto coast=game.s.coast;
    game.s=loaded;
    game.restore();
    assert(game.charted(site) && game.heard_rumor(rumor) && game.paused && game.s.coast==coast);
    game.paused=false;
    for (int steps=0;steps<12 && game.nearby_anchorage()<0;++steps)
        game.helm_step([](Point) { return true; });
    assert(game.dock_helm() && game.s.origin==site && (game.s.visited&(1U<<site)));
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
               !(p.x>=137 && p.x<=140 && p.y>=272 && p.y<=274);
    };
    Game ship;
    ship.s.chart_marks|=1U<<azores;
    assert(ship.start_helm(-1));
    ship.s.helm_x=140*16;
    ship.s.helm_y=270*16;
    const auto channel=ship.harbor_approach(azores,water);
    assert(channel.count>2 && !channel.direct);
    assert(channel.points[0].x==140 && channel.points[0].y==270);
    const auto berth=channel.points[channel.count-1];
    assert(berth.x==Anchor[azores].x && berth.y==Anchor[azores].y);
    for (int i=1;i<channel.count;++i) {
        const auto a=channel.points[i-1], b=channel.points[i];
        assert(water(a) && water(b));
        const int dx=b.x-a.x, dy=b.y-a.y;
        if (dx && dy) assert(water({b.x,a.y}) && water({a.x,b.y}));
    }

    ship.s.helm_x=148*16;
    ship.s.helm_y=276*16;
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

    ship.s.helm_x=25*16;
    ship.s.helm_y=100*16;
    ship.s.heading=2;
    ship.s.day=99999;
    for (int i=0;i<80;++i) ship.helm_step(water);
    assert(ship.s.wake_count==WakeCapacity);
    int previous=-1;
    for (int i=0;i<ship.s.wake_count;++i) {
        const auto point=ship.wake_point(i);
        assert(point.x>previous && point.y==100);
        previous=point.x;
    }
    assert(decode(saved,encode(ship.s)) && saved.wake_count==WakeCapacity);
}

int main() {
    for (int from=0; from<PlaceCount; ++from)
        for (int to=0; to<PlaceCount; ++to) {
            const auto route=passage(from,to);
            assert(from==to ? route.count==0 : route.count>0);
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
    naval_and_duel_combat();
    new_events_and_investment();
    free_helm();
    expeditions();
    port_languages();
    dockside_bargaining();
    search_for_driftwood();
    ship_officers();
    unknown_waters();
    harbor_navigation();
    story_journey(1,0);
    story_journey(2,1);
    std::puts("Routes, markets, combat, expeditions, exploration, navigation and saves passed.");
}
