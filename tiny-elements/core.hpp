// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace elements {
constexpr int W=72,H=48,CELLS=W*H;
constexpr size_t SAVE_SIZE=1780;
enum Material : uint8_t { EMPTY,SAND,WATER,STONE,WOOD,FIRE,STEAM,PLANT,SEED,LAVA,MATERIALS };
enum Ui : uint8_t { WORLD,TASKS,HELP,CONFIRM };
struct State {
    uint8_t cells[CELLS];
    uint32_t rng,steps,scene_start,burned,cooled;
    uint16_t accumulator;
    uint8_t scene,badges,material,brush,paused,ui,pending,target,cx,cy,selected,page,confirm_yes,muted;
};
const char *name(unsigned material);
const char *scene_name(unsigned scene);
const char *objective(unsigned scene);
void init(State &s);
void reset(State &s,unsigned scene);
uint8_t at(const State &s,int x,int y);
bool allowed(unsigned scene,unsigned material);
bool paint(State &s,int x,int y,unsigned material,unsigned brush);
bool stroke(State &s,int x0,int y0,int x1,int y1,unsigned material,unsigned brush);
void step(State &s);
unsigned count(const State &s,unsigned material);
unsigned progress(const State &s);
unsigned goal(unsigned scene);
bool completed(const State &s);
size_t encode(const State &s,uint8_t *out,size_t capacity);
bool decode(State &s,const uint8_t *data,size_t size);
uint32_t checksum(const uint8_t *data,size_t size);
}
