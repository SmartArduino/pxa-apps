#include "pxa_control_capture.h"
#include "garden-guard/main.cpp"

#include <assert.h>
#include <array>

static unsigned save_submits;
static uint8_t submitted_progress;
static int32_t save_status;

int32_t pxa_test_control(const uint8_t* data, uint32_t length) {
    assert(data != NULL);
    assert(length >= PXA_HEADER_BYTES && length <= 4096);
    pxa_event_t event;
    assert(pxa_parse_event(data, length, &event));
    if (event.service == PXA_STORAGE_SERVICE && event.opcode == PXA_STORAGE_SET) {
        ++save_submits;
        pxa_wire_record_view_t key, value;
        size_t consumed;
        assert(pxa_wire_record_decode(event.payload, event.payload_size, &key, &consumed));
        assert(pxa_wire_record_decode(event.payload + consumed, event.payload_size - consumed, &value, &consumed));
        assert(value.payload_size >= 2);
        submitted_progress = value.payload[1];
        return save_status;
    }
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t* data, uint32_t length) {
    (void)handle;
    (void)operation;
    (void)data;
    (void)length;
    return PXA_STATUS_UNSUPPORTED;
}

static void deliver(uint16_t service, uint16_t opcode, const uint8_t* payload, size_t payload_length) {
    uint8_t event[64];
    uint32_t size = 0;
    assert(pxa_build_message(event, sizeof(event), service, opcode, 0,
                                 payload, payload_length, &size));
    assert(pxa_app_on_event(event, size) == PXA_EVENT_HANDLED);
}

static void pointer_contact(uint8_t phase, int16_t x, int16_t y) {
    uint8_t pointer[36];
    pxa_writer_t writer;
    pxa_writer_init(&writer, pointer, sizeof(pointer));
    assert(pxa_put_u32(&writer, PXA_UI_PRIMARY_SURFACE));
    assert(pxa_put_u32(&writer, GAME_NODE));
    assert(pxa_put_u32(&writer, generation));
    assert(pxa_put_u16(&writer, PXA_UI_EVENT_POINTER_KIND));
    assert(pxa_put_u16(&writer, 0));
    assert(pxa_put_u32(&writer, 0));
    assert(pxa_put_u32(&writer, 0));
    assert(pxa_put_u8(&writer, 0));
    assert(pxa_put_u8(&writer, phase));
    assert(pxa_put_u16(&writer, 1));
    assert(pxa_put_u32(&writer, (uint32_t)x));
    assert(pxa_put_u32(&writer, (uint32_t)y));
    deliver(PXA_SERVICE_UI, PXA_UI_EVENT, pointer, sizeof(pointer));
}

static void tap(int16_t x, int16_t y) {
    pointer_contact(PXA_POINTER_DOWN, x, y);
    pointer_contact(PXA_POINTER_UP, x, y);
}

static void clock_tick(uint32_t timestamp_us) {
    uint8_t payload[8];
    pxa_writer_t writer;
    pxa_writer_init(&writer, payload, sizeof(payload));
    assert(pxa_put_u32(&writer, timestamp_us));
    assert(pxa_put_u32(&writer, 0));
    deliver(PXA_SERVICE_CLOCK, PXA_CLOCK_TICK, payload, sizeof(payload));
}

