#define PXA_ARCADE_MODULE_PREFIX pxa_arcade_jump_jump_
#include "pxa_arcade_module.h"

#include "pxa_canvas.h"
#include "pxa_game_sfx.h"

#define GAME_NODE 2u
#define TICK_MS 25u
#define GAME_MAX_CATCHUP_STEPS 2u
#define CHARGE_MAX 48u
#define BUFFERED_TAP_CHARGE 18u
#define FIXED_ONE 16

#define STATE_READY 0u
#define STATE_CHARGING 1u
#define STATE_FLYING 2u
#define STATE_OVER 3u

typedef struct {
    int32_t x;
    int16_t y;
    uint8_t width;
} platform_t;

static uint8_t draw_data[16 * 1024];
static uint8_t ui_commands[3072];
static uint8_t packet[512];
static uint32_t random_state = 0x42ac91e7u;
static uint32_t score;
static uint32_t best_score;
static int32_t player_x_q;
static int32_t player_y_q;
static int32_t camera_x;
static int32_t camera_target_x;
static int16_t velocity_x_q;
static int16_t velocity_y_q;
static uint8_t charge;
static uint8_t roll_phase;
static uint8_t perfect_chain;
static uint8_t feedback_points;
static uint8_t feedback_ticks;
static uint8_t initialized;
static uint8_t state;
static uint8_t started;
static uint8_t pointer_down;
static uint8_t buffered_press;
static uint64_t last_tick_us;
static platform_t platforms[2];
static pxa_game_sfx_t sfx;
static const char background_asset[] = "assets/jump-jump/background.png";
static const char hud_panel_asset[] = "assets/ui/hud-panel.png";
static const char dialog_panel_asset[] = "assets/ui/dialog-panel.png";

static void start_buffered_jump(void);

