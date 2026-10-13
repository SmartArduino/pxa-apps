/* Gameplay-core test for Jump Jump 3D. Everything here runs on the Guest
 * sources without a Host: block generation, the flight solver, the centre-hit
 * scoring ladder, the stay bonuses and both failure paths. */
#include <assert.h>
#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "jump3d_game.hpp"
using namespace jump;

static int near(float left, float right, float tolerance) {
    const float delta = left - right;
    return (delta < 0.0F ? -delta : delta) <= tolerance;
}

/* Exact charge time that lands `distance` world units away, mirroring the
 * release integration in j3_game_release. */
static float charge_for_distance(float distance) {
    // Reference ballistic equation, including the compressed launch height.
    // Binary search is test/preview automation only, outside the game path.
    float low=0, high=3;
    for(int i=0;i<32;++i){
        const float charge=(low+high)*0.5F;
        const float vy=13.5F+1.5F*charge;
        const float rise=0.55F*(1-std::max(0.5F,1-0.32F*charge));
        const float duration=(vy+__builtin_sqrtf(vy*vy-144*rise))/72;
        const float range=std::min(15.0F,7*charge)*duration;
        if(range<distance)low=charge;else high=charge;
    }
    return (low+high)*0.5F;
}

/* Presses, charges exactly `charge` seconds and releases. The charge uses a
 * fine tick so the release lands on the requested distance: the game itself
 * uses the real elapsed DOWN/UP time, independently of clock cadence. */
static void jump_with_charge(j3_game_t *game, float charge) {
    j3_game_press(game);
    assert(game->state == J3_STATE_CHARGING);
    while (game->charge < charge) j3_game_tick(game, 0.002F);
    j3_game_release(game);
    assert(game->state == J3_STATE_FLYING);
}

/* Jumps onto the next block centre plus `error` world units. */
static void jump_centred(j3_game_t *game, float error) {
    const j3_block_t *next = &game->blocks[1];
    const float dx = next->x - game->px;
    const float dz = next->z - game->pz;
    const float distance = sqrtf(dx * dx + dz * dz) + error;
    jump_with_charge(game, charge_for_distance(distance));
}

/* Runs the flight until the character is standing again (or has failed). */
static void settle(j3_game_t *game) {
    int step;
    for (step = 0; step < 400; ++step) {
        j3_game_tick(game, 0.02F);
        if (game->state == J3_STATE_READY || game->state == J3_STATE_TIP ||
            game->state == J3_STATE_FALL || game->state == J3_STATE_OVER)
            return;
    }
    assert(!"the flight never settled");
}

static void discharge(j3_game_t *game) {
    int step;
    for (step = 0; step < 400; ++step) {
        j3_game_tick(game, 0.02F);
        if (game->state == J3_STATE_OVER) return;
    }
    assert(!"the failure animation never finished");
}

static void test_reset_is_deterministic(void) {
    j3_game_t first{};
    j3_game_t second{};
    int index;
    j3_game_reset(&first, 0x1234u);
    j3_game_reset(&second, 0x1234u);
    assert(first.block_count == 2u);
    assert(first.block_count == second.block_count);
    for (index = 0; index < 3; ++index) {
        assert(near(first.blocks[index].x, second.blocks[index].x, 0.0001F));
        assert(near(first.blocks[index].z, second.blocks[index].z, 0.0001F));
        assert(near(first.blocks[index].radius, second.blocks[index].radius,
                    0.0001F));
        assert(first.blocks[index].kind == second.blocks[index].kind);
    }
    /* The opening jump is always straight ahead and reachable. */
    assert(near(first.blocks[0].x, 0.0F, 0.0001F));
    assert(near(first.blocks[0].z, 0.0F, 0.0001F));
    assert(near(first.blocks[1].x, 2.0F, 0.0001F));
    assert(near(first.blocks[1].z, 0.0F, 0.0001F));
    printf("ok: reset is deterministic and starts level\n");
}

