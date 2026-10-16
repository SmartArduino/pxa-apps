#include "pxa_control_capture.h"
#include <pxa/app.hpp>
#include <pxa/raster.h>
#undef PXA_APPLICATION
#define PXA_APPLICATION(AppType)
#include "../../../../jump-jump-3d/main.cpp"
#include <cassert>
#include <cmath>

static unsigned frames,back_requests;
extern "C" std::int32_t pxa_test_control(const std::uint8_t* data,std::uint32_t length){
    assert(length>=20);
    const auto* bytes=reinterpret_cast<const std::byte*>(data);
    if(pxa::wire::get16(bytes)==2&&pxa::wire::get16(bytes+2)==4)++back_requests;
    return 0;
}
extern "C" std::int32_t pxa_io(std::uint64_t handle,std::uint32_t operation,std::uint8_t* data,std::uint32_t length){
    assert(handle==77);
    if(operation==0x102){std::fill_n(data,length,0);return length;}
    assert(operation==0x101);++frames;return length;
}
template<class T>concept HasView=requires(T& app){app.view();};
static_assert(!HasView<JumpJump>);
static_assert(pxa::detail::AppInput<JumpJump>::mask==((1u<<9)|(1u<<6)));

int main(){
    using namespace pxa::input;
    pxa::Context context;context.transport().phase(pxa::Phase::event);
    auto prepare=[&](JumpJump& app){
        app.context=&context;app.renderer.emplace(context.transport(),77,PXA_RASTER_CAP_KNOWN_MASK);
        j3_render_configure(&app.render,320,240,app.renderer->capabilities());
        j3_render_adapt(&app.render,app.display);j3_game_reset(&app.g_game,0x9e3779b9);
    };
    auto pad=[&](JumpJump& app,std::uint32_t buttons,std::uint64_t at,bool connected=true,std::uint8_t id=0){
        app.on_controller(context,{.buttons=buttons,.timestamp_us=at,.controller=id,.connected=connected});
    };
    for(bool queued_late:{false,true}){
        JumpJump touch,controller;prepare(touch);prepare(controller);
        touch.pointer({.timestamp_us=1000000,.pointer_id=9,.phase=pointer_phase_down});
        pad(controller,controller_a,1000000);
        assert(controller.g_game.state==J3_STATE_CHARGING);
        const auto submitted=frames;
        for(unsigned i=1;i<=15;++i)pad(controller,controller_a,1000000+i*50000);
        assert(frames==submitted); // Periodic full state neither re-presses nor renders.
        if(queued_late){touch.advance_to(1900000);controller.advance_to(1900000);}
        touch.pointer({.timestamp_us=1800000,.pointer_id=9,.phase=pointer_phase_up});
        pad(controller,0,1800000);
        assert(controller.g_game.state==J3_STATE_FLYING&&touch.g_game.state==J3_STATE_FLYING);
        assert(std::fabs(controller.g_game.vx-touch.g_game.vx)<1e-6f);
        assert(std::fabs(controller.g_game.launch_vy-touch.g_game.launch_vy)<1e-6f);
        assert(std::fabs(controller.g_game.flight_elapsed-touch.g_game.flight_elapsed)<1e-6f);
        const auto after=frames;pad(controller,0,1850000);assert(frames==after);
    }
    JumpJump game;prepare(game);
    pad(game,controller_a,2000000);pad(game,controller_a|controller_b,2100000);
    assert(game.g_game.state==J3_STATE_READY&&game.charge_pointer==256&&back_requests==1);
    pad(game,controller_a|controller_b,2150000);assert(back_requests==1);
    pad(game,0,2200000);assert(game.g_game.state==J3_STATE_READY);
    pad(game,controller_a,2300000);game.initializing=true;
    pad(game,0,2400000,false);assert(game.g_game.state==J3_STATE_READY);
    game.initializing=false;
    pad(game,controller_a,2500000);assert(game.g_game.state==J3_STATE_READY);
    pad(game,0,2600000);pad(game,controller_a,2700000);
    assert(game.g_game.state==J3_STATE_CHARGING);
    // A real finger cannot release a controller charge, or vice versa.
    game.pointer({.timestamp_us=2750000,.pointer_id=3,.phase=pointer_phase_down});
    game.pointer({.timestamp_us=2800000,.pointer_id=3,.phase=pointer_phase_up});
    assert(game.g_game.state==J3_STATE_CHARGING);
    pad(game,0,2900000);assert(game.g_game.state==J3_STATE_FLYING);
    j3_game_reset(&game.g_game,1);
    game.pointer({.timestamp_us=3000000,.pointer_id=3,.phase=pointer_phase_down});
    pad(game,controller_a,3100000);pad(game,0,3200000);
    assert(game.charge_pointer==3&&game.g_game.state==J3_STATE_CHARGING);
    game.pointer({.timestamp_us=3300000,.pointer_id=3,.phase=pointer_phase_cancel});
    assert(game.g_game.state==J3_STATE_READY);
    pad(game,controller_a,3400000);game.on_background(context);
    assert(game.g_game.state==J3_STATE_READY&&game.charge_pointer==256);
    pad(game,controller_a,3500000);assert(game.g_game.state==J3_STATE_READY);
    pad(game,0,3600000);pad(game,controller_a,3700000, true,1);
    assert(game.g_game.state==J3_STATE_READY); // Other controller cannot own the hold.
    pad(game,controller_a,3800000);assert(game.g_game.state==J3_STATE_CHARGING);
    pad(game,controller_a|controller_b,3900000);pad(game,0,4000000);
    game.g_game.state=J3_STATE_OVER;game.g_game.best=123;
    pad(game,controller_start,4100000);
    assert(game.g_game.state==J3_STATE_READY&&game.g_game.best==123);
    pad(game,0,4200000);game.g_game.state=J3_STATE_OVER;
    pad(game,controller_a,4300000);
    assert(game.g_game.state==J3_STATE_READY&&game.g_game.best==123);
    pad(game,0,4400000);const auto before=back_requests;
    pad(game,controller_b,4500000);assert(back_requests==before+1);
    pad(game,controller_b,4600000);assert(back_requests==before+1);
    std::puts("Jump controller: timestamp parity, late release, no repeat frames, ownership, back/cancel/reconnect/resume/retry OK");
}