static uint32_t random_next(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static int16_t clamp_i16(int16_t value, int16_t minimum, int16_t maximum) {
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static void generate_target(int32_t start_x) {
    const uint8_t difficulty = score >= 30 ? 15 : (uint8_t)(score / 2u);
    platform_t* current = &platforms[0];
    platform_t* target = &platforms[1];
    target->width = (uint8_t)(48u + random_next() % 21u);
    if (difficulty >= 8 && target->width > 44)
        target->width = (uint8_t)(target->width - 8u);
    const int32_t gap = 40 + (int32_t)(random_next() % (31u + difficulty));
    target->x = current->x + current->width + gap;
    const int16_t delta_y = (int16_t)((int32_t)(random_next() % 33u) - 16);
    target->y = clamp_i16((int16_t)(current->y + delta_y), 148, 194);

    const int32_t target_center = target->x + target->width / 2;
    const int32_t distance = target_center - start_x;
    if (distance > 150)
        target->x -= distance - 150;
    else if (distance < 68)
        target->x += 68 - distance;
}

static void reset_game(void) {
    score = 0;
    charge = 0;
    roll_phase = 0;
    perfect_chain = 0;
    feedback_points = 0;
    feedback_ticks = 0;
    state = STATE_READY;
    started = 0;
    pointer_down = 0;
    buffered_press = 0;
    last_tick_us = 0;
    platforms[0].x = 38;
    platforms[0].y = 184;
    platforms[0].width = 66;
    player_x_q = (platforms[0].x + platforms[0].width / 2) * FIXED_ONE;
    player_y_q = platforms[0].y * FIXED_ONE;
    camera_x = 0;
    camera_target_x = 0;
    velocity_x_q = 0;
    velocity_y_q = 0;
    generate_target(player_x_q / FIXED_ONE);
}

static void draw_platform(pxa_canvas_frame_t* frame, const platform_t* platform, uint32_t top_color,
                          uint32_t side_color) {
    const int32_t screen_x = platform->x - camera_x;
    if (screen_x > 310 || screen_x + platform->width < -14)
        return;
    pxa_canvas_rect(frame, (int16_t)screen_x, (int16_t)(platform->y + 7), platform->width,
                  (uint16_t)(240 - platform->y - 7), side_color, 3);
    pxa_canvas_rect(frame, (int16_t)screen_x, platform->y, platform->width, 11, top_color, 4);
}

static void draw_player(pxa_canvas_frame_t* frame, int16_t x, int16_t foot_y) {
    if (state != STATE_FLYING) {
        const uint8_t compression = state == STATE_CHARGING
                                        ? (uint8_t)(charge * 8u / CHARGE_MAX)
                                        : 0;
        const uint8_t body_width = (uint8_t)(16u + compression);
        const uint8_t body_height = (uint8_t)(19u - compression);
        const uint8_t body_radius = (uint8_t)(5u + compression / 3u);
        const uint8_t head_radius = (uint8_t)(8u - compression / 4u);
        const int16_t body_top = (int16_t)(foot_y - body_height);
        const int16_t head_y = (int16_t)(body_top - head_radius + compression / 4u);
        pxa_canvas_rect(frame, (int16_t)(x - body_width / 2), body_top, body_width, body_height,
                      0x3977C3, body_radius);
        pxa_canvas_circle(frame, x, head_y, head_radius, 0xFFD3A3);
        pxa_canvas_circle(frame, (int16_t)(x + head_radius / 3),
                        (int16_t)(head_y - head_radius / 4), 1, 0x26384B);
        return;
    }

    static const int8_t axis_x[8] = {0, 7, 10, 7, 0, -7, -10, -7};
    static const int8_t axis_y[8] = {-10, -7, 0, 7, 10, 7, 0, -7};
    const uint8_t frame_index = (uint8_t)((roll_phase / 2u) & 7u);
    const int16_t head_dx = axis_x[frame_index];
    const int16_t head_dy = axis_y[frame_index];
    const int16_t perpendicular_x = (int16_t)-head_dy;
    const int16_t perpendicular_y = head_dx;
    const int16_t center_y = (int16_t)(foot_y - 14);
    const int16_t body_top_x = (int16_t)(x + head_dx / 10);
    const int16_t body_top_y = (int16_t)(center_y + head_dy / 10);
    const int16_t body_bottom_x = (int16_t)(x - head_dx * 7 / 10);
    const int16_t body_bottom_y = (int16_t)(center_y - head_dy * 7 / 10);
    const int16_t head_x = (int16_t)(x + head_dx);
    const int16_t head_y = (int16_t)(center_y + head_dy);
    const int16_t eye_x = (int16_t)(head_x + perpendicular_x * 3 / 10 + head_dx * 2 / 10);
    const int16_t eye_y = (int16_t)(head_y + perpendicular_y * 3 / 10 + head_dy * 2 / 10);

    pxa_canvas_line(frame, body_top_x, body_top_y, body_bottom_x, body_bottom_y, 0x3977C3, 16);
    pxa_canvas_circle(frame, head_x, head_y, 7, 0xFFD3A3);
    pxa_canvas_circle(frame, eye_x, eye_y, 1, 0x26384B);
}

static int render(void) {
    pxa_canvas_frame_t frame;
    char score_text[10];
    const size_t score_length = pxa_canvas_u32_text(score_text, score);
    char best_text[10];
    const size_t best_length = pxa_canvas_u32_text(best_text, best_score);
    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));

    pxa_canvas_image(&frame, 0, 0, 296, 240, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     background_asset, sizeof(background_asset) - 1);

    draw_platform(&frame, &platforms[0], 0x65C690, 0x388A67);
    draw_platform(&frame, &platforms[1], 0xF0B45A, 0xC47A3E);
    const int16_t target_center = (int16_t)(platforms[1].x + platforms[1].width / 2 - camera_x);
    if (target_center >= 0 && target_center < 296)
        pxa_canvas_circle(&frame, target_center, (int16_t)(platforms[1].y + 2), 4, 0xFFF1C2);

    const int16_t player_x = (int16_t)(player_x_q / FIXED_ONE - camera_x);
    const int16_t player_foot_y = (int16_t)(player_y_q / FIXED_ONE);
    draw_player(&frame, player_x, player_foot_y);

    pxa_canvas_image(&frame, 40, 6, 216, 26, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     hud_panel_asset, sizeof(hud_panel_asset) - 1);
    const char *score_label = PXA_ARCADE_MSG(PXA_MSG_HUD_SCORE);
    pxa_canvas_text_box(&frame, 60, 6, 52, 26, 0x65849A, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_label,
                        pxa_arcade_text_size(score_label));
    pxa_canvas_text_box(&frame, 113, 6, 32, 26, 0x23415A, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_text, score_length);
    const char *best_label = PXA_ARCADE_MSG(PXA_MSG_HUD_BEST);
    pxa_canvas_text_box(&frame, 154, 6, 40, 26, 0x65849A, PXA_CANVAS_ALIGN_RIGHT,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, best_label,
                        pxa_arcade_text_size(best_label));
    pxa_canvas_text_box(&frame, 196, 6, 28, 26, 0x23415A, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, best_text, best_length);

    if (!started && state == STATE_READY) {
        const char *hint = PXA_ARCADE_MSG(PXA_MSG_JUMP_JUMP_HINT_CHARGE);
        pxa_canvas_text(&frame, 58, 214, 180, 0x315B73, PXA_CANVAS_ALIGN_CENTER, hint,
                      pxa_arcade_text_size(hint));
    }

    if (feedback_ticks != 0) {
        char points_text[10];
        const size_t points_length = pxa_canvas_u32_text(points_text, feedback_points);
        static const char plus[] = "+";
        pxa_canvas_text(&frame, 128, 52, 18, 0xE56A3B, PXA_CANVAS_ALIGN_RIGHT, plus, sizeof(plus) - 1);
        pxa_canvas_text(&frame, 146, 52, 30, 0xE56A3B, PXA_CANVAS_ALIGN_LEFT, points_text,
                      points_length);
    }

    if (state == STATE_OVER) {
        const char *over = PXA_ARCADE_MSG(PXA_MSG_JUMP_JUMP_RESULT_MISSED);
        pxa_canvas_image(&frame, 38, 82, 220, 52, 255, PXA_UI_IMAGE_FIT_STRETCH,
                         dialog_panel_asset, sizeof(dialog_panel_asset) - 1);
        pxa_canvas_text(&frame, 50, 99, 196, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, over,
                        pxa_arcade_text_size(over));
    }

    return pxa_arcade_present(GAME_NODE, &frame, &pxa_arcade_ui_generation, &initialized, ui_commands,
                            sizeof(ui_commands), packet, sizeof(packet));
}

