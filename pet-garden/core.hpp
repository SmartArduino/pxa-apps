// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace garden {
enum class Mode : uint8_t { HOME, GAMES, CATCH, RHYTHM, DECOR, HELP, RESULT };
enum class Action : uint8_t { FEED, WASH, PET, PLAY, REST, DECOR };
enum class Event : uint8_t { NONE, FED, WASHED, PETTED, RESTING, AWAKE, NO_FOOD, WAIT, EVOLVED, HARVEST, BOUGHT, TOGGLED, POOR, STARTED, CAUGHT, MISSED, BEAT, FINISHED };
struct Drop { int16_t x, y; uint8_t kind, active; };
struct State {
    uint8_t food = 80, clean = 80, joy = 80, energy = 80;
    uint8_t snacks = 5, stage = 0, branch = 0, decor_owned = 0, decor_on = 0;
    uint8_t selection = 0, room = 0, sleeping = 0, last_game = 0;
    uint16_t coins = 8, growth = 0, affinity[3] = {}, rounds = 0;
    uint16_t best_catch = 0, best_rhythm = 0;
    uint32_t active_ms = 0, needs_ms = 0, harvest_ms = 0;
    Mode mode = Mode::HOME;
    // Position and interrupted minigame state are persistent; timers are foreground only.
    int16_t pet_x = 144, basket_x = 144;
    uint32_t game_ms = 0, spawn_ms = 0, seed = 0x31415926;
    uint16_t score = 0, attempts = 0, reward = 0;
    uint8_t beat_index = 0, beat_done = 0;
    Drop drops[5] = {};
    uint16_t cooldown[3] = {};
};
constexpr size_t SAVE_BYTES = 128;
constexpr uint16_t DECOR_COST[4] = {6, 10, 14, 18};
void fresh(State &s);
Event action(State &s, Action a);
Event step(State &s, uint32_t ms);
void start(State &s, uint8_t game);
Event beat(State &s);
Event decorate(State &s, unsigned item);
void home(State &s);
const char *name(const State &s);
const char *stage_name(const State &s);
size_t encode(const State &s, uint8_t *out, size_t capacity);
bool decode(State &s, const uint8_t *data, size_t size);
}
