// SPDX-License-Identifier: MIT
#include "core.hpp"
#include <string.h>

namespace garden {
namespace {
uint8_t plus(uint8_t a, unsigned b) { return uint8_t(a + b > 100 ? 100 : a + b); }
uint32_t rng(State &s) { s.seed ^= s.seed << 13; s.seed ^= s.seed >> 17; s.seed ^= s.seed << 5; return s.seed; }
Event grow(State &s, unsigned points, unsigned affinity) {
    s.growth = uint16_t(s.growth + points > 999 ? 999 : s.growth + points);
    s.affinity[affinity] = uint16_t(s.affinity[affinity] + points > 999 ? 999 : s.affinity[affinity] + points);
    uint8_t next = s.growth >= 120 ? 2 : s.growth >= 45 ? 1 : 0;
    if (next > s.stage) {
        s.stage = next;
        if (next == 2) {
            unsigned b = 0;
            for (unsigned i = 1; i < 3; ++i) if (s.affinity[i] > s.affinity[b]) b = i;
            s.branch = uint8_t(b + 1);
        }
        return Event::EVOLVED;
    }
    return Event::NONE;
}
Event finish(State &s) {
    if (s.mode != Mode::CATCH && s.mode != Mode::RHYTHM) return Event::NONE;
    bool catch_game = s.mode == Mode::CATCH;
    s.reward = uint16_t(catch_game ? 2 + s.score / 2 : 2 + s.score);
    s.coins = uint16_t(s.coins + s.reward > 9999 ? 9999 : s.coins + s.reward);
    s.snacks = uint8_t(s.snacks + 2 > 20 ? 20 : s.snacks + 2);
    s.joy = plus(s.joy, 12 + s.score);
    s.energy = uint8_t(s.energy > 8 ? s.energy - 8 : 0);
    if (s.rounds < 65535) ++s.rounds;
    if (catch_game && s.score > s.best_catch) s.best_catch = s.score;
    if (!catch_game && s.score > s.best_rhythm) s.best_rhythm = s.score;
    Event evolution = grow(s, 10 + s.score, 1);
    s.mode = Mode::RESULT;
    return evolution == Event::EVOLVED ? evolution : Event::FINISHED;
}
struct Writer {
    uint8_t *p; size_t i = 8;
    void u8(uint8_t n) { p[i++] = n; }
    void u16(uint16_t n) { u8(uint8_t(n)); u8(uint8_t(n >> 8)); }
    void u32(uint32_t n) { u16(uint16_t(n)); u16(uint16_t(n >> 16)); }
};
struct Reader {
    const uint8_t *p; size_t i = 8;
    uint8_t u8() { return p[i++]; }
    uint16_t u16() { uint16_t n = u8(); return uint16_t(n | uint16_t(u8()) << 8); }
    uint32_t u32() { uint32_t n = u16(); return n | uint32_t(u16()) << 16; }
};
uint32_t hash(const uint8_t *p, size_t n) { uint32_t h = 2166136261u; for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u; return h; }
bool valid(const State &s) {
    if (s.food > 100 || s.clean > 100 || s.joy > 100 || s.energy > 100 || s.snacks > 20 || s.coins > 9999 || s.growth > 999) return false;
    if (s.stage != (s.growth >= 120 ? 2 : s.growth >= 45 ? 1 : 0) || (s.stage < 2 ? s.branch != 0 : s.branch < 1 || s.branch > 3)) return false;
    if (s.decor_owned > 15 || s.decor_on > 15 || (s.decor_on & ~s.decor_owned) || s.selection > 5 || s.room > 1 || s.sleeping > 1 || s.last_game > 1) return false;
    if (uint8_t(s.mode) > uint8_t(Mode::RESULT) || s.pet_x < 84 || s.pet_x > 204 || s.basket_x < 20 || s.basket_x > 268) return false;
    if (!s.seed || s.needs_ms >= 12000 || s.harvest_ms >= 30000 || s.game_ms > 15000 || s.spawn_ms >= 900 || s.score > 40 || s.attempts > 20 || s.reward > 42 || s.beat_index > 8 || s.beat_done > 1) return false;
    if (s.best_catch > 40 || s.best_rhythm > 8) return false;
    for (unsigned i = 0; i < 3; ++i) if (s.affinity[i] > s.growth || s.cooldown[i] > 5000) return false;
    if ((s.mode == Mode::GAMES && s.selection > 1) || (s.mode == Mode::DECOR && s.selection > 3)) return false;
    for (const Drop &d : s.drops) if (d.x < 0 || d.x > 288 || d.y < -160 || d.y > 800 || d.kind > 1 || d.active > 1) return false;
    if (s.mode == Mode::RHYTHM && (s.game_ms > 11200 || s.score > 8 || s.attempts > 8)) return false;
    if (s.mode == Mode::CATCH && s.last_game != 0) return false;
    if (s.mode == Mode::RHYTHM && s.last_game != 1) return false;
    return true;
}
}
void fresh(State &s) { s = State{}; }
const char *name(const State &s) { return s.stage < 2 ? "PIP" : s.branch == 1 ? "MOSSBUN" : s.branch == 2 ? "SUNFOX" : "MOONMOTH"; }
const char *stage_name(const State &s) { return s.stage == 0 ? "BUD" : s.stage == 1 ? "SPROUT" : "BLOOM"; }
void home(State &s) { s.mode = Mode::HOME; s.sleeping = 0; s.reward = 0; }
Event action(State &s, Action a) {
    if (s.mode != Mode::HOME) return Event::NONE;
    if (a == Action::PLAY) { s.sleeping = 0; s.mode = Mode::GAMES; s.selection = 0; return Event::NONE; }
    if (a == Action::DECOR) { s.sleeping = 0; s.mode = Mode::DECOR; s.selection = 0; return Event::NONE; }
    if (a == Action::REST) {
        s.sleeping = !s.sleeping;
        return s.sleeping ? Event::RESTING : Event::AWAKE;
    }
    if (s.sleeping) return Event::WAIT;
    unsigned i = unsigned(a);
    if (i > 2 || s.cooldown[i]) return Event::WAIT;
    Event e = Event::NONE;
    unsigned xp = 0, affinity = 0;
    if (a == Action::FEED) {
        if (!s.snacks) return Event::NO_FOOD;
        if (s.food > 85) return Event::WAIT;
        --s.snacks; s.food = plus(s.food, 28); xp = 7; e = Event::FED;
    } else if (a == Action::WASH) {
        // Gentle grooming also builds a bond when already clean.
        s.clean = plus(s.clean, 32); xp = 7; e = Event::WASHED;
    } else {
        s.joy = plus(s.joy, 9); xp = 3; affinity = 2; e = Event::PETTED;
    }
    s.cooldown[i] = a == Action::WASH ? 5000 : 3000;
    Event evolution = grow(s, xp, affinity);
    return evolution == Event::EVOLVED ? evolution : e;
}
void start(State &s, uint8_t game) {
    s.mode = game ? Mode::RHYTHM : Mode::CATCH;
    s.last_game = game ? 1 : 0; s.game_ms = s.spawn_ms = 0;
    s.score = s.attempts = s.reward = 0; s.beat_index = s.beat_done = 0;
    s.sleeping = 0; s.basket_x = 144;
    for (Drop &d : s.drops) d = Drop{};
}
Event decorate(State &s, unsigned item) {
    if (item > 3) return Event::NONE;
    uint8_t bit = uint8_t(1u << item);
    if (s.decor_owned & bit) { s.decor_on ^= bit; return Event::TOGGLED; }
    if (s.coins < DECOR_COST[item]) return Event::POOR;
    s.coins = uint16_t(s.coins - DECOR_COST[item]); s.decor_owned |= bit; s.decor_on |= bit;
    return Event::BOUGHT;
}
Event beat(State &s) {
    if (s.mode != Mode::RHYTHM || s.beat_index >= 8 || s.beat_done) return Event::NONE;
    int32_t delta = int32_t(s.game_ms) - int32_t(1500 + s.beat_index * 1200);
    // A premature hit consumes this bloom, preventing button mashing rewards.
    if (delta < -650 || delta > 350) return Event::NONE;
    s.beat_done = 1; ++s.attempts;
    if (delta >= -230 && delta <= 230) { ++s.score; return Event::BEAT; }
    return Event::MISSED;
}
Event step(State &s, uint32_t ms) {
    if (ms > 100) ms = 100;
    uint32_t before = s.active_ms;
    if (s.active_ms <= 0xffffffffu - ms) s.active_ms += ms;
    for (unsigned i = 0; i < 3; ++i) s.cooldown[i] = uint16_t(s.cooldown[i] > ms ? s.cooldown[i] - ms : 0);
    Event event = Event::NONE;
    s.needs_ms += ms; s.harvest_ms += ms;
    if (s.needs_ms >= 12000) {
        s.needs_ms -= 12000;
        if (s.food) --s.food;
        if (s.clean) --s.clean;
        if (s.joy && !s.sleeping) --s.joy;
        if (s.energy && !s.sleeping) --s.energy;
    }
    if (s.harvest_ms >= 30000) {
        s.harvest_ms -= 30000;
        if (s.snacks < 20) { ++s.snacks; event = Event::HARVEST; }
    }
    if (s.sleeping && before / 1000 != s.active_ms / 1000 && s.energy < 100) {
        s.energy = plus(s.energy, 2);
        if (s.energy == 100) { s.sleeping = 0; event = grow(s, 6, 2); if (event == Event::NONE) event = Event::AWAKE; }
    }
    if (s.mode == Mode::CATCH) {
        // Persist the exact terminal boundary even when a real frame overshoots.
        s.game_ms = s.game_ms + ms > 15000 ? 15000 : s.game_ms + ms;
        s.spawn_ms += ms;
        if (s.spawn_ms >= 900) {
            s.spawn_ms -= 900;
            for (Drop &d : s.drops) if (!d.active) {
                d.x = int16_t(24 + rng(s) % 240); d.y = 0; d.kind = uint8_t(rng(s) % 5 == 0); d.active = 1; break;
            }
        }
        for (Drop &d : s.drops) if (d.active) {
            // Quarter-pixel coordinates preserve movement at a 20 ms tick.
            d.y = int16_t(d.y + int(ms * (d.kind ? 5 : 4) / 10));
            if (d.y >= 432 && d.y <= 508 && d.x >= s.basket_x - 22 && d.x <= s.basket_x + 22) {
                d.active = 0; s.score = uint16_t(s.score + (d.kind ? 2 : 1)); event = Event::CAUGHT;
            } else if (d.y > 520) { d.active = 0; event = Event::MISSED; }
        }
        if (s.game_ms >= 15000) return finish(s);
    } else if (s.mode == Mode::RHYTHM) {
        s.game_ms = s.game_ms + ms > 11200 ? 11200 : s.game_ms + ms;
        while (s.beat_index < 8 && s.game_ms > 1850u + s.beat_index * 1200u) {
            if (!s.beat_done) { ++s.attempts; event = Event::MISSED; }
            ++s.beat_index; s.beat_done = 0;
        }
        if (s.game_ms >= 11200) return finish(s);
    }
    return event;
}
size_t encode(const State &s, uint8_t *out, size_t capacity) {
    if (!out || capacity < SAVE_BYTES || !valid(s)) return 0;
    memset(out, 0, SAVE_BYTES); memcpy(out, "PGD1", 4); out[4] = 1;
    Writer w{out};
    w.u8(s.food); w.u8(s.clean); w.u8(s.joy); w.u8(s.energy);
    w.u8(s.snacks); w.u8(s.stage); w.u8(s.branch); w.u8(s.decor_owned); w.u8(s.decor_on);
    w.u8(s.selection); w.u8(s.room); w.u8(s.sleeping); w.u8(s.last_game); w.u8(uint8_t(s.mode));
    w.u16(s.coins); w.u16(s.growth);
    for (unsigned i = 0; i < 3; ++i) w.u16(s.affinity[i]);
    w.u16(s.rounds); w.u16(s.best_catch); w.u16(s.best_rhythm);
    w.u32(s.active_ms); w.u32(s.needs_ms); w.u32(s.harvest_ms);
    w.u16(uint16_t(s.pet_x)); w.u16(uint16_t(s.basket_x));
    w.u32(s.game_ms); w.u32(s.spawn_ms); w.u32(s.seed);
    w.u16(s.score); w.u16(s.attempts); w.u16(s.reward); w.u8(s.beat_index); w.u8(s.beat_done);
    for (const Drop &d : s.drops) { w.u16(uint16_t(d.x)); w.u16(uint16_t(d.y)); w.u8(d.kind); w.u8(d.active); }
    for (unsigned i = 0; i < 3; ++i) w.u16(s.cooldown[i]);
    uint32_t h = hash(out, SAVE_BYTES - 4);
    for (unsigned i = 0; i < 4; ++i) out[SAVE_BYTES - 4 + i] = uint8_t(h >> (i * 8));
    return SAVE_BYTES;
}
bool decode(State &s, const uint8_t *data, size_t size) {
    if (!data || size != SAVE_BYTES || memcmp(data, "PGD1", 4) || data[4] != 1 || data[5] || data[6] || data[7]) return false;
    uint32_t expected = 0; for (unsigned i = 0; i < 4; ++i) expected |= uint32_t(data[SAVE_BYTES - 4 + i]) << (8 * i);
    if (hash(data, SAVE_BYTES - 4) != expected) return false;
    State next{}; Reader r{data};
    next.food = r.u8(); next.clean = r.u8(); next.joy = r.u8(); next.energy = r.u8();
    next.snacks = r.u8(); next.stage = r.u8(); next.branch = r.u8(); next.decor_owned = r.u8(); next.decor_on = r.u8();
    next.selection = r.u8(); next.room = r.u8(); next.sleeping = r.u8(); next.last_game = r.u8(); next.mode = Mode(r.u8());
    next.coins = r.u16(); next.growth = r.u16();
    for (unsigned i = 0; i < 3; ++i) next.affinity[i] = r.u16();
    next.rounds = r.u16(); next.best_catch = r.u16(); next.best_rhythm = r.u16();
    next.active_ms = r.u32(); next.needs_ms = r.u32(); next.harvest_ms = r.u32();
    next.pet_x = int16_t(r.u16()); next.basket_x = int16_t(r.u16());
    next.game_ms = r.u32(); next.spawn_ms = r.u32(); next.seed = r.u32();
    next.score = r.u16(); next.attempts = r.u16(); next.reward = r.u16(); next.beat_index = r.u8(); next.beat_done = r.u8();
    for (Drop &d : next.drops) { d.x = int16_t(r.u16()); d.y = int16_t(r.u16()); d.kind = r.u8(); d.active = r.u8(); }
    for (unsigned i = 0; i < 3; ++i) next.cooldown[i] = r.u16();
    for (; r.i < SAVE_BYTES - 4; ++r.i) if (data[r.i]) return false;
    if (!valid(next)) return false;
    s = next; return true;
}
}
