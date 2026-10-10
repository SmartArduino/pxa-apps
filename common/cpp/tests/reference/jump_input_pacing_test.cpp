// Exercise the maintained application's actual input/event handlers. A held
// finger produces many MOVE samples; none may submit a frame or change charge.
#include <pxa/app.hpp>
#include <pxa/raster.h>
#undef PXA_APPLICATION
#define PXA_APPLICATION(AppType)
#include "../../../../jump-jump-3d-cpp/main.cpp"
#include "jump_timing_reference.h"
#include <cassert>
#include <cmath>

static unsigned frames;
extern "C" std::int32_t pxa_submit(const std::uint8_t*, std::uint32_t) { return 0; }
extern "C" std::int32_t pxa_io(std::uint64_t handle, std::uint32_t operation,
                              std::uint8_t*, std::uint32_t length) {
    assert(handle == 77);
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
    jump_reference_reset(0x9e3779b9);
    std::uint64_t now = 1000000, previous = 0;
    auto tick = [&] {
        std::array<std::byte, 8> payload;
        pxa::wire::put64(payload.data(), now);
        assert(game.on_event(context, {4, 0x8001, 0, payload}).value());
        const auto steps = jump_reference_steps(&previous, now);
        for (unsigned i=0; i<steps; ++i) jump_reference_tick();
        assert(std::abs(game.g_game.charge-jump_reference_snapshot().charge)<.00001f);
    };
    tick();
    game.pointer({.timestamp_us=now, .phase=pxa::ui::pointer_phase_down});
    jump_reference_press();
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
    }
    const auto charged_frames=frames;
    game.pointer({.timestamp_us=now, .phase=pxa::ui::pointer_phase_up});
    jump_reference_release();
    const auto expected=jump_reference_snapshot();
    assert(game.g_game.state==J3_STATE_FLYING && frames==charged_frames+1);
    assert(std::abs(game.g_game.vx-expected.vx)<.00001f);
    assert(std::abs(game.g_game.vy-expected.vy)<.00001f);
    assert(std::abs(game.g_game.land_x-expected.land_x)<.00001f);
    // A stray held sample after release must not re-render or re-press.
    game.pointer({.phase=pxa::ui::pointer_phase_move});
    assert(frames==charged_frames+1 && game.g_game.state==J3_STATE_FLYING);
    std::puts("actual input handler: 2501 MOVE events produce zero extra frames; C charge/release parity passed");
}
