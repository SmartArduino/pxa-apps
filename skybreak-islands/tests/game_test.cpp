#include "../game.hpp"

#include <cassert>

using namespace sky;

namespace {
constexpr float step=1.f/60.f;

void forward(Game& game,float seconds,Input input={}) {
    for (int i=0;i<int(seconds/step);++i) game.update(input,step);
}

void combat(const World& world) {
    Game game(world);
    game.reset(Phase::flight);
    assert(game.living()==3 && game.wave==1 && game.score==0);
    assert(game.target(.977f,560)==0);
    forward(game,1.2f,{.fire=true});
    assert(game.score==1 && game.living()==2);
    assert(game.launch_missile());
    assert(game.rockets==3 && !game.launch_missile());
    forward(game,2.5f);
    assert(game.score==2);
    for (unsigned wave=1;wave<=3;++wave) {
        assert(game.wave==wave);
        for (auto& e:game.enemies) e.health=0;
        forward(game,2.f);
        if (wave<3) assert(game.living()==int(wave+3));
    }
    assert(game.phase==Phase::victory);
}

void recovery(const World& world) {
    Game game(world);
    game.reset(Phase::flight);
    game.eject();
    assert(game.phase==Phase::ejection);
    game.open_chute();
    assert(game.phase==Phase::ejection);
    forward(game,.45f);
    assert(game.pilot.y>game.player.pos.y);
    game.open_chute();
    assert(game.phase==Phase::parachute);
    const float initial_heading=game.pilot_yaw;
    const auto initial=game.pilot;
    forward(game,1.5f,{.turn=1,.climb=1});
    assert(game.pilot_yaw>initial_heading && game.pilot.y<initial.y);
    assert(game.rescue_visible && game.hatch==0);
    assert(game.rescue_plane.type==game.player.type);
    const float before=norm2(game.rescue_plane.pos-game.pilot);
    forward(game,1.2f);
    assert(game.hatch>.5f && norm2(game.rescue_plane.pos-game.pilot)<before);
    forward(game,1.3f);
    assert(game.phase==Phase::pickup && game.hatch==1);
    forward(game,1.7f);
    assert(game.phase==Phase::rescued && game.airlift && game.hatch==0);

    game.reset(Phase::flight);
    game.player.pos.y=20;
    game.eject();
    forward(game,5.f);
    assert(game.phase==Phase::rescued && game.wet && !game.airlift);

    game.reset(Phase::flight);
    game.eject();
    forward(game,1.5f);
    assert(game.phase==Phase::parachute);
    game.reset(Phase::flight);
    assert(game.hull==5 && game.rockets==4 && game.living()==3 && game.wake_count==0);
    game.player.pos.y=world.ground(game.player.pos)+1;
    forward(game,.1f);
    assert(game.hull==0 && game.phase==Phase::ejection);
}

void wingman_support(const World& world) {
    Game game(world);
    game.player.type=Airframe::dart;
    game.reset(Phase::flight);
    assert(game.wingman.hull==3 && game.wingman.craft.type==Airframe::dart);
    const auto origin=game.wingman.craft.pos;
    forward(game,1.f,{.turn=.35f,.climb=.2f});
    assert(norm2(game.wingman.craft.pos-origin)>60*60);
    assert(norm2(game.wingman.craft.pos-game.player.pos)<100*100);

    game.reset(Phase::flight);
    game.elapsed=5;
    game.enemies={};
    auto& target=game.enemies[0];
    target.health=1;
    target.cooldown=10;
    target.craft.pos=game.wingman.craft.pos+attitude(game.player.yaw).front*138;
    target.craft.yaw=game.player.yaw;
    forward(game,step);
    assert(std::any_of(game.shots.begin(),game.shots.end(),[](const Shot& shot) {
        return shot.life>0 && shot.allied && !shot.hostile;
    }));
    forward(game,.49f);
    assert(game.wingman_score==1 && game.score==0 && !target.health);

    const Vec wing=game.wingman.craft.pos;
    game.shots[0]={wing,wing,{0,0,270},1.f,true};
    forward(game,step);
    assert(game.wingman.hull==2 && game.hull==5);
}

void airframe_damage(const World& world) {
    Game game(world);
    game.reset(Phase::flight);
    const auto effect_seen=[&](EffectKind kind) {
        return std::any_of(game.effects.begin(),game.effects.end(),[&](const Effect& fx) {
            return fx.age<fx.duration && fx.kind==kind;
        });
    };
    const auto hit_right=[&] {
        game.invulnerable=0;
        const auto pose=attitude(game.player.yaw,game.player.pitch,game.player.bank);
        const auto start=game.player.pos+pose.apply({6,0,-5});
        game.shots[0]={start,start,pose.front*300,1.f,true};
        forward(game,step);
    };
    hit_right();
    assert(game.hull==4 && game.player.damage.starboard==1 && !game.player.damage.missing_tip);
    hit_right();
    hit_right();
    assert(game.hull==2 && game.player.damage.missing_tip==1 && effect_seen(EffectKind::wing_piece));
    hit_right();
    game.damage_smoke_timer=0;
    forward(game,step);
    assert(game.hull==1 && effect_seen(EffectKind::burning));
    hit_right();
    assert(game.hull==0 && game.phase==Phase::ejection && game.player.damage.canopy_lost);
    assert(effect_seen(EffectKind::canopy_piece));
    forward(game,4.1f);
    assert(game.wreck_time==0);

    for (Airframe type:{Airframe::patrol,Airframe::dart,Airframe::maritime}) {
        game.player.type=type;
        game.reset(Phase::flight);
        assert(!game.player.damage.starboard && !game.player.damage.missing_tip && !game.player.damage.canopy_lost);
        game.player.pos.y=221;
        game.eject();
        forward(game,5.35f);
        assert(game.rescue_visible && game.rescue_plane.type==type);
        forward(game,1.8f);
        assert(game.phase==Phase::rescued && game.airlift);
    }
}

void visual_events(const World& world) {
    Game game(world);
    game.reset(Phase::flight);
    const auto seen=[&](EffectKind kind) {
        return std::any_of(game.effects.begin(),game.effects.end(),[&](const Effect& fx) {
            return fx.kind==kind && fx.age<fx.duration;
        });
    };
    forward(game,.35f,{.boost=true});
    assert(game.exhaust_heat>.9f && game.speed>112);
    forward(game,.20f);
    assert(game.exhaust_heat>0 && game.exhaust_heat<.9f);
    assert(game.launch_missile());
    forward(game,.12f);
    assert(seen(EffectKind::missile_smoke));
    forward(game,1.3f);
    assert(game.score==1 && seen(EffectKind::explosion));
    game.eject();
    assert(seen(EffectKind::ejection));

    game.reset(Phase::flight);
    game.player.pos.y=29;
    forward(game,.14f);
    assert(seen(EffectKind::spray));
    const auto wake_strength=[&] {
        for (const auto& fx:game.effects)
            if (fx.kind==EffectKind::spray && fx.age<fx.duration) return fx.strength;
        return 0.f;
    };
    const auto high_wake=wake_strength();
    game.reset(Phase::flight);
    game.player.pos.y=14;
    forward(game,.14f);
    assert(wake_strength()>high_wake);
    game.player.pos.y=40;
    forward(game,1.12f);
    assert(!seen(EffectKind::spray));
}

void surface_hits(const World& world) {
    Game game(world);
    game.reset(Phase::flight);
    for (auto& enemy:game.enemies) enemy.health=0;
    auto check_hit=[&](Vec surface,EffectKind kind,bool missile) {
        surface.y=world.ground(surface);
        const Vec start=surface+Vec{0,2,0};
        game.shots[0]={start,start,{0,-590,0},1.f,false,missile};
        forward(game,step);
        assert(game.shots[0].life==0);
        const auto& effect=game.effects[(game.effect_next+game.effects.size()-1)%game.effects.size()];
        assert(effect.kind==kind && effect.pos.y==surface.y);
        assert(effect.strength==(missile ? 2.15f : 1.f));
    };
    check_hit({2048,0,1820},EffectKind::water_hit,false);
    check_hit({1538,0,2498},EffectKind::water_hit,false);
    check_hit({1710,0,2518},EffectKind::water_hit,true);
    check_hit({1575,0,2310},EffectKind::land_hit,true);
    assert(!game.launch_missile());
    game.player.pitch=-.51f;
    assert(game.surface_aim() && game.launch_missile());
    assert(game.shots[0].missile && game.shots[0].target==-1 && game.rockets==3);
}

void inland_water(const World& world) {
    const Vec lake{1538,0,2498};
    const auto sample=[&](Vec p) { return world.surface_at(p.x,p.z); };
    assert(sample(lake).inland() && !sample(lake).flowing);
    assert(sample(lake).height()==14 && sample(lake+Vec{35,0,35}).height()==14);
    assert(sample({2508,0,2675}).inland() && sample({2508,0,2675}).height()==17);
    assert(!sample({1575,0,2310}).water());
    float upstream=sample(lake).height();
    for (Vec p:{Vec{1710,0,2518},Vec{1802,0,2557},Vec{1851,0,2546}}) {
        const auto river=sample(p);
        assert(river.inland() && river.flowing && river.height()<upstream);
        upstream=river.height();
    }
    for (int x=2115;x<=2174;++x)
        assert(world.surface_at(float(x),2512+(x-2115)*11.f/59).water());
    assert(sample({2115,0,2512}).height()==0 && sample({1912,0,2576}).height()==0);

    Game game(world);
    game.reset(Phase::flight);
    game.player.pos=lake+Vec{0,32,0};
    forward(game,step);
    const auto& spray=game.effects[(game.effect_next+game.effects.size()-1)%game.effects.size()];
    assert(spray.kind==EffectKind::spray && spray.pos.y==14);

    game.phase=Phase::parachute;
    game.pilot=lake+Vec{0,16,0};
    game.drift={0,-18,0};
    forward(game,.12f);
    assert(game.phase==Phase::rescued && game.wet && game.pilot.y==15.5f);
}

void enemy_roles(const World& world) {
    Game game(world);
    game.reset(Phase::flight);
    const auto& patrol=game.enemies[0];
    const auto& fast=game.enemies[1];
    auto& heavy=game.enemies[2];
    assert(patrol.craft.type==Airframe::patrol && fast.craft.type==Airframe::dart);
    assert(heavy.craft.type==Airframe::maritime && heavy.health>fast.health);
    const Vec origin=fast.craft.pos;
    const Vec baseline=patrol.craft.pos;
    forward(game,step);
    assert(norm2(fast.craft.pos-origin)>norm2(patrol.craft.pos-baseline));
    heavy.craft={game.player.pos+Vec{0,12,-110},0,0,0,0,Airframe::maritime};
    heavy.cooldown=0;
    forward(game,step);
    assert(heavy.followup && heavy.cooldown<.25f);
    forward(game,.30f);
    assert(!heavy.followup && heavy.cooldown>1.f);
    assert(std::count_if(game.shots.begin(),game.shots.end(),[](const Shot& s) {
        return s.life>0 && s.hostile;
    })==2);
}

void escort(const World& world) {
    Game game(world);
    game.reset(Phase::flight,Sortie::escort);
    assert(game.living()==3 && game.convoy.hull==Convoy::max_hull);
    const auto start=game.convoy.pos;
    game.rockets=1;
    game.hull=3;
    game.player.pos={start.x,115,start.z-38};
    forward(game,step);
    assert(!game.convoy.resupplied);
    game.player.pos.y=55;
    forward(game,step);
    assert(game.convoy.resupplied && game.convoy.resupply_flash>0 && game.rockets==3 && game.hull==4);
    game.rockets=1;
    forward(game,step);
    assert(game.rockets==1);

    const Vec above=game.convoy.pos+Vec{0,12,0};
    game.shots[0]={above,above,{0,-590,0},1.f};
    forward(game,step);
    assert(game.shots[0].life==0 && game.convoy.hull==Convoy::max_hull);
    game.shots[0]={above,above,{0,-590,0},1.f,true};
    forward(game,step);
    assert(game.shots[0].life==0 && game.convoy.hull==Convoy::max_hull-1);
    assert(game.convoy.damage_flash>0 && game.convoy.pos.z>start.z);

    game.reset(Phase::flight,Sortie::escort);
    forward(game,33.f);
    assert(game.phase==Phase::escort_lost && game.convoy.hull==0 && game.convoy.progress<1);

    game.reset(Phase::flight,Sortie::escort);
    for (int i=0;i<60*40 && game.phase==Phase::flight;++i) {
        if (i==50 || i==110) assert(game.launch_missile());
        const bool banking=i>=202 && (i-202)%371<168;
        game.update({.turn=banking ? -1.f : 0.f,.fire=true},step);
    }
    assert(game.phase==Phase::victory && game.convoy.progress==1 && game.convoy.hull>0);
    assert(game.wave==3 && game.score>=3);
    assert(norm2(game.convoy.pos-Convoy::route[3])<.001f);

    for (float t=0;t<Convoy::duration;t+=.5f) {
        const Vec p=Convoy::route_position(t);
        const auto heading=Convoy::route_position(t+.01f)-p;
        const auto axes=attitude(std::atan2(heading.x,heading.z));
        for (float x:{-9.f,9.f}) for (float z:{-30.f,0.f,30.f})
            assert(world.ground(p+axes.right*x+axes.front*z)==0);
    }
}

void fleet_strike(const World& world) {
    Game game(world);
    game.reset(Phase::flight,Sortie::fleet);
    auto& fleet=game.fleet;
    assert(fleet.remaining()==3 && fleet.units[0].friendly());
    for (const auto& ship:fleet.units) {
        const auto surface=world.surface_at(ship.pos.x,ship.pos.z);
        assert(surface.water()==ship.afloat());
        assert(surface.height()==ship.pos.y);
    }
    for (int i=0;i<90;++i) {
        game.elapsed=i*(2*pi/.24f)/90.f;
        game.update_fleet(0);
        const auto& boat=fleet.units[2];
        const auto axes=attitude(boat.yaw);
        for (float x:{-6.6f,6.6f}) for (float z:{-17.2f,17.2f})
            assert(world.ground(boat.pos+axes.apply({x,0,z}))==0);
    }
    game.reset(Phase::flight,Sortie::fleet);
    assert(game.surface_lock()==1 && game.launch_missile() && game.shots[0].surface_target==1);
    forward(game,3.55f);
    assert(fleet.units[1].hull==3 && fleet.launched==1);
    assert(game.enemies[0].deck_launch>0 && game.enemies[0].craft.pos.y>11);

    game.player.pos=fleet.units[0].pos+Vec{0,65,0};
    game.hull=4;
    forward(game,step);
    assert(fleet.supplied && game.rockets==4 && game.hull==5);
    game.rockets=3;
    forward(game,step);
    assert(game.rockets==3);

    const auto fire_through=[&](SurfaceUnit& target,bool hostile,bool missile) {
        const auto center=target.aim();
        Shot shot{center+Vec{0,0,-125},center+Vec{0,0,-125},{0,0,330},1.f,hostile,missile};
        assert(game.hit_fleet(shot,center) && shot.life==0);
    };
    fire_through(fleet.units[0],false,true);
    assert(fleet.units[0].hull==10);
    for (unsigned i=1;i<4;++i) {
        fire_through(fleet.units[i],false,false);
        assert(fleet.units[i].hull==2);
        fire_through(fleet.units[i],false,true);
    }
    assert(fleet.remaining()==0);
    forward(game,1.7f);
    assert(game.phase==Phase::victory);

    game.reset(Phase::flight,Sortie::fleet);
    auto& carrier=fleet.units[0];
    for (int i=0;i<10;++i) fire_through(carrier,true,false);
    assert(!carrier.hull && game.phase==Phase::escort_lost);
    game.reset(Phase::flight);
    assert(fleet.remaining()==0 && !fleet.supplied);
}

void carrier_deck(const World& world) {
    Game game(world);
    for (auto type:{Airframe::patrol,Airframe::dart,Airframe::maritime}) {
        game.player.type=type;
        game.reset(Phase::flight,Sortie::fleet);
        const auto& ship=game.fleet.units[0];
        const auto axes=attitude(ship.yaw);
        assert(game.can_approach_deck() && game.deck_control());
        assert(game.fleet.deck.phase==DeckPhase::approach && game.player.gear);
        assert(!game.launch_missile());
        game.rockets=1;
        game.hull=4;
        forward(game,3.8f);
        assert(game.phase==Phase::flight && game.fleet.deck.phase==DeckPhase::parked);
        assert(game.fleet.supplied && game.rockets==4 && game.hull==5 && game.speed==0);
        assert(game.player.gear && std::abs(game.player.pos.y-CarrierDeck::rest)<.01f);
        assert(std::abs(dot(game.player.pos-ship.pos,axes.right)-CarrierDeck::runway_x)<.01f);
        const float parked_z=dot(game.player.pos-ship.pos,axes.front);
        assert(parked_z>CarrierDeck::touchdown && parked_z<0);
        const auto& shape=airframe(type);
        assert(parked_z+shape.tail>-CarrierDeck::half_length);
        assert(std::abs(CarrierDeck::runway_x)+shape.span<CarrierDeck::half_width);
        assert(game.deck_control() && game.fleet.deck.phase==DeckPhase::launch);
        forward(game,3.f);
        assert(game.phase==Phase::flight && game.fleet.deck.phase==DeckPhase::free && !game.player.gear);
        assert(game.player.pos.y>CarrierDeck::rest && game.speed>100);
        assert(dot(game.player.pos-ship.pos,axes.front)>CarrierDeck::launch_edge);
    }
    game.reset(Phase::flight,Sortie::fleet);
    game.player.yaw+=pi;
    assert(!game.can_approach_deck() && !game.deck_control());
    const auto& ship=game.fleet.units[0];
    const auto axes=attitude(ship.yaw);
    const Vec midpoint=ship.pos+axes.apply({CarrierDeck::runway_x,CarrierDeck::rest,CarrierDeck::touchdown});
    game.player.yaw=ship.yaw;
    game.player.pos=midpoint-Vec{0,.12f,0};
    game.speed=74;
    game.cross_deck(midpoint+Vec{0,.12f,0});
    assert(game.fleet.deck.phase==DeckPhase::arrest);

    for (unsigned i=0;i<CarrierDeck::parking.size();++i) {
        const auto parked=CarrierDeck::parking[i];
        const auto& model=airframe(i==0 ? Airframe::patrol : Airframe::dart);
        assert(parked.x+model.nose<CarrierDeck::half_width-2);
        assert(parked.x+model.tail>CarrierDeck::runway_x+airframe(Airframe::maritime).span+3.5f);
        assert(parked.z-model.span>-CarrierDeck::half_length && parked.z+model.span<-42);
    }
    for (const auto& carrier:game.fleet.units) if (carrier.carrier()) {
        const auto pose=attitude(carrier.yaw);
        for (float x=-37.f;x<=37.f;x+=7.4f) for (float z=-122.f;z<=122.f;z+=8.f)
            assert(world.ground(carrier.pos+pose.apply({x,0,z}))==0);
    }
}

void aircraft_controls(const World& world) {
    Game game(world);
    game.player.type=Airframe::dart;
    game.reset(Phase::flight);
    assert(game.player.type==Airframe::dart && game.speed>110);
    assert(game.enemies[0].craft.type!=game.enemies[1].craft.type);
    game.invulnerable=25;
    forward(game,.55f,{.turn=1,.roll=true});
    assert(std::cos(game.player.bank)<-.9f && game.player.aileron<-.8f);
    forward(game,.50f,{.turn=1,.roll=true});
    assert(std::cos(game.player.bank)>.75f);
    forward(game,.55f,{.roll=true});
    const auto level=game.player.bank;
    forward(game,.30f,{.roll=true});
    assert(std::abs(game.player.bank-level)<.01f);
    forward(game,1.1f);
    assert(std::abs(game.player.bank)<.025f);
    game.reset(Phase::flight);
    game.player.pos.y=220;
    forward(game,.65f,{.climb=1,.boost=true});
    assert(game.player.pitch>.7f && game.player.pos.y>250 && game.speed>175);
    assert(game.player.elevator>.6f && game.flight_path>0 && game.flight_path<game.player.pitch-.06f);
    forward(game,.65f,{.climb=-1});
    assert(game.player.pitch<-.50f && game.player.elevator<-.6f && game.flight_path<0);
}

void flight_energy(const World& world) {
    Game level(world),climb(world),dive(world),bank(world),fast_bank(world);
    for (Game* g:{&level,&climb,&dive,&bank,&fast_bank}) {
        g->reset(Phase::flight);
        g->player.pos.y=220;
        g->player.bank=g->player.yaw=0;
        g->enemies={};
    }
    const auto start=level.player.pos;
    forward(level,.8f);
    forward(climb,.8f,{.climb=.85f});
    forward(dive,.8f,{.climb=-.85f});
    assert(climb.player.pos.y>level.player.pos.y+22);
    assert(dive.player.pos.y<level.player.pos.y-20);
    assert(climb.speed<level.speed && level.speed<dive.speed);
    assert(level.player.pos.z>start.z+80 && std::abs(level.player.pos.y-start.y)<2);

    fast_bank.speed=airframe(fast_bank.player.type).flight.boost;
    forward(bank,.5f,{.turn=1});
    forward(fast_bank,.5f,{.turn=1,.boost=true});
    assert(bank.player.bank<-.5f && bank.player.aileron<0 && bank.player.rudder>0);
    assert(fast_bank.player.yaw>0 && fast_bank.player.yaw<bank.player.yaw);
    const float shortly_after_pull=climb.player.pos.y;
    forward(climb,.10f);
    assert(climb.player.pos.y>shortly_after_pull && climb.player.pitch>0);
}

void ocean(const World& world) {
    const auto still=WaterMotion::at(0),moving=WaterMotion::at(.75f);
    assert(moving.swell_x>0 && moving.cross_x<0 && moving.cross_z>moving.swell_z);
    bool swell_moves=false,cross_moves=false;
    for (int v=905;v<930;++v) for (int u=1005;u<1040;++u) {
        const int coast=World::coast_index(u,v),level=world.relief_map[coast];
        if (level>World::shoreline) continue;
        const auto base=world.water_texel(u,v,coast,level,still,0);
        const WaterMotion swell_only{moving.swell_x,moving.swell_z,0,0,0};
        const WaterMotion cross_only{0,0,moving.cross_x,moving.cross_z,0};
        swell_moves |= base!=world.water_texel(u,v,coast,level,swell_only,0);
        cross_moves |= base!=world.water_texel(u,v,coast,level,cross_only,0);
    }
    assert(swell_moves && cross_moves);
    bool slopes=false;
    for (int v=900;v<1300;++v) for (int u=810;u<1280;u+=4) {
        const int coast=World::coast_index(u+2,v),level=world.relief_map[coast];
        if (level<World::shoreline-10 || level>World::shoreline) continue;
        const int depth=world.surf_depth(u+2,v,coast,level);
        assert(depth>=0 && depth<=World::shoreline*4);
        slopes |= world.surf_depth(u+5,v,coast,level)!=depth;
    }
    assert(slopes);
}
}

int main() {
    World world;
    world.generate();
    assert(world.ground({1575,0,2310})>20);
    assert(*std::max_element(world.heights.begin(),world.heights.end())<=World::highest_land);
    assert(std::count_if(world.terrain_features.begin(),world.terrain_features.end(),[](int cover) {
        return (cover&World::forest_cover)!=0;
    })>2000);
    assert(world.ground({2048,0,1820})==0);
    ocean(world);
    combat(world);
    recovery(world);
    visual_events(world);
    surface_hits(world);
    inland_water(world);
    enemy_roles(world);
    wingman_support(world);
    airframe_damage(world);
    escort(world);
    fleet_strike(world);
    carrier_deck(world);
    aircraft_controls(world);
    flight_energy(world);
}
