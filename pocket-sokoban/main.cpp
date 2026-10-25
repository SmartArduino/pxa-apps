// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-game/runtime.hpp"
#include "../common/pocket-game/music.hpp"
#include "core.hpp"
#include <stdio.h>

namespace {
sokoban::State state{};
using pocket_game_sfx::Cue;
using pocket_game_sfx::cue;
constexpr int GRID_X = 4, GRID_Y = 40, TILE = 18, SIDE = 190, SIDE_W = 94;
bool dragging = false;
int press_x = 0, press_y = 0;
uint32_t blocked_ms = 0;
void changed() { pocket::redraw(); pocket::save(); }
void restart() {
    cue(Cue::Reset);
    sokoban::reset_level(state, state.level);
    blocked_ms = 0; dragging = false;
    changed();
}
void step(sokoban::Direction direction) {
    const uint32_t pushes = state.pushes;
    if (sokoban::move(state, direction)) {
        cue(sokoban::won(state) ? Cue::Goal : state.pushes != pushes ? Cue::Push : Cue::Move);
        blocked_ms = 0; changed();
    } else if (!sokoban::won(state)) { cue(Cue::Blocked); blocked_ms = 320; pocket::redraw(); }
}
void undo() {
    if (sokoban::undo(state)) { cue(Cue::Undo); blocked_ms = 0; changed(); }
}
void choose(int delta) {
    if (sokoban::select(state, delta)) { cue(Cue::Select); blocked_ms = 0; changed(); }
}
void next() {
    if (!sokoban::won(state)) return;
    cue(Cue::Select);
    sokoban::reset_level(state, (unsigned(state.level) + 1) % sokoban::LEVEL_COUNT);
    blocked_ms = 0;
    changed();
}
bool in_grid(int x, int y) {
    return x >= GRID_X && x < GRID_X + TILE * 10 && y >= GRID_Y && y < GRID_Y + TILE * 10;
}
bool in_button(int x, int y, int top, int left = SIDE, int width = SIDE_W, int height = 26) {
    return x >= left && x < left + width && y >= top && y < top + height;
}
void center(int x, int y, int width, const char *text, uint16_t color = pocket::INK) {
    pocket::text(x + (width - pocket::text_width(text)) / 2, y, text, color);
}
void button(int x, int y, int width, int height, const char *label, uint16_t color) {
    pocket::rect(x, y, width, height, pocket::PANEL);
    center(x, y + (height - 10) / 2, width, label, color);
}
void stat(int top, const char *label, uint32_t value) {
    char text[16];
    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(value));
    pocket::text(SIDE + 2, top, label, pocket::MUTED);
    pocket::text(SIDE_W + SIDE - pocket::text_width(text) - 2, top, text, pocket::INK);
}
}
namespace game {
const char *title() { return pocket::tr("Pocket Sokoban", "推箱子"); }
const char *app_id() { return "pxa-pocket-sokoban"; }
void init() { sokoban::init(state); dragging = false; blocked_ms = 0; }
void reset() { restart(); }
void key(pocket::Key key) {
    switch (key) {
    case pocket::Key::UP: step(sokoban::UP); break;
    case pocket::Key::RIGHT: step(sokoban::RIGHT); break;
    case pocket::Key::DOWN: step(sokoban::DOWN); break;
    case pocket::Key::LEFT: step(sokoban::LEFT); break;
    case pocket::Key::B: undo(); break;
    case pocket::Key::X: restart(); break;
    case pocket::Key::L: choose(-1); break;
    case pocket::Key::R: case pocket::Key::Y: choose(1); break;
    case pocket::Key::A: next(); break;
    default: break;
    }
}
void pointer(pocket::Phase phase, int x, int y) {
    if (phase == pocket::Phase::DOWN) {
        dragging = true; press_x = x; press_y = y;
    } else if (phase == pocket::Phase::CANCEL) {
        dragging = false;
    } else if (phase == pocket::Phase::UP && dragging) {
        dragging = false;
        if (x < 0 || x >= pocket::WIDTH || y < 0 || y >= pocket::HEIGHT) return;
        const int dx = x - press_x, dy = y - press_y;
        const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        if (in_grid(press_x, press_y)) {
            if (ax >= 12 || ay >= 12) {
                step(ax > ay ? (dx > 0 ? sokoban::RIGHT : sokoban::LEFT)
                             : (dy > 0 ? sokoban::DOWN : sokoban::UP));
            } else if (in_grid(x, y)) {
                const int cx = (x - GRID_X) / TILE, cy = (y - GRID_Y) / TILE;
                const int px = state.player % 10, py = state.player / 10;
                if (cx == px && cy == py - 1) step(sokoban::UP);
                if (cx == px + 1 && cy == py) step(sokoban::RIGHT);
                if (cx == px && cy == py + 1) step(sokoban::DOWN);
                if (cx == px - 1 && cy == py) step(sokoban::LEFT);
            }
        } else if (in_button(press_x, press_y, 94) && in_button(x, y, 94)) undo();
        else if (in_button(press_x, press_y, 124) && in_button(x, y, 124)) restart();
        else if (in_button(press_x, press_y, 154, SIDE, 44) && in_button(x, y, 154, SIDE, 44)) choose(-1);
        else if (in_button(press_x, press_y, 154, SIDE + 48, 46) && in_button(x, y, 154, SIDE + 48, 46)) choose(1);
        else if (in_button(press_x, press_y, 184, SIDE, SIDE_W, 36) && in_button(x, y, 184, SIDE, SIDE_W, 36)) next();
    }
}
void tick(uint32_t elapsed_ms) {
    if (!blocked_ms) return;
    blocked_ms = elapsed_ms >= blocked_ms ? 0 : blocked_ms - elapsed_ms;
    if (!blocked_ms) pocket::redraw();
}
void draw() {
    using namespace pocket;
    clear();
    text(5, 6, tr("SOKOBAN", "推箱子"), ACCENT, 2);
    char label[32];
    snprintf(label, sizeof(label), tr("LEVEL %02u/%02u", "关卡 %02u/%02u"), unsigned(state.level) + 1, sokoban::LEVEL_COUNT);
    text(110, 7, label);
    snprintf(label, sizeof(label), tr("%u DONE / %u OPEN", "完成%u / 解锁%u"), sokoban::completed_count(state), unsigned(state.unlocked) + 1);
    text(110, 23, label, MUTED);
    text(5, 26, tr("SWIPE / D-PAD", "滑动 / 方向键"), MUTED);
    const uint16_t floor = rgb(34, 48, 65), wall = rgb(73, 91, 112);
    for (unsigned i = 0; i < sokoban::CELLS; ++i) {
        const int x = GRID_X + int(i % 10) * TILE, y = GRID_Y + int(i / 10) * TILE;
        if (state.lower[i] == sokoban::WALL) {
            rect(x, y, TILE - 1, TILE - 1, wall);
            line(x + 1, y + 1, x + TILE - 3, y + 1, rgb(105, 125, 148));
            line(x + 1, y + 2, x + 1, y + TILE - 3, rgb(105, 125, 148));
        } else {
            rect(x, y, TILE - 1, TILE - 1, floor);
            if (state.lower[i] == sokoban::GOAL) {
                outline(x + 5, y + 5, 7, 7, GOOD);
                rect(x + 7, y + 7, 3, 3, GOOD);
            }
        }
        if (state.upper[i]) {
            const bool goal = state.lower[i] == sokoban::GOAL;
            rect(x + 2, y + 2, 13, 13, goal ? GOOD : WARN);
            outline(x + 3, y + 3, 11, 11, goal ? rgb(22, 101, 52) : rgb(146, 64, 14));
            line(x + 4, y + 4, x + 12, y + 12, goal ? rgb(22, 101, 52) : rgb(146, 64, 14));
            line(x + 12, y + 4, x + 4, y + 12, goal ? rgb(22, 101, 52) : rgb(146, 64, 14));
        }
        if (state.player == i) {
            rect(x + 5, y + 2, 7, 7, INK);
            rect(x + 4, y + 9, 9, 6, ACCENT);
            rect(x + 6, y + 4, 1, 2, BG);
            rect(x + 10, y + 4, 1, 2, BG);
        }
    }
    stat(42, tr("MOVES", "步数"), state.steps);
    stat(57, tr("PUSH", "推箱"), state.pushes);
    const bool solved = sokoban::won(state);
    center(SIDE, 77, SIDE_W, solved ? tr("SOLVED!", "已通关") : blocked_ms ? tr("BLOCKED", "走不通") : tr("PUSH TO GOALS", "推箱到目标"), solved ? GOOD : blocked_ms ? BAD : MUTED);
    button(SIDE, 94, SIDE_W, 26, tr("B UNDO", "B 撤销"), state.history_count ? ACCENT : MUTED);
    button(SIDE, 124, SIDE_W, 26, tr("X RETRY", "X 重试"), INK);
    button(SIDE, 154, 44, 26, tr("< L", "L 上关"), state.unlocked ? INK : MUTED);
    button(SIDE + 48, 154, 46, 26, tr("R >", "R 下关"), state.unlocked ? INK : MUTED);
    button(SIDE, 184, SIDE_W, 36, solved ? (unsigned(state.level) + 1 == sokoban::LEVEL_COUNT ? tr("A REPLAY", "A 重玩") : tr("A NEXT", "A 下关")) : tr("FILL ALL GOALS", "填满全部目标"), solved ? GOOD : MUTED);
}
size_t serialize(uint8_t *data, size_t capacity) { return sokoban::serialize(state, data, capacity); }
void load(const uint8_t *data, size_t size) {
    if (sokoban::deserialize(state, data, size)) {
        dragging = false; blocked_ms = 0; pocket::redraw();
    }
}
}
