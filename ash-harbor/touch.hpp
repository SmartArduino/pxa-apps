// SPDX-License-Identifier: MIT
#pragma once
#include "combat.hpp"
namespace ash {
inline constexpr int stickX=42,stickY=205,stickRadius=32;
inline constexpr uint32_t stickDirections[]={Up,Up|Right,Right,Down|Right,Down,Down|Left,Left,Up|Left};
inline uint32_t stickDirection(int dx,int dy){
    if(dx*dx+dy*dy<=36)return 0;
    int ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    uint32_t result=0;
    if(ax*1000>ay*414)result|=dx<0?Left:Right;
    if(ay*1000>ax*414)result|=dy<0?Up:Down;
    return result;
}
}
