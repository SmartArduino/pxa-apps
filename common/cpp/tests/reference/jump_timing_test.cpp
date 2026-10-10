#include "jump_timing_reference.h"
#include "jump3d_clock.hpp"
#include "jump3d_game.hpp"
#include <pxa/clock.hpp>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

static void compare(const jump::j3_game_t& game) {
    const auto original = jump_reference_snapshot();
    const std::array actual{game.charge,game.body_scale,game.px,game.py,game.pz,
        game.vx,game.vy,game.vz,game.land_x,game.land_z};
    const std::array expected{original.charge,original.body_scale,original.px,
        original.py,original.pz,original.vx,original.vy,original.vz,
        original.land_x,original.land_z};
    for (unsigned i=0;i<actual.size();++i) assert(std::abs(actual[i]-expected[i])<.00001f);
    assert(game.state==original.state && game.score==original.score &&
        game.land_result==original.land_result);
}

static void replay(std::uint32_t interval, unsigned hold_ticks) {
    jump::GameClock clock;
    std::uint64_t previous=0,now=1000000;
    jump::j3_game_t game{};
    jump::j3_game_reset(&game,0x51ed2701);
    jump_reference_reset(0x51ed2701);
    auto tick=[&](std::uint64_t timestamp) {
        const auto count=clock.advance(timestamp);
        assert(count==jump_reference_steps(&previous,timestamp));
        for(unsigned i=0;i<count;++i) {
            jump::j3_game_tick(&game,.02f);jump_reference_tick();compare(game);
        }
    };
    tick(now);
    jump::j3_game_press(&game);jump_reference_press();
    for(unsigned i=0;i<hold_ticks;++i) tick(now+=interval);
    const auto charge=game.charge;
    jump::j3_game_release(&game);jump_reference_release();compare(game);
    for(unsigned i=0;i<300;++i) tick(now+=interval);
    if(hold_ticks==25 && interval==32000) assert(std::abs(charge-1.f)<.00001f);
    // Background/foreground resets must discard time spent paused.
    clock.reset();previous=0;tick(now+=60000000);tick(now+=interval);
}

int main() {
    // A 31.25 Hz callback cadence exposed the migration difference: the
    // original rounds each 32 ms callback to 40 ms; the accumulator used by
    // the first C++ port advanced 800 ms instead of 1000 ms over 25 callbacks.
    pxa::FixedStepper migrated_clock(20000,2);
    jump::GameClock original_clock;
    std::uint64_t time=1000000;
    (void)migrated_clock.advance(time);(void)original_clock.advance(time);
    unsigned migrated_steps=0,original_steps=0;
    for(unsigned i=0;i<25;++i) {
        time+=32000;migrated_steps+=migrated_clock.advance(time).count;
        original_steps+=original_clock.advance(time);
    }
    assert(migrated_steps==40 && original_steps==50);
    // Exact C helper parity at half-step boundaries, repeated/reversed/zero
    // timestamps, a coalesced event and recovery after foregrounding.
    jump::GameClock clock;
    std::uint64_t previous=0;
    for(auto now:std::array<std::uint64_t,18>{0,1000000,1000000,999999,
        1000001,1007001,1017000,1027000,1047000,1076999,1106999,
        1146999,1226999,1726999,1727000,0,1726998,1756998})
        assert(clock.advance(now)==jump_reference_steps(&previous,now));
    clock.reset();previous=0;
    assert(clock.advance(90000000)==jump_reference_steps(&previous,90000000));
    for(auto interval:std::array{16000u,20000u,24000u,29999u,30000u,
        31000u,32000u,36000u,40000u,48000u,80000u,100000u})
        for(auto hold:std::array{1u,5u,15u,25u,40u,80u}) replay(interval,hold);
    std::puts("original/C++ clock, charge, release velocity, landing and pause parity passed (72 replays)");
}
