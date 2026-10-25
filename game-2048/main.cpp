// SPDX-License-Identifier: MIT
#include "../common/pocket-game/runtime.hpp"
#include "../common/pocket-game/music.hpp"
#include "core.h"
#include <stdio.h>
#include <string.h>

namespace {
struct State {
    uint8_t board[4][4], undo_board[4][4];
    uint32_t score, best, moves, undo_score, undo_moves;
    bool undo;
};
State state{};
using pocket_game_sfx::Cue;
using pocket_game_sfx::cue;
constexpr int GRID_X = 4, GRID_Y = 38, GRID_SIZE = 180, TILE = 40;
bool dragging = false;
int pointer_x, pointer_y;
uint32_t flash_ms;
int spawned_x = -1, spawned_y = -1;

void changed() { pocket::redraw(); pocket::save(); }
void prepare_board() {
    initBoard(state.board);
    state.score = state.moves = 0;
    state.undo = false;
    dragging = false;
    spawned_x = spawned_y = -1;
    flash_ms = 0;
}
void undo() {
    if (!state.undo) return;
    memcpy(state.board, state.undo_board, sizeof(state.board));
    state.score = state.undo_score;
    state.moves = state.undo_moves;
    state.undo = false;
    flash_ms = 0;
    cue(Cue::Undo);
    changed();
}
void move(pocket::Key key) {
    uint8_t previous[4][4];
    memcpy(previous, state.board, sizeof(previous));
    const uint32_t previous_score = state.score;
    bool moved = false;
    switch (key) {
    case pocket::Key::UP: moved = moveUp(state.board, &state.score); break;
    case pocket::Key::RIGHT: moved = moveRight(state.board, &state.score); break;
    case pocket::Key::DOWN: moved = moveDown(state.board, &state.score); break;
    case pocket::Key::LEFT: moved = moveLeft(state.board, &state.score); break;
    default: return;
    }
    if (!moved) { cue(Cue::Blocked); return; }
    bool was_won = false, now_won = false;
    for (int x = 0; x < 4; ++x) for (int y = 0; y < 4; ++y) {
        was_won = was_won || previous[x][y] >= 11;
        now_won = now_won || state.board[x][y] >= 11;
    }
    cue(now_won && !was_won ? Cue::Goal : state.score != previous_score ? Cue::Merge : Cue::Move);
    memcpy(state.undo_board, previous, sizeof(previous));
    state.undo_score = previous_score;
    state.undo_moves = state.moves++;
    state.undo = true;
    if (state.score > state.best) state.best = state.score;
    memcpy(previous, state.board, sizeof(previous));
    addRandom(state.board);
    for (int x = 0; x < 4; ++x) for (int y = 0; y < 4; ++y)
        if (state.board[x][y] != previous[x][y]) { spawned_x = x; spawned_y = y; }
    flash_ms = 220;
    changed();
}
bool in_grid(int x, int y) {
    return x >= GRID_X && x < GRID_X + GRID_SIZE && y >= GRID_Y && y < GRID_Y + GRID_SIZE;
}
bool in_button(int x, int y, int top) { return x >= 192 && x < 284 && y >= top && y < top + 30; }
void center(int x, int y, int w, const char *value, uint16_t color, int scale = 1) {
    pocket::text(x + (w - pocket::text_width(value, scale)) / 2, y, value, color, scale);
}
void number(int x, int y, uint32_t value, uint16_t color) {
    char text[16];
    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
    center(x, y, 92, text, color);
}
uint16_t tile_color(unsigned exponent) {
    constexpr uint16_t colors[] = {
        pocket::rgb(56, 65, 79), pocket::rgb(238, 228, 218), pocket::rgb(237, 224, 200),
        pocket::rgb(242, 177, 121), pocket::rgb(245, 149, 99), pocket::rgb(246, 124, 95),
        pocket::rgb(246, 94, 59), pocket::rgb(237, 207, 114), pocket::rgb(237, 204, 97),
        pocket::rgb(237, 200, 80), pocket::rgb(237, 197, 63), pocket::rgb(237, 194, 46)
    };
    return exponent < sizeof(colors) / sizeof(colors[0]) ? colors[exponent] : pocket::rgb(153, 104, 206);
}
void put32(uint8_t *p, uint32_t value) { for (int i = 0; i < 4; ++i) p[i] = uint8_t(value >> (i * 8)); }
uint32_t get32(const uint8_t *p) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value |= uint32_t(p[i]) << (i * 8);
    return value;
}
uint32_t checksum(const uint8_t *p, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}
bool valid_board(const uint8_t *p) {
    bool nonempty = false;
    for (int i = 0; i < 16; ++i) { if (p[i] > 31) return false; nonempty |= p[i] != 0; }
    return nonempty;
}
}

