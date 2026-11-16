// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace relic {
enum class Mode : uint8_t { Title, Running, Paused, Dead, Help };
enum class Action : uint8_t { Left, Right, Jump, Slide, Confirm, Pause, Rescue };
enum class Kind : uint8_t { Rock, Root, Gate, Gap, Magnet, Shield };
enum class Feedback : uint8_t { None, Jump, Slide, Dodge, Turn, Quest, Rush, Shield, Rescue };
struct Obstacle { float distance; int8_t lane; Kind kind; bool passed; };
// Preserve the 1.0 save schema and byte encoding; no migration or lost progress.
struct Save { uint32_t magic=0x524c4331, best=0, coins=0, runs=0; uint8_t quality=0, sound=1; uint8_t reserved[2]{}; };
struct State {
    Mode mode=Mode::Title;
    float distance=0, speed=8.8f, elapsed=0, lane_x=0, jump=0, jump_t=0, slide=0;
    float shield=0, magnet=0, flash=0, turn_fx=0, death_t=0, combo_t=0;
    float next_turn=180, next_spawn=30, next_coin=12;
    float energy=0, rush=0, feedback_t=0, chase=0, landing=0, pose_slide=0;
    float turn_lean=0, slide_age=0;
    int lane=0, turn_direction=1, queued_turn=0, turns=0, combo=0;
    uint32_t seed=0x6d2b79f5, coins=0, score=0, steps=0, pickups=0;
    uint32_t gold_score=0, bonus=0, banked_coins=0, avoided=0, best_combo=0, quests=0;
    uint32_t quest_base=0; uint8_t rescues=0;
    Feedback feedback=Feedback::None;
    const char* reason="";
    Obstacle objects[20]{};
    uint8_t count=0;
    bool saved=false,record=false,slide_queued=false;
};
uint32_t random(State& s);
void start(State& s, uint32_t seed);
void action(State& s, Action a);
void update(State& s,float dt);
int multiplier(const State& s);
uint32_t quest_goal(const State& s);
uint32_t quest_progress(const State& s);
int coin_lane(int index);
float coin_height(int index);
bool coin_taken(const State& s, int index);
}
