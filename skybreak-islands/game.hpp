#pragma once
#include "airframe.hpp"
#include "convoy.hpp"
#include "fleet.hpp"

namespace sky {

enum class Phase { title, flight, ejection, parachute, pickup, rescued, lost, escort_lost, victory };
struct Input { float turn=0, climb=0; bool fire=false, boost=false, roll=false; };
struct AirframeDamage {
    std::uint8_t port=0,starboard=0,body=0;
    int missing_tip=0;
    bool canopy_lost=false;
    int hits(float side) const { return side<0 ? port : starboard; }
};
struct Craft {
    Vec pos;
    float yaw=0,pitch=0,bank=0,aileron=0;
    Airframe type=Airframe::patrol;
    float elevator=0,rudder=0;
    AirframeDamage damage{};
    bool gear=false;
};
struct Wingman {
    Craft craft;
    Vec velocity;
    unsigned hull=3;
    float cooldown=0,protection=0;
};
struct Enemy {
    Craft craft;
    int health=0;
    float cooldown=0, age=0, smoke_delay=0;
    bool followup=false;
    float deck_launch=0;
};
struct Shot {
    Vec pos, previous, velocity;
    float life=0;
    bool hostile=false, missile=false;
    int target=-1;
    float exhaust_delay=0;
    bool allied=false;
    int surface_target=-1;
};
enum class EffectKind {
    hit, explosion, smoke, missile_smoke, ejection, spray, water_hit, land_hit, muzzle,
    burning, wing_piece, canopy_piece
};
struct Effect {
    Vec pos, motion;
    float age=0,duration=0;
    EffectKind kind=EffectKind::hit;
    std::uint32_t seed=0;
    float strength=1;
};
struct Wake { Vec left,right; float vapor=0; };

struct Game {
    const World& world;
    Phase phase=Phase::title;
    Sortie sortie=Sortie::patrol;
    Convoy convoy;
    Fleet fleet;
    Craft player;
    Craft rescue_plane;
    Wingman wingman;
    Vec pilot, drift, wreck_velocity;
    Vec seat_start;
    float pilot_yaw=0, pilot_lean=0, elapsed=0, phase_time=0, speed=0, energy=1;
    float shot_delay=0, missile_delay=0, invulnerable=0, wave_delay=0, wreck_time=0;
    unsigned score=0, wingman_score=0, wave=1, hull=5, rockets=4;
    float hatch=0;
    float exhaust_heat=0, hit_feedback=0, damage_feedback=0;
    float roll_rate=0,pitch_rate=0,yaw_rate=0,flight_path=0,side_slip=0;
    bool boosted=false, wet=false, rescue_visible=false, airlift=false;
    std::array<Enemy,5> enemies{};
    std::array<Shot,32> shots{};
    std::array<Effect,52> effects{};
    std::array<Wake,28> wake{};
    int wake_next=0,wake_count=0;
    float wake_timer=0,smoke_timer=0,spray_timer=0,eject_smoke_timer=0;
    unsigned effect_next=0;
    std::uint32_t effect_serial=0;
    float damage_smoke_timer=0;

    explicit Game(const World& terrain) : world(terrain) { reset(Phase::title); }
    void reset_fleet();
    void update_fleet(float dt);
    int surface_lock(float alignment=.85f,float range=870.f) const;
    bool hit_fleet(Shot& shot,Vec end);
    bool can_approach_deck() const;
    bool deck_control();
    void advance_deck(float dt);
    void cross_deck(Vec previous);

