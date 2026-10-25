// SPDX-License-Identifier: CC0-1.0
#include "../chess.hpp"
#include <assert.h>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <vector>

using chess::State;
unsigned square(const char *s) { return unsigned(s[0] - 'a') + unsigned(s[1] - '1') * 8; }
void move(State &s, const char *from, const char *to, char p = 'q') { assert(chess::play(s, {uint8_t(square(from)), uint8_t(square(to)), p})); }
State fixture(const char *fen, bool white = true, unsigned castle = 0, unsigned ep = 15, unsigned count = 0) {
    char board[69] = {}; memset(board, '.', 64); int row = 7, col = 0;
    for (const char *p = fen; *p; ++p) {
        if (*p == '/') { --row; col = 0; }
        else if (*p >= '1' && *p <= '8') col += *p - '0';
        else { assert(row >= 0 && col < 8); board[row * 8 + col++] = *p; }
    }
    board[64] = char(castle | ep); board[65] = white ? 0 : 1; board[66] = char(count);
    State s{}; assert(chess::position(s, board)); return s;
}
uint64_t perft(State &s, int depth) {
    if (!depth) return 1;
    uint64_t n = 0;
    for (unsigned from = 0; from < 64; ++from) {
        uint8_t targets[8]; chess::targets(s, from, targets);
        for (unsigned to = 0; to < 64; ++to) if (targets[to / 8] & (1u << (to % 8))) {
            State child = s;
            assert(chess::play(child, {uint8_t(from), uint8_t(to), 'q'})); n += perft(child, depth - 1);
        }
    }
    return n;
}
void reseal(std::vector<uint8_t> &bytes) {
    uint32_t h = 2166136261u; for (size_t i = 0; i < bytes.size() - 4; ++i) h = (h ^ bytes[i]) * 16777619u;
    for (unsigned i = 0; i < 4; ++i) bytes[bytes.size() - 4 + i] = uint8_t(h >> (8 * i));
}
int main() {
    State s; chess::start(s);
    assert(perft(s, 1) == 20); assert(perft(s, 2) == 400); assert(perft(s, 3) == 8902);
    assert(!chess::legal(s, square("e7"), square("e5"))); assert(!chess::legal(s, 64, 0));
    assert(!chess::play(s, {12, 28, 'k'}));
    move(s, "e2", "e4"); move(s, "a7", "a6"); move(s, "e4", "e5"); move(s, "d7", "d5");
    assert(chess::legal(s, square("e5"), square("d6"))); move(s, "e5", "d6"); assert(s.board[square("d5")] == '.');
    s = fixture("k3r3/8/8/3pP3/8/8/8/4K3", true, 0, 3);
    assert(!chess::legal(s, square("e5"), square("d6"))); // pinned en passant
    s = fixture("r3k2r/8/8/8/8/8/8/R3K2R", true, 0xf0);
    assert(chess::legal(s, 4, 6)); assert(chess::legal(s, 4, 2)); move(s, "e1", "g1"); assert(s.board[5] == 'R' && s.board[7] == '.');
    move(s, "e8", "c8"); assert(s.board[59] == 'r' && s.board[56] == '.');
    s = fixture("k4r2/8/8/8/8/8/8/4K2R", true, 0x10); assert(!chess::legal(s, 4, 6));
    s = fixture("k7/8/8/8/8/8/8/4KB1R", true, 0x10); assert(!chess::legal(s, 4, 6));
    for (char p : {'q', 'r', 'b', 'n'}) {
        s = fixture("7k/P7/8/8/8/8/8/7K"); assert(chess::promotes(s, 48, 56)); move(s, "a7", "a8", p); assert(s.board[56] == char(p & ~32));
    }
    s = fixture("7k/8/8/8/8/8/p7/7K", false); move(s, "a2", "a1", 'n'); assert(s.board[0] == 'n');
    chess::start(s); move(s, "f2", "f3"); move(s, "e7", "e5"); move(s, "g2", "g4"); move(s, "d8", "h4");
    assert(s.result == chess::Result::BLACK_WIN && chess::check(s)); assert(!chess::legal(s, 12, 20));
    s = fixture("7k/5Q2/6K1/8/8/8/8/8", false); assert(s.result == chess::Result::STALEMATE);
    s = fixture("7k/6Q1/6K1/8/8/8/8/8", false); assert(s.result == chess::Result::WHITE_WIN);
    s = fixture("7k/8/8/8/8/8/8/KN6"); assert(s.result == chess::Result::DEAD);
    s = fixture("7k/8/8/8/8/8/8/KNN5"); assert(s.result == chess::Result::PLAYING); // upstream OR-based material detector misses two knights
    s = fixture("7k/8/8/8/8/8/8/KR6", true, 0, 15, 98); move(s, "b1", "b2");
    assert(s.result == chess::Result::PLAYING); move(s, "h8", "g8"); assert(s.result == chess::Result::FIFTY_MOVE);
    chess::start(s);
    for (unsigned i = 0; i < 2; ++i) { move(s, "g1", "f3"); move(s, "g8", "f6"); move(s, "f3", "g1"); move(s, "f6", "g8"); }
    assert(s.result == chess::Result::REPETITION);
    {
        uint8_t repeated[4096]; size_t size = chess::serialize(s, repeated, sizeof(repeated));
        State loaded; chess::start(loaded); assert(chess::restore(loaded, repeated, size));
        assert(loaded.result == chess::Result::REPETITION && loaded.history_count == 9);
        assert(!memcmp(loaded.history, s.history, size_t(s.history_count) * chess::POSITION_BYTES));
    }
    chess::start(s); move(s, "e2", "e4"); move(s, "e7", "e5"); move(s, "g1", "f3");
    uint8_t bytes[4096]; size_t n = chess::serialize(s, bytes, sizeof(bytes)); assert(n > 0 && n < sizeof(bytes));
    State restored; chess::start(restored); assert(chess::restore(restored, bytes, n));
    assert(!memcmp(s.board, restored.board, 69) && restored.ply == s.ply && restored.history_count == s.history_count);
    assert(!chess::serialize(s, bytes, 16));
    std::vector<uint8_t> good(bytes, bytes + n);
    for (size_t size = 0; size < n; ++size) assert(!chess::restore(restored, good.data(), size));
    for (size_t i = 0; i < n; ++i) { auto bad = good; bad[i] ^= 0x80; assert(!chess::restore(restored, bad.data(), bad.size())); }
    auto bad = good; bad[12] = '!'; reseal(bad); assert(!chess::restore(restored, bad.data(), bad.size()));
    bad = good; bad[8] = 255; reseal(bad); assert(!chess::restore(restored, bad.data(), bad.size()));
    bad = good; bad[12 + 4] = '.'; reseal(bad); assert(!chess::restore(restored, bad.data(), bad.size()));
    bad = good; bad[12 + 64] = 0x18; reseal(bad); assert(!chess::restore(restored, bad.data(), bad.size()));
    bad = good; bad[12 + 69] = 255; reseal(bad); assert(!chess::restore(restored, bad.data(), bad.size()));
    assert(!memcmp(restored.board, s.board, 69)); // rejected saves never change live state
    chess::Ai ai; unsigned calls = 0; long long max_us = 0;
    while (!ai.done) {
        char before[69]; memcpy(before, s.board, 69); auto begin = std::chrono::steady_clock::now();
        chess::ai_step(s, ai); auto end = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count(); if (us > max_us) max_us = us;
        assert(!memcmp(before, s.board, 69)); assert(++calls <= 321);
    }
    assert(ai.candidates > 0 && ai.candidates <= 256 && chess::legal(s, ai.best.from, ai.best.to));
    printf("PASS: perft 20/400/8902; castling, en passant/pin, 4 promotions, mate/stalemate, material, 100-ply draw, repetition, save corruption; AI %u calls/%u candidates max host step %lld us\n", calls, ai.candidates, max_us);
}
