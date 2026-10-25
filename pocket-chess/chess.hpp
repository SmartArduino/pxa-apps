// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace chess {
constexpr unsigned BOARD_BYTES = 69, POSITION_BYTES = 34, HISTORY_CAPACITY = 101;
enum class Result : uint8_t { PLAYING, WHITE_WIN, BLACK_WIN, STALEMATE, REPETITION, FIFTY_MOVE, DEAD };
struct State {
    char board[BOARD_BYTES];
    uint8_t history[HISTORY_CAPACITY][POSITION_BYTES];
    uint8_t history_count;
    uint32_t ply;
    Result result;
};
struct Move { uint8_t from = 255, to = 255; char promotion = 'q'; };
// AI does at most one legal source generation OR one static candidate per step.
// No recursion, maximum 256 evaluated candidates and 320 calls per move.
struct Ai {
    uint8_t from = 0, to = 64, moves[8] = {};
    uint16_t calls = 0, candidates = 0;
    int best_score = -1000000;
    Move best;
    bool done = false;
};
bool white(const State &s);
bool check(State &s);
void start(State &s);
bool legal(State &s, unsigned from, unsigned to);
void targets(State &s, unsigned from, uint8_t out[8]);
bool promotes(const State &s, unsigned from, unsigned to);
bool play(State &s, Move move);
// Validates a classic chess position before invoking any upstream rule helpers.
// Used by host fixtures; the UI starts only from the standard initial position.
bool position(State &s, const char board[BOARD_BYTES]);
size_t serialize(const State &s, uint8_t *out, size_t capacity);
bool restore(State &s, const uint8_t *data, size_t size);
bool ai_step(State &s, Ai &ai);
}
