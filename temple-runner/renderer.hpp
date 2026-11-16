// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include "player_pose.hpp"
#include <pxa/game.hpp>
#include <algorithm>
#include <pxa/canvas.hpp>
#include <pxa/ui_display.hpp>
namespace relic {
inline constexpr float camera_height=2.85f,camera_back=4.8f;
struct Face {std::array<int16_t,8> xy,uv;std::array<uint16_t,4> depths;float depth;uint16_t color;int8_t material=-1;uint8_t light=7,palette=0,layer=0;std::array<uint8_t,4> shades{};};
struct Renderer {
    int width=296,height=240,quality=0,count=0,peak_faces=0; float safe_x=0,safe_y=0; bool text_alpha=true; bool textures=true,depth=true;uint8_t mesh_layer=0;uint32_t skipped=0;int theme=0; float time=0,camera_x=0,roll=0,travel=0,bob=0;
    Face faces[960]{};
    uint16_t draw_order[960]{};
    float view_cos=1,view_sin=0,focal=0,screen_y=0;
    struct ClipPlane {V3 normal,absolute;float offset;};
    ClipPlane view_planes[5]{};
    bool object_culling=true;
    uint32_t culled_objects=0;
    pxa::ui::CanvasCommands<4000>* overlay=nullptr;
    const pxa::ui::DisplayMetrics* display=nullptr;
    const pxa::Asset* title_logo=nullptr;
    const pxa::Asset* title_background=nullptr;
    const pxa::Asset* panel_skin=nullptr;
    const pxa::Asset* button_skin=nullptr;
    bool drawing_ui=false;
    float ui_scale() const {
        if(display)return std::clamp(std::min(display->width/296.f,display->height/240.f),.8f,1.6f)*height/display->height;
        return std::clamp(std::min(width/296.f,height/240.f),.8f,1.5f);
    }
    bool upload(pxa::game::Renderer& target);
    bool visible_box(V3 center,V3 half_extent) const;
    void render(pxa::game::Frame& f,const State& s,const Save& save);
    void face(V3 a,V3 b,V3 c,V3 d,uint32_t color,float light=1,int material=-1);
    void box(float x,float y,float z,float w,float h,float d,uint32_t color);
    void limb(V3 a,V3 b,float radius,uint32_t color,int sides=6,bool closed=false);
    void tree(float x,float z,int variant);
    void orb(V3 c,V3 radii,int color,int sides=8);
    void monster(float x,float z);
    void panel(pxa::game::Frame& f,float x,float y,float w,float h,uint32_t fill,uint32_t border);
    void idol(float x,float z,float scale);
    void runner(const State& s);
    void text(pxa::game::Frame& f,const char* str,float x,float y,float scale,uint32_t color,bool center=false);
    void rect(pxa::game::Frame& f,float x,float y,float w,float h,uint32_t color);
};
}
