/* SPDX-License-Identifier: MIT. Independent standalone SAF raster/touch/clock adapter. */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"
#include "scheduler.h"
#include "controls.h"
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
enum { WIDTH=400,HEIGHT=416,GAME_X=72,GAME_Y=52,SCALE=4 };
static uint8_t pixels[WIDTH*HEIGHT],upload[PXA_RASTER_UPLOAD_HEADER_BYTES+WIDTH*HEIGHT];
static struct { pxfb_t fb;saf_clock_t clock;uint32_t seq,generation;int paused,dirty; } fe;
static void rect(int x,int y,int w,int h,uint8_t color) {
    for(int yy=y;yy<y+h;yy++)if(yy>=0&&yy<HEIGHT)
        for(int xx=x;xx<x+w;xx++)if(xx>=0&&xx<WIDTH)pixels[yy*WIDTH+xx]=color;
}
/* Same CC0 four-by-four glyph bytes; UI uses original RGB332 colors unchanged. */
static void text(int x,int y,int scale,const char *value,uint8_t color) {
    for(;*value;value++,x+=5*scale){uint8_t glyph[2];SAF_getFontCharacter((uint8_t)*value,glyph);
        for(int i=0;i<16;i++)if(glyph[i/8]&(1u<<(i%8)))rect(x+(i%4)*scale,y+(i/4)*scale,scale,scale,color);}
}
static void pxa_saf_button(int x,int y,int w,int h,const char *label,int active) {
    rect(x,y,w,h,active?SAF_COLOR_GREEN_DARK:SAF_COLOR_GRAY_DARK);
    int count=(int)strlen(label);text(x+(w-count*15)/2,y+(h-12)/2,3,label,SAF_COLOR_WHITE);
}
static void hexlog(const char *kind,uint32_t frame,unsigned part,const uint8_t *data,unsigned count) {
    const char *digits="0123456789abcdef";char hex[65];for(unsigned i=0;i<count;i++){hex[2*i]=digits[data[i]>>4];hex[2*i+1]=digits[data[i]&15];}hex[2*count]=0;
    pxlog_t log;pxlog_init(&log);pxlog_text(&log,kind);pxlog_field(&log,"frame",(int32_t)frame);pxlog_field(&log,"part",(int32_t)part);pxlog_text(&log," data=");pxlog_text(&log,hex);pxlog_send(&log);
}
static void trace(const char *action) {
#if PXA_RETRO_TRACE

    uint8_t state_wire[1024];unsigned state_bytes=saf_pack_state(state_wire);
    pxlog_t log;pxlog_init(&log);pxlog_text(&log,"SAF2 STATE action=");pxlog_text(&log,action);
    pxlog_field(&log,"game",(int32_t)SAF_ENTRY_ID);pxlog_field(&log,"entry",SAF_ENTRY_ID);
    pxlog_field(&log,"frame",(int32_t)_SAF_frame);pxlog_field(&log,"draw_frame",(int32_t)saf_io.render_frame);
    pxlog_field(&log,"rng",_SAF_currentRandom);pxlog_field(&log,"seq",(int32_t)fe.seq);pxlog_field(&log,"gen",(int32_t)fe.generation);
    pxlog_field(&log,"menu_item",(int32_t)saf_menu_item());
    pxlog_field(&log,"state",(int32_t)saf_current_state());pxlog_field(&log,"bytes",(int32_t)state_bytes);
    pxlog_field(&log,"paused",fe.paused);pxlog_field(&log,"fault",saf_io.fault);pxlog_field(&log,"dropped",(int32_t)fe.clock.dropped);pxlog_text(&log," phase=post");pxlog_send(&log);
    pxlog_init(&log);pxlog_text(&log,"SAF2 HASH");pxlog_field(&log,"frame",(int32_t)_SAF_frame);
    pxlog_field(&log,"pixels",(int32_t)saf_hash(saf_io.screen,4096));pxlog_field(&log,"memory",(int32_t)saf_hash(state_wire,state_bytes));pxlog_field(&log,"save",(int32_t)saf_hash(saf_io.saved,32));pxlog_send(&log);
    pxlog_init(&log);pxlog_text(&log,"SAF2 BTN");pxlog_field(&log,"frame",(int32_t)_SAF_frame);pxlog_field(&log,"mask",saf_io.sampled);pxlog_field(&log,"applied",saf_io.applied);pxlog_field(&log,"pointer",saf_io.pointer);pxlog_field(&log,"controller",saf_io.controller);pxlog_field(&log,"pending",saf_io.pending);
    const char *keys[]={"up","down","left","right","a","b","c"};for(unsigned i=0;i<7;i++)pxlog_field(&log,keys[i],_SAF_buttonStates[i]);pxlog_send(&log);
    pxlog_init(&log);pxlog_text(&log,"SAF2 INPUT");pxlog_field(&log,"frame",(int32_t)_SAF_frame);pxlog_field(&log,"pending_pointer",saf_io.pending_pointer);pxlog_field(&log,"pending_controller",saf_io.pending_controller);pxlog_send(&log);
    for(unsigned i=0;i<state_bytes;i+=32)hexlog("SAF2 MEM",_SAF_frame,i/32,state_wire+i,state_bytes-i<32?state_bytes-i:32);
    hexlog("SAF2 SAVE",_SAF_frame,0,saf_io.saved,32);
    pxlog_init(&log);pxlog_text(&log,"SAF PROFILE silent=1 disk_persistence=0 session_save_bytes=32 tick_ms=40 scale=4");pxlog_send(&log);
    pxlog_init(&log);pxlog_text(&log,"SAF2 END");pxlog_field(&log,"frame",(int32_t)_SAF_frame);pxlog_field(&log,"seq",(int32_t)fe.seq);pxlog_field(&log,"gen",(int32_t)fe.generation);pxlog_send(&log);

#else
    (void)action;
#endif
}
static void draw(void) {
    memset(pixels,SAF_COLOR_GRAY_DARK,sizeof(pixels));
    pxa_saf_button(8,8,120,30,"NEW",0);pxa_saf_button(140,8,120,30,"RESTART",0);pxa_saf_button(272,8,120,30,"MENU",fe.paused);
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)rect(GAME_X+(int)x*SCALE,GAME_Y+(int)y*SCALE,SCALE,SCALE,saf_io.screen[y*64+x]);
    pxa_saf_button(58,316,42,30,"^",saf_io.pointer&1);pxa_saf_button(58,376,42,30,"v",saf_io.pointer&2);
    pxa_saf_button(16,346,42,30,"<",saf_io.pointer&4);pxa_saf_button(100,346,42,30,">",saf_io.pointer&8);
    pxa_saf_button(184,340,64,64,"A",saf_io.pointer&16);pxa_saf_button(250,340,64,64,"B",saf_io.pointer&32);pxa_saf_button(316,340,64,64,"C",saf_io.pointer&64);
