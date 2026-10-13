#pragma once
#include "voxel_items.hpp"
#include <pxa/game.hpp>
namespace voxel {
// Small pixel-art tools in the existing DrawList. No extra atlas or Surface.
inline void draw_tool_icon(pxa::game::Frame& frame,std::uint8_t item,
                           float x,float y,float width,float height)noexcept {
    auto rect=[&](int px,int py,int w,int h,std::uint16_t color){
        auto qx=[&](int a){return std::int16_t(std::lround((x+width*a/16)*16));};
        auto qy=[&](int a){return std::int16_t(std::lround((y+height*a/16)*16));};
        frame.quad({qx(px),qy(py),qx(px+w),qy(py),qx(px+w),qy(py+h),qx(px),qy(py+h)},{color});
    };
    for(int i=0;i<4;++i)rect(3+i,12-2*i,2,3,0x9b64);
    if(item==kStick)return;
    const auto head=std::uint16_t(item>=kStonePickaxe?0xb5b6:0xc488);
    switch(tool_kind(item)){
    case Tool::pickaxe:rect(2,2,11,2,head);rect(2,4,2,3,head);rect(11,4,2,3,head);rect(6,4,2,3,0x9b64);break;
    case Tool::axe:rect(7,2,6,2,head);rect(8,4,5,3,head);rect(7,7,4,1,head);break;
    case Tool::shovel:rect(9,1,4,2,head);rect(8,3,6,3,head);rect(9,6,4,1,head);rect(7,6,2,2,0x9b64);break;
    default:break;
    }
}
} // namespace voxel
