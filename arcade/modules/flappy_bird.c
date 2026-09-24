#define PXA_ARCADE_MODULE_PREFIX pxa_arcade_flappy_bird_
#include "pxa_arcade_module.h"

#include "pxa_canvas.h"
#include "pxa_game_sfx.h"

#define GAME_NODE 2u
#define PIPE_COUNT 3
#define STATE_READY 0u
#define STATE_PLAYING 1u
#define STATE_OVER 2u
#define GAME_TICK_MS 33u
#define GAME_MAX_CATCHUP_STEPS 2u

typedef struct {
    int16_t x;
    int16_t gap_y;
    uint8_t scored;
} pipe_t;

static uint8_t draw_data[16 * 1024];
static uint8_t ui_commands[3072];
static uint8_t packet[512];
static uint32_t random_state = 0x714ac91du;
static uint32_t score;
static uint32_t best_score;
static int16_t bird_y;
static int16_t bird_vy;
static uint8_t animation_tick;
static uint8_t initialized;
static uint8_t state;
static uint64_t last_tick_us;
static pipe_t pipes[PIPE_COUNT];
static pxa_game_sfx_t sfx;
static const char bird_asset[] = "assets/flappy-bird/bird.png";
static const char pipe_body_asset[] = "assets/flappy-bird/pipe-body.png";
static const char pipe_top_cap_asset[] = "assets/flappy-bird/pipe-top-cap.png";
static const char pipe_bottom_cap_asset[] = "assets/flappy-bird/pipe-bottom-cap.png";
static const char ground_asset[] = "assets/flappy-bird/ground.png";
static const char hud_panel_asset[] = "assets/ui/hud-panel.png";
static const char dialog_panel_asset[] = "assets/ui/dialog-panel.png";

static uint32_t random_next(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void reset_game(void) {
    score = 0;
    bird_y = 110;
    bird_vy = 0;
    state = STATE_READY;
    animation_tick = 0;
    last_tick_us = 0;
    for (int i = 0; i < PIPE_COUNT; ++i) {
        pipes[i].x = (int16_t)(250 + i * 115);
        pipes[i].gap_y = (int16_t)(45 + random_next() % 105u);
        pipes[i].scored = 0;
    }
}

static int16_t pipe_gap_half(void) {
    const int16_t reduction = (int16_t)(score / 8u);
    return reduction >= 7 ? 27 : (int16_t)(34 - reduction);
}

static int render(void) {
    pxa_canvas_frame_t frame;
    char score_text[10];
    const size_t score_length = pxa_canvas_u32_text(score_text, score);
    char best_text[10];
    const size_t best_length = pxa_canvas_u32_text(best_text, best_score);
    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));
    pxa_canvas_rect(&frame, 0, 0, 296, 240, 0x63C8EC, 0);
    pxa_canvas_rect(&frame, 0, 35, 296, 58, 0x75D2EE, 0);
    pxa_canvas_circle(&frame, 38, 38, 18, 0xE8F7FF);
    pxa_canvas_circle(&frame, 58, 34, 23, 0xE8F7FF);
    pxa_canvas_circle(&frame, 235, 52, 11, 0xFDF4B5);
    pxa_canvas_circle(&frame, 243, 56, 10, 0x75D2EE);
    for (int i = 0; i < PIPE_COUNT; ++i) {
        const int16_t gap_half = pipe_gap_half();
        const int16_t top_h = (int16_t)(pipes[i].gap_y - gap_half);
        const int16_t bottom_y = (int16_t)(pipes[i].gap_y + gap_half);
        pxa_canvas_image(&frame, pipes[i].x, 0, 30, (uint16_t)(top_h - 10), 255,
                         PXA_UI_IMAGE_FIT_STRETCH, pipe_body_asset,
                         sizeof(pipe_body_asset) - 1);
        pxa_canvas_image(&frame, (int16_t)(pipes[i].x - 3), (int16_t)(top_h - 10), 36, 10,
                         255, PXA_UI_IMAGE_FIT_STRETCH, pipe_top_cap_asset,
                         sizeof(pipe_top_cap_asset) - 1);
        pxa_canvas_image(&frame, pipes[i].x, (int16_t)(bottom_y + 10), 30,
                         (uint16_t)(200 - bottom_y), 255, PXA_UI_IMAGE_FIT_STRETCH,
                         pipe_body_asset, sizeof(pipe_body_asset) - 1);
        pxa_canvas_image(&frame, (int16_t)(pipes[i].x - 3), bottom_y, 36, 10,
                         255, PXA_UI_IMAGE_FIT_STRETCH, pipe_bottom_cap_asset,
                         sizeof(pipe_bottom_cap_asset) - 1);
    }
    pxa_canvas_image(&frame, 0, 210, 296, 30, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     ground_asset, sizeof(ground_asset) - 1);
    pxa_canvas_image(&frame, 53, (int16_t)(bird_y - 11), 34, 21, 255, 0,
                     bird_asset, sizeof(bird_asset) - 1);
    pxa_canvas_image(&frame, 40, 6, 216, 26, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     hud_panel_asset, sizeof(hud_panel_asset) - 1);
    const char *score_label = PXA_ARCADE_MSG(PXA_MSG_HUD_SCORE);
    pxa_canvas_text_box(&frame, 60, 6, 52, 26, 0xDDF5FF, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_label,
                        pxa_arcade_text_size(score_label));
    pxa_canvas_text_box(&frame, 113, 6, 32, 26, 0xFFFFFF, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_text, score_length);
    const char *best_label = PXA_ARCADE_MSG(PXA_MSG_HUD_BEST);
    pxa_canvas_text_box(&frame, 154, 6, 40, 26, 0xDDF5FF, PXA_CANVAS_ALIGN_RIGHT,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, best_label,
                        pxa_arcade_text_size(best_label));
    pxa_canvas_text_box(&frame, 196, 6, 28, 26, 0xFFFFFF, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, best_text, best_length);
    if (state == STATE_READY) {
        const char *hint = PXA_ARCADE_MSG(PXA_MSG_FLAPPY_HINT_START);
        pxa_canvas_text(&frame, 68, 92, 160, 0x12344A,
                        PXA_CANVAS_ALIGN_CENTER, hint,
                        pxa_arcade_text_size(hint));
    } else if (state == STATE_OVER) {
        const char *over = PXA_ARCADE_MSG(PXA_MSG_FLAPPY_RESULT_CRASHED);
        pxa_canvas_image(&frame, 38, 80, 220, 52, 255, PXA_UI_IMAGE_FIT_STRETCH,
                         dialog_panel_asset, sizeof(dialog_panel_asset) - 1);
        pxa_canvas_text(&frame, 58, 96, 180, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, over,
                        pxa_arcade_text_size(over));
    }
    return pxa_arcade_present(GAME_NODE, &frame, &pxa_arcade_ui_generation, &initialized, ui_commands,
                            sizeof(ui_commands), packet, sizeof(packet));
}