static void test_block_helpers(void) {
    j3_game_t game{};
    j3_block_t block;
    j3_game_reset(&game, 7u);

    block = game.blocks[0];
    block.kind = J3_KIND_BOX;
    block.radius = 0.5F;
    block.x = 0.0F;
    block.z = 0.0F;
    block.squash = 1.0F;
    block.appear = 1.0F;
    assert(j3_block_contains(&block, 0.0F, 0.0F));
    assert(j3_block_contains(&block, 0.49F, -0.49F));
    assert(!j3_block_contains(&block, 0.51F, 0.0F));
    assert(near(j3_block_top(&block), J3_BLOCK_HEIGHT, 0.0001F));

    block.kind = J3_KIND_ROUND;
    assert(j3_block_contains(&block, 0.3F, 0.3F));
    assert(!j3_block_contains(&block, 0.4F, 0.4F));

    block.squash = 0.5F;
    assert(near(j3_block_top(&block), J3_BLOCK_HEIGHT * 0.5F, 0.0001F));
    block.appear = 0.0F;
    assert(j3_block_offset(&block) > 1.0F);

    block.kind = J3_KIND_WELL;
    assert(j3_block_bonus(&block) == 5u);
    block.kind = J3_KIND_STORE;
    assert(j3_block_bonus(&block) == 15u);
    block.kind = J3_KIND_MUSIC;
    assert(j3_block_bonus(&block) == 30u);
    block.kind = J3_KIND_RUBIK;
    assert(j3_block_bonus(&block) == 10u);
    block.kind = J3_KIND_BOX;
    assert(j3_block_bonus(&block) == 0u);
    printf("ok: block helpers\n");
}

static void test_centre_hits_double_the_score(void) {
    j3_game_t game{};
    j3_game_reset(&game, 0x51ED2701u);
    assert(game.state == J3_STATE_READY);

    /* The first centre hit awards 2, then 4, 6 and 8 while the chain holds.
     * The quick-jump window is cleared explicitly so only the chain matters. */
    game.quick_window = 0.0F;
    jump_centred(&game, 0.0F);
    assert(game.land_result == J3_LAND_PERFECT);
    settle(&game);
    assert(game.state == J3_STATE_READY);
    assert(game.score == 2u);
    assert(game.combo == 1u);
    assert(game.jump_count == 1u);
    assert(game.popup_points == 2u);

    game.quick_window = 0.0F;
    jump_centred(&game, 0.0F);
    assert(game.land_result == J3_LAND_PERFECT);
    settle(&game);
    assert(game.score == 6u);
    assert(game.combo == 2u);

    game.quick_window = 0.0F;
    jump_centred(&game, 0.0F);
    settle(&game);
    assert(game.score == 12u);
    assert(game.combo == 3u);

    game.quick_window = 0.0F;
    jump_centred(&game, 0.0F);
    settle(&game);
    assert(game.score == 20u);
    assert(game.combo == 4u);

    /* Every centre hit also marks the next block with the aim dot. */
    assert(game.blocks[1].center_dot == 1u);
    printf("ok: centre hits score 2, 4, 6, 8\n");
}

static void test_centre_hit_chain_is_capped(void) {
    j3_game_t game{};
    int index;
    j3_game_reset(&game, 0xBEEF01u);
    for (index = 0; index < 24; ++index) {
        game.quick_window = 0.0F;
        jump_centred(&game, 0.0F);
        settle(&game);
        assert(game.state == J3_STATE_READY);
    }
    assert(game.combo == 24u);
    assert(game.popup_points == 32u); /* 2 * combo, capped at 32 */
    printf("ok: the centre-hit ladder caps at 32\n");
}

static void test_off_centre_lands_reset_the_chain(void) {
    j3_game_t game{};
    j3_game_reset(&game, 0xA11CEu);
    game.quick_window = 0.0F;
    jump_centred(&game, 0.0F);
    settle(&game);
    assert(game.score == 2u);
    assert(game.combo == 1u);

    /* 0.25 units off the centre is still on the block but far from perfect. */
    game.quick_window=0;
    jump_centred(&game, 0.25F);
    assert(game.land_result == J3_LAND_OK);
    settle(&game);
    assert(game.state == J3_STATE_READY);
    assert(game.score == 3u);
    assert(game.combo == 0u);
    assert(game.popup_points == 1u);
    printf("ok: off-centre landings reset the chain\n");
}

