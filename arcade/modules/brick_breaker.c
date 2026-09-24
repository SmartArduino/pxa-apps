#define PXA_ARCADE_MODULE_PREFIX pxa_arcade_brick_breaker_
#include "pxa_arcade_module.h"

#include "pxa_canvas.h"
#include "pxa_game_sfx.h"

#define GAME_NODE 2u
#define BRICK_ROWS 6
#define BRICK_COLS 8
#define STATE_READY 0u
#define STATE_PLAYING 1u
#define STATE_OVER 2u
#define STATE_LEVEL_CLEAR 3u
#define GAME_TICK_MS 25u
#define GAME_MAX_CATCHUP_STEPS 2u
#define BRICK_X 20
#define BRICK_Y 44
#define BRICK_PITCH_X 32
#define BRICK_PITCH_Y 16
#define PADDLE_Y 206

static uint8_t draw_data[16 * 1024];
static uint8_t ui_commands[3600];
static uint8_t packet[512];
static uint32_t score;
static uint8_t level;
static uint8_t lives;
static int16_t paddle_x;
static int16_t ball_x;
static int16_t ball_y;
static int16_t ball_vx;
static int16_t ball_vy;
static uint8_t bricks[BRICK_ROWS][BRICK_COLS];
static uint8_t remaining;
static uint8_t initialized;
static uint8_t state;
static uint64_t last_tick_us;
static pxa_game_sfx_t sfx;
static const char paddle_asset[] = "assets/brick-breaker/paddle.png";
static const char ball_asset[] = "assets/brick-breaker/ball.png";
static const char background_asset[] = "assets/brick-breaker/background.png";
static const char hud_panel_asset[] = "assets/ui/hud-panel.png";
static const char dialog_panel_asset[] = "assets/ui/dialog-panel.png";

static void prepare_ball(void) {
    paddle_x = 112;
    ball_x = 148;
    ball_y = 182;
    ball_vx = 3;
    ball_vy = (int16_t)-(level >= 3 ? 6 : 4 + level - 1);
    state = STATE_READY;
}

