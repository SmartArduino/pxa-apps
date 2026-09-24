#define PXA_ARCADE_MODULE_PREFIX pxa_arcade_minesweeper_
#include "pxa_arcade_module.h"

#include "pxa_canvas.h"
#include "pxa_game_sfx.h"

#include "minesweeper_music.inc"

#define GAME_NODE 2u
#define BOARD_COLS 12
#define BOARD_ROWS 9
#define MINE_COUNT 18
#define CELL_SIZE 32
#define MAP_X 12
#define MAP_Y 32
#define MAP_VIEW_WIDTH 272
#define MAP_VIEW_HEIGHT 132
#define BOARD_WIDTH (BOARD_COLS * CELL_SIZE)
#define BOARD_HEIGHT (BOARD_ROWS * CELL_SIZE)
#define TOOL_Y 172
#define TOOL_HEIGHT 34
#define FLAG_BUTTON_X 12
#define FLAG_BUTTON_WIDTH 132
#define RESTART_BUTTON_X 152
#define RESTART_BUTTON_WIDTH 132
#define GAME_TICK_MS 25u
#define DRAG_THRESHOLD 5

#define CELL_MINE 0x01u
#define CELL_FLAGGED 0x02u
#define CELL_REVEALED 0x04u

#define STATE_PLAYING 0u
#define STATE_WON 1u
#define STATE_LOST 2u

static uint8_t draw_data[16 * 1024];
static uint8_t ui_commands[4096];
static uint8_t packet[512];
static uint8_t board[BOARD_ROWS][BOARD_COLS];
static uint32_t random_state = UINT32_C(0x6c8e9cf5);
static uint8_t initialized;
static uint8_t mines_placed;
static uint8_t state;
static uint8_t flag_mode;
static uint8_t flags_used;
static uint8_t safe_revealed;
static int16_t map_scroll_x;
static int16_t map_scroll_y;
static int16_t touch_start_x;
static int16_t touch_start_y;
static int16_t touch_scroll_x;
static int16_t touch_scroll_y;
static uint8_t touch_active;
static uint8_t drag_active;
static pxa_game_sfx_t sfx;
static const char flag_asset[] = "assets/minesweeper-music/flag.png";
static const char mine_asset[] = "assets/minesweeper-music/mine.png";
static const char background_asset[] = "assets/minesweeper-music/background.png";
static const char hud_panel_asset[] = "assets/ui/hud-panel.png";
static const char dialog_panel_asset[] = "assets/ui/dialog-panel.png";
static const char button_panel_asset[] = "assets/ui/button-panel.png";

static uint32_t random_next(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static int inside_board(int row, int col) {
    return row >= 0 && row < BOARD_ROWS && col >= 0 && col < BOARD_COLS;
}

static uint8_t neighbor_mines(int row, int col) {
    uint8_t count = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx != 0 || dy != 0) && inside_board(row + dy, col + dx) &&
                (board[row + dy][col + dx] & CELL_MINE) != 0)
                ++count;
    return count;
}

static void reset_game(void) {
    for (int row = 0; row < BOARD_ROWS; ++row)
        for (int col = 0; col < BOARD_COLS; ++col)
            board[row][col] = 0;
    mines_placed = 0;
    state = STATE_PLAYING;
    flag_mode = 0;
    flags_used = 0;
    safe_revealed = 0;
    map_scroll_x = 0;
    map_scroll_y = 0;
    touch_active = 0;
    drag_active = 0;
}

static void place_mines(int safe_row, int safe_col) {
    uint8_t placed = 0;
    while (placed < MINE_COUNT) {
        const int row = (int)(random_next() % BOARD_ROWS);
        const int col = (int)(random_next() % BOARD_COLS);
        if ((board[row][col] & CELL_MINE) != 0 ||
            (row >= safe_row - 1 && row <= safe_row + 1 &&
             col >= safe_col - 1 && col <= safe_col + 1))
            continue;
        board[row][col] |= CELL_MINE;
        ++placed;
    }
    mines_placed = 1;
}

