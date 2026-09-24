#define PXA_ARCADE_MODULE_PREFIX pxa_arcade_tetris_
#include "pxa_arcade_module.h"

#include "pxa_canvas.h"
#include "pxa_game_sfx.h"

#define GAME_NODE 2u
#define BOARD_W 10
#define BOARD_H 20
#define CELL 10
#define BOARD_X 98
#define BOARD_Y 28
#define MOVE_Y 120
#define MOVE_WIDTH 80
#define MOVE_HEIGHT 44
#define MOVE_LEFT_X 11
#define MOVE_RIGHT_X 205
#define ACTION_Y (MOVE_Y + MOVE_HEIGHT)
#define ACTION_WIDTH MOVE_WIDTH
#define ACTION_HEIGHT MOVE_HEIGHT
#define ROTATE_X MOVE_LEFT_X
#define DROP_X MOVE_RIGHT_X
#define GAME_TICK_MS 50u
#define GAME_MAX_CATCHUP_STEPS 2u
#define MOVE_REPEAT_DELAY_TICKS 4u

static const uint16_t base_shapes[7] = {0x00F0, 0x0066, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074};
static const uint32_t colors[8] = {0x000000, 0x56CFE1, 0xF4CF5D, 0xB878ED,
                                   0x68D391, 0xEF6A79, 0x6588E8, 0xF09B55};
/* Font Awesome Free: arrow-left, arrow-right, arrows-rotate and arrow-down. */
static const char icon_arrow_left[] = "\xef\x81\xa0";
static const char icon_arrow_right[] = "\xef\x81\xa1";
static const char icon_rotate[] = "\xef\x80\xa1";
static const char icon_arrow_down[] = "\xef\x81\xa3";

static uint8_t draw_data[16 * 1024];
static uint8_t ui_commands[4070];
static uint8_t packet[512];
static uint8_t board[BOARD_H][BOARD_W];
static uint32_t random_state = 0x94a31c27u;
static uint32_t score;
static uint32_t lines_cleared;
static int8_t piece_x;
static int8_t piece_y;
static uint8_t piece_type;
static uint8_t next_piece_type;
static uint8_t piece_rotation;
static uint8_t drop_counter;
static uint8_t initialized;
static uint8_t game_over;
static int8_t held_move_direction;
static uint8_t held_move_repeat_delay;
static uint64_t last_tick_us;
static pxa_game_sfx_t sfx;
static const char background_asset[] = "assets/tetris/background.png";
static const char hud_panel_asset[] = "assets/ui/hud-panel.png";
static const char dialog_panel_asset[] = "assets/ui/dialog-panel.png";
static const char control_button_asset[] = "assets/ui/control-button.png";

static uint32_t random_next(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static uint16_t normalize_mask(uint16_t mask) {
    while ((mask & 0x000F) == 0)
        mask >>= 4;
    while ((mask & 0x1111) == 0)
        mask >>= 1;
    return mask;
}

static uint16_t piece_mask(uint8_t type, uint8_t rotation) {
    uint16_t mask = base_shapes[type];
    for (uint8_t turn = 0; turn < (rotation & 3u); ++turn) {
        uint16_t rotated = 0;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                if (mask & (uint16_t)(1u << (y * 4 + x)))
                    rotated |= (uint16_t)(1u << (x * 4 + (3 - y)));
            }
        mask = normalize_mask(rotated);
    }
    return mask;
}

static uint8_t collides(int8_t px, int8_t py, uint8_t rotation) {
    const uint16_t mask = piece_mask(piece_type, rotation);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            if (!(mask & (uint16_t)(1u << (y * 4 + x))))
                continue;
            const int bx = px + x;
            const int by = py + y;
            if (bx < 0 || bx >= BOARD_W || by >= BOARD_H || (by >= 0 && board[by][bx] != 0))
                return 1;
        }
    return 0;
}

static void spawn_piece(void) {
    piece_type = next_piece_type;
    next_piece_type = (uint8_t)(random_next() % 7u);
    piece_rotation = 0;
    piece_x = 3;
    piece_y = 0;
    drop_counter = 0;
    if (collides(piece_x, piece_y, piece_rotation)) {
        game_over = 1;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_LOSE);
    }
}

static void reset_game(void) {
    for (int y = 0; y < BOARD_H; ++y)
        for (int x = 0; x < BOARD_W; ++x)
            board[y][x] = 0;
    score = 0;
    lines_cleared = 0;
    game_over = 0;
    held_move_direction = 0;
    held_move_repeat_delay = 0;
    next_piece_type = (uint8_t)(random_next() % 7u);
    spawn_piece();
    last_tick_us = 0;
}