static void fill_bricks(void) {
    static const uint8_t patterns[][BRICK_ROWS] = {
        {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
        {0x7e, 0xff, 0xbd, 0xff, 0x7e, 0x3c},
        {0x99, 0xdb, 0xff, 0xff, 0xdb, 0x99},
    };
    const uint8_t* pattern = patterns[(level - 1u) %
                                      (sizeof(patterns) / sizeof(patterns[0]))];
    remaining = 0;
    for (int row = 0; row < BRICK_ROWS; ++row)
        for (int col = 0; col < BRICK_COLS; ++col) {
            uint8_t brick = (pattern[row] & (uint8_t)(1u << col)) != 0 ? 1u : 0u;
            if (brick != 0 && level >= 3 && (row + col + level) % 5 == 0)
                brick = 2;
            bricks[row][col] = brick;
            if (brick != 0)
                ++remaining;
        }
}

static void reset_game(void) {
    score = 0;
    level = 1;
    lives = 3;
    fill_bricks();
    prepare_ball();
    last_tick_us = 0;
}

static void advance_level(void) {
    if (level < 9)
        ++level;
    fill_bricks();
    prepare_ball();
}

static uint32_t brick_color(int row) {
    static const uint32_t colors[BRICK_ROWS] = {0xEF5D72, 0xF08B4B, 0xF3C64D,
                                                0x68C98A, 0x55B7D8, 0x7A83E8};
    return colors[row];
}

static uint8_t brick_sound(int row) {
    static const uint8_t sounds[BRICK_ROWS] = {
        PXA_GAME_SFX_BRICK_RED, PXA_GAME_SFX_BRICK_ORANGE,
        PXA_GAME_SFX_BRICK_YELLOW, PXA_GAME_SFX_BRICK_GREEN,
        PXA_GAME_SFX_BRICK_CYAN, PXA_GAME_SFX_BRICK_PURPLE,
    };
    return sounds[row];
}

static int render(void) {
    pxa_canvas_frame_t frame;
    char score_text[10];
    const size_t score_length = pxa_canvas_u32_text(score_text, score);
    char level_text[10];
    const size_t level_length = pxa_canvas_u32_text(level_text, level);
    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));
    pxa_canvas_image(&frame, 0, 0, 296, 240, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     background_asset, sizeof(background_asset) - 1);
    for (int i = 0; i < 10; ++i)
        pxa_canvas_circle(&frame, (int16_t)(24 + i * 29), (int16_t)(46 + (i * 37) % 154), 1,
                          0x42617D);
    for (int row = 0; row < BRICK_ROWS; ++row) {
        for (int col = 0; col < BRICK_COLS; ++col)
            if (bricks[row][col]) {
                pxa_canvas_rect(&frame, (int16_t)(BRICK_X + col * BRICK_PITCH_X),
                              (int16_t)(BRICK_Y + row * BRICK_PITCH_Y), 29, 11,
                              brick_color(row), 3);
                pxa_canvas_rect(&frame, (int16_t)(BRICK_X + col * BRICK_PITCH_X + 3),
                              (int16_t)(BRICK_Y + row * BRICK_PITCH_Y + 2), 21, 2,
                              0xFFFFFF, 1);
                if (bricks[row][col] > 1) {
                    pxa_canvas_rect_rgba(&frame, (int16_t)(BRICK_X + col * BRICK_PITCH_X + 1),
                                         (int16_t)(BRICK_Y + row * BRICK_PITCH_Y + 1), 27, 9,
                                         pxa_canvas_rgba(0x00000000), 3, 1,
                                         pxa_canvas_rgba(0xFFF3A0));
                    pxa_canvas_circle(&frame, (int16_t)(BRICK_X + col * BRICK_PITCH_X + 15),
                                      (int16_t)(BRICK_Y + row * BRICK_PITCH_Y + 6), 2, 0xFFF7D1);
                }
            }
    }
    pxa_canvas_image(&frame, paddle_x, PADDLE_Y, 72, 9, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     paddle_asset, sizeof(paddle_asset) - 1);
    pxa_canvas_image(&frame, (int16_t)(ball_x - 6), (int16_t)(ball_y - 6), 12, 12, 255,
                     PXA_UI_IMAGE_FIT_STRETCH, ball_asset, sizeof(ball_asset) - 1);
    pxa_canvas_image(&frame, 40, 6, 216, 26, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     hud_panel_asset, sizeof(hud_panel_asset) - 1);
    const char *score_label = PXA_ARCADE_MSG(PXA_MSG_HUD_SCORE);
    pxa_canvas_text_box(&frame, 60, 6, 50, 26, 0x8FB4D8, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_label,
                        pxa_arcade_text_size(score_label));
    pxa_canvas_text_box(&frame, 112, 6, 32, 26, 0xFFFFFF, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_text, score_length);
    const char *level_label = PXA_ARCADE_MSG(PXA_MSG_HUD_LEVEL);
    pxa_canvas_text_box(&frame, 148, 6, 40, 26, 0x8FB4D8, PXA_CANVAS_ALIGN_RIGHT,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, level_label,
                        pxa_arcade_text_size(level_label));
    pxa_canvas_text_box(&frame, 188, 6, 20, 26, 0xFFFFFF, PXA_CANVAS_ALIGN_LEFT,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, level_text, level_length);
    for (int i = 0; i < lives; ++i)
        pxa_canvas_circle(&frame, (int16_t)(227 - i * 15), 18, 5, 0xFF6B7A);
    if (state == STATE_READY) {
        const char *hint_text = level >= 3
            ? PXA_ARCADE_MSG(PXA_MSG_BRICK_HINT_STRONG)
            : PXA_ARCADE_MSG(PXA_MSG_BRICK_HINT_SERVE);
        const size_t hint_length = pxa_arcade_text_size(hint_text);
        pxa_canvas_text(&frame, 58, 174, 180, 0xA9C5DF, PXA_CANVAS_ALIGN_CENTER, hint_text,
                      hint_length);
    } else if (state == STATE_OVER) {
        const char *over = PXA_ARCADE_MSG(PXA_MSG_BRICK_RESULT_LOST);
        pxa_canvas_image(&frame, 38, 140, 220, 52, 255, PXA_UI_IMAGE_FIT_STRETCH,
                         dialog_panel_asset, sizeof(dialog_panel_asset) - 1);
        pxa_canvas_text(&frame, 48, 157, 200, 0xFFFFFF, PXA_CANVAS_ALIGN_CENTER, over,
                      pxa_arcade_text_size(over));
    } else if (state == STATE_LEVEL_CLEAR) {
        const char *win = PXA_ARCADE_MSG(PXA_MSG_BRICK_RESULT_CLEAR);
        pxa_canvas_image(&frame, 38, 140, 220, 52, 255, PXA_UI_IMAGE_FIT_STRETCH,
                         dialog_panel_asset, sizeof(dialog_panel_asset) - 1);
        pxa_canvas_text(&frame, 48, 157, 200, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, win,
                        pxa_arcade_text_size(win));
    }
    return pxa_arcade_present(GAME_NODE, &frame, &pxa_arcade_ui_generation, &initialized, ui_commands,
                            sizeof(ui_commands), packet, sizeof(packet));
}