static void reveal_all_mines(void) {
    for (int row = 0; row < BOARD_ROWS; ++row)
        for (int col = 0; col < BOARD_COLS; ++col)
            if ((board[row][col] & CELL_MINE) != 0)
                board[row][col] |= CELL_REVEALED;
}

static void reveal_safe_area(int start_row, int start_col) {
    uint8_t queue_rows[BOARD_ROWS * BOARD_COLS];
    uint8_t queue_cols[BOARD_ROWS * BOARD_COLS];
    uint8_t head = 0;
    uint8_t tail = 0;
    queue_rows[tail] = (uint8_t)start_row;
    queue_cols[tail++] = (uint8_t)start_col;
    while (head < tail) {
        const int row = queue_rows[head];
        const int col = queue_cols[head++];
        uint8_t* cell = &board[row][col];
        if ((*cell & (CELL_REVEALED | CELL_FLAGGED | CELL_MINE)) != 0)
            continue;
        *cell |= CELL_REVEALED;
        ++safe_revealed;
        if (neighbor_mines(row, col) != 0)
            continue;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int next_row = row + dy;
                const int next_col = col + dx;
                if (inside_board(next_row, next_col) &&
                    (board[next_row][next_col] &
                     (CELL_REVEALED | CELL_FLAGGED | CELL_MINE)) == 0 &&
                    tail < BOARD_ROWS * BOARD_COLS) {
                    queue_rows[tail] = (uint8_t)next_row;
                    queue_cols[tail++] = (uint8_t)next_col;
                }
            }
    }
}

static uint32_t number_text_color(uint8_t number) {
    static const uint32_t colors[] = {
        0x000000, 0x59B9FF, 0x68D391, 0xF4CF5D, 0xF08080,
        0xB995F5, 0x54D6CB, 0xD2A679, 0xF3F6FF,
    };
    return colors[number <= 8 ? number : 8];
}

static void draw_flag(pxa_canvas_frame_t* frame, int16_t x, int16_t y) {
    pxa_canvas_image(frame, x, y, CELL_SIZE, CELL_SIZE, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     flag_asset, sizeof(flag_asset) - 1);
}

static void draw_mine(pxa_canvas_frame_t* frame, int16_t x, int16_t y) {
    pxa_canvas_image(frame, x, y, CELL_SIZE, CELL_SIZE, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     mine_asset, sizeof(mine_asset) - 1);
}

static void draw_cell(pxa_canvas_frame_t* frame, int row, int col,
                      int16_t x, int16_t y) {
    const uint8_t cell = board[row][col];
    if ((cell & CELL_REVEALED) == 0) {
        pxa_canvas_rect_rgba(frame, x + 1, y + 1, CELL_SIZE - 2, CELL_SIZE - 2,
                             pxa_canvas_rgba(0x2B4056), 3, 1,
                             pxa_canvas_rgba(0x496982));
        pxa_canvas_rect(frame, x + 4, y + 3, CELL_SIZE - 8, 2, 0x547995, 1);
        if ((cell & CELL_FLAGGED) != 0)
            draw_flag(frame, x, y);
        return;
    }
    pxa_canvas_rect(frame, x + 1, y + 1, CELL_SIZE - 2, CELL_SIZE - 2,
                    (cell & CELL_MINE) != 0 ? 0x432A38 : 0x1C2A3A, 2);
    if ((cell & CELL_MINE) != 0) {
        draw_mine(frame, x, y);
        return;
    }
    const uint8_t mines = neighbor_mines(row, col);
    if (mines != 0) {
        char text[2] = {(char)('0' + mines), '\0'};
        pxa_canvas_text_role(frame, x, y + 4, CELL_SIZE, pxa_canvas_rgba(number_text_color(mines)),
                             PXA_CANVAS_FONT_TITLE, PXA_CANVAS_ALIGN_CENTER, text, 1);
    }
}

static int16_t clamped_scroll(int value, int maximum) {
    if (value < 0)
        return 0;
    if (value > maximum)
        return (int16_t)maximum;
    return (int16_t)value;
}

static int update_map_scroll(int value_x, int value_y) {
    const int16_t next_x = clamped_scroll(value_x, BOARD_WIDTH - MAP_VIEW_WIDTH);
    const int16_t next_y = clamped_scroll(value_y, BOARD_HEIGHT - MAP_VIEW_HEIGHT);
    if (next_x == map_scroll_x && next_y == map_scroll_y)
        return 0;
    map_scroll_x = next_x;
    map_scroll_y = next_y;
    return 1;
}