    void reset(Phase mode,Sortie mission=Sortie::patrol) {
        phase=mode; player={{2048,173,1820},-.05f,-.015f,-.15f,0,player.type};
        sortie=mission;
        convoy={};
        fleet={};
        elapsed=phase_time=shot_delay=missile_delay=invulnerable=wave_delay=wreck_time=0;
        speed=airframe(player.type).flight.cruise-2; energy=1; hull=5; rockets=4; score=wingman_score=0; wave=1;
        roll_rate=pitch_rate=yaw_rate=flight_path=side_slip=0;
        boosted=wet=rescue_visible=airlift=false;
        hatch=exhaust_heat=hit_feedback=damage_feedback=spray_timer=eject_smoke_timer=pilot_lean=0;
        effect_serial=0;
        damage_smoke_timer=0;
        shots={}; effects={}; wake_next=wake_count=0; effect_next=0; wake_timer=0;
        if (sortie==Sortie::escort) {
            player.pos={2058,128,1790};
            player.yaw=-.015f;
            enemies={};
            spawn_escort_wave();
        } else if (sortie==Sortie::fleet) reset_fleet();
        else spawn_wave();
        const auto axes=attitude(player.yaw);
        wingman={};
        wingman.craft={player.pos-axes.right*31+axes.front*28+Vec{0,8,0},player.yaw,0,0,0,player.type};
        wingman.velocity=axes.front*speed;
    }
    void spawn_escort_wave() {
        const auto axis=attitude(convoy.yaw);
        const float offset=wave==1 ? 0.f : wave==2 ? -167.f : 161.f;
        const int count=wave==1 ? 3 : 2;
        int group=0;
        for (auto& e:enemies) {
            if (e.health || group>=count) continue;
            const float side=group==0 ? 12.f : group==1 ? -105.f : 143.f;
            Vec p=convoy.pos+axis.right*(side+offset)+axis.front*(159.f+58.f*group);
            p.y=std::max(109.f+19.f*group,world.ground(p)+62.f);
            const auto type=Airframe((group+wave-1)%unsigned(Airframe::count));
            const Vec aim=convoy.pos-p;
            e={{p,std::atan2(aim.x,aim.z),-.16f,0,0,type},enemy_profile(type).hull,
                1.15f+group*.62f};
            ++group;
        }
    }
    void spawn_wave() {
        enemies={};
        const auto aim=attitude(player.yaw);
        constexpr float offsets[]{0,-97,131,-201,214};
        for (unsigned i=0; i<wave+2; ++i) {
            auto& e=enemies[i];
            const Vec at=player.pos + aim.right*offsets[i] + aim.front*(228.f+100.f*i);
            e.craft={at,player.yaw,0,i==1 ? .32f : -.23f};
            e.craft.type=Airframe((i+wave-1)%unsigned(Airframe::count));
            e.craft.pos.y=std::max(world.ground(at)+46.f,player.pos.y+float(i%3)*19.f-3.f);
            e.health=enemy_profile(e.craft.type).hull+int(wave>1 && e.craft.type!=Airframe::dart);
            e.cooldown=1.6f+.64f*i;
            e.age=0;
        }
    }
    int living() const {
        return int(std::count_if(enemies.begin(),enemies.end(),[](const Enemy& e){return e.health>0;}));
    }
    bool in_air() const {
        return phase==Phase::flight || phase==Phase::ejection ||
               phase==Phase::parachute || phase==Phase::pickup;
    }

    int target(float min_alignment, float range) const {
        const Vec forward=attitude(player.yaw,player.pitch).front;
        int best=-1;
        for (int i=0;i<int(enemies.size());++i) {
            if (!enemies[i].health) continue;
            const Vec d=enemies[i].craft.pos-player.pos;
            const float distance=norm2(d);
            if (distance>range*range) continue;
            const float along=dot(d,forward);
            if (along<=0 || along*along<=min_alignment*min_alignment*distance) continue;
            const float alignment=along/std::sqrt(std::max(.0001f,distance));
            if (alignment>min_alignment) { best=i; min_alignment=alignment; }
        }
        return best;
    }
    Shot* shot_slot() {
        for (auto& shot:shots) if (shot.life<=0) return &shot;
        return nullptr;
    }
    void emit(EffectKind kind,Vec at,float duration,Vec motion={},float strength=1) {
        effects[effect_next]={at,motion,0,duration,kind,++effect_serial,strength};
        effect_next=(effect_next+1)%effects.size();
    }
    void break_tip(Craft& craft,int side,Vec velocity) {
        if (craft.damage.missing_tip) return;
        craft.damage.missing_tip=side;
        const auto pose=attitude(craft.yaw,craft.pitch,craft.bank);
        const auto& model=airframe(craft.type);
        const Vec edge=craft.pos+pose.apply({side*model.span*.91f,0,model.sweep-model.tip_chord*.6f});
        emit(EffectKind::wing_piece,edge,1.9f,velocity*.64f+pose.right*(side*16.f)+pose.up*8.f);
    }
    void mark_damage(Craft& craft,Vec point,Vec velocity,bool critical) {
        const auto pose=attitude(craft.yaw,craft.pitch,craft.bank);
        const float lateral=dot(point-craft.pos,pose.right);
        const int side=lateral<0 ? -1 : 1;
        auto& damage=craft.damage;
        const bool wing=std::abs(lateral)>airframe(craft.type).body*.89f;
        auto& zone=wing ? (side<0 ? damage.port : damage.starboard) : damage.body;
        zone=std::min(3,int(zone)+1);
        if (critical || (wing && zone>=3)) break_tip(craft,side,velocity);
        emit(EffectKind::hit,point,.30f,velocity*.052f);
    }
    void damage_trail() {
        if (hull>=4 && !player.damage.missing_tip) return;
        if (damage_smoke_timer>0) return;
        const auto pose=attitude(player.yaw,player.pitch,player.bank);
        const auto& spec=airframe(player.type);
        const bool burning=hull<=1;
        const float side=player.damage.port>player.damage.starboard ? -1.f : 1.f;
        const Vec engine=player.pos+pose.apply({spec.nacelle*side,0,spec.tail-1});
        emit(burning ? EffectKind::burning : EffectKind::smoke,engine,
             burning ? 1.65f : 1.22f,{3,6,-1},burning ? 1.f : .72f);
        damage_smoke_timer=burning ? .092f : .145f;
    }

