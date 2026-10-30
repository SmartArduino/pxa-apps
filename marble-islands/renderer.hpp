// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "core.hpp"
#include <math.h>
namespace marble {
constexpr int VIEW_W=288,VIEW_H=200, MAX_VIEW=320, MAX_VIEW_PIXELS=MAX_VIEW*MAX_VIEW;
constexpr int CAMERA_X=1600,CAMERA_Z=-4600;
inline Input tilt_from_screen(float x,float y,bool brake){
    const float length=sqrtf(float(CAMERA_X*CAMERA_X+CAMERA_Z*CAMERA_Z));
    return {(x*(-CAMERA_Z)+y*CAMERA_X)/length,(-x*CAMERA_X+y*(-CAMERA_Z))/length,brake};
}
// Software rasterization via unchanged small3dlib, no heap, bounded 409,600-byte z buffer.
void render(const Run &run,uint16_t *pixels,uint32_t foreground_ms,int width=VIEW_W,int height=VIEW_H);
}
