#include "game.hpp"

namespace sky {
namespace {
constexpr Vec gunboat_center{2073,0,2258};

Vec deck_local(const SurfaceUnit& ship,Vec point) {
    const auto axes=attitude(ship.yaw);
    const Vec offset=point-ship.pos;
    return {dot(offset,axes.right),offset.y,dot(offset,axes.front)};
}

float deck_beam(float z) {
    const float at=z/CarrierDeck::half_length;
    if (at<-.67f) return mix(34.f,40.f,saturate((at+1.f)/.33f));
    if (at>.63f) return mix(33.f,11.2f,saturate((at-.63f)/.37f));
    return mix(40.f,33.f,saturate((at+.67f)/1.30f));
}
}

float SurfaceUnit::contact(Vec from,Vec to) const {
    const float width=carrier() ? CarrierDeck::half_width : afloat() ? 7.f : 11.f;
    const float length=carrier() ? CarrierDeck::half_length : afloat() ? 18.f : 11.f;
    const float height=carrier() ? CarrierDeck::height+1.f : afloat() ? 8.f : 10.f;
    const Vec delta=to-from, offset=from-pos;
    const float reach=length+width+height+10;
    if (norm2(offset)>2*(reach*reach+norm2(delta))) return 2.f;
    const Basis pose=attitude(yaw);
    const Vec start{dot(offset,pose.right),offset.y,dot(offset,pose.front)};
    const Vec step{dot(delta,pose.right),delta.y,dot(delta,pose.front)};
    float first=0,last=1;
    const auto slab=[&](float p,float d,float low,float high) {
        if (std::abs(d)<.0001f) return p>=low && p<=high;
        const float x=(low-p)/d,y=(high-p)/d;
        first=std::max(first,std::min(x,y));
        last=std::min(last,std::max(x,y));
        return first<=last;
    };
    return slab(start.x,step.x,-width,width) && slab(start.y,step.y,0,height) &&
           slab(start.z,step.z,-length,length) ? first : 2.f;
}

void Game::reset_fleet() {
    player.pos={2008,134,1730};
    player.yaw=.055f;
    enemies={};
    fleet.units={{
        {SurfaceRole::ally_carrier,{2048,0,1924},-.24f,10},
        {SurfaceRole::enemy_carrier,{2048,0,2514},pi,6},
        {SurfaceRole::gunboat,gunboat_center,.12f,3,3.8f},
        {SurfaceRole::battery,{1795,world.ground({1795,0,2408}),2408},.87f,3,5.0f},
    }};
}

bool Game::can_approach_deck() const {
    const auto& ship=fleet.units[0];
    if (phase!=Phase::flight || sortie!=Sortie::fleet || !ship.hull ||
        fleet.deck.phase!=DeckPhase::free) return false;
    const Vec local=deck_local(ship,player.pos);
    return local.z>-615 && local.z<-138 && local.y>25 && local.y<185 &&
           std::abs(local.x-CarrierDeck::runway_x)<106 &&
           std::abs(std::remainder(player.yaw-ship.yaw,2*pi))<.79f;
}

bool Game::deck_control() {
    auto& op=fleet.deck;
    if (phase!=Phase::flight || sortie!=Sortie::fleet || !fleet.units[0].hull) return false;
    if (op.phase==DeckPhase::parked) {
        op.phase=DeckPhase::launch;
        return true;
    }
    if (op.phase==DeckPhase::approach) {
        op={};
        speed=std::max(speed,airframe(player.type).flight.cruise*.60f);
        player.gear=false;
        return true;
    }
    if (!can_approach_deck()) return false;
    const Vec local=deck_local(fleet.units[0],player.pos);
    const float duration=std::clamp((CarrierDeck::touchdown-local.z)/67.f,2.4f,8.f);
    op={DeckPhase::approach,local,player.yaw,player.pitch,player.bank,0,duration};
    player.gear=true;
    wake_count=0;
    return true;
}

void Game::advance_deck(float dt) {
    auto& op=fleet.deck;
    const auto& ship=fleet.units[0];
    const auto axes=attitude(ship.yaw);
    Vec local=deck_local(ship,player.pos);
    if (op.phase==DeckPhase::approach) {
        op.elapsed+=dt;
        const float t=saturate(op.elapsed/op.duration);
        const float alignment=saturate(t*2.2f);
        const float smooth=alignment*alignment*(3-2*alignment);
        const float height_blend=t*t*(3-2*t);
        local={mix(op.start.x,CarrierDeck::runway_x,smooth),
               mix(op.start.y,CarrierDeck::rest,height_blend),
               mix(op.start.z,CarrierDeck::touchdown,t)};
        speed=(CarrierDeck::touchdown-op.start.z)/op.duration;
        player.yaw=op.yaw+std::remainder(ship.yaw-op.yaw,2*pi)*smooth;
        const float slope=(CarrierDeck::rest-op.start.y)*6*t*(1-t)/
                          std::max(1.f,CarrierDeck::touchdown-op.start.z);
        player.pitch=mix(op.pitch,std::clamp(slope,-.55f,.08f),smooth);
        player.bank=op.bank*(1-smooth);
        if (t>=1) op.phase=DeckPhase::arrest;
    } else if (op.phase==DeckPhase::arrest) {
        const float braking=59.f;
        const float distance=(speed+std::max(0.f,speed-braking*dt))*.5f*dt;
        speed=std::max(0.f,speed-braking*dt);
        local.x=mix(local.x,CarrierDeck::runway_x,saturate(dt*2.6f));
        local.z+=distance;
        if (speed==0) {
            op.phase=DeckPhase::parked;
            local.x=CarrierDeck::runway_x;
            if (!fleet.supplied) {
                fleet.supplied=true;
                fleet.supply_flash=2.f;
                rockets=4;
                hull=std::min(5u,hull+1);
            }
        }
    } else if (op.phase==DeckPhase::launch) {
        speed=std::min(151.f,speed+76.f*dt);
        local.z+=speed*dt;
        if (local.z>=CarrierDeck::launch_edge) {
            local.y+=1.2f;
            player.gear=false;
            player.pitch=.20f;
            flight_path=.14f;
            op={};
        }
    }
    player.pos=ship.pos+axes.apply(local);
    yaw_rate=pitch_rate=roll_rate=side_slip=0;
    player.aileron=player.elevator=player.rudder=0;
}

void Game::cross_deck(Vec previous) {
    for (const auto& ship:fleet.units) {
        if (!ship.carrier() || !ship.hull || previous.y<=CarrierDeck::rest || player.pos.y>CarrierDeck::rest)
            continue;
        const float crossing=(previous.y-CarrierDeck::rest)/(previous.y-player.pos.y);
        Vec local=deck_local(ship,previous+(player.pos-previous)*crossing);
        if (std::abs(local.z)>CarrierDeck::half_length || std::abs(local.x)>deck_beam(local.z)) continue;
        const bool aligned=std::abs(std::remainder(player.yaw-ship.yaw,2*pi))<.24f;
        const bool slow=std::abs(flight_path)<.16f && speed<118.f && std::abs(player.bank)<.27f;
        const float stopping=speed*speed/(2*59.f);
        const bool clear=local.z<26 && local.z+stopping<86 &&
                         std::abs(local.x-CarrierDeck::runway_x)<4.5f;
        player.pos.y=CarrierDeck::rest;
        if (ship.friendly() && aligned && slow && clear) {
            player.pos=ship.pos+attitude(ship.yaw).apply(local);
            player.yaw=ship.yaw;
            player.pitch=player.bank=0;
            player.gear=true;
            fleet.deck.phase=DeckPhase::arrest;
            wake_count=0;
        } else {
            hull=0;
            break_tip(player,player.bank<=0 ? 1 : -1,attitude(player.yaw,player.pitch).front*speed);
            eject();
        }
        return;
    }
}

int Game::surface_lock(float alignment,float range) const {
    const auto direction=attitude(player.yaw,player.pitch).front;
    int selected=-1;
    for (unsigned i=1;i<fleet.units.size();++i) {
        const auto& unit=fleet.units[i];
        if (!unit.hull) continue;
        const Vec delta=unit.aim()-player.pos;
        const float d=norm2(delta);
        if (d>range*range || d<1) continue;
        const float along=dot(delta,direction);
        if (along<=0 || along*along<=d*alignment*alignment) continue;
        alignment=along/std::sqrt(d);
        selected=int(i);
    }
    return selected;
}

void Game::update_fleet(float dt) {
    for (auto& unit:fleet.units) {
        unit.flash=std::max(0.f,unit.flash-dt);
        if (!unit.hull) unit.wreck_age+=dt;
    }
    if (phase!=Phase::flight) return;
    auto& ally=fleet.units[0];
    auto& carrier=fleet.units[1];
    auto& boat=fleet.units[2];
    const auto close=player.pos-ally.pos;
    fleet.supply_flash=std::max(0.f,fleet.supply_flash-dt);
    if (!fleet.supplied && (rockets<4 || hull<5) && close.y>24 && close.y<97 &&
        close.x*close.x+close.z*close.z<88.f*88.f) {
        fleet.supplied=true;
        fleet.supply_flash=2.f;
        rockets=4;
        hull=std::min(5u,hull+1);
    }
    if (boat.hull) {
        const float t=elapsed*.24f;
        const float u=std::sin(t),v=std::cos(t);
        boat.pos=gunboat_center+Vec{13*u,0,47*(1-v)};
        boat.yaw=std::atan2(13*v,47*u);
    }
    if (carrier.hull && fleet.launched<3 && elapsed>3.3f+8.5f*fleet.launched) {
        for (auto& enemy:enemies) {
            if (enemy.health) continue;
            const auto axes=attitude(carrier.yaw);
            const auto type=fleet.launched&1 ? Airframe::dart : Airframe::patrol;
            enemy={{carrier.pos+axes.apply({CarrierDeck::runway_x,CarrierDeck::rest,36}),carrier.yaw,.22f,0,0,type},
                   enemy_profile(type).hull,2.f,0,0,false,1.55f};
            ++fleet.launched;
            break;
        }
    }
    for (auto& unit:fleet.units) {
        unit.smoke-=dt;
        const int damaged=unit.carrier() ? 3 : 1;
        if (unit.hull<=damaged && unit.smoke<=0 && unit.wreck_age<2.3f) {
            emit(unit.hull ? EffectKind::smoke : EffectKind::burning,unit.aim(),1.32f,{3,8,2});
            unit.smoke=.23f;
        }
        if (!unit.hull || unit.friendly() || unit.carrier()) continue;
        unit.cooldown-=dt;
        if (unit.cooldown>0) continue;
        const Vec origin=unit.aim()+Vec{0,2.5f,0};
        const bool fires_at_ship=unit.afloat() && (int(elapsed*.61f)&1) &&
                                 norm2(player.pos-origin)>240.f*240.f;
        const Vec aim=fires_at_ship ? ally.aim() : player.pos;
        const float distance=norm2(aim-origin);
        if (distance>620.f*620.f) continue;
        auto* shot=shot_slot();
        if (!shot) continue;
        const float travel=std::sqrt(distance)/326.f;
        const auto move=attitude(player.yaw+side_slip,flight_path).front*speed;
        const auto ray=sky::unit(aim-origin+(fires_at_ship ? Vec{} : move*(travel*.86f)));
        *shot={origin,origin,ray*326.f,2.15f,true};
        emit(EffectKind::muzzle,origin,.095f,ray,.83f);
        unit.cooldown=unit.afloat() ? 1.8f : 1.48f;
    }
    if (!fleet.remaining()) {
        fleet.clear_time+=dt;
        if (fleet.clear_time>=1.5f) {
            phase=Phase::victory;
            phase_time=0;
        }
    }
}

bool Game::hit_fleet(Shot& shot,Vec end) {
    SurfaceUnit* target=nullptr;
    float first=2.f;
    for (auto& unit:fleet.units) {
        if (!unit.hull || (shot.hostile && !unit.friendly())) continue;
        const float t=unit.contact(shot.previous,end);
        if (t<first) { target=&unit; first=t; }
    }
    if (!target) return false;
    shot.life=0;
    if (target->friendly() && (!shot.hostile || phase!=Phase::flight)) return true;
    target->hull=std::max(0,target->hull-(shot.missile ? 3 : 1));
    const bool destroyed=!target->hull;
    target->flash=.23f;
    const auto point=shot.previous+(end-shot.previous)*first;
    emit(destroyed ? EffectKind::explosion : EffectKind::hit,point,destroyed ? 1.65f : .29f);
    if (destroyed) {
        if (target->afloat()) surface_impact(target->pos,{0,0,1},2.8f);
        if (target->friendly() && phase==Phase::flight) {
            phase=Phase::escort_lost;
            phase_time=0;
        }
    }
    if (!shot.hostile && !shot.allied) hit_feedback=destroyed ? .47f : .20f;
    return true;
}

} // namespace sky
