#define PXA_ARCADE_MODULE_PREFIX pxa_arcade_jumping_
#include "pxa_arcade_module.h"

#include "pxa_canvas.h"
#include "pxa_game_sfx.h"

#define GAME_NODE 2u
#define PLATFORM_COUNT 8
#define GAME_TICK_MS 33u
#define GAME_MAX_CATCHUP_STEPS 2u

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t id;
    uint8_t width;
} platform_t;

static uint8_t draw_data[16 * 1024];
static uint8_t ui_commands[3072];
static uint8_t packet[512];
static uint32_t random_state = 0x531ca9efu;
static uint32_t score;
static uint32_t best_score;
static uint16_t next_platform_id;
static uint16_t last_platform_id;
static int16_t player_x;
static int16_t player_y;
static int16_t velocity_y;
static uint8_t initialized;
static uint8_t game_over;
static uint64_t last_tick_us;
static platform_t platforms[PLATFORM_COUNT];
static pxa_game_sfx_t sfx;
static const char player_asset[] = "assets/jumping/player.png";
static const char background_asset[] = "assets/jumping/background.png";
static const char hud_panel_asset[] = "assets/ui/hud-panel.png";
static const char dialog_panel_asset[] = "assets/ui/dialog-panel.png";

static uint32_t random_next(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void reset_game(void) {
    player_x = 135;
    player_y = 174;
    velocity_y = -10;
    score = 0;
    last_platform_id = 0;
    game_over = 0;
    last_tick_us = 0;
    for (int i = 0; i < PLATFORM_COUNT; ++i) {
        platforms[i].x = (int16_t)(18 + random_next() % 215u);
        platforms[i].y = (int16_t)(215 - i * 31);
        platforms[i].width = (uint8_t)(48 + random_next() % 22u);
        platforms[i].id = ++next_platform_id;
    }
    platforms[0].x = 110;
    platforms[0].width = 78;
}

static int render(void) {
    pxa_canvas_frame_t frame;
    char score_text[10];
    size_t score_length = pxa_canvas_u32_text(score_text, score);
    char best_text[10];
    const size_t best_length = pxa_canvas_u32_text(best_text, best_score);
    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));
    pxa_canvas_image(&frame, 0, 0, 296, 240, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     background_asset, sizeof(background_asset) - 1);
    for (int i = 0; i < 18; ++i) {
        const int16_t x = (int16_t)((i * 47 + 19) % 292);
        const int16_t y = (int16_t)((i * 71 + 13) % 205);
        pxa_canvas_circle(&frame, x, y, 1, 0xCFEAFF);
    }
    for (int i = 0; i < PLATFORM_COUNT; ++i) {
        pxa_canvas_rect(&frame, platforms[i].x, platforms[i].y, platforms[i].width, 7, 0x62C98A, 3);
        pxa_canvas_rect(&frame, (int16_t)(platforms[i].x + 5), (int16_t)(platforms[i].y + 6),
                      (uint16_t)(platforms[i].width - 10), 3, 0x2D7657, 2);
    }
    pxa_canvas_image(&frame, player_x, player_y, 22, 26, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     player_asset, sizeof(player_asset) - 1);
    pxa_canvas_image(&frame, 40, 6, 216, 26, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     hud_panel_asset, sizeof(hud_panel_asset) - 1);
    const char *score_label = PXA_ARCADE_MSG(PXA_MSG_HUD_SCORE);
    pxa_canvas_text_box(&frame, 60, 6, 52, 26, 0x8FB6D8, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_label,
                        pxa_arcade_text_size(score_label));
    pxa_canvas_text_box(&frame, 113, 6, 32, 26, 0xFFFFFF, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_text, score_length);
    const char *best_label = PXA_ARCADE_MSG(PXA_MSG_HUD_BEST);
    pxa_canvas_text_box(&frame, 154, 6, 40, 26, 0x8FB6D8, PXA_CANVAS_ALIGN_RIGHT,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, best_label,
                        pxa_arcade_text_size(best_label));
    pxa_canvas_text_box(&frame, 196, 6, 28, 26, 0xFFFFFF, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, best_text, best_length);
    if (game_over) {
        const char *over = PXA_ARCADE_MSG(PXA_MSG_JUMPING_RESULT_FELL);
        pxa_canvas_image(&frame, 38, 87, 220, 52, 255, PXA_UI_IMAGE_FIT_STRETCH,
                         dialog_panel_asset, sizeof(dialog_panel_asset) - 1);
        pxa_canvas_text(&frame, 50, 104, 196, 0xFFFFFF, PXA_CANVAS_ALIGN_CENTER, over,
                      pxa_arcade_text_size(over));
    } else if (score == 0) {
        const char *hint = PXA_ARCADE_MSG(PXA_MSG_JUMPING_HINT_DRAG);
        pxa_canvas_text(&frame, 68, 32, 160, 0xA8C8E8,
                        PXA_CANVAS_ALIGN_CENTER, hint,
                        pxa_arcade_text_size(hint));
    }
    return pxa_arcade_present(GAME_NODE, &frame, &pxa_arcade_ui_generation, &initialized, ui_commands,
                            sizeof(ui_commands), packet, sizeof(packet));
}