static uint8_t clear_lines(void) {
    uint8_t cleared = 0;
    for (int y = BOARD_H - 1; y >= 0; --y) {
        uint8_t full = 1;
        for (int x = 0; x < BOARD_W; ++x)
            if (board[y][x] == 0)
                full = 0;
        if (!full)
            continue;
        for (int move_y = y; move_y > 0; --move_y)
            for (int x = 0; x < BOARD_W; ++x)
                board[move_y][x] = board[move_y - 1][x];
        for (int x = 0; x < BOARD_W; ++x)
            board[0][x] = 0;
        ++cleared;
        ++y;
    }
    if (cleared != 0) {
        static const uint16_t points[] = {0, 100, 300, 500, 800};
        const uint32_t level = 1u + lines_cleared / 10u;
        score += (uint32_t)points[cleared] * level;
        lines_cleared += cleared;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_SCORE);
    }
    return cleared;
}

static void lock_piece(void) {
    const uint16_t mask = piece_mask(piece_type, piece_rotation);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            if (!(mask & (uint16_t)(1u << (y * 4 + x))))
                continue;
            const int bx = piece_x + x;
            const int by = piece_y + y;
            if (by < 0) {
                game_over = 1;
                return;
            }
            if (by < BOARD_H && bx >= 0 && bx < BOARD_W)
                board[by][bx] = (uint8_t)(piece_type + 1);
        }
    pxa_game_sfx_play(&sfx, PXA_GAME_SFX_PLACE);
    clear_lines();
    spawn_piece();
}

static void render_cell(pxa_canvas_frame_t* frame, int x, int y, uint32_t color) {
    pxa_canvas_rect(frame, (int16_t)(BOARD_X + x * CELL + 1), (int16_t)(BOARD_Y + y * CELL + 1),
                  CELL - 2, CELL - 2, color, 2);
}

static void draw_button_surface(pxa_canvas_frame_t* frame, int16_t x, int16_t y,
                                uint16_t width, uint16_t height) {
    pxa_canvas_image(frame, x, y, width, height, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     control_button_asset, sizeof(control_button_asset) - 1);
}

static void draw_button_icon(pxa_canvas_frame_t* frame, int16_t x, int16_t y,
                             uint16_t width, uint16_t height, const char* icon) {
    pxa_canvas_text_role(frame, x, (int16_t)(y + (height - 20u) / 2u), width,
                         pxa_canvas_rgba(0xF6FBFF), PXA_CANVAS_FONT_ICON,
                         PXA_CANVAS_ALIGN_CENTER, icon, 3);
}

static void draw_move_button(pxa_canvas_frame_t* frame, int16_t x, uint8_t right) {
    draw_button_surface(frame, x, MOVE_Y, MOVE_WIDTH, MOVE_HEIGHT);
    draw_button_icon(frame, x, MOVE_Y, MOVE_WIDTH, MOVE_HEIGHT,
                     right ? icon_arrow_right : icon_arrow_left);
}

static void draw_rotate_button(pxa_canvas_frame_t* frame) {
    draw_button_surface(frame, ROTATE_X, ACTION_Y, ACTION_WIDTH, ACTION_HEIGHT);
    draw_button_icon(frame, ROTATE_X, ACTION_Y, ACTION_WIDTH, ACTION_HEIGHT,
                     icon_rotate);
}

static void draw_drop_button(pxa_canvas_frame_t* frame) {
    draw_button_surface(frame, DROP_X, ACTION_Y, ACTION_WIDTH, ACTION_HEIGHT);
    draw_button_icon(frame, DROP_X, ACTION_Y, ACTION_WIDTH, ACTION_HEIGHT,
                     icon_arrow_down);
}

