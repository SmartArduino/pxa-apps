#include "../combat_animation.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdio>

using namespace wind;

void ticks(Game& game,int count) {
    for (int i=0;i<count;++i) game.step();
}

void walking_and_visits() {
    Game town;
    town.reset();
    town.s.heroes[1]={};
    assert(town.build({7,10},Forge) && town.upgrade(3));
    auto& hero=town.s.heroes[0];
    hero.hp=40;
    hero.energy=100;
    hero.wallet=11;
    ticks(town,3);
    const auto& walker=town.walkers[0];
    assert(walker.shop==1 && walker.length>0);
    assert(town.activity(0)==Activity::ToShop);
    while (walker.next<walker.length) town.step();
    assert(walker.walk_beats==WalkTileBeats && town.s.visits==0);
    ticks(town,WalkTileBeats-1);
    assert(walker.walk_beats==1 && town.s.visits==0);
    ticks(town,3);
    assert(hero.wallet==0 && town.s.visits==1 && walker.dwell>0);
    assert(town.activity(0)==Activity::Shop);

    assert(town.launch(0,1));
    assert(town.activity(0)==Activity::ToCave);
    town.step();
    const auto first=walker.at;
    ticks(town,ExpeditionTileBeats-1);
    assert(walker.at==first && walker.walk_beats==1);
    town.step();
    assert(walker.at!=first && walker.walk_beats==ExpeditionTileBeats);
    while (walker.next<walker.length) town.step();
    assert(walker.at==Cave && town.s.trip.phase==Outbound);
    ticks(town,ExpeditionTileBeats-1);
    assert(town.s.trip.phase==Outbound);
    town.step();
    assert(town.s.trip.phase==Exploring && town.activity(0)==Activity::CaveTrail);
}

void combat_timing() {
    State s{};
    s.trip={0,3,Fighting,24,0,1,0};
    s.heroes[0].hp=100;
    s.heroes[1].hp=82;
    CombatAnimation motion;
    motion.record(32,{100,100},s);
    assert(motion.guardian_hp(s.trip,99)==32 && motion.guardian_hp(s.trip,100)==24);
    assert(motion.ally_pose(99)==1 && motion.ally_pose(100)==2);
    assert(motion.guardian_pose(100)==1 && motion.guardian_pose(220)==2);
    assert(motion.ally_hp(s.heroes[1],1,289)==100 && motion.ally_hp(s.heroes[1],1,290)==82);
    assert(motion.ally_hp(s.heroes[0],0,289)==100 && motion.guardian_pose(290)==3);
    assert(motion.guardian_pose(366)==4 && motion.guardian_pose(470)==0);

    s.trip.hp=0;
    s.trip.phase=Returning;
    s.trip.result=1;
    motion.record(24,{100,82},s);
    assert(motion.finishing(s.trip,239) && !motion.finishing(s.trip,240));
    assert(motion.guardian_hp(s.trip,99)==24 && motion.guardian_hp(s.trip,100)==0);
    assert(motion.target<0 && motion.guardian_pose(290)==0);
    for (int i=0;i<12;++i) motion.step();
    assert(motion.age==CombatAnimation::End);
}

template<size_t Size>
std::array<std::byte,Size> older_save(const State& state,char version) {
    std::array<std::byte,Size> bytes{};
    const auto current=encode(state);
    std::copy_n(current.begin(),Size-4,bytes.begin());
    bytes[3]=std::byte(version);
    uint32_t hash=2166136261u;
    for (std::size_t i=0;i<Size-4;++i)
        hash=(hash^std::to_integer<unsigned>(bytes[i]))*16777619u;
    for (std::size_t i=0;i<4;++i) bytes[Size-4+i]=std::byte((hash>>(8*i))&255u);
    return bytes;
}

void shop_management() {
    Game town;
    town.reset();
    town.s.heroes[1]={};
    town.s.count=2;
    town.s.day=3;
    assert(town.accept_commission(1) && town.s.commission.kind==CommissionKind::Hunt);
    auto& hero=town.s.heroes[0];
    auto& inn=town.s.buildings[1];
    hero.wallet=8;
    hero.hp=40;
    assert(!town.set_service_policy(0,ServicePolicy::Value));
    assert(!town.set_service_policy(1,static_cast<ServicePolicy>(255)));
    assert(town.set_service_policy(1,ServicePolicy::Value));
    assert(service_price(inn,ServicePolicy::Value)==8 && service_price(inn,ServicePolicy::Standard)==11);
    ticks(town,3);
    assert(town.walkers[0].shop==1);

    Game repriced=town;
    assert(repriced.set_service_policy(1,ServicePolicy::Premium));
    ticks(repriced,55);
    assert(repriced.s.heroes[0].wallet==8 && repriced.s.visits==0);
    assert(repriced.s.buildings[1].account.today.revenue==0);

    Game standard=town, premium=town;
    assert(standard.set_service_policy(1,ServicePolicy::Standard));
    assert(premium.set_service_policy(1,ServicePolicy::Premium));
    standard.s.heroes[0].wallet=premium.s.heroes[0].wallet=20;
    for (int i=0;i<55 && !town.s.visits;++i) {
        town.step();
        standard.step();
        premium.step();
    }
    assert(town.s.visits==1 && standard.s.visits==1 && premium.s.visits==1);
    assert(hero.wallet==0 && inn.account.today.visits==1 && inn.account.today.revenue==8 && town.s.income==8);
    assert(hero.mood==standard.s.heroes[0].mood+2);
    assert(premium.s.heroes[0].hp==standard.s.heroes[0].hp+6);
    assert(premium.s.buildings[1].account.today.revenue==16);

    town.s.clock=Game::DayBeats-1;
    town.step();
    assert(inn.account.yesterday.visits==1 && inn.account.yesterday.revenue==8 && inn.account.today.revenue==0);
    State loaded;
    assert(decode(loaded,encode(town.s)) && loaded.buildings[1].account.policy==ServicePolicy::Value);
    assert(loaded.buildings[1].account.yesterday.revenue==8 && loaded.commission.kind==CommissionKind::Hunt);
    assert(decode(loaded,older_save<DisciplineSaveSize>(town.s,'8')));
    assert(loaded.buildings[1].account.policy==ServicePolicy::Standard);
    assert(loaded.buildings[1].account.yesterday.revenue==0 && loaded.commission.kind==CommissionKind::Hunt);
}