static int render(void) {
    pxa_canvas_frame_t frame;
    char mines_text[4];
    const uint8_t mines_left = flags_used <= MINE_COUNT ? (uint8_t)(MINE_COUNT - flags_used) : 0;
    const size_t mines_length = pxa_canvas_u32_text(mines_text, mines_left);
    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));
    pxa_canvas_image(&frame, 0, 0, 296, 240, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     background_asset, sizeof(background_asset) - 1);
    pxa_canvas_image(&frame, 16, 5, 264, 24, 255, PXA_UI_IMAGE_FIT_STRETCH,
                     hud_panel_asset, sizeof(hud_panel_asset) - 1);
    pxa_canvas_circle(&frame, 31, 18, 6, 0xF46A7B);
    const char *title = PXA_ARCADE_MSG(PXA_MSG_MINES_TITLE);
    const char *remaining = PXA_ARCADE_MSG(PXA_MSG_MINES_REMAINING);
    const char *mode = flag_mode
        ? PXA_ARCADE_MSG(PXA_MSG_MINES_MODE_FLAG)
        : PXA_ARCADE_MSG(PXA_MSG_MINES_MODE_REVEAL);
    const char *restart = PXA_ARCADE_MSG(PXA_MSG_MINES_ACTION_RESTART);
    pxa_canvas_text_box_role(&frame, 43, 5, 72, 24, pxa_canvas_rgba(0xF4F8FC),
                             PXA_CANVAS_FONT_BODY, PXA_CANVAS_ALIGN_LEFT,
                             PXA_CANVAS_TEXT_ALIGN_MIDDLE, title,
                             pxa_arcade_text_size(title));
    pxa_canvas_text_box_role(&frame, 116, 5, 92, 24, pxa_canvas_rgba(0x8FB3CC),
                             PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_RIGHT,
                             PXA_CANVAS_TEXT_ALIGN_MIDDLE, remaining,
                             pxa_arcade_text_size(remaining));
    pxa_canvas_text_box_role(&frame, 222, 5, 50, 24, pxa_canvas_rgba(0xFFFFFF),
                             PXA_CANVAS_FONT_BODY, PXA_CANVAS_ALIGN_LEFT,
                             PXA_CANVAS_TEXT_ALIGN_MIDDLE, mines_text, mines_length);
    pxa_canvas_rect_rgba(&frame, MAP_X - 2, MAP_Y - 2, MAP_VIEW_WIDTH + 4, MAP_VIEW_HEIGHT + 4,
                         pxa_canvas_rgba(0x273A4D), 5, 1, pxa_canvas_rgba(0x4E7491));
    pxa_canvas_clip_push(&frame, MAP_X, MAP_Y, MAP_VIEW_WIDTH, MAP_VIEW_HEIGHT);
    for (int row = 0; row < BOARD_ROWS; ++row) {
        const int16_t y = (int16_t)(MAP_Y + row * CELL_SIZE - map_scroll_y);
        if (y + CELL_SIZE <= MAP_Y || y >= MAP_Y + MAP_VIEW_HEIGHT)
            continue;
        for (int col = 0; col < BOARD_COLS; ++col) {
            const int16_t x = (int16_t)(MAP_X + col * CELL_SIZE - map_scroll_x);
            if (x + CELL_SIZE > MAP_X && x < MAP_X + MAP_VIEW_WIDTH)
                draw_cell(&frame, row, col, x, y);
        }
    }
    pxa_canvas_clip_pop(&frame);
    pxa_canvas_rect(&frame, (int16_t)(MAP_X + map_scroll_x * MAP_VIEW_WIDTH / BOARD_WIDTH),
                    (int16_t)(MAP_Y + MAP_VIEW_HEIGHT - 2),
                    MAP_VIEW_WIDTH * MAP_VIEW_WIDTH / BOARD_WIDTH, 2, 0x6DB4CE, 1);
    pxa_canvas_rect(&frame, (int16_t)(MAP_X + MAP_VIEW_WIDTH - 2),
                    (int16_t)(MAP_Y + map_scroll_y * MAP_VIEW_HEIGHT / BOARD_HEIGHT), 2,
                    MAP_VIEW_HEIGHT * MAP_VIEW_HEIGHT / BOARD_HEIGHT, 0x6DB4CE, 1);

    pxa_canvas_image(&frame, FLAG_BUTTON_X, TOOL_Y, FLAG_BUTTON_WIDTH, TOOL_HEIGHT, 255,
                     PXA_UI_IMAGE_FIT_STRETCH, button_panel_asset,
                     sizeof(button_panel_asset) - 1);
    pxa_canvas_text_role(&frame, FLAG_BUTTON_X, TOOL_Y + 4, FLAG_BUTTON_WIDTH,
                         pxa_canvas_rgba(flag_mode ? 0xFFF4D6 : 0xD8EBFC),
                         PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER,
                         mode, pxa_arcade_text_size(mode));
    pxa_canvas_image(&frame, RESTART_BUTTON_X, TOOL_Y, RESTART_BUTTON_WIDTH, TOOL_HEIGHT, 255,
                     PXA_UI_IMAGE_FIT_STRETCH, button_panel_asset,
                     sizeof(button_panel_asset) - 1);
    pxa_canvas_text_role(&frame, RESTART_BUTTON_X, TOOL_Y + 4, RESTART_BUTTON_WIDTH,
                         pxa_canvas_rgba(0xE5F8FF), PXA_CANVAS_FONT_CAPTION,
                         PXA_CANVAS_ALIGN_CENTER, restart,
                         pxa_arcade_text_size(restart));
    if (state != STATE_PLAYING) {
        const char *message = state == STATE_WON
            ? PXA_ARCADE_MSG(PXA_MSG_MINES_RESULT_CLEAR)
            : PXA_ARCADE_MSG(PXA_MSG_MINES_RESULT_TRIGGERED);
        const char *tap_restart = PXA_ARCADE_MSG(PXA_MSG_MINES_HINT_RESTART);
        pxa_canvas_image(&frame, 38, 84, 220, 52, 255, PXA_UI_IMAGE_FIT_STRETCH,
                         dialog_panel_asset, sizeof(dialog_panel_asset) - 1);
        pxa_canvas_text_role(&frame, 58, 95, 180, pxa_canvas_rgba(0xFFFFFF),
                             PXA_CANVAS_FONT_BODY, PXA_CANVAS_ALIGN_CENTER,
                             message, pxa_arcade_text_size(message));
        pxa_canvas_text_role(&frame, 58, 116, 180, pxa_canvas_rgba(0xD8E8F3),
                             PXA_CANVAS_FONT_CAPTION, PXA_CANVAS_ALIGN_CENTER,
                             tap_restart, pxa_arcade_text_size(tap_restart));
    }
    return pxa_arcade_present(GAME_NODE, &frame, &pxa_arcade_ui_generation, &initialized,
                            ui_commands, sizeof(ui_commands), packet, sizeof(packet));
}

