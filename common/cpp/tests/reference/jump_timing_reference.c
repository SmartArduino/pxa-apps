#include "jump_timing_reference.h"
#include "jump3d_game.h"
#include <pxa_clock.h>
static j3_game_t game;
uint8_t jump_reference_steps(uint64_t *previous, uint64_t now) {
    return pxa_clock_tick_steps(previous, now, 20, 2);
}
void jump_reference_reset(uint32_t seed) { j3_game_reset(&game, seed); }
void jump_reference_press(void) { j3_game_press(&game); }
void jump_reference_release(void) { j3_game_release(&game); }
void jump_reference_tick(void) { j3_game_tick(&game, .02f); }
struct jump_timing_snapshot jump_reference_snapshot(void) {
    return (struct jump_timing_snapshot){
        game.charge, game.body_scale, game.px, game.py, game.pz,
        game.vx, game.vy, game.vz, game.land_x, game.land_z,
        game.score, game.state, game.land_result};
}