void training_and_housing() {
    Game town;
    town.reset();
    const int gold=town.s.gold, energy=town.s.heroes[0].energy;
    assert(!town.train(-1) && !town.train(3));
    assert(town.train(0) && town.s.heroes[0].level==2);
    assert(town.s.gold==gold-Game::TrainingCost && town.s.heroes[0].energy==energy-Game::TrainingEnergy);
    assert(!town.train(0) && town.trained(0));
    assert(!town.assign_home(0,1));
    town.s.gold=600;
    assert(town.build({7,11},House));
    const int house=town.building({7,11});
    assert(!town.assign_home(0,house));
    for (auto& h:town.s.heroes) if (h.level) h.mood=Game::SettleMood;
    assert(town.build({9,11},House));
    const int second=town.building({9,11});
    assert(town.assign_home(0,second) && !town.assign_home(1,second));
    const int fame=town.s.fame;
    assert(town.assign_home(0,house) && town.s.fame==fame && !town.occupants(second));
    assert(town.upgrade(house) && !town.assign_home(1,house));
    assert(town.assign_home(1,second));
    assert(town.occupants(house)==1 && town.occupants(second)==1 && town.daily_rent()==9);

    State shared=town.s;
    shared.heroes[1].home=uint8_t(house);
    Game legacy;
    assert(decode(legacy.s,encode(shared)));
    legacy.restore();
    assert(legacy.occupants(house)==2 && legacy.daily_rent()==10);
    assert(legacy.assign_home(1,second) && legacy.occupants(house)==1);

    Game restored;
    assert(decode(restored.s,encode(town.s)));
    restored.restore();
    assert(restored.trained(0) && restored.daily_rent()==9 && restored.s.heroes[0].home==house);
    restored.s.clock=Game::EveningBeat;
    ticks(restored,40);
    assert(restored.activity(0)==Activity::Home && restored.activity(1)==Activity::Home);
    const int treasury=restored.s.gold;
    const int wallet=restored.s.heroes[0].wallet;
    restored.s.clock=Game::DayBeats-1;
    restored.step();
    assert(restored.s.yesterday==21 && restored.s.gold==treasury+21);
    assert(restored.s.heroes[0].wallet==std::min(120,wallet+40)-5);
    assert(!restored.trained(0) && restored.can_train(0));
    ticks(restored,12);
    assert(!restored.walkers[0].at_home && !restored.walkers[1].at_home);
}

void southern_routes_and_cache() {
    Game town;
    town.reset();
    const auto old=older_save<ShopSaveSize>(town.s,'9');
    assert(decode(town.s,old));
    town.restore();
    assert(town.traffic[cell({8,22})] && town.traffic[cell({18,22})]);
    assert(town.terrain({14,19}).ground==Bridge && town.terrain({15,19}).ground==Stair);
    assert(town.build({9,18},Road));
    assert(town.build({9,20},House));
    State copy;
    assert(decode(copy,encode(town.s)) && copy.buildings[3].at()==Point(9,20));
    assert(copy.roads[cell({9,18})/8]&(1u<<(cell({9,18})%8)));

    Game& forest=town;
    forest.s.clock=uint8_t(wild_spawn_clock(WildSites[2],forest.s.day));
    forest.step();
    assert(forest.wild[2].hp==WildSites[2].health);
    forest.s.clock=Game::EveningBeat;
    forest.step();
    assert(!forest.wild[2].hp);

    Game cave;
    cave.reset();
    assert(cave.launch(0,3));
    while (cave.s.trip.phase==Outbound) cave.step();
    ticks(cave,cave_length(0,Route::Standard)/4);
    assert(cave.s.trip.phase==Exploring && cave.s.trip.patrol_hp==patrol_health(0,Route::Standard));
    assert(decode(copy,encode(cave.s)) && copy.trip.patrol_hp==cave.s.trip.patrol_hp);
    cave.s=copy;
    cave.restore();
    assert(cave.walkers[0].at==Cave);
    for (int i=0;i<100 && !cave.s.trip.cache_found;++i) cave.step();
    assert(cave.s.trip.phase==Exploring && cave.s.trip.cache_found && cave.s.relics==1);
    assert(cave.owned(Item::LeafPotion)==2 && cave.retreat());
    while (cave.s.trip.phase!=Idle) cave.step();
    assert(cave.launch(0,3));
    while (cave.s.trip.phase==Outbound || cave.s.trip.phase==Exploring) cave.step();
    assert(cave.s.trip.phase==Fighting && !cave.s.trip.cache_found && cave.s.relics==1);
}