    void update_wingman(float dt) {
        auto& wing=wingman;
        if (!wing.hull) return;
        auto& c=wing.craft;
        wing.cooldown-=dt;
        wing.protection-=dt;
        if (phase!=Phase::flight) {
            wing.velocity.y=mix(wing.velocity.y,16.f,saturate(dt*2.7f));
            c.pos+=wing.velocity*dt;
            c.pos.y=std::max(c.pos.y,world.ground(c.pos)+16);
            return;
        }
        const auto leader=attitude(player.yaw);
        Vec station=player.pos-leader.right*32+leader.front*31+Vec{0,7,0};
        Vec track=attitude(player.yaw+side_slip,flight_path).front*speed;
        if (fleet.deck.phase!=DeckPhase::free) {
            const float orbit=elapsed*.50f;
            const float s=std::sin(orbit),c=std::cos(orbit);
            station=fleet.units[0].pos+Vec{177*s,104,177*c};
            track={88.5f*c,0,-88.5f*s};
        }
        station.y=std::max(station.y,world.ground(station)+26);
        Vec correction=(station-c.pos)*1.72f;
        const float catchup=norm2(correction);
        if (catchup>63.f*63.f) correction=unit(correction)*63.f;
        wing.velocity+=(track+correction-wing.velocity)*saturate(dt*3.2f);
        c.pos+=wing.velocity*dt;
        c.pos.y=std::max(c.pos.y,world.ground(c.pos)+18);
        const float course=std::atan2(wing.velocity.x,wing.velocity.z);
        const float yaw_error=std::remainder(course-c.yaw,2*pi);
        const float pitch=std::atan2(wing.velocity.y,std::hypot(wing.velocity.x,wing.velocity.z));
        c.yaw+=std::clamp(yaw_error*3,-2.4f,2.4f)*dt;
        c.bank=mix(c.bank,-std::clamp(yaw_error*.84f,-.92f,.92f),saturate(dt*4));
        c.aileron=mix(c.aileron,-std::clamp(yaw_error,-1.f,1.f),saturate(dt*7));
        c.elevator=std::clamp((pitch-c.pitch)*2.3f,-1.f,1.f);
        c.rudder=std::clamp(yaw_error,-1.f,1.f);
        c.pitch=mix(c.pitch,pitch,saturate(dt*5));
        if (wing.cooldown>0 || elapsed<1.7f) return;
        const auto pose=attitude(c.yaw,c.pitch,c.bank);
        const Vec muzzle=c.pos+pose.apply({-3.8f,-.55f,8.5f});
        float distance=345.f*345.f;
        const Enemy* threat=nullptr;
        for (const auto& e:enemies) {
            const auto to=e.craft.pos-muzzle;
            const float length=norm2(to),forward=dot(to,pose.front);
            if (!e.health || length>=distance || forward<=0 || forward*forward<length*.60f) continue;
            threat=&e;
            distance=length;
        }
        if (!threat) return;
        constexpr float muzzle_speed=520.f;
        const auto& foe=threat->craft;
        const Vec offset=foe.pos-muzzle;
        const Vec motion=attitude(foe.yaw,foe.pitch).front*(enemy_profile(foe.type).speed+wave*4);
        const float gap=muzzle_speed*muzzle_speed-norm2(motion);
        const float projection=dot(offset,motion);
        const float travel_time=(projection+std::sqrt(projection*projection+gap*distance))/gap;
        const Vec ray=offset+motion*travel_time;
        const float along=saturate(dot(player.pos-muzzle,ray)/std::max(1.f,norm2(ray)));
        if (norm2(player.pos-(muzzle+ray*along))<19*19.f) return;
        if (auto* bullet=shot_slot()) {
            const auto aim=unit(ray);
            *bullet={muzzle,muzzle,aim*muzzle_speed,.89f,false,false,-1,0,true};
            emit(EffectKind::muzzle,muzzle,.07f,aim,.72f);
            wing.cooldown=1.45f;
        }
    }
    void update_convoy(float dt) {
        convoy.move(elapsed,dt);
        if (wave<3 && convoy.progress>=float(wave)*.34f && living()<=3) {
            ++wave;
            spawn_escort_wave();
        }
        if ((rockets<4 || hull<5) && convoy.can_resupply(player.pos)) {
            convoy.resupplied=true;
            convoy.resupply_flash=2.1f;
            rockets=std::min(4u,rockets+2);
            hull=std::min(5u,hull+1);
        }
        if (convoy.hull<=Convoy::max_hull/2) {
            convoy.smoke_delay-=dt;
            if (convoy.smoke_delay<=0) {
                emit(EffectKind::smoke,convoy.pos+Vec{0,9,0},1.65f,{4,8,-3},.91f);
                convoy.smoke_delay=.19f;
            }
        }
    }
    void shoot() {
        if (phase!=Phase::flight || fleet.deck.phase!=DeckPhase::free) return;
        if (shot_delay>0) return;
        auto* shot=shot_slot();
        if (!shot) return;
        const auto basis=attitude(player.yaw,player.pitch,player.bank);
        const auto& spec=airframe(player.type);
        const float side=int(elapsed*25)%2==0 ? -1 : 1;
        const Vec origin=player.pos+basis.apply({side*(spec.body+1),-.55f,spec.nose*.67f});
        Vec direction=basis.front;
        const int lock=target(.977f,560);
        const int ground_lock=lock<0 && sortie==Sortie::fleet ? surface_lock(.984f,540.f) : -1;
        if (ground_lock>=0) direction=unit(fleet.units[ground_lock].aim()-origin);
        else if (lock>=0) {
            const auto& e=enemies[lock].craft;
            const float lead=std::sqrt(norm2(e.pos-origin))/590.f;
            direction=unit(e.pos+attitude(e.yaw,e.pitch).front*((enemy_profile(e.type).speed+wave*4)*lead)-origin);
        }
        *shot={origin,origin,direction*590.f,1.35f};
        emit(EffectKind::muzzle,origin,.085f,direction);
        shot_delay=.125f;
    }
    bool surface_aim() const {
        return player.pitch<-.14f && player.pos.y < -player.pitch*880.f;
    }
    bool launch_missile() {
        if (phase!=Phase::flight || !rockets || missile_delay>0 || fleet.deck.phase!=DeckPhase::free) return false;
        const int ground_lock=sortie==Sortie::fleet ? surface_lock() : -1;
        const int lock=ground_lock<0 ? target(.74f,930) : -1;
        auto* shot=shot_slot();
        if ((lock<0 && ground_lock<0 && !surface_aim()) || !shot) return false;
        const auto pose=attitude(player.yaw,player.pitch,player.bank);
        const Vec origin=player.pos+pose.front*6+pose.right*(rockets%2==0 ? -8.f : 8.f);
        *shot={origin,origin,pose.front*238.f,4.f,false,true,lock};
        shot->surface_target=ground_lock;
        --rockets; missile_delay=.65f;
        return true;
    }
    void surface_impact(Vec at,Vec velocity,float strength) {
        const auto surface=world.surface_at(at.x,at.z);
        at.y=surface.height();
        const Vec travel=unit({velocity.x,0,velocity.z});
        const bool water=surface.water();
        emit(water ? EffectKind::water_hit : EffectKind::land_hit,at,
             water ? .84f+strength*.31f : .62f+strength*.46f,travel,strength);
    }
    bool find_surface(Vec from,Vec to,Vec& impact) const {
        if (std::min(from.y,to.y)>World::highest_land) return false;
        const Vec travel=to-from;
        // Match the terrain sampling grid so a shot cannot skip a low ridge.
        const int samples=1+int(std::max(std::abs(travel.x),std::abs(travel.z))*.5f);
        float last=0;
        for (int i=1;i<=samples;++i) {
            float end=float(i)/samples;
            const Vec point=from+travel*end;
            if (point.y>world.ground(point)) { last=end; continue; }
            for (int j=0;j<4;++j) {
                const float middle=(last+end)*.5f;
                const Vec probe=from+travel*middle;
                if (probe.y<=world.ground(probe)) end=middle;
                else last=middle;
            }
            impact=from+travel*end;
            return true;
        }
        return false;
    }
    void update_enemies(float dt) {
        const bool active=phase==Phase::flight;
        const bool escort=active && sortie==Sortie::escort;
        const Vec protected_ship=convoy.pos+Vec{0,4.5f,0};
        const Vec ship_lead=escort ? attitude(convoy.yaw).front*Convoy::speed : Vec{};
        const Vec player_heading=attitude(player.yaw,player.pitch).front;
        for (unsigned i=0; i<enemies.size(); ++i) {
            auto& e=enemies[i];
            if (!e.health) continue;
            auto& c=e.craft;
            const auto& spec=enemy_profile(c.type);
            if (e.deck_launch>0) {
                e.deck_launch=std::max(0.f,e.deck_launch-dt);
                c.pitch=mix(c.pitch,.39f,saturate(dt*3.f));
                c.pos+=attitude(c.yaw,c.pitch).front*((spec.speed+10.f)*dt);
                continue;
            }
            const bool heavy=c.type==Airframe::maritime;
            const bool fast=c.type==Airframe::dart;
            const bool attack_ship=escort && (!fast || norm2(player.pos-c.pos)>290.f*290.f);
            const Vec chase=attack_ship ? protected_ship : active ? player.pos : pilot;
            const Vec to=chase-c.pos;
            const float distance=norm2(to);
            e.age+=dt; e.cooldown-=dt; e.smoke_delay-=dt;
            float height=c.pos.y;
            if (escort || e.age>2.1f+i*.47f) {
                const float room=saturate(1.f-distance/(spec.spacing*spec.spacing));
                const float side=i&1 ? -1.f : 1.f;
                const float arc=std::sin(e.age*(fast ? .76f : .43f)+i*1.7f);
                const float yaw=std::atan2(to.x,to.z)+arc*.19f+side*room*(attack_ship ? .81f : heavy ? 1.86f : 1.33f);
                const float diff=std::remainder(yaw-c.yaw,2*pi);
                c.yaw+=std::clamp(diff,-spec.turn,spec.turn)*dt;
                const float last_bank=c.bank;
                c.bank=mix(c.bank,-std::clamp(diff,-1.f,1.f),dt*4);
                c.aileron=std::clamp((c.bank-last_bank)*6,-1.f,1.f);
                height=chase.y+spec.altitude+arc*(fast ? 45.f : 18.f);
            }
            height=std::clamp(std::max(height,world.ground(c.pos)+43.f),63.f,410.f);
            const float pitch_error=std::clamp((height-c.pos.y)*.026f,-.37f,.49f)-c.pitch;
            c.elevator=std::clamp(pitch_error*2.6f,-1.f,1.f);
            c.rudder=std::clamp(-c.bank*.47f,-1.f,1.f);
            c.pitch+=pitch_error*(dt*3);
            const auto pose=attitude(c.yaw,c.pitch);
            c.pos+=pose.front*((spec.speed+wave*4)*dt);
            if (e.health==1 && e.smoke_delay<=0 && distance<650.f*650.f) {
                emit(EffectKind::smoke,c.pos-pose.front*10,.91f,{1,5,0},.64f);
                e.smoke_delay=.15f;
            }
            if (!active || e.cooldown>0 || distance>spec.range*spec.range ||
                (attack_ship && distance<94.f*94.f)) continue;
            const float facing=dot(to,pose.front);
            if (facing<=0 || facing*facing<distance*.43f) continue;
            if (auto* shot=shot_slot()) {
                const float side=heavy ? (e.followup ? -3.5f : 3.5f) : 0.f;
                const Vec origin=c.pos+pose.front*10+pose.right*side;
                const Vec lead=attack_ship ? ship_lead*(std::sqrt(distance)/269.f) :
                    player_heading*(fast ? 42.f : 26.f);
                const Vec aim=unit(chase-origin+lead);
                *shot={origin,origin,aim*(fast ? 313.f : 269.f),2.2f,true};
                emit(EffectKind::muzzle,origin,.086f,aim,.65f);
                e.followup=heavy && !e.followup;
                e.cooldown=e.followup ? .23f : spec.reload+i*.11f;
            }
        }
    }
    void eject() {
        if (phase!=Phase::flight) return;
        fleet.deck={};
        const auto basis=attitude(player.yaw,player.pitch,player.bank);
        pilot=player.pos+basis.up*5;
        pilot_yaw=player.yaw;
        pilot_lean=0;
        wreck_velocity=attitude(player.yaw+side_slip,flight_path).front*speed;
        drift=wreck_velocity*.34f+Vec{0,40,0};
        wreck_time=7.2f;
        smoke_timer=0;
        wake_count=0;
        boosted=false;
        phase=Phase::ejection;
        phase_time=0;
        eject_smoke_timer=0;
        player.damage.canopy_lost=true;
        emit(EffectKind::canopy_piece,player.pos+basis.up*4+basis.front,
             1.7f,wreck_velocity*.65f+basis.up*31-basis.right*11);
        emit(EffectKind::ejection,pilot,.72f,{0,-14,0});
    }
    void open_chute() {
        if (phase!=Phase::ejection || phase_time<.24f) return;
        phase=Phase::parachute; phase_time=0;
        drift.y=std::max(drift.y,-18.f);
    }
    void guide_rescue() {
        const auto frame=attitude(pilot_yaw);
        const float approach=saturate((phase_time-.55f)/3.25f);
        if (phase_time<.55f) return;
        const float offset=1-approach;
        const Vec ahead=pilot+frame.front*(-28.f*offset)-frame.right*(148.f*offset*offset)
                         +Vec{0,-5.f-14.f*offset,0};
        const float terrain=world.ground(ahead);
        if (ahead.y<terrain+7.f) { rescue_visible=false; return; }
        rescue_visible=true;
        const float next_yaw=pilot_yaw+std::atan2(296.f*offset,58.f);
        rescue_plane={ahead,next_yaw,-.05f*offset,-.38f*std::sin(approach*pi),0,player.type};
        hatch=saturate((phase_time-1.7f)/1.30f);
        if (approach>=1 && norm2(pilot-(ahead+Vec{0,5,0}))<4.f) {
            seat_start=pilot;
            phase=Phase::pickup;
            phase_time=0;
        }
    }
    void secure_pilot(float dt) {
        const auto pose=attitude(rescue_plane.yaw,rescue_plane.pitch);
        const float settled=saturate(phase_time/.52f);
        rescue_plane.pos+=pose.front*((18.f+phase_time*28.f)*dt);
        rescue_plane.pos.y+=std::max(0.f,phase_time-.45f)*13.f*dt;
        rescue_plane.pitch=mix(rescue_plane.pitch,.24f,saturate(dt*5.f));
        const auto cockpit=rescue_plane.pos+pose.apply({0,2.15f,1.1f});
        pilot=cockpit+(seat_start-cockpit)*(1-settled);
        hatch=1-saturate((phase_time-.52f)/.57f);
        if (phase_time>=1.4f) {
            airlift=true;
            phase=Phase::rescued;
            phase_time=0;
        }
    }

