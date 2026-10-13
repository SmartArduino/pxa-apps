#include "pxa_control_capture.h"
// Exercise the maintained application's actual input/event handlers. A held
// finger produces many MOVE samples; none may submit a frame or change charge.
#include <pxa/app.hpp>
#include <pxa/raster.h>
#undef PXA_APPLICATION
#define PXA_APPLICATION(AppType)
#include "../../../../jump-jump-3d/main.cpp"

#include <cassert>
#include <cmath>

static unsigned frames;
extern "C" std::int32_t pxa_test_control(const std::uint8_t*, std::uint32_t) { return 0; }
extern "C" std::int32_t pxa_io(std::uint64_t handle, std::uint32_t operation,
                              std::uint8_t* data, std::uint32_t length) {
    assert(handle == 77);
    if(operation==0x102){
        assert(length==104);auto* bytes=reinterpret_cast<std::byte*>(data);
        std::fill_n(bytes,length,std::byte{});
        pxa::wire::put64(bytes+24,uint64_t(frames)*15000);
        pxa::wire::put64(bytes+88,frames);return length;
    }
    assert(operation == 0x101); // The real application's encoded draw list.
    ++frames;
    return length;
}

int main() {
    pxa::Context context;
    context.transport().phase(pxa::Phase::event);
    JumpJump game;
    game.context = &context;
    game.renderer.emplace(context.transport(), 77, PXA_RASTER_CAP_KNOWN_MASK);
    j3_render_configure(&game.render, 296, 240, game.renderer->capabilities());
    j3_render_adapt(&game.render, game.display);
    j3_game_reset(&game.g_game, 0x9e3779b9);

    std::uint64_t now = 1000000;
    auto tick = [&] {
        std::array<std::byte, 8> payload;
        pxa::wire::put64(payload.data(), now);
        assert(game.on_event(context, {4, 0x8001, 0, payload}).value());

    };
    tick();
    game.pointer({.timestamp_us=now, .phase=pxa::ui::pointer_phase_down});

    assert(game.g_game.state == J3_STATE_CHARGING);
    assert(frames == 2); // First clock frame and immediate press feedback.
    for (unsigned step=0; step<25; ++step) {
        const auto before=frames;
        const auto charge=game.g_game.charge;
        for (unsigned move=0; move<100; ++move)
            game.pointer({.timestamp_us=now+move*200, .x=int(move), .y=120,
                          .phase=pxa::ui::pointer_phase_move});
        assert(frames==before && game.g_game.charge==charge);
        now+=32000;
        tick();
        assert(frames==before+1);
        assert(std::abs(game.g_game.charge-float(step+1)*.032f)<.00001f);
    }
    const auto charged_frames=frames;
    game.pointer({.timestamp_us=now, .phase=pxa::ui::pointer_phase_up});
    assert(game.g_game.state==J3_STATE_FLYING && frames==charged_frames+1);
    assert(std::abs(game.g_game.vx-5.6f)<.00001f);
    assert(std::abs(game.g_game.launch_vy-14.7f)<.00001f);
    // A stray held sample after release must not re-render or re-press.
    game.pointer({.phase=pxa::ui::pointer_phase_move});
    assert(frames==charged_frames+1 && game.g_game.state==J3_STATE_FLYING);
    // Only the initiating pointer owns the hold. Extra physical contacts on
    // Mosaico's two-contact driver must neither reset nor release the charge.
    j3_game_reset(&game.g_game, 0x9e3779b9);
    game.pointer({.pointer_id=7, .phase=pxa::ui::pointer_phase_down});
    j3_game_tick(&game.g_game, .2f);
    const auto held_charge=game.g_game.charge;
    const auto held_frames=frames;
    game.pointer({.pointer_id=8, .phase=pxa::ui::pointer_phase_down});
    game.pointer({.pointer_id=8, .phase=pxa::ui::pointer_phase_up});
    game.pointer({.pointer_id=8, .phase=pxa::ui::pointer_phase_cancel});
    game.pointer({.pointer_id=7, .phase=pxa::ui::pointer_phase_down});
    assert(game.g_game.state==J3_STATE_CHARGING && game.g_game.charge==held_charge);
    assert(frames==held_frames);
    game.pointer({.pointer_id=7, .phase=pxa::ui::pointer_phase_up});
    assert(game.g_game.state==J3_STATE_FLYING && frames==held_frames+1);
    game.pointer({.pointer_id=7, .phase=pxa::ui::pointer_phase_up});
    assert(frames==held_frames+1);
    j3_game_reset(&game.g_game, 0x9e3779b9);
    game.pointer({.pointer_id=255, .phase=pxa::ui::pointer_phase_down});
    game.pointer({.pointer_id=255, .phase=pxa::ui::pointer_phase_cancel});
    assert(game.g_game.state==J3_STATE_READY && game.charge_pointer==256);
    assert(game.g_game.body_scale==1 && game.g_game.blocks[0].squash==1);
    game.pointer({.pointer_id=3, .phase=pxa::ui::pointer_phase_down});
    assert(game.g_game.state==J3_STATE_CHARGING && game.charge_pointer==3);
    game.on_background(context);
    assert(game.g_game.state==J3_STATE_READY && game.charge_pointer==256);
    assert(game.g_game.body_scale==1 && game.g_game.blocks[0].squash==1);
    game.pointer({.pointer_id=3, .phase=pxa::ui::pointer_phase_up});
    assert(game.g_game.state==J3_STATE_READY);
    j3_game_reset(&game.g_game,1);game.stepper.reset();
    now=2000000;tick();
    game.pointer({.timestamp_us=now,.phase=pxa::ui::pointer_phase_down});
    now+=800000;tick();
    game.pointer({.timestamp_us=now-100000,.phase=pxa::ui::pointer_phase_up});
    assert(std::abs(game.g_game.launch_vy-14.55f)<.00001f);
    assert(std::abs(game.g_game.flight_elapsed-.1f)<.00001f);
    const auto audio_time=game.g_audio.tick_us;now-=50000;tick();
    assert(game.g_audio.tick_us==audio_time);
    // Retain the submitted result instead of repeatedly rasterising its full
    // screen alpha overlay. Audio/clock work continues, then restart redraws.
    game.g_game.state=J3_STATE_OVER;game.result_drawn=false;game.quality_ticks=47;
    now+=100000;tick();const auto result_frames=frames;
    assert(game.result_drawn);
    for(unsigned i=0;i<2000;++i){now+=20000;tick();}
    assert(frames==result_frames&&game.quality_ticks==0);
    game.draw_frame(); // Display/lifecycle invalidation is still honored.
    assert(frames==result_frames+1);
    game.pointer({.timestamp_us=now,.pointer_id=4,.phase=pxa::ui::pointer_phase_down});
    assert(game.g_game.state==J3_STATE_READY&&!game.result_drawn);
    assert(frames==result_frames+2);
    std::puts("result screen: 2000 clock ticks retain one frame, invalidation/restart redraw");
    // Inspect the actual audio requests, including high-combo and special
    // block lifecycles. No Host playback is required to catch a wrong clip.
    j3_game_reset(&game.g_game,1);game.g_last_jumps=0;game.g_last_state=J3_STATE_READY;
    for(unsigned combo:{1,8,9,20}){
        ++game.g_game.jump_count;game.g_game.combo=combo;game.play_state_sounds();
        assert(game.g_audio.voices[J3_CHANNEL_COMBO].clip==J3_CLIP_COMBO1+std::min(combo-1,7u));
    }
    for(unsigned kind:{J3_KIND_MUSIC,J3_KIND_STORE,J3_KIND_WELL,J3_KIND_RUBIK}){
        game.g_game.blocks[0].kind=kind;game.g_game.blocks[0].bonus_done=1;
        game.g_game.bonus_kind=kind;game.g_game.bonus_popup_timer=1;
        game.g_bonus_played=0;game.play_state_sounds();
        const auto expected=kind==J3_KIND_MUSIC?J3_CLIP_SING:kind==J3_KIND_STORE?J3_CLIP_STORE:
                            kind==J3_KIND_WELL?J3_CLIP_WATER:J3_CLIP_POP;
        assert(game.g_audio.voices[J3_CHANNEL_BONUS].clip==expected);
        game.g_game.bonus_popup_timer=0;game.g_bonus_repeat_timer=-1;
        game.g_audio.voices[J3_CHANNEL_BONUS].requested=0;
        game.play_state_sounds();
        assert(bool(game.g_audio.voices[J3_CHANNEL_BONUS].requested)==(kind==J3_KIND_MUSIC));
        j3_game_press(&game.g_game);game.play_state_sounds();
        assert(!game.g_audio.voices[J3_CHANNEL_BONUS].requested);
        j3_game_cancel(&game.g_game);game.g_last_state=J3_STATE_READY;
    }
    // Releasing a long music-box asset must preserve still-active effects.
    j3_audio_t audio;
    pxa::AssetDescriptor descriptor;descriptor.kind=pxa::AssetKind::audio;
    for(unsigned i=0;i<3;++i)
        audio.bank.sounds[i].emplace(context.transport(),100+i,descriptor);
    audio.bank.ids[0]=J3_CLIP_SING;
    audio.bank.ids[1]=J3_CLIP_SCALE_INTRO;
    audio.bank.ids[2]=J3_CLIP_COMBO1;
    j3_audio_play(&audio,J3_CHANNEL_CHARGE,J3_CLIP_SCALE_INTRO,J3_GAIN_FULL,0);
    j3_audio_play(&audio,J3_CHANNEL_BONUS,J3_CLIP_SING,J3_GAIN_FULL,0);
    j3_audio_stop(&audio,J3_CHANNEL_BONUS);
    assert(!audio.bank.sounds[0]);
    assert(audio.bank.sounds[1]);
    assert(!audio.bank.sounds[2]);
    std::puts("audio cache: inactive long clip released, active charge preserved");
    std::puts("actual input handler: 2501 MOVE events produce zero extra frames; elapsed-time charge/release, ownership and cancellation passed");
}