void cave_fork_choices() {
    Game game;
    game.reset();
    assert(game.launch(0,3));
    for (int i=0;i<400 && !waiting_at_fork(game.s.trip);++i) game.step(true);
    assert(waiting_at_fork(game.s.trip) && game.s.trip.cache_found);
    const int distance=game.s.trip.distance;
    for (int i=0;i<20;++i) game.step(true);
    assert(waiting_at_fork(game.s.trip) && game.s.trip.distance==distance);

    Game side;
    assert(decode(side.s,older_save<ForkSaveSize>(game.s,'B')));
    side.restore();
    assert(waiting_at_fork(side.s.trip) && side.walkers[0].at==Cave);
    const int stamina=side.s.heroes[0].energy;
    side.s.heroes[0].energy=Game::SideCaveCost-1;
    assert(!side.choose_cave_path(CavePath::Side) && side.can_choose_cave_path(CavePath::Main));
    side.s.heroes[0].energy=uint8_t(stamina);
    assert(side.choose_cave_path(CavePath::Side));
    assert(side.s.heroes[0].energy==stamina-Game::SideCaveCost && !side.choose_cave_path(CavePath::Side));
    assert(cave_distance(side.s.trip)>cave_distance(game.s.trip));
    bool fought_side_patrol=false;
    for (int i=0;i<260 && !side.s.trip.side_cache_found;++i) {
        side.step();
        if (in_side_cave(side.s.trip) && side.s.trip.patrol_hp) fought_side_patrol=true;
    }
    assert(fought_side_patrol && side.s.trip.side_cache_found && side.s.relics==4);
    assert(side.owned(Item::BlueTonic)==1 && side.retreat());
    while (side.s.trip.phase!=Idle) side.step();
    assert(side.launch(0,3));
    for (int i=0;i<400 && !waiting_at_fork(side.s.trip);++i) side.step(true);
    assert(side.choose_cave_path(CavePath::Side));
    while (side.s.trip.phase==Exploring) side.step();
    assert(side.s.trip.phase==Fighting && side.s.relics==4 &&
           !side.s.trip.side_cache_found && side.owned(Item::BlueTonic)==1);

    game.step();
    assert(game.s.trip.cave_path==CavePath::Main && game.s.trip.distance==distance+1);
    State restored;
    assert(decode(restored,older_save<SouthernSaveSize>(game.s,'A')) &&
           restored.trip.cave_path==CavePath::Main && restored.trip.distance==distance+1);
}

void frontier_hunts() {
    constexpr auto route=Route::Frontier;
    Game town;
    town.reset();
    assert(!town.frontier_visible() && !town.launch(0,3,Tactic::Balanced,route));
    town.s.rank=FrontierHunt.rank;
    town.s.heroes[0].gear=3;
    town.s.heroes[1].gear=3;
    assert(town.traffic[cell(FrontierGround)] && town.frontier_visible());
    assert(!town.floor_open(1,route) && town.accept_commission(0));
    assert(town.launch(0,3,Tactic::Guard,route) && town.activity(0)==Activity::ToFrontier);
    assert(!town.launch(0,3) && !town.launch(0,3,Tactic::Guard,route));
    const auto& walker=town.walkers[0];
    assert(walker.length>=10);
    town.step();
    assert(walker.walk_tile_beats==WalkTileBeats);
    for (int i=0;i<300 && town.s.trip.phase==Outbound;++i) town.step();
    assert(town.s.trip.phase==Fighting && walker.at==frontier_party_tile(3,0));
    assert(town.walkers[1].at==frontier_party_tile(3,1));
    assert(town.activity(0)==Activity::FrontierBattle && !town.s.trip.distance &&
           !town.can_choose_cave_path(CavePath::Main));
    assert(heavy_round(0,route,3) && !heavy_round(0,route,2));

    Game battle;
    assert(decode(battle.s,encode(town.s)));
    battle.restore();
    assert(battle.walkers[0].at==frontier_party_tile(3,0));
    assert(battle.retreat() && battle.s.trip.result==2 && !battle.s.frontier_next_day);
    assert(battle.s.trip.phase==Returning && battle.frontier_visible());
    for (int i=0;i<300 && town.s.trip.phase==Fighting;++i) town.step();
    assert(town.s.trip.phase==Returning && town.s.trip.result==1);
    assert(!town.frontier_visible() && town.s.frontier_next_day==town.s.day+FrontierRestDays);
    assert(town.owned(Item::RiverPendant)==1 && town.s.trip.reward_gold==FrontierHunt.reward &&
           town.s.trip.reward_xp==20 && town.s.relics==FrontierHunt.loot);
    assert(!town.s.cleared && !town.s.depth_cleared && !town.s.commission.progress && !town.s.trip.cache_found);
    assert(decode(battle.s,encode(town.s)));
    battle.restore();
    for (int i=0;i<300 && battle.s.trip.phase!=Idle;++i) battle.step();
    assert(battle.s.trip.phase==Idle && !battle.launch(0,3,Tactic::Balanced,route));
    for (int i=0;i<Game::DayBeats*FrontierRestDays && !battle.frontier_visible();++i) battle.step();
    assert(battle.s.day==battle.s.frontier_next_day && battle.frontier_visible());
    assert(battle.preview_reward(0,route).loot==Item::BlueTonic);
    assert(battle.launch(0,3,Tactic::Balanced,route));
    assert(!decode(town.s,older_save<ForkSaveSize>(battle.s,'B')));
    battle.s.trip.distance=1;
    assert(!decode(town.s,encode(battle.s)));
}