static int handle_board_tap(int row, int col) {
    uint8_t* cell = &board[row][col];
    if (state != STATE_PLAYING || (flag_mode == 0 && (*cell & CELL_FLAGGED) != 0))
        return 0;
    if (flag_mode) {
        if ((*cell & CELL_REVEALED) != 0)
            return 0;
        if ((*cell & CELL_FLAGGED) != 0) {
            *cell &= (uint8_t)~CELL_FLAGGED;
            --flags_used;
        } else if (flags_used < MINE_COUNT) {
            *cell |= CELL_FLAGGED;
            ++flags_used;
        } else {
            return 0;
        }
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_TAP);
        return 1;
    }
    if (!mines_placed)
        place_mines(row, col);
    if ((*cell & CELL_MINE) != 0) {
        *cell |= CELL_REVEALED;
        reveal_all_mines();
        state = STATE_LOST;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_EXPLODE);
        return 1;
    }
    reveal_safe_area(row, col);
    if (safe_revealed == BOARD_ROWS * BOARD_COLS - MINE_COUNT) {
        state = STATE_WON;
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_WIN);
    } else {
        pxa_game_sfx_play(&sfx, PXA_GAME_SFX_PLACE);
    }
    return 1;
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
    pxa_game_sfx_set_song(&sfx, &minesweeper_gymnopedie_song);
    pxa_game_sfx_start(&sfx, packet, sizeof(packet));
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t* event, uint32_t length) {
    pxa_canvas_event_t parsed;
    pxa_ui_pointer_data_t pointer;
    uint8_t changed = 0;
    if (!pxa_canvas_parse_event(event, length, &parsed))
        return PXA_EVENT_UNHANDLED;
    if (parsed.service == PXA_SERVICE_SYSTEM &&
        parsed.opcode == PXA_SYSTEM_CONFIGURATION_EVENT)
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    if (pxa_game_sfx_handle_event(&sfx, &parsed, packet, sizeof(packet)))
        return PXA_EVENT_HANDLED;
    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK &&
        parsed.payload_length == 8) {
        random_state ^= (uint32_t)pxa_read_u64(parsed.payload);
        pxa_game_sfx_tick(&sfx, &parsed);
        return PXA_EVENT_HANDLED;
    }
    if (!pxa_canvas_parse_pointer(&parsed, GAME_NODE, &pointer))
        return PXA_EVENT_UNHANDLED;
    const int16_t x = (int16_t)pointer.x;
    const int16_t y = (int16_t)pointer.y;
    if (state != STATE_PLAYING) {
        if (pointer.phase == PXA_POINTER_DOWN) {
            reset_game();
            pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
            changed = 1;
        }
        return changed && !render() ? PXA_STATUS_INTERNAL : PXA_EVENT_HANDLED;
    }
    if (pointer.phase == PXA_POINTER_DOWN) {
        touch_active = 0;
        drag_active = 0;
        if (y >= TOOL_Y && y < TOOL_Y + TOOL_HEIGHT) {
            if (x >= FLAG_BUTTON_X && x < FLAG_BUTTON_X + FLAG_BUTTON_WIDTH) {
                flag_mode = (uint8_t)!flag_mode;
                pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
                changed = 1;
            } else if (x >= RESTART_BUTTON_X && x < RESTART_BUTTON_X + RESTART_BUTTON_WIDTH) {
                reset_game();
                pxa_game_sfx_play(&sfx, PXA_GAME_SFX_ACTION);
                changed = 1;
            }
        } else if (x >= MAP_X && x < MAP_X + MAP_VIEW_WIDTH &&
                   y >= MAP_Y && y < MAP_Y + MAP_VIEW_HEIGHT) {
            touch_active = 1;
            touch_start_x = x;
            touch_start_y = y;
            touch_scroll_x = map_scroll_x;
            touch_scroll_y = map_scroll_y;
        }
    } else if (pointer.phase == PXA_POINTER_MOVE && touch_active) {
        const int delta_x = x - touch_start_x;
        const int delta_y = y - touch_start_y;
        if (!drag_active && (delta_x > DRAG_THRESHOLD || delta_x < -DRAG_THRESHOLD ||
                             delta_y > DRAG_THRESHOLD || delta_y < -DRAG_THRESHOLD))
            drag_active = 1;
        if (drag_active)
            changed = (uint8_t)update_map_scroll(touch_scroll_x - delta_x,
                                                  touch_scroll_y - delta_y);
    } else if (pointer.phase == PXA_POINTER_UP && touch_active) {
        if (!drag_active && x >= MAP_X && x < MAP_X + MAP_VIEW_WIDTH &&
            y >= MAP_Y && y < MAP_Y + MAP_VIEW_HEIGHT) {
            const int map_x = x - MAP_X + map_scroll_x;
            const int map_y = y - MAP_Y + map_scroll_y;
            changed = (uint8_t)handle_board_tap(map_y / CELL_SIZE, map_x / CELL_SIZE);
        }
        touch_active = 0;
        drag_active = 0;
    } else if (pointer.phase == PXA_POINTER_CANCEL) {
        touch_active = 0;
        drag_active = 0;
    }
    if (!changed)
        return PXA_EVENT_HANDLED;
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