static int render(void) {
    pxa_canvas_frame_t frame;
    char score_text[10];
    const size_t score_length = pxa_canvas_u32_text(score_text, score);
    char lines_text[10];
    const size_t lines_length = pxa_canvas_u32_text(lines_text, lines_cleared);
    char level_text[10];
    const uint32_t level = 1u + lines_cleared / 10u;
    const size_t level_length = pxa_canvas_u32_text(level_text, level);
    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));
    pxa_canvas_image(&frame, 0, 0, 296, 240, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     background_asset, sizeof(background_asset) - 1);
    pxa_canvas_rect_rgba(&frame, (int16_t)(BOARD_X - 3), (int16_t)(BOARD_Y - 3),
                         BOARD_W * CELL + 6, BOARD_H * CELL + 6, pxa_canvas_rgba(0x273A52), 5, 1,
                         pxa_canvas_rgba(0x4B7194));
    pxa_canvas_rect(&frame, BOARD_X, BOARD_Y, BOARD_W * CELL, BOARD_H * CELL, 0x192438, 2);
    for (int y = 1; y < BOARD_H; ++y)
        pxa_canvas_line(&frame, BOARD_X, (int16_t)(BOARD_Y + y * CELL),
                        (int16_t)(BOARD_X + BOARD_W * CELL), (int16_t)(BOARD_Y + y * CELL),
                        0x223149, 1);
    for (int x = 1; x < BOARD_W; ++x)
        pxa_canvas_line(&frame, (int16_t)(BOARD_X + x * CELL), BOARD_Y,
                        (int16_t)(BOARD_X + x * CELL), (int16_t)(BOARD_Y + BOARD_H * CELL),
                        0x223149, 1);
    for (int y = 0; y < BOARD_H; ++y)
        for (int x = 0; x < BOARD_W; ++x)
            if (board[y][x])
                render_cell(&frame, x, y, colors[board[y][x]]);
    if (!game_over) {
        const uint16_t mask = piece_mask(piece_type, piece_rotation);
        int8_t ghost_y = piece_y;
        while (!collides(piece_x, (int8_t)(ghost_y + 1), piece_rotation))
            ++ghost_y;
        if (ghost_y != piece_y) {
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                    if ((mask & (uint16_t)(1u << (y * 4 + x))) && ghost_y + y >= 0)
                        render_cell(&frame, piece_x + x, ghost_y + y, 0x405064);
        }
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                if ((mask & (uint16_t)(1u << (y * 4 + x))) && piece_y + y >= 0)
                    render_cell(&frame, piece_x + x, piece_y + y, colors[piece_type + 1]);
    }
    pxa_canvas_image(&frame, 40, 6, 216, 24, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     hud_panel_asset, sizeof(hud_panel_asset) - 1);
    const char *score_label = PXA_ARCADE_MSG(PXA_MSG_HUD_SCORE);
    pxa_canvas_text_box(&frame, 48, 6, 40, 24, 0x7FA6C8, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_label,
                        pxa_arcade_text_size(score_label));
    pxa_canvas_text_box(&frame, 91, 6, 64, 24, 0xFFFFFF, PXA_CANVAS_ALIGN_LEFT,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, score_text, score_length);
    const char *lines_label = PXA_ARCADE_MSG(PXA_MSG_TETRIS_HUD_LINES);
    pxa_canvas_text_box(&frame, 161, 6, 40, 24, 0x7FA6C8, PXA_CANVAS_ALIGN_CENTER,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, lines_label,
                        pxa_arcade_text_size(lines_label));
    pxa_canvas_text_box(&frame, 204, 6, 44, 24, 0xFFFFFF, PXA_CANVAS_ALIGN_LEFT,
                        PXA_CANVAS_TEXT_ALIGN_MIDDLE, lines_text, lines_length);
    const char *level_label = PXA_ARCADE_MSG(PXA_MSG_HUD_LEVEL);
    pxa_canvas_text(&frame, 14, 66, 64, 0x7FA6C8, PXA_CANVAS_ALIGN_CENTER, level_label,
                  pxa_arcade_text_size(level_label));
    pxa_canvas_text(&frame, 14, 88, 64, 0xFFFFFF, PXA_CANVAS_ALIGN_CENTER, level_text, level_length);
    const char *next_label = PXA_ARCADE_MSG(PXA_MSG_TETRIS_HUD_NEXT);
    pxa_canvas_text(&frame, 216, 66, 64, 0x7FA6C8, PXA_CANVAS_ALIGN_CENTER, next_label,
                  pxa_arcade_text_size(next_label));
    const uint16_t next_mask = piece_mask(next_piece_type, 0);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            if (next_mask & (uint16_t)(1u << (y * 4 + x)))
                pxa_canvas_rect(&frame, (int16_t)(232 + x * 9), (int16_t)(90 + y * 9), 8, 8,
                              colors[next_piece_type + 1], 2);
        }
    draw_move_button(&frame, MOVE_LEFT_X, 0);
    draw_move_button(&frame, MOVE_RIGHT_X, 1);
    draw_rotate_button(&frame);
    draw_drop_button(&frame);
    if (game_over) {
        const char *over = PXA_ARCADE_MSG(PXA_MSG_TETRIS_HINT_RESTART);
        pxa_canvas_image(&frame, 38, 89, 220, 52, 255, PXA_UI_IMAGE_FIT_STRETCH,
                         dialog_panel_asset, sizeof(dialog_panel_asset) - 1);
        pxa_canvas_text(&frame, 60, 106, 176, 0xFFFFFF,
                        PXA_CANVAS_ALIGN_CENTER, over,
                        pxa_arcade_text_size(over));
    }
    return pxa_arcade_present(GAME_NODE, &frame, &pxa_arcade_ui_generation, &initialized, ui_commands,
                            sizeof(ui_commands), packet, sizeof(packet));
}