static void test_quick_jump_doubles_a_centre_hit(void) {
    j3_game_t game{};
    j3_game_reset(&game, 0xFA57u);
    game.quick_window = 0.8F;
    jump_centred(&game, 0.0F);
    assert(game.quick_jump);
    settle(&game);
    assert(game.score == 4u && game.combo==1);
    // Quick +4 is part of the multiplier ladder: the following perfect is +6.
    game.quick_window=0;
    jump_centred(&game,0);settle(&game);
    assert(game.score==10 && game.popup_points==6);
    printf("ok: quick is captured on release; the perfect ladder continues 4, 6\n");
}

static void test_short_jump_returns_to_the_near_block(void) {
    j3_game_t game{};
    j3_game_reset(&game, 0x5A07u);
    /* Barely any charge: the character lands back on the block it left. */
    jump_with_charge(&game, 0.05F);
    assert(game.land_result == J3_LAND_BACK);
    settle(&game);
    assert(game.state == J3_STATE_READY);
    assert(game.score == 0u && game.jump_count == 0u);
    assert(game.combo == 0u);
    jump_centred(&game, 0);settle(&game);
    assert(game.score==2 && game.state==J3_STATE_READY);
    printf("ok: a short jump returns safely and can be followed by another jump\n");
}

static void test_overshoot_misses_the_far_block(void) {
    j3_game_t game{};
    j3_game_reset(&game, 0x0091u);
    jump_with_charge(&game, 1.6F);
    assert(game.land_result == J3_LAND_MISS);
    settle(&game);
    assert(game.state == J3_STATE_FALL || game.state == J3_STATE_TIP);
    discharge(&game);
    assert(game.state == J3_STATE_OVER);
    printf("ok: an overshoot misses and ends the run\n");
}

static void test_stay_bonus_and_cancel(void) {
    j3_game_t game{};
    j3_game_reset(&game, 0x60D5u);

    /* Land on a music box and stay: 30 points after two seconds. */
    game.blocks[1].kind = J3_KIND_MUSIC;
    jump_centred(&game, 0.0F);
    settle(&game);
    assert(game.state == J3_STATE_READY);
    assert(game.score == 2u);
    assert(game.blocks[0].bonus_timer > 0.0F);
    {
        int step;
        for (step = 0; step < 120; ++step) j3_game_tick(&game, 0.02F);
    }
    assert(game.score == 32u);
    assert(game.bonus_popup_timer > 0.0F);
    assert(game.blocks[0].bonus_done == 1u);

    /* The collected bonus never triggers twice. */
    game.blocks[0].bonus_timer = 0.0F;
    {
        int step;
        for (step = 0; step < 60; ++step) j3_game_tick(&game, 0.02F);
    }
    assert(game.score == 32u);

    /* Landing on a well and jumping away before two seconds cancels it. */
    game.blocks[1].kind = J3_KIND_WELL;
    game.quick_window = 0.0F;
    jump_centred(&game, 0.0F);
    settle(&game);
    assert(game.state == J3_STATE_READY);
    assert(game.blocks[0].kind == J3_KIND_WELL);
    assert(game.blocks[0].bonus_timer > 0.0F);
    assert(game.score == 36u); /* 32 + the second chain step */

    game.quick_window = 0.0F;
    jump_centred(&game, 0.0F);
    assert(game.blocks[0].kind == J3_KIND_WELL);
    assert(game.blocks[0].bonus_timer == 0.0F);
    settle(&game);
    assert(game.score == 42u); /* +6: the 5 point well bonus was cancelled */
    printf("ok: stay bonuses award once and cancel on the next jump\n");
}