#if SAF_ENTRY_ID == 1
    pxa_saf_button(184,316,94,20,"B+<",saf_io.pointer==36);pxa_saf_button(282,316,98,20,"B+>",saf_io.pointer==40);
#else
    text(184,316,2,"D-PAD + A/B/C",SAF_COLOR_WHITE);
#endif
    if(fe.paused){rect(70,112,260,150,SAF_COLOR_GRAY_DARK);text(137,128,3,"PAUSED",SAF_COLOR_WHITE);pxa_saf_button(108,168,184,34,"CONTINUE",0);pxa_saf_button(108,220,184,34,"NEW GAME",0);}
    if(saf_io.fault){rect(70,112,260,150,SAF_COLOR_RED_DARK);text(95,144,3,"CORE SAFETY STOP",SAF_COLOR_WHITE);text(94,180,2,"NEW OR HOST HOME",SAF_COLOR_WHITE);}
    fe.dirty=0;if(fe.fb.ready&&pxfb_present(&fe.fb)<0){pxa_log_write(4,"SAF raster failed");__builtin_trap();}
}
static void command(unsigned action) {
    saf_release_all();saf_clock_pause(&fe.clock);fe.seq++;
    if(action<2){if(action)saf_restart_game();else saf_new_game();fe.generation++;fe.paused=0;trace(action?"restart":"new");}
    else{fe.paused=1;trace("menu");}fe.dirty=1;
}
static void pointer(void) {
    int x=fe.fb.pointer_x,y=fe.fb.pointer_y;unsigned phase=fe.fb.pointer_phase;
    if(phase==PXA_POINTER_CANCEL){saf_release_all();fe.seq++;trace("cancel");fe.dirty=1;return;}
    if(phase==PXA_POINTER_UP){saf_pointer_mask(0,1);fe.seq++;trace("up");fe.dirty=1;return;}
    if(phase!=PXA_POINTER_DOWN&&phase!=PXA_POINTER_MOVE)return;
    if(fe.paused){if(phase!=PXA_POINTER_DOWN)return;
        if(x>=108&&x<292&&y>=168&&y<202){saf_resume();saf_clock_pause(&fe.clock);fe.paused=0;fe.seq++;trace("continue");fe.dirty=1;}
        else if(x>=108&&x<292&&y>=220&&y<254)command(0);return;}
    if(phase==PXA_POINTER_DOWN&&y>=8&&y<38){if(x>=8&&x<128)command(0);else if(x>=140&&x<260)command(1);else if(x>=272&&x<392)command(2);return;}
    if(saf_io.fault)return;saf_pointer_mask((uint8_t)saf_touch_mask(x,y),0);
    fe.seq++;trace(phase==PXA_POINTER_DOWN?"down":"move");fe.dirty=1;
}
int32_t pxa_app_start(const uint8_t *config,uint32_t bytes) {
    memset(&fe,0,sizeof(fe));saf_boot();fe.generation=1;fe.paused=1;fe.dirty=1;
    fe.fb.width=WIDTH;fe.fb.height=HEIGHT;fe.fb.pixels=pixels;fe.fb.upload=upload;fe.fb.upload_bytes=sizeof(upload);
    for(unsigned i=0;i<256;i++)fe.fb.palette[i]=saf_rgb565((uint8_t)i);
    trace("start");return pxfb_start(&fe.fb,config,bytes,40);
}
int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t size) {
    uint32_t previous_buttons=fe.fb.buttons;uint32_t kind=pxfb_event(&fe.fb,bytes,size);
    if(kind&PXFB_ERROR){pxa_log_write(4,"SAF render context failed");__builtin_trap();}
    if(kind&PXFB_POINTER)pointer();
    if(kind&PXFB_CONTROLLER){uint32_t pressed=fe.fb.buttons&~previous_buttons;
if(fe.paused&&(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START))){saf_resume();saf_clock_pause(&fe.clock);fe.paused=0;fe.dirty=1;}
else if(!fe.paused&&(pressed&PXA_CONTROLLER_START))command(2);
else if(!fe.paused&&!saf_io.fault)saf_controller_mask((uint8_t)(fe.fb.buttons&127));fe.seq++;trace("controller");}
    if(kind&PXFB_LIFECYCLE){saf_release_all();saf_clock_pause(&fe.clock);fe.seq++;trace("lifecycle");fe.dirty=1;}
    if(kind&PXFB_BACK){saf_release_all();saf_clock_pause(&fe.clock);fe.paused=1;fe.seq++;trace("back");fe.dirty=1;}
    if((kind&PXFB_TICK)&&!fe.paused&&!saf_io.fault){unsigned due=saf_clock_due(&fe.clock,(uint32_t)(fe.fb.timestamp_us/1000));
        for(unsigned i=0;i<due;i++){if(!saf_frame())fe.paused=1;trace("tick");if(saf_io.fault||fe.paused)break;}if(due)fe.dirty=1;}
    if((kind&PXFB_READY)||fe.dirty)draw();return kind?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(uint32_t reason) { (void)reason;saf_release_all();saf_clock_pause(&fe.clock);trace("stop");pxfb_stop(&fe.fb); }
void __wrap___assert_fail(const char *a,const char *b,unsigned c,const char *d) { (void)a;(void)b;(void)c;(void)d;pxa_log_write(4,"SAF libc assertion");__builtin_trap(); }
void __wrap_abort(void) { pxa_log_write(4,"SAF libc abort");__builtin_trap(); }
void __wrap_exit(int code) { (void)code;pxa_log_write(4,"SAF libc exit");__builtin_trap(); }