namespace game {
const char *title() { return "2048"; }
const char *app_id() { return "pxa-game-2048"; }
void init() {
    state = {};
    game2048_set_random(pocket::random);
    prepare_board();
    pocket::save();
}
void reset() { prepare_board(); cue(Cue::Reset); changed(); }
void key(pocket::Key key) {
    switch (key) {
    case pocket::Key::B: undo(); break;
    case pocket::Key::X: reset(); break;
    default: move(key); break;
    }
}
void pointer(pocket::Phase phase, int x, int y) {
    if (phase == pocket::Phase::DOWN) {
        dragging = true; pointer_x = x; pointer_y = y;
    } else if (phase == pocket::Phase::CANCEL) {
        dragging = false;
    } else if (phase == pocket::Phase::UP && dragging) {
        dragging = false;
        const int dx = x - pointer_x, dy = y - pointer_y;
        const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        if (in_grid(pointer_x, pointer_y) && (ax >= 14 || ay >= 14)) {
            move(ax > ay ? (dx > 0 ? pocket::Key::RIGHT : pocket::Key::LEFT)
                         : (dy > 0 ? pocket::Key::DOWN : pocket::Key::UP));
        } else if (in_button(pointer_x, pointer_y, 142) && in_button(x, y, 142)) {
            undo();
        } else if (in_button(pointer_x, pointer_y, 182) && in_button(x, y, 182)) {
            reset();
        }
    }
}
void tick(uint32_t elapsed_ms) {
    if (!flash_ms) return;
    flash_ms = elapsed_ms >= flash_ms ? 0 : flash_ms - elapsed_ms;
    if (!flash_ms) pocket::redraw();
}
void draw() {
    using namespace pocket;
    clear();
    text(8, 8, "2048", WARN, 3);
    text(111, 9, tr("SWIPE / D-PAD", "滑动 / 方向键"), MUTED);
    text(111, 22, tr("START: PAUSE", "START 暂停"), MUTED);
    rect(GRID_X, GRID_Y, GRID_SIZE, GRID_SIZE, PANEL);
    bool won = false;
    for (int x = 0; x < 4; ++x) for (int y = 0; y < 4; ++y) {
        const int tx = GRID_X + 4 + 44 * x, ty = GRID_Y + 4 + 44 * y;
        const unsigned exponent = state.board[x][y];
        rect(tx, ty, TILE, TILE, tile_color(exponent));
        if (!exponent) continue;
        char value[16];
        if (exponent <= 16) snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(uint32_t(1) << exponent));
        else snprintf(value, sizeof(value), "2^%u", exponent);
        const int scale = text_width(value, 2) <= TILE - 4 ? 2 : 1;
        const int label_y = ty + (TILE - 7 * scale) / 2;
        const uint16_t label_color = exponent <= 2 ? PANEL : BG;
        center(tx, label_y, TILE, value, label_color, scale);
        if (exponent > 16) {
            const int caret_x = tx + (TILE - text_width(value, scale)) / 2 + 6;
            line(caret_x, label_y + 3, caret_x + 2, label_y + 1, label_color);
            line(caret_x + 2, label_y + 1, caret_x + 4, label_y + 3, label_color);
        }
        if (flash_ms && x == spawned_x && y == spawned_y) outline(tx, ty, TILE, TILE, INK);
        won |= exponent >= 11;
    }
    center(192, 42, 92, tr("SCORE", "得分"), MUTED);
    number(192, 56, state.score, INK);
    center(192, 76, 92, tr("BEST", "最高分"), MUTED);
    number(192, 90, state.best, WARN);
    if (gameEnded(state.board)) {
        center(192, 112, 92, tr("GAME OVER", "游戏结束"), BAD);
        center(192, 125, 92, state.undo ? tr("UNDO OR NEW", "撤销或新局") : tr("TRY AGAIN", "再试一次"), MUTED);
    } else {
        center(192, 113, 92, won ? "2048!" : tr("MERGE TILES", "合并方块"), won ? GOOD : MUTED);
    }
    rect(192, 142, 92, 30, PANEL);
    center(192, 152, 92, tr("B UNDO", "B 撤销"), state.undo ? ACCENT : MUTED);
    rect(192, 182, 92, 30, PANEL);
    center(192, 192, 92, tr("X NEW", "X 新局"), INK);
}
size_t serialize(uint8_t *data, size_t capacity) {
    if (capacity < 64) return 0;
    memset(data, 0, 64);
    memcpy(data, "P204", 4); data[4] = 1; data[5] = state.undo;
    put32(data + 8, state.score); put32(data + 12, state.best); put32(data + 16, state.moves);
    memcpy(data + 20, state.board, 16);
    put32(data + 36, state.undo_score); put32(data + 40, state.undo_moves);
    memcpy(data + 44, state.undo_board, 16);
    put32(data + 60, checksum(data, 60));
    return 64;
}
void load(const uint8_t *data, size_t size) {
    if (size != 64 || memcmp(data, "P204", 4) || data[4] != 1 || data[5] > 1 || data[6] || data[7] ||
        get32(data + 60) != checksum(data, 60) || !valid_board(data + 20) ||
        (data[5] && !valid_board(data + 44)) || get32(data + 12) < get32(data + 8)) return;
    State restored{};
    restored.score = get32(data + 8); restored.best = get32(data + 12); restored.moves = get32(data + 16);
    memcpy(restored.board, data + 20, 16);
    restored.undo_score = get32(data + 36); restored.undo_moves = get32(data + 40);
    memcpy(restored.undo_board, data + 44, 16); restored.undo = data[5];
    state = restored;
    flash_ms = 0; dragging = false;
    pocket::redraw();
}
}
