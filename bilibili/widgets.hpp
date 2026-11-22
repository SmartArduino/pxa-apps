// SPDX-License-Identifier: MIT
#pragma once
#include "../common/media/canvas.hpp"
namespace bili::ui {
struct Palette {
    bool dark=false;
    uint32_t background=0xf7f8fa,surface=0xffffff,primary=0xfb7299,on_primary=0xffffff,
        text=0x24292f,muted=0x8b9099,border=0xe9edf2,fill=0xf0f2f5,selection=0xffe6ef,danger=0xd45a67;
};
inline Palette colors;
inline uint32_t mix(uint32_t a,uint32_t b,unsigned weight) {
    uint32_t result=0;for(unsigned shift:{0,8,16})result|=((((a>>shift)&255)*(100-weight)+((b>>shift)&255)*weight)/100)<<shift;
    return result;
}
inline void theme(const pxa_ui_theme_t& t) {
    colors.dark=t.color_scheme==PXA_UI_COLOR_SCHEME_DARK;
    colors.background=t.rgba[PXA_UI_THEME_BACKGROUND]>>8;colors.surface=t.rgba[PXA_UI_THEME_SURFACE]>>8;
    colors.primary=t.rgba[PXA_UI_THEME_PRIMARY]>>8;colors.on_primary=t.rgba[PXA_UI_THEME_ON_PRIMARY]>>8;
    colors.text=t.rgba[PXA_UI_THEME_TEXT]>>8;colors.muted=t.rgba[PXA_UI_THEME_MUTED]>>8;
    colors.border=t.rgba[PXA_UI_THEME_BORDER]>>8;colors.danger=t.rgba[PXA_UI_THEME_DANGER]>>8;
    colors.fill=mix(colors.surface,colors.text,7);colors.selection=mix(colors.surface,colors.primary,16);
}
// Supersampled original PNGs retain alpha edges at fractional display density.
inline void icon(media::Canvas& c,const char *name,int x,int y,uint32_t color,int size=24) {
    unsigned luminance=((color>>16)&255)*299+((color>>8)&255)*587+(color&255)*114;
    unsigned tint=luminance>=128000?0xffffff:0x24292f;
    unsigned physical=(uint64_t(size)*c.density+32768)/65536,source=physical>48?96:48;
    char path[96];std::snprintf(path,sizeof(path),"assets/ui/%s-%06x-%u.png",name,tint,source);
    pxa_canvas_image(&c.frame,x-size/2+c.viewport.content.x,y-size/2+c.viewport.content.y,size,size,255,2,path,std::strlen(path));
}

inline void label(media::Canvas& c,int x,int y,int w,int h,const char *value,uint32_t color,unsigned align=0,unsigned role=0) {
    if(w<=0||h<=0)return;
    c.clip(x,y,w,h);
    pxa_canvas_text_box_role(&c.frame,x+c.viewport.content.x,y+c.viewport.content.y,w,h,
        pxa_canvas_rgba(color),role,align,PXA_CANVAS_TEXT_ALIGN_MIDDLE,value,std::strlen(value));
    c.unclip();
}
inline void pill(media::Canvas& c,unsigned id,int x,int y,int w,const char *value,bool selected,uint32_t accent,uint32_t text,uint32_t background) {
    c.rect(x,y,w,30,selected?accent:background,15);
    label(c,x+4,y,w-8,30,value,selected?colors.on_primary:text,1);
    c.hit(id,x,y,w,30);
}
struct DetailLayout {
    bool split;
    int cover_x,cover_y,cover_w,cover_h,info_x,info_y,info_w,body_bottom;
};
inline DetailLayout detail_layout(int w,int h) {
    const bool split=w>=440 && w>h*5/4;
    const int cw=split?(w-40)/2:std::min(w-24,std::max(160,(h-190)*16/9));
    const int ch=split?std::min(cw*9/16,std::max(60,h-130)):std::min(190,cw*9/16);
    return {split,split?12:(w-cw)/2,54,cw,ch,split?cw+28:14,split?54:54+ch+12,split?w-cw-42:w-28,h-66};
}
}
