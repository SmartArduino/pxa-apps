/* SPDX-License-Identifier: MIT. PXA classic-game native display and touch adapter. */
#include "backend.h"
#include "Arduboy2.h"
#define PXFB_NATIVE_IDENTITY 1
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
#include <string.h>
extern "C" const uint8_t *arduboy_pxa_font(void);
enum { W=296,H=240,GX=20,GY=42,SCALE=2 };
static uint8_t pixels[W*H],upload[PXFB_UPLOAD_BYTES];
static struct { pxfb_t fb; uint64_t last_host; int started,paused,dirty; } fe;
static void rect(int x,int y,int w,int h,uint8_t color) {
 for(int yy=y;yy<y+h;yy++)if(yy>=0&&yy<H)for(int xx=x;xx<x+w;xx++)if(xx>=0&&xx<W)pixels[yy*W+xx]=color;
}
static void text(int x,int y,int scale,const char *s,uint8_t color) {
 const uint8_t *font=arduboy_pxa_font();
 for(;*s;++s,x+=6*scale)for(int col=0;col<5;col++) {
  uint8_t bits=font[(uint8_t)*s*5+col];
  for(int row=0;row<8;row++)if(bits&(1u<<row))rect(x+col*scale,y+row*scale,scale,scale,color);
 }
}
static void button(int x,int y,int w,int h,const char *label,int active) {
 rect(x,y,w,h,active?3:2);text(x+(w-(int)strlen(label)*6)/2,y+(h-8)/2,1,label,1);
}
static uint8_t touch_mask(int x,int y) {
 // pai-touch system chrome consumes touches near the left edge. Keep the
 // complete hit regions, rather than just their centres, beyond x=32.
 if(x>=72&&x<107&&y>=176&&y<195)return UP_BUTTON;
 if(x>=72&&x<107&&y>=200&&y<219)return DOWN_BUTTON;
 if(x>=34&&x<69&&y>=187&&y<208)return LEFT_BUTTON;
 if(x>=110&&x<145&&y>=187&&y<208)return RIGHT_BUTTON;
 if(x>=151&&x<213&&y>=180&&y<219)return A_BUTTON;
 if(x>=225&&x<287&&y>=180&&y<219)return B_BUTTON;
 return 0;
}
static uint8_t controller_mask(uint16_t b) {
 return (b&1?UP_BUTTON:0)|(b&2?DOWN_BUTTON:0)|(b&4?LEFT_BUTTON:0)|
        (b&8?RIGHT_BUTTON:0)|(b&16?A_BUTTON:0)|(b&32?B_BUTTON:0);
}
static void trace(const char *action) {
#if PXA_RETRO_TRACE

 pxlog_t log;pxlog_init(&log);pxlog_text(&log,"CLASSIC game=");pxlog_text(&log,arduboy_game_name());
 pxlog_text(&log," action=");pxlog_text(&log,action);
 pxlog_field(&log,"clock",arduboy_pxa_state.clock_ms);pxlog_field(&log,"fault",arduboy_pxa_state.fault);
 pxlog_field(&log,"display",arduboy_pxa_state.display_count);pxlog_field(&log,"buttons",arduboy_pxa_state.pointer);
 pxlog_field(&log,"paused",fe.paused);pxlog_field(&log,"hash",arduboy_pxa_hash(arduboy_pxa_state.display,1024));pxlog_send(&log);

#else
    (void)action;
#endif
}
static void draw() {
 memset(pixels,0,sizeof pixels);
 button(34,18,40,18,"NEW",0);button(79,18,50,18,"RESET",0);button(239,18,49,18,"PAUSE",fe.paused);
 char name[18];strncpy(name,arduboy_game_name(),17);name[17]=0;text(134,23,1,name,1);
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<128;x++)
  if(arduboy_pxa_state.display[(y/8)*128+x]&(1u<<(y&7)))rect(GX+x*SCALE,GY+y*SCALE,SCALE,SCALE,1);
 button(72,176,35,19,"UP",arduboy_pxa_state.pointer&UP_BUTTON);
 button(72,200,35,19,"DN",arduboy_pxa_state.pointer&DOWN_BUTTON);
 button(34,187,35,21,"LT",arduboy_pxa_state.pointer&LEFT_BUTTON);
 button(110,187,35,21,"RT",arduboy_pxa_state.pointer&RIGHT_BUTTON);
 button(151,180,62,39,"A",arduboy_pxa_state.pointer&A_BUTTON);
 button(225,180,62,39,"B",arduboy_pxa_state.pointer&B_BUTTON);
 if(fe.paused) {
  rect(40,65,216,91,2);text(118,76,1,"PAUSED",1);button(75,96,146,34,"CONTINUE",0);
 }
 if(arduboy_pxa_state.fault) {rect(40,65,216,91,4);text(85,101,1,"SOFTWARE STOP",1);}
 fe.dirty=0;
 if(fe.fb.ready&&pxfb_present(&fe.fb)<0){pxa_log_write(4,"Classic raster failed");__builtin_trap();}
}
static void restart() {
 arduboy_pxa_release();fe.last_host=0;fe.paused=0;arduboy_pxa_new();trace("new");fe.dirty=1;
}
static void pointer() {
 int x=fe.fb.pointer_x,y=fe.fb.pointer_y;unsigned phase=fe.fb.pointer_phase;
 if(phase==PXA_POINTER_CANCEL){arduboy_pxa_release();trace("cancel");fe.dirty=1;return;}
 if(phase==PXA_POINTER_UP){arduboy_pxa_pointer(0,1);trace("up");fe.dirty=1;return;}
 if(phase!=PXA_POINTER_DOWN&&phase!=PXA_POINTER_MOVE)return;
 if(phase==PXA_POINTER_DOWN&&y>=18&&y<36) {
  if((x>=34&&x<74)||(x>=79&&x<129))restart();
  else if(x>=239&&x<288){arduboy_pxa_release();fe.last_host=0;fe.paused=!fe.paused;trace("pause");fe.dirty=1;}
  return;
 }
 if(fe.paused) {
  if(phase==PXA_POINTER_DOWN&&x>=75&&x<221&&y>=96&&y<130) {
   arduboy_pxa_release();fe.last_host=0;fe.paused=0;trace("continue");fe.dirty=1;
  }
  return;
 }
 if(!arduboy_pxa_state.fault){arduboy_pxa_pointer(touch_mask(x,y),0);trace(phase==PXA_POINTER_DOWN?"down":"move");fe.dirty=1;}
}
extern "C" int32_t pxa_app_start(const uint8_t *config,uint32_t size) {
 memset(&fe,0,sizeof fe);fe.dirty=1;fe.fb.width=W;fe.fb.height=H;
 fe.fb.pixels=pixels;fe.fb.upload=upload;fe.fb.upload_bytes=sizeof upload;
 fe.fb.palette[0]=0;fe.fb.palette[1]=0xffff;fe.fb.palette[2]=0x2104;fe.fb.palette[3]=0x0340;fe.fb.palette[4]=0xf800;
 return pxfb_start(&fe.fb,config,size,33);
}
extern "C" int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t size) {
 uint32_t previous_buttons=fe.fb.buttons;uint32_t kind=pxfb_event(&fe.fb,bytes,size);
 if(kind&PXFB_ERROR){pxa_log_write(4,"Classic UI failed");__builtin_trap();}
 if(!fe.started) {
  if(kind&PXFB_TICK){arduboy_pxa_boot((uint32_t)fe.fb.timestamp_us);fe.started=1;fe.last_host=0;trace("start");}
  if(fe.started)draw();return kind?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
 }
 if(kind&PXFB_POINTER)pointer();
 if(kind&PXFB_CONTROLLER){uint32_t pressed=fe.fb.buttons&~previous_buttons;
 if(fe.paused&&(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START))){arduboy_pxa_release();fe.last_host=0;fe.paused=0;fe.dirty=1;}
 else if(!fe.paused&&(pressed&PXA_CONTROLLER_START)){arduboy_pxa_release();fe.last_host=0;fe.paused=1;fe.dirty=1;}
 else if(!fe.paused)arduboy_pxa_controller(controller_mask(fe.fb.buttons));trace("controller");}
 if(kind&PXFB_LIFECYCLE){arduboy_pxa_release();fe.last_host=0;trace("lifecycle");fe.dirty=1;}
 if(kind&PXFB_BACK){arduboy_pxa_release();fe.last_host=0;fe.paused=1;trace("back");fe.dirty=1;}
 if((kind&PXFB_TICK)&&!fe.paused&&!arduboy_pxa_state.fault) {
  uint64_t now=fe.fb.timestamp_us;
  uint32_t elapsed=(fe.last_host&&now>=fe.last_host)?(uint32_t)(now/1000u-fe.last_host/1000u):1;
  fe.last_host=now;arduboy_pxa_advance(elapsed);fe.dirty=1;
 }
 if((kind&PXFB_READY)||fe.dirty)draw();
 return kind?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
extern "C" void pxa_app_stop(uint32_t) {arduboy_pxa_release();fe.last_host=0;if(fe.started)trace("stop");pxfb_stop(&fe.fb);}
extern "C" void __wrap___assert_fail(const char*,const char*,unsigned,const char*){pxa_log_write(4,"Classic assertion");__builtin_trap();}
extern "C" void __wrap_abort(){pxa_log_write(4,"Classic abort");__builtin_trap();}
extern "C" void __wrap_exit(int){pxa_log_write(4,"Classic exit");__builtin_trap();}