void wild_hunts() {
    Game town;
    town.reset();
    town.s.day=3;
    assert(town.accept_commission(1) && town.s.commission.kind==CommissionKind::Hunt);
    town.s.clock=uint8_t(wild_spawn_clock(WildSites[0],town.s.day));
    town.step();
    assert(town.wild[0].hp==WildSites[0].health && !town.wild[1].hp);
    ticks(town,3);
    const int hunter=town.wild[0].hunter;
    assert(hunter>=0 && town.hunt_target(hunter)==0 && town.hunt_target(1-hunter)<0);
    assert(town.activity(hunter)==Activity::ToHunt);
    assert(town.launch(0,1u<<hunter));
    assert(town.hunt_target(hunter)<0 && town.wild[0].hp==WildSites[0].health);
    for (int i=0;i<180 && town.wild[0].hp;++i) town.step();
    assert(town.wild[0].hp==0 && town.hunt_target(1-hunter)<0);
    assert(town.s.heroes[1-hunter].xp==3);
    assert(town.s.commission.progress==1 && !town.claim_commission());

    Game resumed;
    assert(decode(resumed.s,encode(town.s)) && resumed.s.commission.progress==1);
    resumed.restore();
    ticks(resumed,Game::DayBeats);
    assert(resumed.s.commission.complete());
    const auto reward=commission_prize(resumed.s.commission);
    const int gold=resumed.s.gold;
    assert(resumed.claim_commission() && resumed.s.gold==gold+reward.gold && !resumed.claim_commission());
}

void festival_attendance() {
    Game town;
    town.reset();
    assert(town.festival() && !town.festival());
    ticks(town,3);
    assert(town.activity(0)==Activity::ToFestival && town.activity(1)==Activity::ToFestival);
    ticks(town,6*WalkTileBeats+4);
    assert(town.activity(0)==Activity::Festival && town.activity(1)==Activity::Festival);
    assert(town.walkers[0].at==Square && town.walkers[1].at==Square);
    assert(town.launch(0,1) && town.activity(0)==Activity::ToCave);
    assert(!town.walkers[0].festival_guest);
    ticks(town,27);
    assert(town.walkers[1].festival_day==1 && town.activity(1)!=Activity::Festival);
    State legacy;
    assert(decode(legacy,older_save<FrontierSaveSize>(town.s,'C')));
    assert(legacy.festival==2 && legacy.event==TownEvent::Lanterns && !legacy.event_attended);
}

void town_event_attendance() {
    Game town;
    town.reset();
    assert(!town.host_event(TownEvent::None) && !town.host_event(TownEvent::Feast));
    assert(town.train(0) && town.choose_discipline(0,Discipline::Guard));
    assert(town.launch(0,2));
    const int before_gold=town.s.gold, before_xp=town.s.heroes[0].xp;
    assert(town.host_event(TownEvent::Drill) && !town.festival());
    assert(town.s.gold==before_gold-event_info(TownEvent::Drill).gold);
    assert(!town.s.event_attended && town.s.heroes[0].xp==before_xp);
    for (int i=0;i<100 && !town.s.event_attended;++i) town.step();
    assert(town.s.event_attended==1 && town.walkers[0].at==Square && town.activity(0)==Activity::Drill);
    assert(town.s.heroes[0].xp==before_xp+DrillExperience && town.s.heroes[0].practice[1]==1+DrillPractice);
    Game loaded;
    assert(decode(loaded.s,encode(town.s)));
    loaded.restore();
    for (int i=0;i<30;++i) {
        loaded.step();
        assert(loaded.activity(0)!=Activity::ToTownEvent && loaded.activity(0)!=Activity::Drill);
    }
    assert(loaded.s.event_attended==1 && loaded.s.heroes[0].xp==before_xp+DrillExperience);

    town.reset();
    town.s.rank=2;
    const auto baseline=town.travel_cost(0,Tactic::Balanced);
    town.s.clock=Game::EveningBeat;
    assert(town.host_event(TownEvent::Feast));
    ticks(town,Game::DayBeats-Game::EveningBeat);
    assert(!town.s.event_attended && !town.prepared(0));
    for (int i=0;i<110 && town.s.event_attended!=3;++i) town.step();
    assert(town.s.event_attended==3 && town.prepared(0) && town.prepared(1));
    assert(town.travel_cost(0,Tactic::Balanced)==baseline-PreparedDiscount);
    assert(decode(loaded.s,encode(town.s)));
    loaded.restore();
    const int energy=loaded.s.heroes[0].energy;
    assert(!loaded.launch(1,1) && loaded.prepared(0));
    assert(loaded.launch(0,1) && !loaded.prepared(0) && loaded.prepared(1));
    assert(loaded.s.heroes[0].energy==energy-baseline+PreparedDiscount);
    assert(loaded.travel_cost(0,Tactic::Balanced)==baseline);
    for (int i=0;i<Game::DayBeats+1 && loaded.s.festival;++i) loaded.step();
    assert(!loaded.s.festival && loaded.s.event==TownEvent::Feast && loaded.s.event_attended==3);
    assert(loaded.prepared(1));
    assert(loaded.host_event(TownEvent::Lanterns) && loaded.prepared(1) && !loaded.s.event_attended);
    assert(decode(town.s,encode(loaded.s)));
    loaded.s.prepared|=128;
    assert(!decode(town.s,encode(loaded.s)));
}