static int tick_step(void) {
    if (state != STATE_PLAYING)
        return 1;
    int16_t next_x = (int16_t)(ball_x + ball_vx);
    int16_t next_y = (int16_t)(ball_y + ball_vy);
    if (next_x < 20 || next_x > 276) {
        ball_vx = (int16_t)-ball_vx;
        next_x = (int16_t)(ball_x + ball_vx);
    }
    if (next_y < 36) {
        ball_vy = (int16_t)-ball_vy;
        next_y = (int16_t)(ball_y + ball_vy);
    }
    if (ball_vy > 0 && next_y + 6 >= PADDLE_Y && ball_y + 6 < PADDLE_Y && next_x >= paddle_x - 5 &&
        next_x <= paddle_x + 77) {
        ball_vy = (int16_t)-ball_vy;
        const int16_t offset = (int16_t)(next_x - (paddle_x + 36));
        ball_vx = offset < -18 ? -4 : offset > 18 ? 4 : offset < 0 ? -3 : 3;
        next_y = (int16_t)(PADDLE_Y - 7);
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_PADDLE);
    }
    if (next_y < 134) {
        const int col = (next_x - BRICK_X) / BRICK_PITCH_X;
        const int row = (next_y - BRICK_Y) / BRICK_PITCH_Y;
        if (row >= 0 && row < BRICK_ROWS && col >= 0 && col < BRICK_COLS && bricks[row][col] &&
            next_x >= BRICK_X + col * BRICK_PITCH_X &&
            next_x <= BRICK_X + 29 + col * BRICK_PITCH_X &&
            next_y >= BRICK_Y + row * BRICK_PITCH_Y &&
            next_y <= BRICK_Y + 11 + row * BRICK_PITCH_Y) {
            if (bricks[row][col] > 1) {
                --bricks[row][col];
                score += level;
                pxa_game_sfx_play(&sfx, PXA_GAME_SFX_HIT);
            } else {
                bricks[row][col] = 0;
                --remaining;
                score += (uint32_t)(BRICK_ROWS - row) * level;
                pxa_game_sfx_play(&sfx, brick_sound(row));
            }
            ball_vy = (int16_t)-ball_vy;
            next_y = (int16_t)(ball_y + ball_vy);
            if (remaining == 0) {
                state = STATE_LEVEL_CLEAR;
                pxa_game_sfx_play(&sfx, PXA_GAME_SFX_WIN);
            }
        }
    }
    ball_x = next_x;
    ball_y = next_y;
    if (ball_y > 226) {
        if (lives > 0)
            --lives;
        if (lives == 0) {
            state = STATE_OVER;
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_LOSE);
        } else {
            prepare_ball();
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ALERT);
        }
    }
    return 1;
}

static int tick(uint8_t steps) {
    if (steps == 0 || state != STATE_PLAYING) return 1;
    while (steps-- != 0) {
        if (!tick_step()) return 0;
        if (state != STATE_PLAYING) break;
    }
    return render();
}

static void move_paddle(int16_t x) {
    paddle_x = (int16_t)(x - 36);
    if (paddle_x < 24)
        paddle_x = 24;
    if (paddle_x > 200)
        paddle_x = 200;
    if (state == STATE_READY)
        ball_x = (int16_t)(paddle_x + 36);
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
    pxa_game_sfx_set_theme(&sfx, PXA_GAME_SFX_THEME_BRICK);
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
        if (!tick(pxa_clock_tick_steps(&last_tick_us, &parsed, GAME_TICK_MS,
                                       GAME_MAX_CATCHUP_STEPS)))
            return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    } else if (pxa_canvas_parse_pointer(&parsed, GAME_NODE, &pointer)) {
        const uint8_t phase = pointer.phase;
        if (phase == PXA_POINTER_DOWN) {
            if (state == STATE_OVER) {
                reset_game();
                pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
            } else if (state == STATE_LEVEL_CLEAR) {
                advance_level();
                pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
            }
        }
        if (phase == PXA_POINTER_DOWN || phase == PXA_POINTER_MOVE)
            move_paddle((int16_t)pointer.x);
        if (state == STATE_READY && phase == PXA_POINTER_DOWN) {
            state = STATE_PLAYING;
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
        }
        /* The game clock presents the latest paddle position every 25 ms.
         * Rendering on every sampled MOVE can otherwise starve that clock. */
        if (phase == PXA_POINTER_DOWN && !render())
            return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