static void land_on_target(void) {
    const platform_t target = platforms[1];
    const int32_t player_x = player_x_q / FIXED_ONE;
    int32_t center_delta = player_x - (target.x + target.width / 2);
    if (center_delta < 0)
        center_delta = -center_delta;
    if (center_delta <= 5) {
        if (perfect_chain < 5)
            ++perfect_chain;
        feedback_points = (uint8_t)(1u + perfect_chain);
    } else {
        perfect_chain = 0;
        feedback_points = 1;
    }
    score += feedback_points;
    if (score > best_score)
        best_score = score;
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_SCORE);
    feedback_ticks = 28;
    player_y_q = target.y * FIXED_ONE;
    velocity_x_q = 0;
    velocity_y_q = 0;
    roll_phase = 0;
    platforms[0] = target;
    generate_target(player_x);
    camera_target_x = platforms[0].x - 38;
    state = STATE_READY;
    start_buffered_jump();
}

static int advance_camera(void) {
    const int32_t remaining = camera_target_x - camera_x;
    if (remaining <= 0) {
        camera_x = camera_target_x;
        return 0;
    }
    if (remaining <= 4) {
        camera_x = camera_target_x;
    } else {
        int32_t step = remaining / 2;
        if (step < 4)
            step = 4;
        camera_x += step;
    }
    return 1;
}