    void fly(Input in,const FlightTuning& tuning,float dt) {
        const float ratio=speed/tuning.cruise;
        const int missing=player.damage.missing_tip;
        const float authority=mix(.39f,1.f,saturate((ratio-.47f)*1.95f))*(missing ? .86f : 1.f);
        const float bank_error=std::remainder(-in.turn*1.04f-missing*.17f-player.bank,2*pi);
        const float commanded_roll=(in.roll ? -in.turn*5.1f*tuning.roll :
            std::clamp(bank_error*5.6f*tuning.roll,-4.8f,4.8f))*authority;
        roll_rate=mix(roll_rate,commanded_roll,saturate(dt*12));
        player.bank=std::remainder(player.bank+roll_rate*dt,2*pi);
        player.aileron=mix(player.aileron,std::clamp(commanded_roll*.32f,-1.f,1.f),saturate(dt*16));

        const float bank_sine=std::sin(player.bank);
        const float upright=std::max(0.f,std::cos(player.bank));
        const float load=1-upright;
        const float command_yaw=tuning.turn*authority*((in.roll ? -.37f : -.96f)*bank_sine /
            std::clamp(ratio,.63f,1.9f)+(in.roll ? 0.f : .085f*in.turn));
        yaw_rate=mix(yaw_rate,command_yaw,saturate(dt*7));
        player.rudder=mix(player.rudder,std::clamp(yaw_rate*.69f-in.turn*.14f,-1.f,1.f),saturate(dt*9));
        player.yaw=std::remainder(player.yaw+yaw_rate*dt,2*pi);
        side_slip=mix(side_slip,(-yaw_rate*.060f-roll_rate*.014f),saturate(dt*4.1f));

        constexpr float trim=.024f;
        const float target_pitch=trim+in.climb*(in.climb>0 ? .74f : .63f)*tuning.pitch;
        const float pitch_error=target_pitch-player.pitch;
        pitch_rate=mix(pitch_rate,std::clamp(pitch_error*4.35f,-2.75f,2.75f)*authority,saturate(dt*13));
        player.pitch=std::clamp(player.pitch+pitch_rate*dt,-.88f,.96f);
        const float elevator=std::clamp(in.climb*.62f+pitch_error*1.35f,-1.f,1.f);
        player.elevator=mix(player.elevator,elevator,saturate(dt*12));

        // Keep the velocity vector moving through a pitch input and account for lost lift in a steep bank.
        const float low_speed=saturate((.79f-ratio)*2.8f);
        float path_target=player.pitch-trim-load*.108f-low_speed*.24f;
        if (path_target>0) path_target*=saturate((420.f-player.pos.y)/38.f);
        flight_path=mix(flight_path,path_target,saturate(dt*5.7f*authority));
        const float vertical=std::sin(flight_path);
        const float throttle=boosted ? tuning.boost : tuning.cruise;
        const float drag=load*16.f+std::max(0.f,pitch_rate)*2.3f+(hull<=2 ? 5.6f : 0.f);
        speed=std::clamp(speed+dt*((throttle-speed)*(boosted ? 1.9f : 1.22f)-9.81f*vertical-drag),
                         tuning.cruise*.53f,tuning.boost+9.f);
        const Vec track=attitude(player.yaw+side_slip,flight_path).front;
        player.pos+=track*(speed*dt);
        player.pos.y=std::min(player.pos.y,420.f);
    }