static int tick_step(void) {
    if (game_over)
        return 1;
    const int16_t old_y = player_y;
    velocity_y = (int16_t)(velocity_y + 1);
    if (velocity_y > 10)
        velocity_y = 10;
    player_y = (int16_t)(player_y + velocity_y);
    if (velocity_y >= 0) {
        for (int i = 0; i < PLATFORM_COUNT; ++i) {
            if (old_y + 26 <= platforms[i].y && player_y + 26 >= platforms[i].y &&
                player_x + 20 >= platforms[i].x &&
                player_x + 2 <= platforms[i].x + platforms[i].width) {
                player_y = (int16_t)(platforms[i].y - 26);
                velocity_y = -11;
                pxa_game_sfx_play(&sfx, PXA_GAME_SFX_JUMP);
                if (platforms[i].id != last_platform_id) {
                    last_platform_id = platforms[i].id;
                    ++score;
                    if (score > best_score)
                        best_score = score;
                    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_SCORE);
                }
                break;
            }
        }
    }
    if (player_y < 76) {
        const int16_t shift = (int16_t)(76 - player_y);
        player_y = 76;
        for (int i = 0; i < PLATFORM_COUNT; ++i)
            platforms[i].y = (int16_t)(platforms[i].y + shift);
    }
    int16_t highest = 240;
    for (int i = 0; i < PLATFORM_COUNT; ++i)
        if (platforms[i].y < highest)
            highest = platforms[i].y;
    for (int i = 0; i < PLATFORM_COUNT; ++i) {
        if (platforms[i].y > 242) {
            platforms[i].y = (int16_t)(highest - 30 - random_next() % 12u);
            platforms[i].x = (int16_t)(12 + random_next() % 220u);
            platforms[i].width = (uint8_t)(48 + random_next() % 22u);
            if (score >= 12 && platforms[i].width > 54)
                platforms[i].width -= 8;
            platforms[i].id = ++next_platform_id;
            highest = platforms[i].y;
        }
    }
    if (player_y > 240) {
        game_over = 1;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_DEATH);
    }
    return 1;
}

static int tick(uint8_t steps) {
    if (steps == 0 || game_over) return 1;
    while (steps-- != 0) {
        if (!tick_step()) return 0;
        if (game_over) break;
    }
    return render();
}

static void move_player(int16_t touch_x) {
    player_x = (int16_t)(touch_x - 11);
    if (player_x < 20)
        player_x = 20;
    if (player_x > 254)
        player_x = 254;
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
    pxa_game_sfx_set_theme(&sfx, PXA_GAME_SFX_THEME_JUMPING);
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
    } else if (pxa_canvas_parse_pointer(&parsed, GAME_NODE, &pointer)) {
        const uint8_t phase = pointer.phase;
        if (game_over && phase == PXA_POINTER_DOWN) {
            reset_game();
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
        }
        if (phase == PXA_POINTER_DOWN || phase == PXA_POINTER_MOVE) {
            move_player((int16_t)pointer.x);
            /* Input updates state; the 33 ms game frame presents it. */
            if (phase == PXA_POINTER_DOWN && !render())
                return PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
