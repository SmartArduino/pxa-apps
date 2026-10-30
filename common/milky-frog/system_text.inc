// SPDX-License-Identifier: MIT
// Retained transparent Canvas: all glyphs come from the Host system font.
struct SystemText {int x,y,w,scale,font_size,box_y,box_h;uint16_t color;uint8_t align;bool visible;char text[128];};
SystemText system_texts[96];unsigned system_text_count;
uint8_t system_text_bytes[8192];
uint32_t system_text_generation,system_text_hash,system_text_density=65536;
uint8_t system_text_initialized=1;
int system_text_logical(int native);
#include "native_images.inc"
#include "icons.inc"
bool system_text_create(pxa_ui_transaction_t *tx){
    return pxa_ui_create(tx,2,1,0,PXA_UI_NODE_CANVAS)&&
        pxa_ui_set_length(tx,2,PXA_UI_PROPERTY_WIDTH,PXA_UI_LENGTH_FILL,0)&&
        pxa_ui_set_length(tx,2,PXA_UI_PROPERTY_HEIGHT,PXA_UI_LENGTH_FILL,0)&&
        pxa_ui_set_u8(tx,2,PXA_UI_PROPERTY_COMPOSITION,PXA_UI_COMPOSITION_ALPHA_OVERLAY)&&
        pxa_ui_set_rgba(tx,2,PXA_UI_PROPERTY_BACKGROUND,0)&&pxa_ui_set_event_mask(tx,2,0);
}
void system_text_begin(){system_text_count=system_icon_count=0;native_images_begin();}
void system_text_add(int x,int y,int w,const char *value,uint16_t color,int scale,unsigned align){
    if(!value||!*value||w<=0||scale<1||system_text_count>=96)return;
    const size_t n=strlen(value);if(n>=128)return;
    auto &item=system_texts[system_text_count++];item={x,y,w,scale,0,0,0,color,uint8_t(align),true,{}};memcpy(item.text,value,n+1);
}
void system_text_occlude(int x,int y,int w,int h){
    native_images_occlude(x,y,w,h);
    system_icons_occlude(x,y,w,h);
    if(w<=2||h<=2)return; // Outlines must not erase a centered label's whole box.
    for(unsigned i=0;i<system_text_count;++i){auto &item=system_texts[i];
        // Text lives on the UI plane; later opaque game rectangles hide it.
        const int estimated_width=item.align?item.w:pocket_ui::text_width(item.text,item.scale);
        const int height=item.font_size?item.font_size:milky::text_height(item.scale)-2;
        if(x<item.x+estimated_width&&x+w>item.x&&y<item.y+height&&y+h>item.y)item.visible=false;
    }
}
int system_text_logical(int native){return int((int64_t(native)*65536+system_text_density/2)/system_text_density);}
bool system_text_present(){
    pxa_canvas_frame_t frame;pxa_canvas_begin(&frame,system_text_bytes,sizeof(system_text_bytes));
    if(!native_images_encode(&frame))return false;
    if(!system_icons_encode(&frame))return false;
    for(unsigned i=0;i<system_text_count;++i){const auto &item=system_texts[i];if(!item.visible)continue;
        const int px=viewport.x+item.x*viewport.physical_width()/WIDTH;
        const int pw=std::max(1,viewport.x+(item.x+item.w)*viewport.physical_width()/WIDTH-px);
        const int font=std::clamp((item.font_size?item.font_size:milky::text_height(item.scale)-2)*viewport.physical_height()/HEIGHT,8,128);
        const int box_top=viewport.y+item.box_y*viewport.physical_height()/HEIGHT;
        const int box_height=std::max(1,viewport.y+(item.box_y+item.box_h)*viewport.physical_height()/HEIGHT-box_top);
        // Center in native pixels after scaling, avoiding two rounds of
        // truncation that made button captions sit high on larger screens.
        const int py=item.box_h?box_top+(box_height-font)/2-font/4:
            viewport.y+item.y*viewport.physical_height()/HEIGHT-font/4;
        const uint32_t r=((item.color>>11)&31)*255/31,g=((item.color>>5)&63)*255/63,b=(item.color&31)*255/31;
        // UI 0.6 sized-text, the same primitive as CanvasCommands::text_sized.
        const size_t length=strlen(item.text);
        if(item.box_h&&!pxa_canvas_clip_push(&frame,system_text_logical(px),system_text_logical(box_top),std::max(1,system_text_logical(pw)),std::max(1,system_text_logical(box_height))))return false;
        if(!pxa_canvas_primitive(&frame,12,uint16_t(24+length))||
            !pxa_put_u32(&frame,uint32_t(system_text_logical(px)))||!pxa_put_u32(&frame,uint32_t(system_text_logical(py)))||
            !pxa_put_u32(&frame,uint32_t(std::max(1,system_text_logical(pw))))||!pxa_put_u32(&frame,uint32_t(std::max(1,system_text_logical(font*2+4))))||
            !pxa_put_u32(&frame,(r<<24)|(g<<16)|(b<<8)|255)||!pxa_put_u16(&frame,uint16_t(font))||
            !pxa_put_u8(&frame,item.align)||!pxa_put_u8(&frame,0)||!pxa_put_bytes(&frame,reinterpret_cast<const uint8_t*>(item.text),length))return false;
        if(item.box_h&&!pxa_canvas_clip_pop(&frame))return false;
    }
    uint32_t hash=2166136261u;for(size_t i=0;i<frame.length;++i)hash=(hash^system_text_bytes[i])*16777619u;
    if(hash==system_text_hash){native_images_service();return true;}
    if(!pxa_canvas_present_regions(2,&frame,&system_text_generation,&system_text_initialized,0,nullptr,0,packet,sizeof(packet),nullptr,0))return false;
    system_text_hash=hash;native_images_service();return true;
}