static int tick_step(int* changed) {
    const int camera_moved = advance_camera();
    if (feedback_ticks != 0) {
        --feedback_ticks;
        *changed = 1;
    }
    if (state == STATE_CHARGING) {
        if (charge < CHARGE_MAX)
            ++charge;
        if (charge == CHARGE_MAX / 2 || charge == CHARGE_MAX)
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_CHARGE);
        *changed = 1;
        return 1;
    }
    if (state == STATE_FLYING) {
        const int32_t old_y_q = player_y_q;
        player_x_q += velocity_x_q;
        velocity_y_q = (int16_t)(velocity_y_q + FIXED_ONE);
        player_y_q += velocity_y_q;
        roll_phase = (uint8_t)((roll_phase + 1u) & 15u);
        const platform_t* target = &platforms[1];
        const int32_t player_x = player_x_q / FIXED_ONE;
        const int32_t target_y_q = target->y * FIXED_ONE;
        if (velocity_y_q > 0 && old_y_q <= target_y_q && player_y_q >= target_y_q &&
            player_x >= target->x + 6 && player_x <= target->x + target->width - 6) {
            land_on_target();
        } else if (player_y_q / FIXED_ONE > 260) {
            state = STATE_OVER;
            perfect_chain = 0;
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_DEATH);
        }
        *changed = 1;
        return 1;
    }
    if (camera_moved) *changed = 1;
    return 1;
}

static int tick_steps(uint8_t steps) {
    int changed = 0;
    while (steps-- != 0)
        if (!tick_step(&changed)) return 0;
    return !changed || render();
}

static int tick(void) { return tick_steps(1); }

static void start_charge(void) {
    charge = 0;
    state = STATE_CHARGING;
    started = 1;
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_CHARGE);
}

static void release_jump(void) {
    velocity_x_q = (int16_t)(52 + (uint16_t)charge * 5u / 2u);
    velocity_y_q = -9 * FIXED_ONE;
    roll_phase = 0;
    state = STATE_FLYING;
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_JUMP);
}

static void start_buffered_jump(void) {
    if (buffered_press == 0)
        return;
    buffered_press = 0;
    start_charge();
    if (pointer_down == 0) {
        charge = BUFFERED_TAP_CHARGE;
        release_jump();
    }
}


int32_t pxa_app_start(const uint8_t* config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    initialized = 0;
    if (!pxa_window_fullscreen())
        return PXA_STATUS_INTERNAL;
    reset_game();
    if (!render() || !pxa_clock_set_period(TICK_MS))
        return PXA_STATUS_INTERNAL;
    pxa_game_sfx_set_theme(&sfx, PXA_GAME_SFX_THEME_JUMP_JUMP);
    pxa_game_sfx_start(&sfx, packet, sizeof(packet));
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t* event, uint32_t length) {
    pxa_canvas_event_t parsed;
    pxa_ui_pointer_data_t pointer;
    if (!pxa_canvas_parse_event(event, length, &parsed))
        return PXA_EVENT_UNHANDLED;
    if (parsed.service == PXA_SERVICE_SYSTEM &&
        parsed.opcode == PXA_SYSTEM_CONFIGURATION_EVENT)
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    if (pxa_game_sfx_handle_event(&sfx, &parsed, packet, sizeof(packet)))
        return PXA_EVENT_HANDLED;
    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK &&
        parsed.payload_length == 8) {
        pxa_game_sfx_tick(&sfx, &parsed);
        const uint8_t steps = pxa_clock_tick_steps(
            &last_tick_us, &parsed, TICK_MS, GAME_MAX_CATCHUP_STEPS);
        random_state ^= (uint32_t)pxa_read_u64(parsed.payload);
        if (!(steps == 1 ? tick() : tick_steps(steps)))
            return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    } else if (pxa_canvas_parse_pointer(&parsed, GAME_NODE, &pointer)) {
        const uint8_t phase = pointer.phase;
        if (state == STATE_OVER && phase == PXA_POINTER_DOWN) {
            reset_game();
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
            if (!render())
                return PXA_STATUS_INTERNAL;
        } else if (phase == PXA_POINTER_DOWN) {
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_TAP);
            pointer_down = 1;
            if (state == STATE_READY) {
                start_charge();
            } else if (state == STATE_FLYING) {
                buffered_press = 1;
            }
            if (!render())
                return PXA_STATUS_INTERNAL;
        } else if (phase == PXA_POINTER_UP) {
            pointer_down = 0;
            if (state == STATE_CHARGING)
                release_jump();
            if (!render())
                return PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
