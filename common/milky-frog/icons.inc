// SPDX-License-Identifier: MIT
// Navigation uses the same Host icon font as the Store app. No app font assets.
struct SystemIcon {milky::Icon kind;milky::Rect box;uint16_t color;bool visible;};
SystemIcon system_icons[32];unsigned system_icon_count;
void system_icon_add(milky::Icon kind,milky::Rect box,uint16_t color){
    if(kind!=milky::Icon::None&&box.w>0&&box.h>0&&system_icon_count<32)
        system_icons[system_icon_count++]={kind,box,color,true};
}
void system_icons_occlude(int x,int y,int w,int h){
    if(w<=2||h<=2)return;
    for(unsigned i=0;i<system_icon_count;++i){auto &icon=system_icons[i];const auto &r=icon.box;
        if(x<r.right()&&x+w>r.x&&y<r.bottom()&&y+h>r.y)icon.visible=false;
    }
}
bool system_icons_encode(pxa_canvas_frame_t *frame){
    for(unsigned i=0;i<system_icon_count;++i){const auto &item=system_icons[i];if(!item.visible)continue;
        const auto &r=item.box;
        const int px=viewport.x+r.x*viewport.physical_width()/WIDTH,py=viewport.y+r.y*viewport.physical_height()/HEIGHT;
        const int pw=std::max(1,viewport.x+r.right()*viewport.physical_width()/WIDTH-px);
        const int ph=std::max(1,viewport.y+r.bottom()*viewport.physical_height()/HEIGHT-py);
        const uint32_t red=((item.color>>11)&31)*255/31,green=((item.color>>5)&63)*255/63,blue=(item.color&31)*255/31;
        const uint32_t color=(red<<24)|(green<<16)|(blue<<8)|255;
        // Store's ICON_BACK and the Host's matching navigation symbols.
        const char *glyph=item.kind==milky::Icon::Back?"\xef\x81\x93":
            item.kind==milky::Icon::Next?"\xef\x81\x94":
            item.kind==milky::Icon::Pause?PXA_CANVAS_ICON_PAUSE:nullptr;
        if(glyph){
            if(!pxa_canvas_text_box_role(frame,system_text_logical(px),system_text_logical(py),
                std::max(1,system_text_logical(pw)),std::max(1,system_text_logical(ph)),color,
                PXA_CANVAS_FONT_ICON,PXA_CANVAS_ALIGN_CENTER,PXA_CANVAS_TEXT_ALIGN_MIDDLE,glyph,3))return false;
            continue;
        }
        const int side=std::min(pw,ph),left=px+(pw-side)/2,top=py+(ph-side)/2;
        const auto x=[&](int u){return system_text_logical(left+(u*side+12)/24);};
        const auto y=[&](int u){return system_text_logical(top+(u*side+12)/24);};
        const auto length=[&](int u){return std::max(1,system_text_logical((u*side+12)/24));};
        const uint16_t stroke=uint16_t(length(2));
        const auto line=[&](int a,int b,int c,int d){return pxa_canvas_line_rgba(frame,x(a),y(b),x(c),y(d),color,stroke);};
        const auto dot=[&](int a,int b){return pxa_canvas_ellipse_rgba(frame,x(a-1),y(b-1),length(2),length(2),color,0,0);};
        const auto rect=[&](int a,int b,int c,int d,int radius,bool filled){return pxa_canvas_rect_rgba(frame,x(a),y(b),std::max(1,x(c)-x(a)),std::max(1,y(d)-y(b)),filled?color:0,uint16_t(length(radius)),filled?0:stroke,color);};
        switch(item.kind){
        case milky::Icon::Lock:
            if(!pxa_canvas_arc(frame,x(12),y(9),length(4),180,360,color,stroke)||!line(8,9,8,11)||!line(16,9,16,11)||
               !rect(5,10,19,21,2,false)||!dot(12,15)||!line(12,15,12,17))return false;break;
        default:break;
        }
    }
    return true;
}
