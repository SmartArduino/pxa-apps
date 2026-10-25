// SPDX-License-Identifier: MIT
// Movement / rewind ported from NibbleSokoban (c) 2021 snsdosen.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "levels.hpp"

namespace sokoban {
constexpr unsigned WIDTH = 10, HEIGHT = 10, CELLS = WIDTH * HEIGHT;
constexpr unsigned HISTORY = 2048;
constexpr uint32_t MAX_MOVES = 1000000000;
enum Tile : uint8_t { FLOOR = 0, WALL = 1, GOAL = 2 };
enum Direction : uint8_t { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };
struct State {
    // Original WorkAreaLower / WorkAreaUpper split preserves goals under boxes.
    uint8_t lower[CELLS], upper[CELLS];
    uint8_t player, level, unlocked;
    uint64_t completed;
    uint32_t steps, pushes;
    // Original LIFO direction | 0x10 (pushed box), now statically allocated.
    uint8_t history[HISTORY];
    uint16_t history_head, history_count;
};
void init(State &state);
void reset_level(State &state, unsigned level);
bool select(State &state, int delta);
bool move(State &state, Direction direction);
bool undo(State &state);
bool won(const State &state);
unsigned completed_count(const State &state);
bool valid(const State &state);
size_t serialize(const State &state, uint8_t *data, size_t capacity);
// Transactional: failure leaves state byte-for-byte unchanged.
bool deserialize(State &state, const uint8_t *data, size_t size);
}
