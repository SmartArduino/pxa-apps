// SPDX-License-Identifier: MIT
#pragma once
#include "filtered_art.hpp"
#include <cmath>
namespace garden {
inline unsigned animation_frame(garden_art::Clip clip,float time){return clip.first+(unsigned(std::max(0.f,time)/clip.seconds*clip.count)%clip.count);}
// Convert the common canvas/foot anchor once, after movement. Frame dimensions
// depend on scale only, so moving a sprite never changes its cached size.
inline Rect actor_bounds(garden_art::Frame f,float cx,float ground,int unit,float zoom){
 float factor=float(unit)*zoom/32.f;
 return {int(std::lround(cx*zoom))+int(std::lround((float(f.ox)-f.cw*.5f)*factor)),
         int(std::lround(ground*zoom))+int(std::lround((float(f.oy)-f.ch)*factor)),
         std::max(1,int(std::lround(f.w*factor))),std::max(1,int(std::lround(f.h*factor)))};
}
}
