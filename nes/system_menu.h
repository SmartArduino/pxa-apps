/* GPL-2.0-or-later. Retained Host UI is present only while emulation is paused.
 * UI 0.6 primitive 12 is also exposed by the C++ SDK as CanvasCommands::text_sized.
 * This bounded C encoder supports the older C SDK used by deployed firmware. */
#pragma once
#include "pxa_canvas.h"
#define NES_UI_SIZED_TEXT (UINT64_C(1)<<11)
static unsigned system_menu_supported;
static uint32_t menu_generation=1,menu_zoom_q16=65536,menu_font_scale_q16=65536;
static uint8_t menu_initialized;
static pxa_canvas_frame_t *menu_commands;
static nes_display_rect_t menu_area;
static unsigned menu_pixels(unsigned dp) {
    uint64_t px=((uint64_t)dp*input_density_q16+65535)>>16;
    return px>UINT32_MAX?UINT32_MAX:(unsigned)px;
}
static int menu_origin(unsigned px) {return ((uint64_t)px*65536+input_density_q16/2)/input_density_q16;}
static int menu_dp(int coordinate) {
    return (int)(((int64_t)coordinate*menu_zoom_q16+input_density_q16/2)/input_density_q16);
}
static void menu_environment(const pxa_ui_environment_t *env) {
    system_menu_supported=(env->features&NES_UI_SIZED_TEXT)!=0;
    unsigned inset[4],radius[4];
    for(unsigned i=0;i<4;++i){inset[i]=menu_pixels(env->safe_insets[i]);radius[i]=menu_pixels(env->corner_radii[i]);}
    menu_area=nes_display_safe_rect(width,height,env->display_shape,inset,radius);
    /* A contradictory Host geometry must never cause division by zero. */
    if(!menu_area.width || !menu_area.height)menu_area=(nes_display_rect_t){0,0,width,height};
    uint32_t sx=(uint64_t)menu_area.width*65536/240,sy=(uint64_t)menu_area.height*65536/240;
    menu_zoom_q16=sx<sy?sx:sy;
    if(!menu_zoom_q16)menu_zoom_q16=1;
    menu_font_scale_q16=env->font_scale_q16?env->font_scale_q16:65536;
    nes_port_log("NES UI font=%s safe=%u,%u,%u,%u shape=%u zoom_q16=%u",
        system_menu_supported?"system":"bitmap",menu_area.x,menu_area.y,menu_area.width,menu_area.height,
        env->display_shape,menu_zoom_q16);
}
static uint32_t menu_rgba(uint16_t rgb) {
    unsigned r=(rgb>>11)*255/31,g=((rgb>>5)&63)*255/63,b=(rgb&31)*255/31;
    return (r<<24)|(g<<16)|(b<<8)|255;
}
static void menu_box(int x,int y,int w,int h,int radius,uint16_t color) {
    if(w<=0 || h<=0)return;
    int xx=menu_dp(x),yy=menu_dp(y);
    pxa_canvas_rect_rgba(menu_commands,xx+menu_origin(menu_area.x),yy+menu_origin(menu_area.y),menu_dp(x+w)-xx,menu_dp(y+h)-yy,
        menu_rgba(color),menu_dp(radius),0,0);
}
static void menu_label(int x,int y,int w,int h,const char *label,uint16_t color,unsigned align) {
    if(w<=0 || h<=0)return;
    size_t size=strlen(label);if(size>UINT16_MAX-24){menu_commands->failed=1;return;}
    /* Size is native pixels; geometry is dp. Do not apply density twice. */
    unsigned pixels=((uint64_t)14*menu_zoom_q16*menu_font_scale_q16+(UINT64_C(1)<<31))>>32;
    unsigned limit=((uint64_t)(h-2)*menu_zoom_q16)>>16;
    if(pixels>limit)pixels=limit;if(pixels<8)pixels=8;if(pixels>128)pixels=128;
    /* Host retains at most 16 font sizes. A bounded family remains reusable
     * after repeated density/font-scale/window changes. */
    static const uint8_t sizes[]={8,10,12,14,16,18,20,22,24,28,32,40};
    unsigned chosen=8;
    for(unsigned i=0;i<sizeof(sizes);++i)if(sizes[i]<=pixels)chosen=sizes[i];
    pixels=chosen;
    int xx=menu_dp(x),yy=menu_dp(y);
    int ok=pxa_canvas_primitive(menu_commands,12,(uint16_t)(24+size)) &&
        pxa_put_u32(menu_commands,xx+menu_origin(menu_area.x)) && pxa_put_u32(menu_commands,yy+menu_origin(menu_area.y)) &&
        pxa_put_u32(menu_commands,menu_dp(x+w)-xx) &&
        pxa_put_u32(menu_commands,menu_dp(y+h)-yy) &&
        pxa_put_u32(menu_commands,menu_rgba(color)) && pxa_put_u16(menu_commands,pixels) &&
        pxa_put_u8(menu_commands,align) && pxa_put_u8(menu_commands,0) &&
        pxa_put_bytes(menu_commands,(const uint8_t*)label,size);
    if(ok)++menu_commands->records;
}
static int menu_dismiss(void) {
    if(!menu_initialized)return surface_visibility(1);
    uint8_t packet[256];pxa_ui_transaction_t tx={0};
    if(!pxa_ui_transaction_begin(&tx,++menu_generation,PXA_UI_TRANSACTION_REPLACE_SURFACE,packet,sizeof(packet)) ||
        !pxa_ui_create(&tx,1,0,0,PXA_UI_NODE_ROOT) ||
        !pxa_ui_set_rgba(&tx,1,PXA_UI_PROPERTY_BACKGROUND,0) ||
        !pxa_ui_set_event_mask(&tx,1,PXA_UI_EVENT_MASK_POINTER|PXA_UI_EVENT_MASK_CONTROLLER_STATE) ||
        !pxa_ui_transaction_commit(&tx)) {
        if(tx.active)pxa_ui_transaction_cancel(&tx);return 0;
    }
    menu_initialized=0;
    nes_port_log("NES MENU removed system-font layer");
    return surface_visibility(1);
}