static void drop_one(void) {
    if (!collides(piece_x, (int8_t)(piece_y + 1), piece_rotation))
        ++piece_y;
    else
        lock_piece();
}

static void rotate_piece(void) {
    static const int8_t kicks[] = {0, -1, 1, -2, 2};
    const uint8_t next = (uint8_t)((piece_rotation + 1) & 3u);
    for (size_t i = 0; i < sizeof(kicks) / sizeof(kicks[0]); ++i) {
        const int8_t kicked_x = (int8_t)(piece_x + kicks[i]);
        if (!collides(kicked_x, piece_y, next)) {
            piece_x = kicked_x;
            piece_rotation = next;
            return;
        }
    }
}

static int move_piece(int8_t direction) {
    const int8_t next_x = (int8_t)(piece_x + direction);
    if (collides(next_x, piece_y, piece_rotation))
        return 0;
    piece_x = next_x;
    return 1;
}

static void hard_drop(void) {
    while (!collides(piece_x, (int8_t)(piece_y + 1), piece_rotation)) {
        ++piece_y;
        score += 2;
    }
    lock_piece();
}

static int in_button(int16_t x, int16_t y, int16_t button_x, int16_t button_y,
                     uint16_t width, uint16_t height) {
    return x >= button_x && x < button_x + width &&
           y >= button_y && y < button_y + height;
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
    pxa_game_sfx_set_theme(&sfx, PXA_GAME_SFX_THEME_TETRIS);
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
            &last_tick_us, &parsed, GAME_TICK_MS, GAME_MAX_CATCHUP_STEPS);
        random_state ^= (uint32_t)pxa_read_u64(parsed.payload);
        if (steps == 0 || game_over)
            return PXA_EVENT_HANDLED;
        const uint32_t level = 1u + lines_cleared / 10u;
        const uint8_t drop_period = level >= 8 ? 3 : (uint8_t)(10 - level + 1u);
        for (uint8_t step = 0; step < steps && !game_over; ++step) {
            if (held_move_direction != 0) {
                if (held_move_repeat_delay != 0)
                    --held_move_repeat_delay;
                else
                    (void)move_piece(held_move_direction);
            }
            if (++drop_counter >= drop_period) {
                drop_counter = 0;
                drop_one();
            }
        }
        if (!render())
            return PXA_STATUS_INTERNAL;
        return PXA_EVENT_HANDLED;
    } else if (pxa_canvas_parse_pointer(&parsed, GAME_NODE, &pointer)) {
        const int16_t x = (int16_t)pointer.x;
        const int16_t y = (int16_t)pointer.y;
        if (game_over && pointer.phase == PXA_POINTER_DOWN) {
            reset_game();
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
            if (!render())
                return PXA_STATUS_INTERNAL;
        } else if (pointer.phase == PXA_POINTER_DOWN) {
            int changed = 0;
            uint8_t feedback = PXA_GAME_SFX_TAP;
            held_move_direction = 0;
            held_move_repeat_delay = 0;
            if (in_button(x, y, MOVE_LEFT_X, MOVE_Y, MOVE_WIDTH, MOVE_HEIGHT)) {
                changed = move_piece(-1);
                held_move_direction = -1;
                held_move_repeat_delay = MOVE_REPEAT_DELAY_TICKS;
            } else if (in_button(x, y, MOVE_RIGHT_X, MOVE_Y, MOVE_WIDTH, MOVE_HEIGHT)) {
                changed = move_piece(1);
                held_move_direction = 1;
                held_move_repeat_delay = MOVE_REPEAT_DELAY_TICKS;
            } else if (in_button(x, y, ROTATE_X, ACTION_Y, ACTION_WIDTH, ACTION_HEIGHT)) {
                rotate_piece();
                changed = 1;
                feedback = PXA_GAME_SFX_ACTION;
            } else if (in_button(x, y, DROP_X, ACTION_Y, ACTION_WIDTH, ACTION_HEIGHT)) {
                hard_drop();
                changed = 1;
                feedback = PXA_GAME_SFX_ACTION;
            }
            if (changed) {
                pxa_game_sfx_play(&sfx, feedback);
                if (!render())
                    return PXA_STATUS_INTERNAL;
            }
        } else if (pointer.phase == PXA_POINTER_UP ||
                   pointer.phase == PXA_POINTER_CANCEL) {
            held_move_direction = 0;
            held_move_repeat_delay = 0;
        }
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
