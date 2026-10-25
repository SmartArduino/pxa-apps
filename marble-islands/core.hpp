// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace marble {
constexpr int SIZE=7, LEVELS=8;
constexpr float RADIUS=.18f;
struct Level { const char *name; const char *hint; const char *rows[SIZE]; uint32_t gold_ms; uint16_t color; };
extern const Level levels[LEVELS];
enum class Phase : uint8_t { ROLLING, FALLING, WON };
struct Run {
    float x,y,vx,vy;
    uint32_t elapsed_ms;
    uint16_t coins, fall_ms;
    uint8_t level;
    Phase phase;
};
struct Progress { uint32_t best_ms[LEVELS]; uint8_t medals[LEVELS]; uint8_t unlocked; };
struct State { Progress progress; Run run; };
struct Input { float x,y; bool brake; };
enum Event : unsigned { NONE=0, COLLECT=1, BOUNCE=2, FALL=4, WIN=8 };
void init(State &s);
bool start(State &s,unsigned level);
char cell(unsigned level,int x,int y);
unsigned coin_count(unsigned level);
unsigned collected(const Run &r);
bool gate_open(const Run &r,int x,int y);
bool supported(unsigned level,float x,float y);
unsigned step(State &s,Input input,unsigned milliseconds); // bounded to 100 ms, in 10 ms steps
size_t serialize(const State &s,uint8_t *data,size_t capacity);
bool load(State &s,const uint8_t *data,size_t size); // atomic validation
}
