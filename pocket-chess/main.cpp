// SPDX-License-Identifier: CC0-1.0
#include "../common/pocket-game/runtime.hpp"
#include "../common/pocket-game/music.hpp"
#include "chess.hpp"
#include <string.h>

namespace {
chess::State state;
using pocket_game_sfx::Cue;
using pocket_game_sfx::cue;
void move_sound(bool capture) {
    cue(state.result != chess::Result::PLAYING ? Cue::Goal :
        chess::check(state) ? Cue::Check : capture ? Cue::Capture : Cue::Move);
}
chess::Ai ai;
int cursor = 12, selected = -1, promotion_to = -1, promotion_index = 0;
int last_from = -1, last_to = -1, pressed = -1;
bool local_two = false, thinking = false;
uint32_t ai_delay = 0;
uint8_t highlights[8] = {};
constexpr int BX = 8, BY = 24, CELL = 24;
constexpr uint16_t LIGHT = pocket::rgb(213, 224, 216), DARK = pocket::rgb(75, 116, 110);
constexpr uint16_t WHITE_PIECE = pocket::rgb(255, 250, 224), BLACK_PIECE = pocket::rgb(20, 28, 46);
const char promotions[] = "qrbn";
bool human_turn() { return state.result == chess::Result::PLAYING && (local_two || chess::white(state)); }
void changed(bool persist = true) { pocket::redraw(); if (persist) pocket::save(); }
void clear_selection() { selected = promotion_to = -1; memset(highlights, 0, sizeof(highlights)); }
void restart_ai() { ai = chess::Ai{}; thinking = false; ai_delay = 0; }
void commit(int to, char promotion) {
    if (selected < 0) return;
    chess::Move move{uint8_t(selected), uint8_t(to), promotion};
    const bool capture = state.board[to] != '.' ||
        ((state.board[selected] & ~32) == 'P' && selected % 8 != to % 8);
    if (chess::play(state, move)) { move_sound(capture); last_from = selected; last_to = to; cursor = to; clear_selection(); restart_ai(); changed(); }
}
void choose(int square) {
    if (!human_turn() || square < 0 || square > 63 || promotion_to >= 0) return;
    cursor = square;
    if (selected >= 0 && chess::legal(state, unsigned(selected), unsigned(square))) {
        if (chess::promotes(state, unsigned(selected), unsigned(square))) { promotion_to = square; promotion_index = 0; changed(); }
        else commit(square, 'q');
        return;
    }
    if (selected == square) clear_selection();
    else {
        uint8_t moves[8]; chess::targets(state, unsigned(square), moves);
        const char p = state.board[square];
        if (p != '.' && ((p >= 'A' && p <= 'Z') == chess::white(state))) { selected = square; memcpy(highlights, moves, 8); }
        else clear_selection();
    }
    cue(selected >= 0 ? Cue::Select : Cue::Blocked);
    changed();
}
int square_at(int x, int y) {
    if (x < BX || x >= BX + 192 || y < BY || y >= BY + 192) return -1;
    return (7 - (y - BY) / CELL) * 8 + (x - BX) / CELL;
}
int action_at(int x, int y) {
    int square = square_at(x, y); if (square >= 0) return square;
    if (x < 208 || x >= 284) return -1;
    if (promotion_to >= 0 && y >= 94 && y < 166) return 100 + (y - 94) / 18;
    if (y >= 176 && y < 194) return 200;
    if (y >= 198 && y < 216) return 201;
    return -1;
}
void toggle_mode() { cue(Cue::Select); local_two = !local_two; clear_selection(); restart_ai(); changed(); }
void piece(int x, int y, char p) {
    if (p == '.') return;
    bool white = p >= 'A' && p <= 'Z';
    uint16_t fill = white ? WHITE_PIECE : BLACK_PIECE;
    uint16_t ink = white ? BLACK_PIECE : WHITE_PIECE;
    // Original token design, with a centered name in the selected language.
    pocket::rect(x + 5, y + 3, 14, 17, fill);
    pocket::rect(x + 3, y + 19, 18, 3, fill);
    pocket::outline(x + 5, y + 3, 14, 17, ink);
    const char *label = "";
    switch (p & ~32) {
    case 'K': label = pocket::tr("K", "王"); break;
    case 'Q': label = pocket::tr("Q", "后"); break;
    case 'R': label = pocket::tr("R", "车"); break;
    case 'B': label = pocket::tr("B", "象"); break;
    case 'N': label = pocket::tr("N", "马"); break;
    case 'P': label = pocket::tr("P", "兵"); break;
    }
    pocket::text(x + (CELL - pocket::text_width(label)) / 2, y + 7, label, ink);
}
const char *result_text() {
    switch (state.result) {
    case chess::Result::WHITE_WIN: return pocket::tr("WHITE WINS", "白方胜");
    case chess::Result::BLACK_WIN: return pocket::tr("BLACK WINS", "黑方胜");
    case chess::Result::STALEMATE: return pocket::tr("STALEMATE", "逼和");
    case chess::Result::REPETITION: return pocket::tr("REPETITION", "重复局面");
    case chess::Result::FIFTY_MOVE: return pocket::tr("50 MOVES", "五十回合");
    case chess::Result::DEAD: return pocket::tr("NO MATE", "子力不足");
    default: return "";
    }
}
}
namespace game {
const char *title() { return pocket::tr("Pocket Chess", "国际象棋"); }
const char *app_id() { return "pxa-pocket-chess"; }
void init() {
    chess::start(state); cursor = 12; clear_selection(); restart_ai(); last_from = last_to = pressed = -1; changed();
}
void reset() { init(); cue(Cue::Reset); }
void key(pocket::Key key) {
    using pocket::Key;
    if (key == Key::X) { reset(); return; }
    if (key == Key::Y) { toggle_mode(); return; }
    if (key == Key::B) { cue(Cue::Undo); clear_selection(); changed(); return; }
    if (!human_turn()) return;
    if (promotion_to >= 0) {
        if (key == Key::LEFT || key == Key::UP) promotion_index = (promotion_index + 3) % 4;
        if (key == Key::RIGHT || key == Key::DOWN) promotion_index = (promotion_index + 1) % 4;
        if (key == Key::A) { commit(promotion_to, promotions[promotion_index]); return; }
        changed(); return;
    }
    switch (key) {
    case Key::UP: if (cursor < 56) cursor += 8; break;
    case Key::DOWN: if (cursor >= 8) cursor -= 8; break;
    case Key::LEFT: if (cursor % 8) --cursor; break;
    case Key::RIGHT: if (cursor % 8 != 7) ++cursor; break;
    case Key::A: choose(cursor); return;
    default: return;
    }
    changed();
}
void pointer(pocket::Phase phase, int x, int y) {
    if (phase == pocket::Phase::DOWN) pressed = action_at(x, y);
    if (phase == pocket::Phase::CANCEL) pressed = -1;
    if (phase != pocket::Phase::UP) return;
    const int action = action_at(x, y), down = pressed; pressed = -1;
    if (action < 0 || action != down) return;
    if (action == 200) { toggle_mode(); return; }
    if (action == 201) { reset(); return; }
    if (promotion_to >= 0 && action >= 100 && action <= 103) { commit(promotion_to, promotions[action - 100]); return; }
    if (action < 64) choose(action);
}
void tick(uint32_t elapsed_ms) {
    if (local_two || chess::white(state) || state.result != chess::Result::PLAYING) return;
    if (!thinking) { thinking = true; ai = chess::Ai{}; pocket::redraw(); }
    ai_delay += elapsed_ms;
    if (ai_delay < 200) return;
    if (chess::ai_step(state, ai)) {
        const bool capture = ai.best.from < 64 && ai.best.to < 64 &&
            (state.board[ai.best.to] != '.' || ((state.board[ai.best.from] & ~32) == 'P' && ai.best.from % 8 != ai.best.to % 8));
        if (ai.best.from < 64 && chess::play(state, ai.best)) {
            move_sound(capture);
            last_from = ai.best.from; last_to = ai.best.to; clear_selection(); restart_ai(); changed();
        } else { thinking = false; pocket::log("Chess AI found no legal move"); }
    }
}
void draw() {
    using namespace pocket;
    clear(); text(8, 6, tr("POCKET CHESS", "国际象棋"), INK, 1);
    for (int row = 0; row < 8; ++row) {
        char rank[2] = {char('8' - row), 0}; text(1, BY + row * CELL + 9, rank, MUTED);
        for (int col = 0; col < 8; ++col) {
            int sq = (7 - row) * 8 + col, x = BX + col * CELL, y = BY + row * CELL;
            rect(x, y, CELL, CELL, (row + col) % 2 ? DARK : LIGHT);
            if (sq == last_from || sq == last_to) outline(x + 1, y + 1, 22, 22, rgb(205, 179, 69));
            piece(x, y, state.board[sq]);
            if (selected >= 0 && (highlights[sq / 8] & (1u << (sq % 8)))) {
                if (state.board[sq] == '.') rect(x + 10, y + 10, 4, 4, rgb(18, 153, 105));
                else outline(x + 2, y + 2, 20, 20, GOOD);
            }
            if (sq == selected) { outline(x + 1, y + 1, 22, 22, ACCENT); outline(x + 2, y + 2, 20, 20, ACCENT); }
            if (sq == cursor) outline(x, y, CELL, CELL, WARN);
        }
    }
    for (int c = 0; c < 8; ++c) { char label[2] = {char('a' + c), 0}; text(BX + c * CELL + 9, 217, label, MUTED); }
    text(208, 28, local_two ? tr("2 PLAYERS", "本地双人") : tr("VS LIGHT AI", "入门电脑"), ACCENT);
    text(208, 44, chess::white(state) ? tr("WHITE TURN", "白方走棋") : tr("BLACK TURN", "黑方走棋"));
    if (state.result != chess::Result::PLAYING) {
        text(208, 64, result_text(), GOOD);
        if (state.result == chess::Result::WHITE_WIN || state.result == chess::Result::BLACK_WIN) text(208, 78, tr("CHECKMATE", "将死"), WARN);
        else text(208, 78, tr("DRAW", "和棋"), WARN);
    } else if (chess::check(state)) text(208, 64, tr("CHECK!", "将军"), BAD);
    else if (thinking) text(208, 64, tr("THINKING...", "思考中"), WARN);
    if (promotion_to >= 0) {
        text(208, 80, tr("PROMOTE", "兵升变"), ACCENT);
        const char *names[4] = {tr("Q QUEEN", "Q 后"), tr("R ROOK", "R 车"), tr("B BISHOP", "B 象"), tr("N KNIGHT", "N 马")};
        for (int i = 0; i < 4; ++i) { rect(208, 94 + i * 18, 76, 16, i == promotion_index ? rgb(50, 97, 111) : PANEL); text(212, 97 + i * 18, names[i]); }
    } else {
        text(208, 96, tr("A SELECT", "A 选择"), MUTED); text(208, 110, tr("B CANCEL", "B 取消"), MUTED);
        text(208, 130, tr("TAP 2", "依次点击"), MUTED); text(208, 142, tr("SQUARES", "起点终点"), MUTED);
        text(208, 160, tr("START MENU", "START 菜单"), MUTED);
    }
    rect(208, 176, 76, 18, PANEL); text(212, 180, tr("Y MODE", "Y 模式"), ACCENT);
    rect(208, 198, 76, 18, PANEL); text(212, 202, tr("X NEW", "X 新局"), WARN);
}
size_t serialize(uint8_t *data, size_t capacity) {
    if (!data || capacity < 16) return 0;
    memcpy(data, "PCU1", 4); data[4] = local_two; data[5] = uint8_t(cursor);
    data[6] = uint8_t(selected + 1); data[7] = uint8_t(promotion_to + 1); data[8] = uint8_t(promotion_index);
    data[9] = uint8_t(last_from + 1); data[10] = uint8_t(last_to + 1); data[11] = 0;
    size_t n = chess::serialize(state, data + 12, capacity - 12); return n ? n + 12 : 0;
}
void load(const uint8_t *data, size_t size) {
    if (!data || size < 12 || memcmp(data, "PCU1", 4) || data[4] > 1 || data[5] >= 64 || data[6] > 64 || data[7] > 64 || data[8] > 3 || data[9] > 64 || data[10] > 64 || data[11]) return;
    chess::State candidate;
    if (!chess::restore(candidate, data + 12, size - 12)) return;
    int sel = int(data[6]) - 1, prom = int(data[7]) - 1;
    if (sel >= 0 && (candidate.result != chess::Result::PLAYING || (!data[4] && !chess::white(candidate)) || candidate.board[sel] == '.' || ((candidate.board[sel] >= 'A' && candidate.board[sel] <= 'Z') != chess::white(candidate)))) return;
    if (prom >= 0 && (sel < 0 || !chess::promotes(candidate, unsigned(sel), unsigned(prom)) || !chess::legal(candidate, unsigned(sel), unsigned(prom)))) return;
    state = candidate; local_two = data[4]; cursor = data[5]; selected = sel; promotion_to = prom; promotion_index = data[8];
    last_from = int(data[9]) - 1; last_to = int(data[10]) - 1; memset(highlights, 0, 8);
    if (selected >= 0) chess::targets(state, unsigned(selected), highlights);
    pressed = -1; restart_ai(); pocket::redraw();
}
}
