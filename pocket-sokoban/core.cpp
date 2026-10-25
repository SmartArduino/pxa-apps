// SPDX-License-Identifier: MIT
// Adapted from Player.ino and LifoBuffer.ino in NibbleSokoban,
// Copyright (c) 2021 snsdosen. Full license: upstream/NibbleSokoban/LICENSE.
// The four original movement branches are consolidated into a checked neighbor
// operation; dual lower/upper maps and direction|0x10 rewind semantics remain.
// PXA changes: explicit two-cell bounds, fixed storage, counters, progression,
// and validated versioned persistence. No upstream levels/graphics are used.
#include "core.hpp"
#include <string.h>

namespace sokoban {
static_assert(LEVEL_COUNT <= 64, "completion bitset capacity");
namespace {
int neighbor(unsigned cell, unsigned direction) {
    if (cell >= CELLS || direction > LEFT) return -1;
    const int x = int(cell % WIDTH), y = int(cell / WIDTH);
    const int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
    const int nx = x + dx[direction], ny = y + dy[direction];
    return nx >= 0 && nx < int(WIDTH) && ny >= 0 && ny < int(HEIGHT)
        ? ny * int(WIDTH) + nx : -1;
}
void lifo_push(State &s, uint8_t value) {
    s.history[s.history_head] = value;
    s.history_head = uint16_t((s.history_head + 1) % HISTORY);
    if (s.history_count < HISTORY) ++s.history_count;
}
uint8_t lifo_pop(State &s) {
    s.history_head = uint16_t((s.history_head + HISTORY - 1) % HISTORY);
    --s.history_count;
    return s.history[s.history_head];
}
bool rewind(State &s) {
    if (!s.history_count || !s.steps) return false;
    const unsigned last = (s.history_head + HISTORY - 1) % HISTORY;
    const uint8_t entry = s.history[last];
    if ((entry & ~0x13u) || (entry & 0xfu) > LEFT) return false;
    const unsigned dir = entry & 0xfu;
    const int previous = neighbor(s.player, (dir + 2) % 4);
    if (previous < 0 || s.lower[previous] == WALL || s.upper[previous]) return false;
    if (entry & 0x10u) {
        const int crate = neighbor(s.player, dir);
        if (crate < 0 || !s.upper[crate] || s.upper[s.player] || !s.pushes) return false;
        s.upper[crate] = 0;
        s.upper[s.player] = 1;
        --s.pushes;
    }
    lifo_pop(s);
    s.player = uint8_t(previous);
    --s.steps;
    return true;
}
void put16(uint8_t *p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
uint16_t get16(const uint8_t *p) { return uint16_t(p[0] | (unsigned(p[1]) << 8)); }
void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
}
uint32_t get32(const uint8_t *p) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i);
    return v;
}
uint32_t checksum(const uint8_t *p, size_t size) {
    // CRC-32/ISO-HDLC; integrity check, not authentication.
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= p[i];
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
}
void reset_level(State &s, unsigned level) {
    if (level >= LEVEL_COUNT) return;
    s.level = uint8_t(level);
    s.steps = s.pushes = 0;
    s.history_head = s.history_count = 0;
    memset(s.history, 0, sizeof(s.history));
    const char *cells = LEVELS[level].cells;
    for (unsigned i = 0; i < CELLS; ++i) {
        s.lower[i] = cells[i] == '#' ? WALL : (cells[i] == '.' || cells[i] == '*' || cells[i] == '+') ? GOAL : FLOOR;
        s.upper[i] = cells[i] == '$' || cells[i] == '*';
        if (cells[i] == '@' || cells[i] == '+') s.player = uint8_t(i);
    }
}
void init(State &s) { s = {}; reset_level(s, 0); }
bool select(State &s, int delta) {
    if (!delta || !s.unlocked) return false;
    const unsigned count = unsigned(s.unlocked) + 1;
    const unsigned next = delta < 0 ? (s.level + count - 1) % count : (s.level + 1) % count;
    reset_level(s, next);
    return true;
}
bool won(const State &s) {
    unsigned boxes = 0;
    for (unsigned i = 0; i < CELLS; ++i) if (s.upper[i]) {
        ++boxes;
        if (s.lower[i] != GOAL) return false;
    }
    return boxes > 0;
}
bool move(State &s, Direction direction) {
    if (unsigned(direction) > LEFT || s.steps >= MAX_MOVES || won(s)) return false;
    const int next = neighbor(s.player, direction);
    if (next < 0 || s.lower[next] == WALL) return false;
    uint8_t entry = uint8_t(direction);
    if (s.upper[next]) {
        // Original: only one crate, and its destination must be empty/non-wall.
        const int beyond = neighbor(unsigned(next), direction);
        if (beyond < 0 || s.upper[beyond] || s.lower[beyond] == WALL) return false;
        s.upper[next] = 0;
        s.upper[beyond] = 1;
        entry |= 0x10;
        ++s.pushes;
    }
    s.player = uint8_t(next);
    lifo_push(s, entry);
    ++s.steps;
    if (won(s)) {
        s.completed |= uint64_t(1) << s.level;
        if (unsigned(s.level) + 1 < LEVEL_COUNT && s.unlocked <= s.level)
            s.unlocked = uint8_t(s.level + 1);
    }
    return true;
}
bool undo(State &s) { return rewind(s); }
unsigned completed_count(const State &s) {
    unsigned count = 0;
    for (unsigned i = 0; i < LEVEL_COUNT; ++i) count += unsigned((s.completed >> i) & 1u);
    return count;
}
bool valid(const State &s) {
    if (s.level >= LEVEL_COUNT || s.unlocked >= LEVEL_COUNT || s.level > s.unlocked ||
        s.player >= CELLS || s.history_head >= HISTORY || s.history_count > HISTORY ||
        s.history_count > s.steps || s.steps > MAX_MOVES || s.pushes > s.steps) return false;
    const uint64_t mask = ~uint64_t(0) >> (64 - LEVEL_COUNT);
    if ((s.completed & ~mask) || (s.unlocked < 63 && (s.completed >> (s.unlocked + 1)))) return false;
    const uint64_t required = (uint64_t(1) << s.unlocked) - 1;
    if ((s.completed & required) != required) return false;
    State initial{};
    reset_level(initial, s.level);
    unsigned boxes = 0, expected = 0;
    for (unsigned i = 0; i < CELLS; ++i) {
        if (s.lower[i] != initial.lower[i] || s.upper[i] > 1 || (s.upper[i] && s.lower[i] == WALL)) return false;
        boxes += s.upper[i];
        expected += initial.upper[i];
    }
    if (boxes != expected || s.lower[s.player] == WALL || s.upper[s.player]) return false;
    if (won(s) && !(s.completed & (uint64_t(1) << s.level))) return false;
    // Verify the entire persisted undo chain on a temporary object.
    State reverse = s;
    while (reverse.history_count) if (!rewind(reverse)) return false;
    if (!reverse.steps && (reverse.pushes || reverse.player != initial.player ||
                          memcmp(reverse.upper, initial.upper, CELLS))) return false;
    // Older moves may have fallen out of the circular buffer; after undoing
    // retained moves, steps can legitimately exceed the remaining history.
    return true;
}
size_t serialize(const State &s, uint8_t *data, size_t capacity) {
    const size_t size = 232 + s.history_count;
    if (!data || capacity < size) return 0;
    memset(data, 0, 28);
    memcpy(data, "PSKB", 4);
    data[4] = 1; data[5] = s.level; data[6] = s.unlocked;
    put32(data + 8, s.steps); put32(data + 12, s.pushes);
    for (unsigned i = 0; i < 8; ++i) data[16 + i] = uint8_t(s.completed >> (8 * i));
    data[24] = s.player;
    put16(data + 26, s.history_count);
    memcpy(data + 28, s.lower, CELLS);
    memcpy(data + 128, s.upper, CELLS);
    for (unsigned i = 0; i < s.history_count; ++i)
        data[228 + i] = s.history[(s.history_head + HISTORY - s.history_count + i) % HISTORY];
    put32(data + size - 4, checksum(data, size - 4));
    return size;
}
bool deserialize(State &s, const uint8_t *data, size_t size) {
    if (!data || size < 232 || size > 232 + HISTORY || memcmp(data, "PSKB", 4) ||
        data[4] != 1 || data[7] || data[25] ||
        size != 232u + get16(data + 26) ||
        get32(data + size - 4) != checksum(data, size - 4)) return false;
    State candidate{};
    candidate.level = data[5]; candidate.unlocked = data[6];
    candidate.steps = get32(data + 8); candidate.pushes = get32(data + 12);
    for (unsigned i = 0; i < 8; ++i) candidate.completed |= uint64_t(data[16 + i]) << (8 * i);
    candidate.player = data[24]; candidate.history_count = get16(data + 26);
    candidate.history_head = uint16_t(candidate.history_count % HISTORY);
    memcpy(candidate.lower, data + 28, CELLS);
    memcpy(candidate.upper, data + 128, CELLS);
    memcpy(candidate.history, data + 228, candidate.history_count);
    if (!valid(candidate)) return false;
    s = candidate;
    return true;
}
}
