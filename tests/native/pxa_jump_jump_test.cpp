#include "pxa_control_capture.h"
#include "arcade/modules/jump_jump.cpp"

#include <assert.h>

typedef struct {
    platform_t platforms[2];
    uint32_t random_state;
    uint32_t score;
    uint32_t best_score;
    int32_t player_x_q;
    int32_t player_y_q;
    int32_t camera_x;
    int32_t camera_target_x;
    uint8_t perfect_chain;
    uint8_t feedback_points;
    uint8_t feedback_ticks;
    uint8_t roll_phase;
    uint8_t started;
    uint8_t pointer_down;
    uint8_t buffered_press;
} game_snapshot_t;

int32_t pxa_test_control(const uint8_t* data, uint32_t length) {
    assert(data != NULL);
    assert(length >= PXA_HEADER_BYTES && length <= 4096);
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t* data,
               uint32_t length) {
    (void)handle;
    (void)operation;
    (void)data;
    (void)length;
    return PXA_STATUS_UNSUPPORTED;
}

static game_snapshot_t save_game(void) {
    game_snapshot_t snapshot = {
        .platforms = {platforms[0], platforms[1]},
        .random_state = random_state,
        .score = score,
        .best_score = best_score,
        .player_x_q = player_x_q,
        .player_y_q = player_y_q,
        .camera_x = camera_x,
        .camera_target_x = camera_target_x,
        .perfect_chain = perfect_chain,
        .feedback_points = feedback_points,
        .feedback_ticks = feedback_ticks,
        .roll_phase = roll_phase,
        .started = started,
        .pointer_down = pointer_down,
        .buffered_press = buffered_press,
    };
    return snapshot;
}

static void restore_game(const game_snapshot_t* snapshot) {
    platforms[0] = snapshot->platforms[0];
    platforms[1] = snapshot->platforms[1];
    random_state = snapshot->random_state;
    score = snapshot->score;
    best_score = snapshot->best_score;
    player_x_q = snapshot->player_x_q;
    player_y_q = snapshot->player_y_q;
    camera_x = snapshot->camera_x;
    camera_target_x = snapshot->camera_target_x;
    perfect_chain = snapshot->perfect_chain;
    feedback_points = snapshot->feedback_points;
    feedback_ticks = snapshot->feedback_ticks;
    roll_phase = snapshot->roll_phase;
    started = snapshot->started;
    pointer_down = snapshot->pointer_down;
    buffered_press = snapshot->buffered_press;
    velocity_x_q = 0;
    velocity_y_q = 0;
    charge = 0;
    state = STATE_READY;
}

static uint8_t try_charge(const game_snapshot_t* snapshot, uint8_t test_charge) {
    restore_game(snapshot);
    charge = test_charge;
    release_jump();
    for (int tick_count = 0; tick_count < 48 && state == STATE_FLYING; ++tick_count)
        assert(tick());
    return state == STATE_READY;
}

int main(void) {
    reset_game();
    const game_snapshot_t initial = save_game();
    charge = CHARGE_MAX / 2u;
    release_jump();
    assert(roll_phase == 0);
    assert(tick());
    assert(state == STATE_FLYING);
    assert(roll_phase == 1);
    restore_game(&initial);

    for (int round = 0; round < 16; ++round) {
        const game_snapshot_t snapshot = save_game();
        int successful_charge = -1;
        for (uint8_t test_charge = 0; test_charge <= CHARGE_MAX; ++test_charge) {
            if (try_charge(&snapshot, test_charge)) {
                successful_charge = test_charge;
                break;
            }
        }
        assert(successful_charge >= 0);
        assert(try_charge(&snapshot, (uint8_t)successful_charge));
        assert(roll_phase == 0);
        for (int tick_count = 0; tick_count < 48 && camera_x != camera_target_x; ++tick_count)
            assert(tick());
        assert(state == STATE_READY);
        assert(camera_x == camera_target_x);
    }

    reset_game();
    state = STATE_FLYING;
    pointer_down = 1;
    buffered_press = 1;
    land_on_target();
    assert(state == STATE_CHARGING);
    assert(charge == 0);

    reset_game();
    state = STATE_FLYING;
    pointer_down = 0;
    buffered_press = 1;
    land_on_target();
    assert(state == STATE_FLYING);
    assert(charge == BUFFERED_TAP_CHARGE);
    return 0;
}
