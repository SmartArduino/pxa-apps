// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <pxa/game.hpp>
namespace relic {
struct V3 {float x,y,z;};
struct Face {std::array<int16_t,8> xy;float depth;uint16_t color;};
struct Renderer {
    int width=296,height=240,quality=0,count=0,peak_faces=0; float safe_x=0,safe_y=0; bool text_alpha=true; float time=0,camera_x=0,roll=0,travel=0;
    Face faces[920]{};
    bool upload(pxa::game::Renderer& target);
    void render(pxa::game::Frame& f,const State& s,const Save& save);
    void face(V3 a,V3 b,V3 c,V3 d,uint32_t color,float light=1);
    void box(float x,float y,float z,float w,float h,float d,uint32_t color);
    void limb(V3 a,V3 b,float radius,uint32_t color,int sides=6);
    void tree(float x,float z,int variant);
    void idol(float x,float z,float scale);
    void runner(const State& s);
    void text(pxa::game::Frame& f,const char* str,float x,float y,float scale,uint32_t color,bool center=false);
    void rect(pxa::game::Frame& f,float x,float y,float w,float h,uint32_t color);
};
}
