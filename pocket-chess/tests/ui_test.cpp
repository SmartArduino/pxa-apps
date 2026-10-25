// SPDX-License-Identifier: CC0-1.0
#include "../../common/pocket-game/runtime.hpp"
#include "../chess.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>

namespace {
struct Label { int x, y, w, h; std::string value; };
std::vector<Label> labels;
bool visible(const char *s) { for (const auto &label : labels) if (label.value == s) return true; return false; }
}
namespace pocket {
unsigned tone_calls = 0, silence_calls = 0;
void tone(float hz, float seconds, float when) {
    assert(hz >= 30 && hz <= 6000 && seconds > 0 && seconds <= 4 && when >= 0);
    ++tone_calls;
}
void silence() { ++silence_calls; }
void clear(uint16_t) { labels.clear(); }
void rect(int x, int y, int w, int h, uint16_t) { assert(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= WIDTH && y + h <= HEIGHT); }
void outline(int x, int y, int w, int h, uint16_t c) { rect(x, y, w, h, c); }
void line(int x, int y, int x1, int y1, uint16_t) { assert(x >= 0 && x1 < WIDTH && y >= 0 && y1 < HEIGHT); }
void text(int x, int y, const char *s, uint16_t, int scale) {
    int h = 7;
    for (const char *p = s; *p;) if (pocket_ui::next_utf8(p) >= 128) h = 10;
    h *= scale;
    const int w = text_width(s, scale);
    assert(x >= 0 && y >= 0 && x + w <= WIDTH && y + h <= HEIGHT);
    for (const auto &other : labels)
        assert(x + w <= other.x || other.x + other.w <= x || y + h <= other.y || other.y + other.h <= y);
    labels.push_back({x, y, w, h, s});
}
int text_width(const char *s, int scale) { return pocket_ui::text_width(s, scale); }
void redraw() {} void save() {} uint32_t random() { return 7; } uint32_t millis() { return 0; } void log(const char *) { assert(false); }
}
std::vector<uint8_t> saved() { std::vector<uint8_t> out(4096); size_t n = game::serialize(out.data(), out.size()); assert(n); out.resize(n); return out; }
chess::State board() { auto bytes = saved(); chess::State s; assert(chess::restore(s, bytes.data() + 12, bytes.size() - 12)); return s; }
void tap(int x, int y) { game::pointer(pocket::Phase::DOWN, x, y); game::pointer(pocket::Phase::UP, x, y); }
void tap_square(unsigned file, unsigned rank) { tap(8 + int(file) * 24 + 12, 24 + int(7 - rank) * 24 + 12); }
void draw_both(const char *en, const char *zh) {
    const auto before = saved();
    for (bool english : {false, true}) {
        pocket::set_english(english); game::draw(); assert(visible(english ? en : zh));
        assert(saved() == before);
    }
    pocket::set_english(false);
}
chess::State position(const char *fen, bool white = true, unsigned count = 0) {
    char pieces[69] = {}; memset(pieces, '.', 64); int row = 7, col = 0;
    for (const char *p = fen; *p; ++p) {
        if (*p == '/') { --row; col = 0; }
        else if (*p >= '1' && *p <= '8') col += *p - '0';
        else pieces[row * 8 + col++] = *p;
    }
    pieces[64] = 15; pieces[65] = white ? 0 : 1; pieces[66] = char(count);
    chess::State out; assert(chess::position(out, pieces)); return out;
}
void show_position(const chess::State &s, const char *en, const char *zh) {
    std::vector<uint8_t> bytes(4096); memcpy(bytes.data(), "PCU1", 4); bytes[4] = 1; bytes[5] = 12;
    const size_t n = chess::serialize(s, bytes.data()+12, bytes.size()-12); assert(n);
    game::load(bytes.data(), n+12); assert(board().result == s.result); draw_both(en, zh);
}
int main() {
    using pocket::Key; game::init(); game::draw(); assert(board().ply == 0);
    game::key(Key::A); game::key(Key::UP); game::key(Key::UP); game::key(Key::A); // initial e2 to e4
    assert(board().board[28] == 'P' && board().ply == 1 && pocket::tone_calls == 2); auto ai_save = saved();
    for (int i = 0; i < 330 && board().ply == 1; ++i) game::tick(20);
    assert(board().ply == 2); game::draw();
    game::load(ai_save.data(), ai_save.size()); assert(board().ply == 1);
    for (int i = 0; i < 330 && board().ply == 1; ++i) game::tick(20);
    assert(board().ply == 2);
    game::key(Key::X); game::key(Key::Y); assert(saved()[4] == 1);
    tap_square(4, 1); auto selected = saved(); assert(selected[6] == 13); game::key(Key::B); assert(saved()[6] == 0);
    game::load(selected.data(), selected.size()); tap_square(4, 3); assert(board().board[28] == 'P' && board().ply == 1);
    tap_square(4, 6); tap_square(4, 4); assert(board().board[36] == 'p' && board().ply == 2);
    for (int i = 0; i < 400; ++i) game::tick(20);
    assert(board().ply == 2);
    game::key(Key::X); game::pointer(pocket::Phase::DOWN, 116, 180); game::pointer(pocket::Phase::CANCEL, 116, 180); game::pointer(pocket::Phase::UP, 116, 180); assert(saved()[6] == 0);
    game::pointer(pocket::Phase::DOWN, 116, 180); game::pointer(pocket::Phase::UP, -1, -1); assert(saved()[6] == 0);
    auto valid = saved(); auto invalid = valid; invalid[6] = 65; game::load(invalid.data(), invalid.size()); assert(saved() == valid);
    // Inject a valid near-promotion fixture through the public save contract.
    char pieces[69] = {}; memset(pieces, '.', 64); pieces[7] = 'K'; pieces[63] = 'k'; pieces[48] = 'P'; pieces[64] = 15;
    chess::State s; assert(chess::position(s, pieces)); std::vector<uint8_t> fixture(4096); memcpy(fixture.data(), valid.data(), 12);
    fixture[5] = 48; fixture[6] = fixture[7] = fixture[8] = 0;
    size_t n = chess::serialize(s, fixture.data() + 12, fixture.size() - 12); fixture.resize(n + 12); game::load(fixture.data(), fixture.size());
    tap_square(0, 6); tap_square(0, 7); auto promoting = saved(); assert(promoting[7] == 57); game::draw();
    game::key(Key::B); assert(saved()[7] == 0); game::load(promoting.data(), promoting.size());
    game::key(Key::DOWN); game::key(Key::DOWN); game::key(Key::DOWN); game::key(Key::A); assert(board().board[56] == 'N'); game::draw();
    // Malformed pending promotion must not alter current state.
    auto before = saved(); auto bad = before; bad[6] = 1; bad[7] = 2; game::load(bad.data(), bad.size()); assert(saved() == before);
    tap(230, 205); assert(board().ply == 0); tap(230, 185); assert(saved()[4] == 0);
    draw_both("VS LIGHT AI", "入门电脑");
    for (bool en : {false, true}) {
        pocket::set_english(en); game::draw();
        assert(!strcmp(game::title(), en ? "Pocket Chess" : "国际象棋"));
        for (const char *name : (en ? std::vector<const char*>{"K","Q","R","B","N","P"} : std::vector<const char*>{"王","后","车","象","马","兵"})) assert(visible(name));
    }
    game::load(ai_save.data(),ai_save.size()); game::tick(20); draw_both("THINKING...", "思考中");
    game::key(Key::Y); draw_both("2 PLAYERS", "本地双人");
    game::load(promoting.data(),promoting.size()); draw_both("PROMOTE", "兵升变");
    for (bool en : {false, true}) {
        pocket::set_english(en); game::draw();
        for (const char *name : (en ? std::vector<const char*>{"Q QUEEN","R ROOK","B BISHOP","N KNIGHT"} : std::vector<const char*>{"Q 后","R 车","B 象","N 马"})) assert(visible(name));
    }
    show_position(position("7k/5Q2/6K1/8/8/8/8/8",false), "STALEMATE", "逼和");
    show_position(position("7k/6Q1/6K1/8/8/8/8/8",false), "WHITE WINS", "白方胜");
    show_position(position("7k/8/8/8/8/8/8/KN6"), "NO MATE", "子力不足");
    show_position(position("7k/8/8/8/8/8/4r3/4K3"), "CHECK!", "将军");
    chess::start(s);
    for (chess::Move move : {chess::Move{13,21,'q'}, {52,36,'q'}, {14,30,'q'}, {59,31,'q'}}) assert(chess::play(s,move));
    show_position(s,"BLACK WINS","黑方胜");
    s=position("7k/8/8/8/8/8/8/KR6",true,98);
    assert(chess::play(s,{1,9,'q'}) && chess::play(s,{63,62,'q'})); show_position(s,"50 MOVES","五十回合");
    chess::start(s);
    for (int i=0;i<2;++i) for (chess::Move move : {chess::Move{6,21,'q'}, {62,45,'q'}, {21,6,'q'}, {45,62,'q'}}) assert(chess::play(s,move));
    show_position(s,"REPETITION","重复局面");
    const auto stable = saved(); unsigned before_tones = pocket::tone_calls;
    for (int i=0;i<100;++i) game::tick(20);
    assert(saved() == stable && pocket::tone_calls == before_tones);
    puts("PASS: controller/touch/AI/save/promotion and bilingual pieces/modes/help/check/all results; draw bounds, no text overlap, save invariance");
}