void specialization_and_workshop() {
    Game town;
    town.reset();
    assert(!town.choose_discipline(0,Discipline::Guard));
    assert(town.train(0) && town.s.heroes[0].level==DisciplineLevel);
    auto& h=town.s.heroes[0];
    const auto original=town.stats(0);
    const int gold=town.s.gold;
    assert(town.choose_discipline(0,Discipline::Guard) && town.guards(0));
    assert(town.s.gold==gold-DisciplineCost && h.practice[1]==1);
    assert(town.stats(0).armor==original.armor+2);
    assert(!town.choose_discipline(0,Discipline::Guard));

    h.practice[1]=MasteryLimit-1;
    h.level=LevelLimit;
    town.s.trained_today=0;
    assert(town.train(0) && h.practice[1]==MasteryLimit);
    const auto mastered=town.stats(0);
    assert(!h.practicing() && town.choose_discipline(0,Discipline::Trail));
    assert(!town.guards(0) && town.stats(0).armor==mastered.armor-2);
    assert(town.stats(0).stamina==mastered.stamina+3);
    const int after=town.s.gold;
    assert(town.choose_discipline(0,Discipline::Guard) && town.s.gold==after);
    assert(town.launch(0,1) && !town.choose_discipline(0,Discipline::Trail));
    State loaded;
    assert(decode(loaded,encode(town.s)) && loaded.heroes[0].practice==h.practice);
    const auto old=older_save<ExpeditionSaveSize>(town.s,'7');
    assert(decode(loaded,old) && loaded.heroes[0].discipline==Discipline::None && loaded.trip.phase==Outbound);
    State corrupt=town.s;
    corrupt.heroes[0].practice[1]=MasteryLimit+1;
    assert(!decode(loaded,encode(corrupt)));

    Game forge;
    forge.reset();
    assert(forge.build({7,10},Forge));
    const int shop=forge.building({7,10});
    forge.s.relics=2;
    forge.s.stock[item_index(Item::IronSword)]=1;
    assert(!forge.refine(shop,1) && !forge.can_refine(1,1) && !forge.can_refine(shop,-1));
    forge.s.rank=2;
    forge.s.stock[item_index(Item::BriarBow)]=StockLimit;
    const auto before=encode(forge.s);
    assert(!forge.refine(shop,1) && before==encode(forge.s));
    forge.s.stock[item_index(Item::BriarBow)]=0;
    const int balance=forge.s.gold;
    assert(forge.refine(shop,1));
    assert(forge.owned(Item::IronSword)==0 && forge.owned(Item::BriarBow)==1 &&
           forge.s.relics==0 && forge.s.gold==balance-Recipes[1].gold);
    assert(!forge.refine(shop,1));
}

void expedition_tactics() {
    Game game;
    game.reset();
    game.s.fame=35;
    assert(game.recruit(3));
    constexpr unsigned team=(1u<<0)|(1u<<3);
    game.s.heroes[0].energy=26;
    const auto before=encode(game.s);
    assert(!game.launch(0,team,Tactic::Assault) && encode(game.s)==before);
    assert(game.can_launch(0,team,Tactic::Guard));
    game.s.heroes[0].energy=96;
    const int low=game.opening_strike(0,team,Tactic::Guard,0);
    assert(low<game.opening_strike(0,team,Tactic::Balanced,0));
    assert(game.opening_strike(0,team,Tactic::Balanced,6)<game.opening_strike(0,team,Tactic::Assault,6));
    assert(game.launch(0,team,Tactic::Guard));
    assert(game.s.heroes[0].energy==71 && !game.can_train(0));
    while (game.s.trip.phase==Outbound) game.step();
    while (!game.s.trip.round) game.step();
    assert(game.s.trip.phase==Fighting && game.s.heroes[0].hp==100);
    assert(game.s.heroes[3].hp>=95 && game.s.heroes[3].hp<100);
    const int strike=Quests[0].hp-game.s.trip.hp;
    assert(strike>=low && strike<=game.opening_strike(0,team,Tactic::Guard,6));
    assert(game.supply());
    game.s.trained_today=2;

    State loaded;
    assert(decode(loaded,encode(game.s)) && loaded.trip.tactic==Tactic::Guard && loaded.trained_today==2);
    assert(loaded.trip.round==1 && loaded.trip.supplied);
    const auto prior=older_save<CommissionSaveSize>(game.s,'4');
    assert(decode(loaded,prior) && loaded.trip.tactic==Tactic::Balanced && !loaded.trained_today);
    assert(loaded.trip.round==1 && loaded.trip.supplied);
    loaded.trip.tactic=static_cast<Tactic>(4);
    assert(!decode(loaded,encode(loaded)));
}