static void test_all_platforms_are_playable(void) {
    for (uint8_t kind = J3_KIND_BOX; kind < J3_KIND_COUNT; ++kind) {
        j3_game_t game{};
        j3_game_reset(&game, 123);
        auto& target = game.blocks[1];
        target.kind = kind;
        target.radius = 0.35F;
        assert(j3_block_contains(&target, target.x, target.z));
        assert(!j3_block_contains(&target, target.x+0.36F, target.z));
        const bool corner_inside = j3_block_contains(&target,target.x+0.30F,target.z+0.30F);
        assert(corner_inside != j3_block_round(&target));
        if (kind >= J3_KIND_CLOCK) assert(j3_block_bonus(&target) == 0);
        jump_centred(&game, 0);
        assert(game.land_result == J3_LAND_PERFECT);
        settle(&game);
        assert(game.state == J3_STATE_READY);
        assert(game.blocks[0].kind == kind);
        assert(near(game.py,j3_block_top(&game.blocks[0]),0.0001F));
        const uint32_t score = game.score;
        j3_game_tick(&game,3);
        assert(game.score == score + j3_block_bonus(&game.blocks[0]));
    }
    printf("ok: all platform footprints, landings and stay rewards\n");
}

static void test_platform_generation_covers_catalog(void) {
    bool seen[J3_KIND_COUNT]{};
    for (unsigned seed : {123u,456u,789u,101112u}) {
        j3_game_t game{};
        j3_game_reset(&game,seed);
        for (unsigned i=0;i<200;++i) {
            const auto& next=game.blocks[1];
            const auto& future=game.blocks[2];
            assert(future.kind>0 && future.kind<J3_KIND_COUNT);
            seen[future.kind]=true;
            const float gap=std::abs(future.x-next.x)+std::abs(future.z-next.z)
                -next.radius-future.radius;
            assert(gap>=0.0999F && gap<=game.max_distance+0.0001F);
            game.quick_window=0;
            jump_centred(&game,0);
            settle(&game);
            assert(game.state==J3_STATE_READY);
        }
    }
    for (unsigned i=J3_KIND_BOX;i<J3_KIND_COUNT;++i) assert(seen[i]);
    printf("ok: deterministic generation reaches every platform and preserves gaps\n");
}

static void test_difficulty_ramps_up(void) {
    j3_game_t game{};
    float start_min;
    float start_distance;
    int index;
    j3_game_reset(&game, 0xD1FFu);
    start_min = game.min_radius_scale;
    start_distance = game.max_distance;
    for (index = 0; index < 12; ++index) {
        game.quick_window = 0.0F;
        jump_centred(&game, 0.0F);
        settle(&game);
        assert(game.state == J3_STATE_READY);
    }
    assert(game.min_radius_scale < start_min);
    assert(game.min_radius_scale >= 0.25F);
    assert(game.max_radius_scale <= 1.0F);
    assert(game.max_distance > start_distance);
    assert(game.max_distance <= 2.2F);

    /* Blocks stay reachable: the longest gap needs less than a full charge. */
    {
        const j3_block_t *next = &game.blocks[1];
        const j3_game_t *snapshot = &game;
        const float dx = next->x - snapshot->px;
        const float dz = next->z - snapshot->pz;
        const float distance = sqrtf(dx * dx + dz * dz);
        assert(charge_for_distance(distance) < 1.2F);
    }
    printf("ok: difficulty ramps up and stays reachable\n");
}

int main(void) {
    test_reset_is_deterministic();
    test_block_helpers();
    test_centre_hits_double_the_score();
    test_centre_hit_chain_is_capped();
    test_off_centre_lands_reset_the_chain();
    test_quick_jump_doubles_a_centre_hit();
    test_short_jump_returns_to_the_near_block();
    test_overshoot_misses_the_far_block();
    test_stay_bonus_and_cancel();
    test_all_platforms_are_playable();
    test_platform_generation_covers_catalog();
    test_difficulty_ramps_up();
    printf("jump-jump-3d gameplay tests passed\n");
    return 0;
}