static void finish_game(void) {
    state = STATE_OVER;
    if (score > best_score)
        best_score = score;
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_DEATH);
}

static int tick_step(void) {
    if (state == STATE_READY) {
        bird_y = (int16_t)(110 + ((animation_tick++ / 3u) % 7u) - 3);
        return 1;
    }
    if (state != STATE_PLAYING)
        return 1;
    bird_vy = (int16_t)(bird_vy + 1);
    if (bird_vy > 8)
        bird_vy = 8;
    bird_y = (int16_t)(bird_y + bird_vy);
    for (int i = 0; i < PIPE_COUNT; ++i) {
        const int16_t speed = score >= 16 ? 5 : score >= 7 ? 4 : 3;
        pipes[i].x = (int16_t)(pipes[i].x - speed);
        if (!pipes[i].scored && pipes[i].x + 30 < 70) {
            pipes[i].scored = 1;
            ++score;
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_SCORE);
        }
        if (pipes[i].x < -38) {
            pipes[i].x = 307;
            pipes[i].gap_y = (int16_t)(45 + random_next() % 105u);
            pipes[i].scored = 0;
        }
        const int16_t gap_half = pipe_gap_half();
        if (pipes[i].x < 83 && pipes[i].x + 30 > 57 &&
            (bird_y - 12 < pipes[i].gap_y - gap_half || bird_y + 12 > pipes[i].gap_y + gap_half))
            finish_game();
    }
    if (bird_y < 42 || bird_y > 198)
        finish_game();
    return 1;
}

static int tick(uint8_t steps) {
    if (steps == 0 || state == STATE_OVER) return 1;
    while (steps-- != 0) {
        if (!tick_step()) return 0;
        if (state == STATE_OVER) break;
    }
    return render();
}


int32_t pxa_app_start(const uint8_t* config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    initialized = 0;
    if (!pxa_window_fullscreen())
        return PXA_STATUS_INTERNAL;
    reset_game();
    if (!render() || !pxa_clock_set_period(GAME_TICK_MS))
        return PXA_STATUS_INTERNAL;
    pxa_game_sfx_set_theme(&sfx, PXA_GAME_SFX_THEME_FLAPPY);
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
        random_state ^= (uint32_t)pxa_read_u64(parsed.payload);
        if (!tick(pxa_clock_tick_steps(&last_tick_us, &parsed, GAME_TICK_MS,
                                       GAME_MAX_CATCHUP_STEPS)))
            return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    } else if (pxa_canvas_parse_pointer(&parsed, GAME_NODE, &pointer) &&
               pointer.phase == PXA_POINTER_DOWN) {
        if (state == STATE_OVER)
            reset_game();
        state = STATE_PLAYING;
        bird_vy = -7;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_JUMP);
        if (!render())
            return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