void equipment_and_growth() {
    Game town;
    town.reset();
    assert(town.build({7,10},Forge));
    const int forge=town.building({7,10}), market=town.building({6,10});
    const int gold=town.s.gold;
    assert(!town.buy(market,Item::IronSword) && !town.buy(forge,Item::StarStaff));
    assert(town.s.gold==gold);
    assert(town.buy(forge,Item::IronSword) && town.buy(forge,Item::ClothVest));
    assert(town.buy(market,Item::RiverPendant) && town.owned(Item::IronSword)==1);
    const auto base=town.stats(0);
    assert(town.equip(0,Item::IronSword) && town.owned(Item::IronSword)==0);
    assert(!town.equip(1,Item::IronSword) && !town.equip(0,Item::LeafPotion));
    assert(town.equip(0,Item::ClothVest) && town.equip(0,Item::RiverPendant));
    const auto dressed=town.stats(0);
    assert(dressed.attack==base.attack+5 && dressed.armor==base.armor+2 && dressed.stamina==3);
    assert(dressed.rating()==base.rating()+15 && town.travel_cost(0,Tactic::Assault)==27);
    assert(town.unequip(0,ItemGroup::Weapon) && town.equip(1,Item::IronSword));
    assert(!town.can_use(0,Item::LeafPotion) && town.owned(Item::LeafPotion)==1);
    town.s.heroes[0].hp=60;
    assert(town.use(0,Item::LeafPotion) && town.s.heroes[0].hp==95 && !town.owned(Item::LeafPotion));
    assert(town.train(0) && town.growth_points(0)==1);
    assert(town.invest(0,Talent::Arms) && !town.invest(0,Talent::Grit));
    assert(town.stats(0).attack==base.attack+5);
    assert(town.use(0,Item::HoneyBun) && !town.owned(Item::HoneyBun));
    assert(town.buy(market,Item::HoneyBun));
    const int proceeds=town.s.gold;
    assert(town.sell(Item::HoneyBun) && town.s.gold==proceeds+item_type(Item::HoneyBun).price/3);
    assert(town.launch(0,1,Tactic::Assault) && !town.unequip(0,ItemGroup::Armor));
    assert(!town.can_invest(0,Talent::Grit) && !town.equip(0,Item::ClothVest));

    State loaded;
    assert(decode(loaded,encode(town.s)) && loaded.trip.tactic==Tactic::Assault);
    assert(loaded.heroes[0].talents[0]==1 && loaded.heroes[0].equipment[1]==Item::ClothVest);
    assert(loaded.heroes[1].equipment[0]==Item::IronSword && !loaded.stock[item_index(Item::IronSword)]);
    loaded.heroes[0].equipment[1]=Item::IronSword;
    assert(!decode(loaded,encode(loaded)));
    const auto prior=older_save<TacticsSaveSize>(town.s,'5');
    assert(decode(loaded,prior) && loaded.trip.tactic==Tactic::Assault && loaded.trained_today);
    assert(loaded.heroes[0].level==2 && loaded.heroes[0].equipment[1]==Item::None && !loaded.heroes[0].talents[0]);

    Game fight;
    fight.reset();
    fight.s.trip={0,1,Fighting,Quests[0].hp,0,0,0};
    fight.beat=9;
    Game armored=fight;
    armored.s.heroes[0].equipment[1]=Item::ClothVest;
    fight.step(); armored.step();
    assert(fight.s.trip.hp==armored.s.trip.hp && armored.s.heroes[0].hp==fight.s.heroes[0].hp+2);
    fight.s.trip.hp=1;
    ticks(fight,10);
    assert(fight.s.trip.loot==Item::IronSword && fight.owned(Item::IronSword)==1);
    assert(decode(loaded,encode(fight.s)) && loaded.trip.loot==Item::IronSword);
}

void deep_expeditions() {
    Game town;
    town.reset();
    assert(!town.launch(0,3,Tactic::Balanced,Route::Depths));
    town.s.cleared=1;
    assert(town.floor_open(0,Route::Depths) && !town.floor_open(1,Route::Depths));
    assert(town.preview_reward(0,Route::Depths).loot==Item::ClothVest);
    assert(town.preview_reward(0,Route::Depths).gold==135);
    assert(town.opening_strike(0,3,Tactic::Balanced,0,Route::Depths)==
           town.opening_strike(0,3,Tactic::Balanced,0)-Depths[0].armor);
    assert(town.launch(0,3,Tactic::Balanced,Route::Depths) && town.s.trip.hp==43);
    assert(town.s.heroes[0].energy==70);

    while (town.s.trip.phase==Outbound) town.step();
    assert(town.walkers[0].at==DepthsCave && town.walkers[1].at==DepthsCave);
    while (town.s.trip.phase==Exploring) town.step();
    assert(town.s.trip.phase==Fighting && town.s.trip.cache_found);
    town.beat=14;
    town.step();
    assert(!town.s.trip.round && town.s.trip.hp==43);
    Game heavy=town;
    heavy.s.trip.round=2;
    town.step(); heavy.step();
    const auto total_health=[](const Game& game){return game.s.heroes[0].hp+game.s.heroes[1].hp;};
    assert(town.s.trip.round==1 && heavy.s.trip.round==3 && town.s.trip.hp==heavy.s.trip.hp);
    assert(total_health(town)-total_health(heavy)==Depths[0].heavy_bonus);
    heavy.s.trip.hp=1;
    ticks(heavy,16);
    assert(heavy.s.trip.result==1 && heavy.s.depth_cleared==1 && heavy.s.cleared==1);
    assert(heavy.s.trip.loot==Item::ClothVest && heavy.owned(Item::ClothVest)==1);
    assert(heavy.s.trip.reward_gold==135 && heavy.s.trip.reward_fame==17 &&
           heavy.s.trip.reward_relics==5 && heavy.s.trip.reward_xp==20);
    assert(heavy.preview_reward(0,Route::Depths).loot==Item::BlueTonic);
    assert(heavy.preview_reward(0,Route::Standard).loot==Item::LeafPotion);

    State loaded;
    assert(decode(loaded,encode(heavy.s)) && loaded.trip.route==Route::Depths &&
           loaded.depth_cleared==1 && loaded.trip.reward_gold==135);
    loaded.cleared=0;
    assert(!decode(loaded,encode(loaded)));
    const auto prior=older_save<InventorySaveSize>(heavy.s,'6');
    assert(decode(loaded,prior) && loaded.trip.route==Route::Standard && !loaded.depth_cleared &&
           loaded.trip.loot==Item::ClothVest && !loaded.trip.reward_xp);
}

