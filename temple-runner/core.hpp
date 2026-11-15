// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace relic {
enum class Mode : uint8_t { Title, Running, Paused, Dead, Help };
enum class Action : uint8_t { Left, Right, Jump, Slide, Confirm, Pause };
enum class Kind : uint8_t { Rock, Root, Gate, Gap, Magnet, Shield };
struct Obstacle { float distance; int8_t lane; Kind kind; bool passed; };
struct Save { uint32_t magic=0x524c4331, best=0, coins=0, runs=0; uint8_t quality=0, sound=1; uint8_t reserved[2]{}; };
struct State {
    Mode mode=Mode::Title;
    float distance=0, speed=8.8f, elapsed=0, lane_x=0, jump=0, jump_t=0, slide=0;
    float shield=0, magnet=0, flash=0, turn_fx=0, death_t=0, combo_t=0;
    float next_turn=180, next_spawn=30, next_coin=12;
    int lane=0, turn_direction=1, queued_turn=0, turns=0, combo=0;
    uint32_t seed=0x6d2b79f5, coins=0, score=0, steps=0, pickups=0;
    const char* reason="";
    Obstacle objects[20]{};
    uint8_t count=0;
    bool saved=false;
};
uint32_t random(State& s);
void start(State& s, uint32_t seed);
void action(State& s, Action a);
void update(State& s,float dt);
int coin_lane(int index);
float coin_height(int index);
bool coin_taken(const State& s, int index);
}