int main(void) {
    assert(MOWER_HOME_X + 30 <= BOARD_X);
    assert(BOARD_X + BOARD_COLS * CELL_W <= 280);
    assert(pxa_app_start(NULL, 0) == PXA_STATUS_OK);
    assert(state == STATE_HOME);
    // Canvas dimensions are dp while Environment extents are physical pixels.
    // Drawing and hit testing must use the same compact layout on dense/round panels.
    for (const auto& profile : {std::array<uint32_t, 4>{480,480,124928,8},
                               std::array<uint32_t, 4>{176,176,65536,26}}) {
        pxa_ui_environment_t environment{};
        environment.width=profile[0];environment.height=profile[1];
        environment.density_q16=profile[2];
        for(auto& inset:environment.safe_insets)inset=profile[3];
        assert(pxa_game_screen_apply_canvas(&game_screen,&environment));
        assert(game_screen.width==uint64_t(profile[0])*65536/profile[2]);
        assert(game_screen.safe_left==profile[3]);
        garden_layout_update();
        if (GARDEN_W >= BOARD_COLS*CELL_W)
            assert(BOARD_X >= 0 && BOARD_X+BOARD_COLS*CELL_W <= GARDEN_W);
        assert(HOME_X >= int(game_screen.safe_left));
        assert(HOME_X+HOME_W <= GARDEN_W-int(game_screen.safe_right));
        assert(HOME_MORE_Y+HOME_MORE_H <= GARDEN_H-int(game_screen.safe_bottom));
        assert(hit_home_start(HOME_X+HOME_W/2, HOME_START_Y+HOME_START_H/2));
        assert(hit_home_almanac(HOME_X+HOME_W-2, HOME_MORE_Y+HOME_MORE_H/2));
        assert(!hit_home_start(HOME_X+HOME_W/2,HOME_START_Y+HOME_START_H));
    }
    pxa_game_screen_reset(&game_screen);
    garden_layout_update();

#if GARDEN_DEBUG_UNLOCK_ALL
    assert(max_unlocked == LEVELS_COUNT);
    for (uint8_t type = 1; type < PLANT_TYPE_COUNT; ++type)
        assert(is_plant_unlocked(type));
    storage_pending = 0;
    garden_save();
    assert(storage_pending == 0);
#else
    assert(max_unlocked == 1);
#endif
    const uint32_t initial_unlocked_mask = unlocked_mask;
    unlocked_mask = (1u << PLANT_TYPE_COUNT) - 1u;
    open_level_for_selection(5);
    assert(chosen_count == levels[5].slots);
    assert(chosen_plants[0] == PLANT_PUFF);
    assert(chosen_plants[1] == PLANT_SUNSHROOM);
    assert(chosen_plants[2] == PLANT_FUME);
    assert(chosen_plants[3] == PLANT_DOOM);
    assert(chosen_plants[4] == PLANT_MAGNET);

    chosen_count = 3;
    chosen_plants[0] = PLANT_PUFF;
    chosen_plants[1] = PLANT_SUNSHROOM;
    chosen_plants[2] = PLANT_MAGNET;
    focused_plant = PLANT_EMPTY;
    uint8_t right_card = PLANT_EMPTY;
    assert(hit_seed_avail(230, 122, &right_card));
    tap(230, 122);
    assert(focused_plant == right_card);
    focused_plant = PLANT_EMPTY;
    assert(hit_seed_avail(270, 122, &right_card));
    tap(270, 122);
    assert(focused_plant == right_card);

    unlocked_mask = initial_unlocked_mask;
    open_level_for_selection(0);
    assert(state == STATE_SEED_SELECT);
    reset_game_for_level(0);
    assert(state == STATE_READY);

    tap(SEED_X + SEED_STRIDE + 4, SEED_Y + 4);
    assert(selected_plant == PLANT_SUNFLOWER);
    tap(SEED_X + SEED_STRIDE + 4, SEED_Y + 4);
    assert(selected_plant == PLANT_EMPTY);
    tap(SEED_X + SEED_STRIDE + 4, SEED_Y + 4);
    assert(selected_plant == PLANT_SUNFLOWER);

    tap(BOARD_X + 4, BOARD_Y + 10);
    assert(state == STATE_PLAYING);
    assert(plants[0][0].type == PLANT_SUNFLOWER);
    assert(selected_plant == PLANT_EMPTY);
    suns[0].active = 1;
    suns[0].x = 120;
    suns[0].y = 118;
    suns[0].value = 25;
    const uint32_t sun_before_collect = sun_count;
    pointer_contact(PXA_POINTER_MOVE, 120, 118);
    assert(!suns[0].active);
    assert(sun_count == sun_before_collect + 25);
    const uint16_t cooldown_before_pause = plant_cooldowns[PLANT_SUNFLOWER];
    const uint16_t plant_timer_before_pause = plants[0][0].timer;
    const uint16_t spawn_timer_before_pause = spawn_timer;
    const uint32_t random_before_pause = random_state;

    tap(PAUSE_X + 1, PAUSE_Y + 1);
    assert(state == STATE_PAUSED);
    clock_tick(50000);
    clock_tick(100000);
    assert(plant_cooldowns[PLANT_SUNFLOWER] == cooldown_before_pause);
    assert(plants[0][0].timer == plant_timer_before_pause);
    assert(spawn_timer == spawn_timer_before_pause);
    assert(random_state == random_before_pause);

    tap(PAUSE_X + 1, PAUSE_Y + 1);
    assert(state == STATE_PLAYING);
    clock_tick(150000);
    assert(plant_cooldowns[PLANT_SUNFLOWER] + 1 == cooldown_before_pause);

    const uint16_t cooldown_before_audio_ticks =
        plant_cooldowns[PLANT_SUNFLOWER];
    clock_tick(170000);
    clock_tick(190000);
    assert(plant_cooldowns[PLANT_SUNFLOWER] == cooldown_before_audio_ticks);
    clock_tick(210000);
    assert(plant_cooldowns[PLANT_SUNFLOWER] + 1 ==
           cooldown_before_audio_ticks);

    reset_game_for_level(current_level);
    state = STATE_PLAYING;
    plants[2][3].type = PLANT_BOMB;
    plants[2][3].health = plant_health(PLANT_BOMB);
    zombies[0].active = 1;
    zombies[0].row = 1;
    zombies[0].x = cell_x(2);
    zombies[1].active = 1;
    zombies[1].row = 2;
    zombies[1].x = cell_x(3);
    zombies[2].active = 1;
    zombies[2].row = 3;
    zombies[2].x = cell_x(4);
    zombies[3].active = 1;
    zombies[3].row = 0;
    zombies[3].x = cell_x(3);
    explode_bomb(2, 3);
    assert(!zombies[0].active && !zombies[1].active && !zombies[2].active);
    assert(zombies[3].active);
    assert(defeated == 3);
    assert(plants[2][3].type == PLANT_EMPTY);
    assert(blasts[0].active && blasts[0].timer == BOMB_BLAST_TICKS);
    assert(render());
    for (uint8_t tick = 0; tick < BOMB_BLAST_TICKS; ++tick)
        update_blasts();
    assert(!blasts[0].active);

    reset_game_for_level(current_level);
    state = STATE_PLAYING;
    plants[0][0].type = PLANT_FUME;
    plants[0][0].health = plant_health(PLANT_FUME);
    plants[0][0].timer = FUME_FIRE_TICKS - 1;
    zombies[0].active = 1;
    zombies[0].row = 0;
    zombies[0].x = (int16_t)(fume_end_x(0) - 1);
    zombies[0].health = zombie_health(ZOMBIE_REGULAR);
    assert(fume_end_x(0) - fume_start_x(0) == FUME_RANGE_PX);
    update_plants();
    assert(zombies[0].health == zombie_health(ZOMBIE_REGULAR) - 1);
    assert(render());

    reset_game_for_level(current_level);
    state = STATE_PLAYING;
    plants[0][0].type = PLANT_PUFF;
    plants[0][0].health = plant_health(PLANT_PUFF);
    plants[0][0].timer = PUFF_FIRE_TICKS - 1;
    zombies[0].active = 1;
    zombies[0].row = 0;
    zombies[0].x = 100;
    zombies[0].health = zombie_health(ZOMBIE_REGULAR);
    update_plants();
    assert(projectiles[0].active);
    assert(projectiles[0].kind == PROJECTILE_SPORE);
    assert(plants[0][0].stage == PUFF_FLASH_TICKS);
    suns[0].active = 1;
    suns[0].x = 120;
    suns[0].y = 118;
    suns[0].value = 15;
    assert(render());

    reset_game_for_level(current_level);
    plants[0][0].type = PLANT_PUFF;
    plants[0][0].health = plant_health(PLANT_PUFF);
    plants[0][0].timer = PUFF_FIRE_TICKS - 1;
    update_plants();
    assert(plants[0][0].stage == 0);
    for (int i = 0; i < MAX_PROJECTILES; ++i)
        assert(!projectiles[i].active);

    reset_game_for_level(current_level);
    state = STATE_PLAYING;
    selected_plant = PLANT_EMPTY;
    suns[0].active = 1;
    suns[0].x = 180;
    suns[0].y = 110;
    suns[0].value = 25;
    suns[1].active = 1;
    suns[1].x = 220;
    suns[1].y = 150;
    suns[1].value = 25;
    const uint32_t sun_before_double_tap = sun_count;
    last_tick_us = 100000;
    tap(BOARD_X + 4, BOARD_Y + 10);
    last_tick_us = 200000;
    tap(BOARD_X + 4, BOARD_Y + 10);
    assert(!suns[0].active && !suns[1].active);
    assert(sun_count == sun_before_double_tap + 50);

    plants[1][1].type = PLANT_WALL;
    plants[1][1].health = plant_health(PLANT_WALL);
    tap(SHOVEL_X + 2, SHOVEL_Y + 2);
    assert(shovel_mode && selected_plant == PLANT_EMPTY);
    tap(cell_x(1) + 4, cell_y(1) + 10);
    assert(plants[1][1].type == PLANT_EMPTY && !shovel_mode);

    reset_game_for_level(current_level);
    sun_count = 1000;
    chosen_count = 6;
    for (uint8_t i = 0; i < chosen_count; ++i)
        chosen_plants[i] = (uint8_t)(PLANT_SHOOTER + i);
    pointer_contact(PXA_POINTER_DOWN, SEED_VIEW_X + 190, SEED_Y + 8);
    pointer_contact(PXA_POINTER_MOVE, SEED_VIEW_X + 20, SEED_Y + 8);
    pointer_contact(PXA_POINTER_UP, SEED_VIEW_X + 20, SEED_Y + 8);
    assert(seed_scroll == seed_scroll_limit_chosen());
    assert(selected_plant == PLANT_EMPTY);

    for (int row = 0; row < BOARD_ROWS; ++row)
        for (int col = 0; col < BOARD_COLS; ++col)
            plants[row][col].type =
                (uint8_t)(PLANT_SHOOTER +
                          (row * BOARD_COLS + col) %
                              (PLANT_TYPE_COUNT - PLANT_SHOOTER));
    for (int index = 0; index < MAX_ZOMBIES; ++index) {
        zombies[index].active = 1;
        zombies[index].row = (uint8_t)(index % BOARD_ROWS);
        zombies[index].x = (int16_t)(180 + index * 24);
        zombies[index].type = (uint8_t)(index % 5);
    }
    assert(render());
#if !GARDEN_DEBUG_UNLOCK_ALL
    storage_pending = 0;
    save_submits = 0;
    max_unlocked = 2;
    garden_save();
    assert(storage_pending == 1 && save_submits == 1 && submitted_progress == 2);
    max_unlocked = 3;
    garden_save();
    assert(storage_pending == 2 && save_submits == 1);
    auto complete = [](int32_t status) {
        uint8_t packet[24], payload[4]; uint32_t written;
        pxa_store_u32(payload, static_cast<uint32_t>(status));
        assert(pxa_build_message(packet, sizeof(packet), PXA_STORAGE_SERVICE,
            PXA_STORAGE_SET, STORAGE_SET_REQ, payload, sizeof(payload), &written));
        assert(pxa_app_on_event(packet, written) == PXA_EVENT_HANDLED);
    };
    complete(0);
    assert(storage_pending == 1 && save_submits == 2 && submitted_progress == 3);
    complete(0);
    assert(storage_pending == 0);
    save_status = PXA_STATUS_WOULD_BLOCK;
    garden_save();
    assert(storage_pending == 3);
    save_status = 0;
    clock_tick(1000000);
    assert(storage_pending == 1 && save_submits == 4);
    complete(PXA_STATUS_BUSY);
    assert(storage_pending == 3);
    clock_tick(1020000);
    assert(storage_pending == 1 && save_submits == 5);
    complete(0);
    assert(storage_pending == 0);
#else
    assert(save_submits == 0 && submitted_progress == 0);
#endif
    return 0;
}
