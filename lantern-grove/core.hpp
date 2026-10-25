// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace grove {
constexpr uint32_t LIMIT = 999999;
constexpr uint64_t MAX_IDLE_MS = 8ull*60*60*1000;
constexpr size_t SAVE_BYTES = 128;
constexpr uint64_t MIN_UTC = 1704067200ull, MAX_UTC = 4133980800ull;
struct State {
    uint32_t resources[3] = {12,8,4}, pending[3] = {}, fraction[3] = {};
    uint8_t level[4] = {1,0,0,0}, goals = 0, sound = 1;
    uint8_t view = 0, selected = 0;
    uint32_t collected = 0, taps = 0, cycle_ms = 0;
    uint64_t produced_ms = 0, last_wall = 0;
};
struct Price { uint32_t value[3]; };
struct Away { uint32_t seconds = 0; bool valid = false; };
void fresh(State &s);
bool valid_wall(uint64_t seconds);
uint32_t rate(const State &s, unsigned resource); // thousandths per second
uint32_t capacity(const State &s);
void produce(State &s, uint64_t elapsed_ms); // constant work, capped to 8h
Price price(const State &s, unsigned upgrade);
bool buy(State &s, unsigned upgrade);
bool collect(State &s);
bool achieved(const State &s, unsigned goal);
bool claim(State &s, unsigned goal);
const char *goal_title(unsigned goal);
const char *goal_detail(unsigned goal);
const char *upgrade_name(unsigned item);
const char *resource_name(unsigned item);
Away offline(State &s, uint64_t now_wall);
void stamp(State &s, uint64_t now_wall);
size_t encode(const State &s, uint8_t *out, size_t capacity);
bool decode(State &s, const uint8_t *data, size_t size);
// Standard synchronous WASI clocks, isolated for deterministic native tests.
uint64_t wall_seconds();
uint64_t monotonic_ms();
}
