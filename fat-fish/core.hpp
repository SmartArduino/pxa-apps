// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace fish {
struct State {
    uint8_t food = 72, joy = 80, energy = 86, affection = 0;
    bool sleeping = false, night = false, wander = true, autoplay = true;
    bool autocare = true, auto_sleep = false;
    uint8_t room = 0;
    int16_t position = 0;
    uint32_t seconds = 0, touches = 0, second_ms = 0;
};
enum class Action { PET, FEED, PLAY, REST };
enum class Result { PETTED, FED, PLAYED, SLEPT, WOKE, FULL, SLEEPING, TIRED };
inline uint8_t add(uint8_t value, unsigned amount) {
    return uint8_t(std::min(100u, unsigned(value) + amount));
}
inline Result act(State &s, Action action) {
    if (action == Action::REST || (action == Action::PET && s.sleeping)) {
        s.auto_sleep = false;
        s.sleeping = !s.sleeping;
        return s.sleeping ? Result::SLEPT : Result::WOKE;
    }
    if (s.sleeping) return Result::SLEEPING;
    if (action == Action::FEED) {
        if (s.food >= 95) return Result::FULL;
        s.food = add(s.food, 20);
        s.joy = add(s.joy, 3);
        return Result::FED;
    }
    if (action == Action::PLAY) {
        if (s.energy < 10) return Result::TIRED;
        s.energy -= 6;
        s.joy = add(s.joy, 15);
        return Result::PLAYED;
    }
    s.joy = add(s.joy, 4);
    s.affection = add(s.affection, 1);
    s.touches = std::min(1000000000u, s.touches + 1);
    return Result::PETTED;
}
inline bool automatic_care(const State &s, Action &action) {
    if (!s.autocare) return false;
    if (s.sleeping) {
        action = Action::REST;
        return s.auto_sleep && s.energy >= 90;
    }
    if (s.food <= 30) action = Action::FEED;
    else if (s.energy <= 20) action = Action::REST;
    else if (s.joy <= 35 && s.energy >= 35) action = Action::PLAY;
    else return false;
    return true;
}
inline void step(State &s, uint32_t ms) {
    s.second_ms += std::min(ms, 100u);
    if (s.second_ms < 1000) return;
    s.second_ms -= 1000;
    s.seconds = std::min(1000000000u, s.seconds + 1);
    if (s.sleeping && s.seconds % 5 == 0) s.energy = add(s.energy, 3);
    if (s.seconds % 60 == 0) {
        if (s.food) --s.food;
        if (!s.sleeping && s.energy) --s.energy;
        if (!s.sleeping && s.joy) --s.joy;
    }
}
inline uint32_t checksum(const uint8_t *data, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}
inline void put32(uint8_t *data, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) data[i] = uint8_t(value >> (8 * i));
}
inline uint32_t get32(const uint8_t *data) {
    return uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16 | uint32_t(data[3]) << 24;
}
inline size_t encode(const State &s, uint8_t *data, size_t capacity) {
    if (capacity < 64) return 0;
    memset(data, 0, 64);
    memcpy(data, "FFP1", 4);
    data[4] = 1;
    data[8] = s.food; data[9] = s.joy; data[10] = s.energy; data[11] = s.affection;
    data[12] = s.sleeping; data[13] = s.night; data[14] = s.wander; data[15] = s.autoplay;
    data[16] = uint8_t(s.position + 64);
    data[17] = s.room;
    data[18] = 1; // Positions are relative to the rug since the room-placement update.
    data[19] = s.autocare ? 2 : 1; // Zero identifies saves before automatic care.
    put32(data + 20, s.seconds); put32(data + 24, s.touches);
    put32(data + 28, s.second_ms);
    data[32] = s.auto_sleep;
    put32(data + 60, checksum(data, 60));
    return 64;
}
inline bool decode(State &s, const uint8_t *data, size_t size) {
    if (size != 64 || memcmp(data, "FFP1", 4) || data[4] != 1 ||
        get32(data + 60) != checksum(data, 60)) return false;
    for (unsigned i = 5; i < 8; ++i) if (data[i]) return false;
    for (unsigned i = 8; i < 12; ++i) if (data[i] > 100) return false;
    for (unsigned i = 12; i < 16; ++i) if (data[i] > 1) return false;
    if (data[16] > 128 || data[17] > 2 || data[18] > 1 || data[19] > 2 || data[32] > 1 ||
        (data[32] && !data[12]) ||
        get32(data + 20) > 1000000000u || get32(data + 24) > 1000000000u ||
        get32(data + 28) >= 1000) return false;
    for (unsigned i = 33; i < 60; ++i) if (data[i]) return false;
    State loaded;
    loaded.food = data[8]; loaded.joy = data[9]; loaded.energy = data[10]; loaded.affection = data[11];
    loaded.sleeping = data[12]; loaded.night = data[13]; loaded.wander = data[14]; loaded.autoplay = data[15];
    loaded.position = data[18] ? int16_t(int(data[16]) - 64) : 0;
    loaded.room = data[17];
    loaded.autocare = data[19] != 1;
    loaded.auto_sleep = data[32];
    loaded.seconds = get32(data + 20); loaded.touches = get32(data + 24);
    loaded.second_ms = get32(data + 28);
    s = loaded;
    return true;
}
// Output is published only after a complete successful decode by the caller.
bool unpack(const uint8_t *input, size_t bytes, uint8_t *output, size_t pixels,
            const uint8_t *previous = nullptr);
} // namespace fish
