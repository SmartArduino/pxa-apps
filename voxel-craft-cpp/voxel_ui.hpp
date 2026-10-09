#pragma once
#include <pxa/ui_display.hpp>
#include <algorithm>
#include <cmath>
namespace voxel {
struct Rect {
    int x=0,y=0,w=0,h=0;
    bool contains(int px,int py)const noexcept{return px>=x&&py>=y&&px<x+w&&py<y+h;}
    int cx()const noexcept{return x+w/2;} int cy()const noexcept{return y+h/2;}
};
struct ControlLayout {
    pxa::ui::DisplayMetrics display;
    float scale=1, text_scale=1, stick_radius=36;
    Rect menu, bag, actions[5], hotbar;
    int slot=22,gap=2;
    int width()const noexcept{return int(display.width);} int height()const noexcept{return int(display.height);}
    // Intersection of the safe rectangle and the rounded outline at both
    // vertical edges of a control. Computed only when metrics change.
    std::pair<int,int> horizontal(int y,int h)const noexcept {
        float left=float(display.safe.left),right=float(width()-int(display.safe.right));
        auto edge=[&](int yy){
            float l=0,r=float(width());
            for(int side=0;side<2;++side)for(int bottom=0;bottom<2;++bottom){
                int index=bottom?(side?2:3):(side?1:0);
                float radius=std::min(float(display.corners[index]),float(std::min(width(),height()))/2);
                if(display.shape==2) radius=float(std::min(width(),height()))/2;
                float dy=bottom?float(yy)-(height()-radius):radius-float(yy);
                if(radius>0&&dy>0){
                    float inset=radius-std::sqrt(std::max(0.f,radius*radius-dy*dy));
                    if(side)r=std::min(r,float(width())-inset);else l=std::max(l,inset);
                }
            }
            left=std::max(left,l);right=std::min(right,r);
        };edge(y);edge(y+h);return {int(std::ceil(left)),int(std::floor(right))};
    }
    Rect row(int y,int h,int desired)const noexcept {
        auto [l,r]=horizontal(y,h);int margin=std::max(2,int(5*scale));
        int w=std::max(1,std::min(desired,r-l-2*margin));return {(l+r-w)/2,y,w,h};
    }
    static ControlLayout make(pxa::ui::DisplayMetrics d)noexcept {
        ControlLayout v;v.display=d;
        // Guard unreasonable host values before conversions and geometry.
        v.display.width=std::clamp(d.width,96u,2047u);v.display.height=std::clamp(d.height,96u,2047u);
        auto& s=v.display.safe;
        s.left=std::min(s.left,v.display.width/3);s.right=std::min(s.right,v.display.width/3);
        s.top=std::min(s.top,v.display.height/3);s.bottom=std::min(s.bottom,v.display.height/3);
        float fit=std::min(float(v.width()-s.left-s.right)/250.f,float(v.height()-s.top-s.bottom)/208.f);
        v.scale=std::clamp(std::min(float(d.density_q16)/65536.f,fit),.4f,3.f);
        v.text_scale=v.scale*std::clamp(float(d.font_scale_q16)/65536.f,1.f,1.4f);
        int pad=std::max(3,int(6*v.scale));int b=std::max(16,int(30*v.scale));
        int y=int(s.top)+pad;auto [l,r]=v.horizontal(y,b);
        v.menu={r-pad-b,y,b,b};
        v.slot=std::max(12,int(23*v.scale));v.gap=std::max(1,int(3*v.scale));
        y=v.height()-int(s.bottom)-pad-v.slot;
        auto bar=v.row(y,v.slot,9*v.slot+8*v.gap);
        v.slot=std::min(v.slot,std::max(8,(bar.w-8*v.gap)/9));
        bar=v.row(y,v.slot,9*v.slot+8*v.gap);v.hotbar=bar;
        v.bag={bar.x+8*(v.slot+v.gap),bar.y,v.slot,v.slot};
        int top=std::max(v.menu.y+b+pad,(bar.y-3*b-2*pad+v.menu.y+b)/2);
        for(int i=0;i<3;++i){int ay=top+i*(b+pad);auto span=v.horizontal(ay,b);v.actions[i]={span.second-pad-b,ay,b,b};}
        v.actions[3]={v.actions[0].x-b-pad,v.actions[0].y+b/2,b,b};
        v.actions[4]={v.actions[3].x,v.actions[3].y+b+pad,b,b};
        v.stick_radius=std::max(18.f,36*v.scale);return v;
    }
};
enum class Screen:std::uint8_t {title,game,pause,settings_page,load_slots,save_slots,inventory,workbench,confirm_title,confirm_overwrite};
enum class UiAction:std::uint8_t {none,new_game,load_slots,settings,resume,save_slots,title,back,slot,confirm,fly,sensitivity,invert,distance,auto_jump,defaults,item,quick_slot};
struct MenuHit {Rect rect;UiAction action=UiAction::none;int argument=0;};
struct Settings {
    std::uint8_t sensitivity=1;bool invert_y=false,auto_distance=true,flight=false,auto_jump=true;
};
} // namespace voxel
