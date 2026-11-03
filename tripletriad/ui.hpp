// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "match.hpp"
namespace pxa_tt {
constexpr int Width=400,Height=416;
struct UI {
 Match game;
 uint8_t pixels[Width*Height]{};
 unsigned selected=0, cursor=4, rules_mode=0;
 bool board_focus=false;
 uint32_t seed=1, frames=0;
 UI();
 void draw();
 void pointer(int x,int y);
 void buttons(uint32_t pressed);
 void tick(uint32_t ms);
 void start();
};
}