    void update_wreck(float dt) {
        if (wreck_time<=0) return;
        wreck_time-=dt;
        player.pos+=wreck_velocity*dt;
        wreck_velocity.y-=37.f*dt;
        player.bank+=1.9f*dt;
        player.pitch=std::max(-1.5f,player.pitch-.76f*dt);
        smoke_timer-=dt;
        if (smoke_timer<=0) {
            emit(EffectKind::burning,player.pos,1.58f,{3,6,0},.96f);
            smoke_timer=.11f;
        }
        const float ground=world.ground(player.pos);
        if (player.pos.y<=ground) {
            surface_impact(player.pos,wreck_velocity,2.65f);
            emit(EffectKind::explosion,{player.pos.x,ground+4,player.pos.z},1.35f,{0,5,0});
            wreck_time=0;
        } else if (wreck_time<=0) {
            emit(EffectKind::explosion,player.pos,1.35f,wreck_velocity*.044f);
        }
    }

    void update(Input in, float dt) {
        if (!in_air()) return;
        elapsed+=dt; phase_time+=dt;
        for (auto& e:effects) if (e.age<e.duration) e.age+=dt;
        hit_feedback=std::max(0.f,hit_feedback-dt);
        damage_feedback=std::max(0.f,damage_feedback-dt);
        exhaust_heat=saturate(exhaust_heat+dt*(phase==Phase::flight && fleet.deck.phase==DeckPhase::free &&
                                              in.boost && energy>0.03f ? 5.f : -1.85f));
        if (phase==Phase::flight) {
            shot_delay-=dt; missile_delay-=dt; invulnerable-=dt;
            damage_smoke_timer-=dt;
            const Vec previous=player.pos;
            const bool deck_active=fleet.deck.phase!=DeckPhase::free;
            boosted=!deck_active && in.boost && energy>0.03f;
            energy=saturate(energy+dt*(boosted ? -.33f : .21f));
            const auto& spec=airframe(player.type);
            if (deck_active) advance_deck(dt);
            else {
                fly(in,spec.flight,dt);
                if (sortie==Sortie::fleet) cross_deck(previous);
            }
            const auto pose=attitude(player.yaw,player.pitch,player.bank);
            damage_trail();
            if (in.fire) shoot();
            wake_timer-=dt;
            if (wake_timer<=0 && fleet.deck.phase==DeckPhase::free) {
                wake_timer=.053f;
                const float vapor=saturate((std::abs(player.bank)*.64f+std::abs(player.pitch)*.78f-.26f)*2);
                wake[wake_next]={player.pos+pose.apply({-spec.span,0,spec.sweep-spec.tip_chord}),
                                 player.pos+pose.apply({spec.span,0,spec.sweep-spec.tip_chord}),vapor};
                wake_next=(wake_next+1)%wake.size();
                wake_count=std::min(wake_count+1,int(wake.size()));
            }
            const auto surface=world.surface_at(player.pos.x,player.pos.z);
            const float ground=surface.height();
            const float clearance=player.pos.y-ground;
            spray_timer-=dt;
            if (fleet.deck.phase==DeckPhase::free && surface.water() && clearance>4 && clearance<35 && speed>88 && spray_timer<=0) {
                const float strength=saturate((35.f-clearance)/27.f)*saturate((speed-76.f)/49.f);
                emit(EffectKind::spray,{player.pos.x,ground,player.pos.z},1.04f,attitude(player.yaw).right,strength);
                spray_timer=.092f;
            }
            if (player.pos.y<=ground+4) {
                player.pos.y=ground+4;
                hull=0;
                break_tip(player,player.bank<=0 ? 1 : -1,pose.front*speed);
                eject();
            }
            if (phase==Phase::flight && sortie==Sortie::escort) update_convoy(dt);
        } else if (phase==Phase::pickup) {
            secure_pilot(dt);
        } else {
            if (phase==Phase::ejection) {
                drift.y-=62.f*dt;
                pilot+=drift*dt;
                eject_smoke_timer-=dt;
                if (phase_time<.36f && eject_smoke_timer<=0) {
                    emit(EffectKind::missile_smoke,pilot+Vec{0,-3,0},.60f,{2,-6,0});
                    eject_smoke_timer=.065f;
                }
                if (phase_time>1.28f || (phase_time>.28f && in.fire)) open_chute();
            } else {
                pilot_lean=mix(pilot_lean,-in.turn,std::min(1.f,dt*5.5f));
                pilot_yaw=std::remainder(pilot_yaw+in.turn*1.55f*dt,2*pi);
                const auto front=attitude(pilot_yaw).front;
                const Vec desired=front*(19+in.climb*10)+Vec{7,-24+in.climb*3,0};
                drift+=(desired-drift)*std::min(1.f,dt*3.5f);
                pilot+=drift*dt;
            }
            const auto surface=world.surface_at(pilot.x,pilot.z);
            const float ground=surface.height();
            if (pilot.y<=ground+1.5f) {
                pilot.y=ground+1.5f;
                wet=surface.water();
                phase=phase==Phase::parachute ? Phase::rescued : Phase::lost;
                phase_time=0;
            } else if (phase==Phase::parachute) guide_rescue();
        }

        if (phase!=Phase::flight) update_wreck(dt);
        update_wingman(dt);
        update_enemies(dt);
        if (sortie==Sortie::fleet) update_fleet(dt);

        for (auto& s:shots) {
            if (s.life<=0) continue;
            s.life-=dt;
            if (s.missile && ((s.target>=0 && enemies[s.target].health) ||
                              (s.surface_target>=0 && fleet.units[s.surface_target].hull))) {
                const auto aim=s.surface_target>=0 ? fleet.units[s.surface_target].aim() : enemies[s.target].craft.pos;
                const auto desired=unit(aim-s.pos)*328.f;
                s.velocity=unit(s.velocity+(desired-s.velocity)*(dt*7.f))*328.f;
            }
            s.previous=s.pos;
            s.pos+=s.velocity*dt;
            if (phase==Phase::flight && sortie==Sortie::escort && convoy.intercepts(s.previous,s.pos)) {
                if (s.hostile && convoy.damage_flash<=0) {
                    --convoy.hull;
                    convoy.damage_flash=.20f;
                    emit(convoy.hull ? EffectKind::hit : EffectKind::explosion,convoy.pos+Vec{0,7,0},
                         convoy.hull ? .36f : 1.8f);
                    if (!convoy.hull) {
                        phase=Phase::escort_lost;
                        phase_time=0;
                    }
                }
                s.life=0;
                continue;
            }
            Vec impact;
            const bool terrain_hit=find_surface(s.previous,s.pos,impact);
            if (sortie==Sortie::fleet && hit_fleet(s,terrain_hit ? impact : s.pos)) continue;
            if (terrain_hit) {
                surface_impact(impact,s.velocity,s.missile ? 2.15f : s.hostile ? .71f : 1.f);
                s.life=0;
                continue;
            }
            if (s.missile) {
                s.exhaust_delay-=dt;
                if (s.exhaust_delay<=0) {
                    emit(EffectKind::missile_smoke,s.previous,.67f,{4,3,0});
                    s.exhaust_delay=.057f;
                }
            }
            const Vec segment=s.pos-s.previous;
            const float inverse_length=1.f/std::max(1.f,norm2(segment));
            const auto contact=[&](Vec center) {
                const float t=saturate(dot(center-s.previous,segment)*inverse_length);
                return s.previous+segment*t;
            };
            const auto hits=[&](Vec center,float radius) {
                return norm2(center-contact(center)) < radius*radius;
            };
            if (s.hostile) {
                if (phase==Phase::flight && hits(player.pos,11)) {
                    s.life=0;
                    if (invulnerable<=0) {
                        --hull; invulnerable=.65f;
                        damage_feedback=.38f;
                        mark_damage(player,contact(player.pos),attitude(player.yaw+side_slip,flight_path).front*speed,hull<=1);
                        if (!hull) eject();
                    }
                } else if (phase==Phase::flight && wingman.hull && hits(wingman.craft.pos,10)) {
                    s.life=0;
                    if (wingman.protection<=0) {
                        --wingman.hull;
                        wingman.protection=.45f;
                        mark_damage(wingman.craft,contact(wingman.craft.pos),wingman.velocity,!wingman.hull);
                        if (!wingman.hull) emit(EffectKind::explosion,wingman.craft.pos,1.2f,wingman.velocity*.096f);
                    }
                }
                continue;
            }
            for (auto& e:enemies) if (e.health && hits(e.craft.pos,s.missile ? 19.f : 14.f)) {
                s.life=0;
                e.health=std::max(0,e.health-(s.missile ? 3 : 1));
                const bool down=e.health==0;
                const Vec velocity=attitude(e.craft.yaw,e.craft.pitch).front*(enemy_profile(e.craft.type).speed+wave*4);
                mark_damage(e.craft,contact(e.craft.pos),velocity,down);
                if (down) emit(EffectKind::explosion,e.craft.pos,1.16f,velocity*.052f);
                if (s.allied) wingman_score+=down;
                else {
                    hit_feedback=down ? .47f : .20f;
                    score+=down;
                }
                break;
            }
        }
        if (phase==Phase::flight && sortie==Sortie::escort && convoy.progress>=1.f) {
            phase=Phase::victory;
            phase_time=0;
        } else if (phase==Phase::flight && sortie==Sortie::patrol && !living()) {
            wave_delay+=dt;
            if (wave_delay>1.8f) {
                if (wave==3) phase=Phase::victory;
                else {
                    ++wave;
                    rockets=std::min(rockets+2u,4u);
                    hull=std::min(hull+1u,5u);
                    wave_delay=0;
                    spawn_wave();
                }
            }
        }
    }
};

} // namespace sky