void commissions_and_supply() {
    Game expedition;
    expedition.reset();
    assert(!expedition.accept_commission(2) && expedition.accept_commission(0));
    assert(!expedition.accept_commission(1) && !expedition.claim_commission());
    assert(expedition.s.commission.kind==CommissionKind::Survey);
    assert(expedition.launch(0,3) && !expedition.supply() && !expedition.retreat());
    expedition.s.clock=Game::DayBeats-2;
    while (expedition.s.trip.phase==Outbound) expedition.step();
    assert(expedition.s.day==2);
    expedition.s.heroes[0].hp=48;
    expedition.s.heroes[1].hp=0;
    const int gold=expedition.s.gold;
    assert(expedition.supply());
    assert(expedition.s.gold==gold-Game::SupplyCost && expedition.s.heroes[0].hp==48+Game::SupplyHealth);
    assert(!expedition.s.heroes[1].hp && !expedition.supply());
    Game restored;
    assert(decode(restored.s,encode(expedition.s)));
    restored.restore();
    assert(restored.s.trip.supplied && !restored.supply());
    assert(restored.retreat() && !restored.retreat());
    ticks(restored,180);
    assert(restored.s.trip.phase==Idle && !restored.s.commission.complete());
    assert(restored.launch(0,3));
    ticks(restored,400);
    assert(restored.s.commission.complete() && !restored.abandon_commission());
    const int before=restored.s.gold, fame=restored.s.fame;
    assert(restored.claim_commission());
    assert(restored.s.gold==before+45 && restored.s.fame==fame+5);
    assert(!restored.claim_commission() && !restored.accept_commission(0));
    const auto completed=encode(restored.s);
    assert(decode(restored.s,completed) && !restored.accept_commission(1));
    ticks(restored,Game::DayBeats);
    assert(restored.accept_commission(0) && restored.abandon_commission());

    Game town;
    town.reset();
    assert(town.accept_commission(1));
    assert(town.s.commission.kind==CommissionKind::Trade);
    town.s.visits=9999;
    ticks(town,Game::DayBeats*4);
    assert(town.s.commission.complete() && town.s.visits==9999);
    assert(town.claim_commission());
    town.s.day=8;
    town.s.rank=3;
    assert(town.accept_commission(1) && town.s.commission.kind==CommissionKind::Build);
    assert(town.build({7,14},Road) && town.s.commission.progress==0);
    assert(town.build({7,10},Forge) && town.s.commission.progress==1);
    assert(town.upgrade(town.building({7,10})) && town.s.commission.complete());
}

void suggested_teams() {
    Game town;
    town.reset();
    assert(town.suggest_team(0,Tactic::Balanced)==3);
    assert(!town.suggest_team(0,Tactic::Balanced,Route::Depths));
    town.s.cleared=1;
    town.s.fame=50;
    assert(town.recruit(2) && town.recruit(3));
    const unsigned expedition=town.suggest_team(0,Tactic::Balanced,Route::Depths);
    assert(std::popcount(expedition)==3 && (expedition&(1u<<3)));

    town.s.trip={0,1,Idle,0,1,1,0};
    town.s.trip.route=Route::Depths;
    assert(town.suggest_team(0,Tactic::Balanced,Route::Depths)==1);
    town.s.heroes[0].energy=21;
    assert(town.suggest_team(0,Tactic::Balanced,Route::Depths)==8);
    assert(town.suggest_team(0,Tactic::Balanced)==12);
}

