#include "scene.hpp"
#include "aircraft.hpp"
#include "clouds.hpp"
#include "effects.hpp"
#include "landscape.hpp"
#include "pilot.hpp"

namespace sky {

namespace {

void cloud_passage(Scene& scene,const CloudField& sky,const Game& game,float wind_time,float display_time,bool cockpit) {
    const auto& camera=scene.camera;
    if (camera.overhead || game.phase!=Phase::flight) return;
    const float eye=sky.density_at(camera.pos+camera.axes.front*5,wind_time);
    const float airframe=cockpit ? eye : .57f*sky.density_at(game.player.pos,wind_time);
    const int alpha=int(23*saturate((std::max(eye,airframe)-.075f)*1.35f));
    if (alpha<2) return;
    auto& d=scene.out;
    d.tint({0,0,d.width,d.height},rgb(0xddebf0),alpha);
    const int phase=int(display_time*30);
    for (int i=0;i<15;++i) {
        const auto seed=hash(i,479);
        const int run=(phase+int(seed&127))&127;
        const int axis=int((seed>>7)&127)-64;
        const int side=(seed>>15)&1 ? -1 : 1;
        const int x=d.width/2+side*(d.width/8+run*d.width/270);
        const int y=d.height/2+axis*(43+run)/170;
        const int length=1+run/28;
        d.tint({x,y,length,1},rgb(0xf6fff7),std::min(11,alpha*std::min(run,127-run)/50));
    }
}

} // namespace

CanopyBank CanopyBank::project(float bank,int width,int height) {
    if (std::abs(bank)<.018f) return {};
    const float cosine=std::cos(bank),sine=std::sin(bank);
    // Widen the source frustum only enough to cover the roll rotation.
    const float x=width*.5f,y=height*eye_line;
    float extent=1;
    for (float dx:{-x,x}) for (float dy:{-y,height-y}) {
        const float a=cosine*dx+sine*dy,b=-sine*dx+cosine*dy;
        extent=std::max({extent,std::abs(a)/x,b<0 ? -b/y : b/(height-y)});
    }
    return {cosine,sine,extent};
}

Camera flight_camera(const Game& game,int w,int h,float orbit,bool compact,View view) {
    view=active_view(game.phase,view);
    const float focal=std::min(w,h)*.94f;
    if (game.sortie!=Sortie::patrol && (game.phase==Phase::victory || game.phase==Phase::escort_lost)) {
        const bool fleet=game.sortie==Sortie::fleet;
        const auto& enemy=game.fleet.units[1];
        const auto& ship=game.fleet.units[game.phase==Phase::victory && enemy.wreck_age<3.9f ? 1 : 0];
        const auto axes=attitude((fleet ? ship.yaw : game.convoy.yaw)+.66f);
        const float height=(compact ? 33.f : 47.f)+(fleet ? 37.f : 0.f);
        const float distance=(compact ? 78.f : 103.f)+(fleet ? 104.f : 0.f);
        const Vec pos=(fleet ? ship.pos : game.convoy.pos)-axes.front*distance+Vec{0,height,0};
        const float anchor=h*(w<h ? .63f : .61f);
        return {pos,axes,focal,w*.5f,anchor-focal*height/distance};
    }
    const bool pilot=game.phase==Phase::ejection || game.phase==Phase::parachute || game.phase==Phase::pickup ||
                     (game.phase==Phase::rescued && !game.airlift) || game.phase==Phase::lost;
    const auto& aircraft=game.airlift ? game.rescue_plane : game.player;
    const float heading=pilot ? game.pilot_yaw : aircraft.yaw;
    const Vec subject=pilot ? game.pilot : aircraft.pos;
    const auto axes=attitude(heading+orbit);
    if (view==View::cockpit) {
        const auto pose=attitude(aircraft.yaw,aircraft.pitch,aircraft.bank);
        const Vec eye=subject+pose.apply({0,3.7f,2.5f});
        const auto bank=CanopyBank::project(aircraft.bank,w,h);
        const float lens=std::min(w,h)*.85f/bank.extent;
        return {eye,axes,lens,w*.5f,h*CanopyBank::eye_line+lens*std::tan(aircraft.pitch)};
    }
    if (view==View::overhead) {
        const Basis down{axes.right,axes.front,{0,-1,0}};
        const Vec pos{subject.x,std::max(480.f,subject.y+125.f),subject.z};
        return {pos,down,std::min(w,h)/(pilot ? 220.f : 320.f),w*.5f,h*.59f,true};
    }
    const bool composing=game.phase==Phase::flight;
    const float climb=composing ? aircraft.pitch : 0;
    const float height=pilot ? 15.f : (compact ? 19.f : 30.f)-climb*22.f;
    const float back=pilot ? 48.f : 76.f;
    auto pos=subject-axes.front*back+axes.right*(pilot ? 0 : 3.f)+Vec{0,height,0};
    if (composing) pos.y=std::max(pos.y,game.world.ground(pos)+6);
    const float anchor=composing ? (w<h ? .60f : .58f)-std::max(0.f,climb)*.085f :
                       compact ? .50f : w<h ? .65f : game.phase==Phase::title ? .52f : .61f;
    const float horizon=h*anchor-focal*(pos.y-subject.y)/back;
    return {pos,axes,focal,w*.5f,horizon};
}

void render_scene(Canvas& canvas,const Game& game,const CloudField& sky,float display_time,bool compact,
                  View view,int viewport_height) {
    view=active_view(game.phase,view);
    const float orbit=game.phase==Phase::title ? -.23f+std::sin(display_time*.48f)*.42f : 0;
    Scene scene{canvas,flight_camera(game,canvas.width,viewport_height ? viewport_height : canvas.height,orbit,compact,view)};
    const float wind_time=game.elapsed+display_time*.18f;
    const GroundLight shadow{sky.shadows,wind_time*4,wind_time*1.5f};
    scene.terrain(game.world,game.elapsed+display_time*.3f,shadow);
    const bool player_visible=game.phase==Phase::flight || game.phase==Phase::title ||
                              game.phase==Phase::victory || game.wreck_time>0;
    const bool rescue_visible=game.rescue_visible &&
        (game.phase==Phase::parachute || game.phase==Phase::pickup || game.airlift);
    const bool wingman_visible=game.wingman.hull && game.in_air();
    Craft player=game.player;
    if (game.phase==Phase::title) {
        player.bank=-.29f+std::sin(display_time*.78f)*.33f;
        player.pitch=.10f;
        player.aileron=std::cos(display_time*.78f)*.41f;
        player.elevator=.28f;
    }
    if (player_visible) aircraft_shadow(scene,game.world,player);
    if (wingman_visible) aircraft_shadow(scene,game.world,game.wingman.craft);
    if (rescue_visible) aircraft_shadow(scene,game.world,game.rescue_plane);
    for (const auto& enemy:game.enemies)
        if (enemy.health) aircraft_shadow(scene,game.world,enemy.craft);
    draw_woodland(scene,game.world,shadow);
    if (game.sortie==Sortie::escort)
        draw_convoy(scene,game.convoy,display_time,
                    shadow(game.convoy.pos.x,5.f,game.convoy.pos.z));
    if (game.sortie==Sortie::fleet) for (const auto& unit:game.fleet.units)
        draw_surface_unit(scene,unit,display_time,shadow(unit.pos.x,unit.aim().y,unit.pos.z));
    if (player_visible && view!=View::cockpit)
        draw_aircraft(scene,player,game.phase==Phase::flight || game.phase==Phase::title ||
            game.phase==Phase::victory ? (game.fleet.deck.phase==DeckPhase::parked ? Paint::deck : Paint::player)
                                       : Paint::wreck,game.exhaust_heat,0,display_time);
    if (rescue_visible)
        draw_aircraft(scene,game.rescue_plane,Paint::rescue,0,game.hatch,display_time);
    if (wingman_visible)
        draw_aircraft(scene,game.wingman.craft,Paint::wingman,0,0,display_time);
    for (const auto& enemy:game.enemies)
        if (enemy.health) draw_aircraft(scene,enemy.craft,Paint::enemy,0,0,display_time);
    render_flight_effects(scene,game,view!=View::cockpit);
    if (game.phase==Phase::ejection || game.phase==Phase::parachute ||
        (game.phase==Phase::pickup && game.phase_time<.52f) ||
        (game.phase==Phase::rescued && !game.airlift))
        draw_pilot(scene,game);
    sky.draw(scene,wind_time);
    cloud_passage(scene,sky,game,wind_time,display_time,view==View::cockpit);
}

} // namespace sky
