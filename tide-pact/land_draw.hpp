#pragma once

#include "draw.hpp"

namespace tide {

inline void draw_land_ground(Draw& d,Rect tile,LandGround ground,int cell,bool coast) {
    const bool paved=ground==LandRoad || ground==LandGoal;
    d.fill(tile,paved?rgb(coast?0x947652:0x96734d):rgb(coast?0x66654a:0x51603a));
    for (int i=0;i<12;++i) {
        const int x=tile.x+2+(cell*13+i*11)%(tile.w-4);
        const int y=tile.y+2+(cell*7+i*13)%(tile.h-4);
        d.fill({x,y,2+i%3,1},paved?rgb(i%2?0xa9895b:0x795e4b):rgb(i%2?0x727d4c:0x485336));
    }
    const int mid=tile.x+tile.w/2, floor=tile.bottom()-5;
    if (ground==LandBarrier) {
        if (coast) {
            d.fill({tile.x+3,tile.y+5,tile.w-6,tile.h-9},rgb(0x67533e));
            d.fill({tile.x+4,tile.y+5,tile.w-8,5},rgb(0xb19268));
            d.line({tile.x+4,tile.y+12},{tile.right()-5,tile.y+12},rgb(0x382f2a));
        } else {
            d.fill({mid-2,floor-5,4,5},rgb(0x64442c));
            d.diamond({mid,floor-tile.h/3},tile.h/3,rgb(0x293f31));
            d.diamond({mid-2,floor-tile.h/3-2},tile.h/4,rgb(0x3f5c3a));
        }
    } else if (ground==LandCover) {
        d.fill({mid-6,floor-7,12,8},rgb(0x54402e));
        d.fill({mid-6,floor-9,12,3},rgb(0xb09563));
        d.line({mid-5,floor-1},{mid+5,floor-6},rgb(0x8e7149));
    } else if (ground==LandGoal) {
        d.line({mid-7,floor-1},{mid-7,floor-13},rgb(0x3e3329));
        d.fill({mid-6,floor-13,11,5},rgb(0xedc584));
        d.fill({mid+2,floor-10,3,2},rgb(0x885e3e));
    }
    d.frame(tile,rgb(0x464a36));
}

} // namespace tide