int main() {
    southern_routes_and_cache();
    cave_fork_choices();
    frontier_hunts();
    shop_management();
    walking_and_visits();
    wild_hunts();
    festival_attendance();
    town_event_attendance();
    specialization_and_workshop();
    combat_timing();
    training_and_housing();
    expedition_tactics();
    equipment_and_growth();
    deep_expeditions();
    suggested_teams();
    commissions_and_supply();
    Game g;
    g.reset();
    assert(g.s.gold==320 && g.s.rank==1 && g.hero_count()==2);
    assert(g.terrain({8,2}).height==2 && g.terrain({8,0}).height==3);
    assert(g.traffic[cell({8,0})] && g.traffic[cell({18,5})]);
    assert(g.usable({8,0},{8,1}) && g.usable({17,5},{18,5}));
    assert(g.usable({8,3},{8,4}));
    assert(g.terrain({13,5}).ground==River && g.terrain({13,5}).height==1);
    assert(g.terrain({13,6}).ground==River && g.terrain({13,6}).height==0);
    assert(g.placement({13,5},LowerGround)==Placement::Occupied);
    assert(g.usable({13,8},{14,8}));
    assert(!g.usable({13,7},{14,7}));
    assert(g.usable({14,8},{15,8}));
    assert(g.traffic[cell(Cave)]);
    assert(g.placement({13,7},Forge)==Placement::Occupied);
    assert(g.placement({9,5},Forge)==Placement::Locked);
    assert(!g.promote() && !g.launch(1,3));
    assert(g.build({7,10},Forge));
    assert(g.upgrade(3));
    assert(g.build({7,14},Road));
    assert(g.road({7,14}) && g.traffic[cell({7,14})]);

    ticks(g,Game::DayBeats*4);
    assert(g.s.visits>0 && g.s.yesterday>0);
    assert(g.s.heroes[0].gear>0 || g.s.heroes[1].gear>0);
    // Simulate one old town building on the newly paved gate loop.
    g.s.buildings[g.s.count++]={9,13,House,1};
    g.s.heroes[0].home=uint8_t(g.s.count-1);
    const auto previous=older_save<LegacySaveSize>(g.s,'2');
    Game migrated;
    bool legacy=false;
    assert(decode(migrated.s,previous,&legacy) && legacy);
    migrated.restore(legacy);
    assert(migrated.s.count==g.s.count && migrated.s.gold==g.s.gold);
    assert(migrated.s.heroes[0].home==g.s.heroes[0].home);
    assert(migrated.s.buildings[g.s.count-1].at()!=g.s.buildings[g.s.count-1].at());
    assert(!migrated.road(migrated.s.buildings[g.s.count-1].at()));
    --g.s.count;
    g.s.buildings[g.s.count]={};
    g.s.heroes[0].home=Game::NoHome;

    assert(g.launch(0,3));
    assert(!g.launch(0,1) && !g.gift(1));
    while (g.s.trip.phase==Outbound || g.s.trip.phase==Exploring) g.step();
    assert(g.s.trip.phase==Fighting);
    const auto before=encode(g.s);
    State loaded;
    assert(decode(loaded,before) && encode(loaded)==before);
    auto corrupted=before;
    corrupted[18]^=std::byte{1};
    assert(!decode(loaded,corrupted));
    Game restored;
    restored.s=loaded;
    restored.restore();
    ticks(restored,200);
    assert(restored.s.trip.phase==Idle && restored.s.cleared==1 && restored.s.relics==3);
    assert(restored.s.heroes[0].xp>0 || restored.s.heroes[0].level>1);

    for (int floor=1;floor<QuestCount;++floor) {
        while (restored.s.rank<Quests[floor].rank) {
            const auto& goal=Ranks[restored.s.rank];
            ticks(restored,Game::DayBeats*16);
            if (restored.s.fame<goal.fame) {
                assert(restored.festival());
                ticks(restored,Game::DayBeats*2);
            }
            for (int try_again=0;restored.s.fame<goal.fame && try_again<8;++try_again) {
                assert(restored.launch(0,3));
                ticks(restored,Game::DayBeats*3);
            }
            for (int i=2;i<HeroCount && restored.hero_count()<goal.people;++i)
                restored.recruit(i);
            assert(restored.promote());
        }
        if (restored.s.rank>=2 && restored.s.buildings[restored.s.count-1].kind==Forge) {
            assert(restored.build({7,11},House));
            while (restored.s.heroes[0].mood<45) assert(restored.gift(0));
            const int house=restored.building({7,11});
            assert(restored.assign_home(0,house));
            assert(!restored.assign_home(0,house));
        }
        if (restored.s.rank>=3 && !restored.s.charms) {
            assert(restored.build({9,7},Shrine));
            assert(restored.craft());
        }
        for (int id=2;id<HeroCount;++id) restored.recruit(id);
        unsigned team=3;
        int third=-1;
        for (int id=2;id<HeroCount;++id) if (restored.s.heroes[id].level) third=id;
        if (third>=2) team|=1u<<third;
        for (int id=0;id<HeroCount;++id) if (team&(1u<<id))
            for (int gear=0;gear<2;++gear) restored.gift(id);
        ticks(restored,Game::DayBeats*2);
        assert(restored.launch(floor,team));
        ticks(restored,Game::DayBeats*2);
        assert(restored.s.trip.phase==Idle && (restored.s.cleared&(1u<<floor)));
    }
    for (int i=2;i<HeroCount;++i) restored.recruit(i);
    assert(restored.promote());
    assert(restored.s.victory && restored.s.rank==5);
    assert(decode(loaded,encode(restored.s)));
    assert(loaded.cleared==63);
    Game terraces;
    terraces.reset();
    constexpr Point landing{10,10}, lot{10,11};
    assert(terraces.placement(landing,StoneSteps)==Placement::NoStairLink);
    assert(terraces.build(landing,RaiseGround));
    assert(terraces.build(lot,RaiseGround));
    assert(terraces.terrain(lot).height==1);
    assert(terraces.placement(lot,Market)==Placement::NoRoad);
    assert(terraces.build(lot,RaiseGround));
    assert(terraces.build(lot,RaiseGround));
    assert(terraces.placement(lot,RaiseGround)==Placement::ElevationLimit);
    assert(terraces.placement(lot,StoneSteps)==Placement::NoStairLink);
    assert(terraces.build(lot,LowerGround) && terraces.build(lot,LowerGround));
    assert(terraces.build(landing,StoneSteps));
    assert(terraces.usable(landing,{11,10}) && terraces.traffic[cell(landing)]);
    assert(terraces.build(lot,Market));
    assert(!terraces.build(landing,LowerGround));
    assert(!terraces.build(lot,LowerGround));
    const auto terrain_save=encode(terraces.s);
    Game reloaded;
    assert(decode(reloaded.s,terrain_save));
    reloaded.restore();
    assert(reloaded.terrain(landing).height==1 && reloaded.terrain(landing).ground==Stair);
    assert(reloaded.terrain(lot).height==1 && reloaded.building(lot)>=0);
    assert(reloaded.traffic[cell(landing)]);
    assert(decode(reloaded.s,older_save<MapSaveSize>(terraces.s,'3'),&legacy) && !legacy);
    reloaded.restore(legacy);
    assert(reloaded.s.commission.kind==CommissionKind::None && reloaded.traffic[cell(landing)]);
    std::puts("Commission progression, supply, retreat, and current and legacy saves verified.");
    std::puts("All four elevations connect through steps, with terrain editing and saves.");
    std::printf("Six treasures, %d-star town, %d adventurers, %d shop visits and valid saves\n",
                loaded.rank,restored.hero_count(),loaded.visits);
}
