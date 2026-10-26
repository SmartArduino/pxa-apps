/* SPDX-License-Identifier: MIT. Independent original Arduboy raster/input frontend. */
#include "backend.h"
#include "Arduboy2.h"
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
#include <string.h>
#ifndef ARDU_GAME_ID
#define ARDU_GAME_ID 0
#endif
extern "C" const uint8_t *arduboy_pxa_font(void);
enum {FE_WIDTH=400,FE_HEIGHT=416,FE_GAME_X=72,FE_GAME_Y=80,FE_SCALE=2};
static uint8_t pixels[FE_WIDTH*FE_HEIGHT],upload[PXA_RASTER_UPLOAD_HEADER_BYTES+FE_WIDTH*FE_HEIGHT];
static struct{pxfb_t fb;uint64_t last_host;uint32_t seq,generation,elapsed;int started,paused,dirty;} fe;
static void ui_rect(int x,int y,int w,int h,uint8_t color){for(int yy=y;yy<y+h;yy++)if(yy>=0&&yy<FE_HEIGHT)for(int xx=x;xx<x+w;xx++)if(xx>=0&&xx<FE_WIDTH)pixels[yy*FE_WIDTH+xx]=color;}
static void ui_text(int x,int y,int scale,const char *s,uint8_t color){const uint8_t*font=arduboy_pxa_font();for(;*s;++s,x+=6*scale)for(int col=0;col<5;col++){uint8_t bits=font[(uint8_t)*s*5+col];for(int row=0;row<8;row++)if(bits&(1u<<row))ui_rect(x+col*scale,y+row*scale,scale,scale,color);}}
static void ui_button(int x,int y,int w,int h,const char*label,int active){ui_rect(x,y,w,h,active?3:2);ui_text(x+(w-(int)strlen(label)*12)/2,y+(h-16)/2,2,label,1);}
static uint8_t touch_mask(int x,int y){
 if(y>=268&&y<304){if(x>=16&&x<104)return B_BUTTON|UP_BUTTON;if(x>=110&&x<198)return B_BUTTON|DOWN_BUTTON;if(x>=204&&x<292)return B_BUTTON|LEFT_BUTTON;if(x>=298&&x<386)return B_BUTTON|RIGHT_BUTTON;}
#if ARDU_GAME_ID == 1
 if(x>=184&&x<380&&y>=276&&y<304)return UP_BUTTON|DOWN_BUTTON;
#endif
 if(x>=58&&x<100&&y>=316&&y<346)return UP_BUTTON;
 if(x>=58&&x<100&&y>=376&&y<406)return DOWN_BUTTON;
 if(x>=16&&x<58&&y>=346&&y<376)return LEFT_BUTTON;
 if(x>=100&&x<142&&y>=346&&y<376)return RIGHT_BUTTON;
 if(x>=184&&x<276&&y>=340&&y<404)return A_BUTTON;
 if(x>=286&&x<380&&y>=340&&y<404)return B_BUTTON;return 0;
}
static uint8_t controller_mask(uint16_t b){return (b&1?UP_BUTTON:0)|(b&2?DOWN_BUTTON:0)|(b&4?LEFT_BUTTON:0)|(b&8?RIGHT_BUTTON:0)|(b&16?A_BUTTON:0)|(b&32?B_BUTTON:0);}
static void hexlog(uint32_t clock,unsigned part,const uint8_t*data,unsigned n){const char*digits="0123456789abcdef";char hex[65];for(unsigned i=0;i<n;i++){hex[i*2]=digits[data[i]>>4];hex[i*2+1]=digits[data[i]&15];}hex[n*2]=0;pxlog_t log;pxlog_init(&log);pxlog_text(&log,"ARDU MEM");pxlog_field(&log,"clock",(int32_t)clock);pxlog_field(&log,"part",part);pxlog_text(&log," data=");pxlog_text(&log,hex);pxlog_send(&log);}
static void trace(const char*action){
#if PXA_RETRO_TRACE

 uint8_t wire[3136+ARDU_GAME_MAX_BYTES];size_t game_bytes=arduboy_game_pack(nullptr,0);
 if(game_bytes>ARDU_GAME_MAX_BYTES||arduboy_pxa_pack(wire,3136)!=3136||arduboy_game_pack(wire+3136,ARDU_GAME_MAX_BYTES)!=game_bytes){arduboy_pxa_state.fault=5;pxa_log_write(4,"Arduboy canonical state invalid");__builtin_trap();}
 const size_t count=3136+game_bytes;pxlog_t log;pxlog_init(&log);pxlog_text(&log,"ARDU STATE action=");pxlog_text(&log,action);
 pxlog_field(&log,"game",ARDU_GAME_ID);pxlog_field(&log,"seq",fe.seq);pxlog_field(&log,"gen",fe.generation);pxlog_field(&log,"clock",arduboy_pxa_state.clock_ms);pxlog_field(&log,"steps",arduboy_pxa_state.steps);
 pxlog_field(&log,"frame",Arduboy2Base::frameCount);pxlog_field(&log,"period",wire[46]);pxlog_field(&log,"bytes",count);pxlog_field(&log,"core_bytes",3136);pxlog_field(&log,"game_bytes",game_bytes);
 pxlog_field(&log,"paused",fe.paused);pxlog_text(&log," phase=header");pxlog_send(&log);
 pxlog_init(&log);pxlog_text(&log,"ARDU STATE2");pxlog_field(&log,"game",ARDU_GAME_ID);pxlog_field(&log,"seq",fe.seq);pxlog_field(&log,"gen",fe.generation);pxlog_field(&log,"clock",arduboy_pxa_state.clock_ms);pxlog_field(&log,"fault",arduboy_pxa_state.fault);pxlog_field(&log,"ctors",arduboy_pxa_state.constructors);pxlog_field(&log,"dropped",arduboy_pxa_state.dropped_steps);
 pxlog_field(&log,"draw_step",arduboy_pxa_state.display_step);pxlog_field(&log,"display_count",arduboy_pxa_state.display_count);pxlog_field(&log,"seed",arduboy_pxa_state.seed);pxlog_field(&log,"rng",arduboy_pxa_state.rng);pxlog_field(&log,"elapsed",fe.elapsed);pxlog_text(&log," phase=post");pxlog_send(&log);
 pxlog_init(&log);pxlog_text(&log,"ARDU HASH");pxlog_field(&log,"clock",arduboy_pxa_state.clock_ms);pxlog_field(&log,"memory",arduboy_pxa_hash(wire,count));pxlog_field(&log,"pixels",arduboy_pxa_hash(arduboy_pxa_state.display,1024));pxlog_field(&log,"eeprom",arduboy_pxa_hash(arduboy_pxa_state.eeprom,1024));pxlog_send(&log);
 pxlog_init(&log);pxlog_text(&log,"ARDU BTN");pxlog_field(&log,"clock",arduboy_pxa_state.clock_ms);pxlog_field(&log,"pointer",arduboy_pxa_state.pointer);pxlog_field(&log,"controller",arduboy_pxa_state.controller);pxlog_field(&log,"pending_pointer",arduboy_pxa_state.pending_pointer);pxlog_field(&log,"pending_controller",arduboy_pxa_state.pending_controller);pxlog_field(&log,"applied",arduboy_pxa_state.applied);pxlog_field(&log,"sampled",arduboy_pxa_state.sampled);pxlog_send(&log);
 for(size_t i=0;i<count;i+=32)hexlog(arduboy_pxa_state.clock_ms,(unsigned)(i/32),wire+i,(unsigned)(count-i<32?count-i:32));
 pxlog_init(&log);pxlog_text(&log,"ARDU END");pxlog_field(&log,"clock",arduboy_pxa_state.clock_ms);pxlog_field(&log,"seq",fe.seq);pxlog_field(&log,"gen",fe.generation);pxlog_send(&log);

#else
    (void)action;
#endif
}
static void draw(){memset(pixels,0,sizeof pixels);ui_button(8,8,120,30,"NEW",0);ui_button(140,8,120,30,"RESTART",0);ui_button(272,8,120,30,"MENU",fe.paused);
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<128;x++)if(arduboy_pxa_state.display[(y/8)*128+x]&(1u<<(y&7)))ui_rect(FE_GAME_X+x*2,FE_GAME_Y+y*2,2,2,1);
 ui_text(72,232,2,arduboy_game_name(),1);
 ui_button(58,316,42,30,"^",arduboy_pxa_state.pointer&UP_BUTTON);ui_button(58,376,42,30,"v",arduboy_pxa_state.pointer&DOWN_BUTTON);
 ui_button(16,346,42,30,"<",arduboy_pxa_state.pointer&LEFT_BUTTON);ui_button(100,346,42,30,">",arduboy_pxa_state.pointer&RIGHT_BUTTON);
 ui_button(184,340,92,64,"A",arduboy_pxa_state.pointer&A_BUTTON);ui_button(286,340,94,64,"B",arduboy_pxa_state.pointer&B_BUTTON);
#if ARDU_GAME_ID == 1
 ui_button(184,276,196,28,"UP+DOWN",arduboy_pxa_state.pointer==(UP_BUTTON|DOWN_BUTTON));
#endif
 ui_button(16,268,88,36,"B+^",arduboy_pxa_state.pointer==(B_BUTTON|UP_BUTTON));ui_button(110,268,88,36,"B+v",arduboy_pxa_state.pointer==(B_BUTTON|DOWN_BUTTON));ui_button(204,268,88,36,"B+<",arduboy_pxa_state.pointer==(B_BUTTON|LEFT_BUTTON));ui_button(298,268,88,36,"B+>",arduboy_pxa_state.pointer==(B_BUTTON|RIGHT_BUTTON));
 if(fe.paused){ui_rect(70,70,260,176,2);ui_text(152,82,2,"PAUSED",1);ui_button(108,120,184,34,"CONTINUE",0);ui_button(108,174,184,34,"NEW GAME",0);}
 if(arduboy_pxa_state.fault){ui_rect(70,70,260,176,4);ui_text(86,120,2,"SOFTWARE STOP",1);}
 fe.dirty=0;if(fe.fb.ready&&pxfb_present(&fe.fb)<0){pxa_log_write(4,"Arduboy raster failed");__builtin_trap();}
}
static void command(unsigned action){arduboy_pxa_release();fe.last_host=0;fe.elapsed=0;++fe.seq;if(action<2){arduboy_pxa_new();++fe.generation;fe.paused=0;trace(action?"restart":"new");}else{fe.paused=1;trace("menu");}fe.dirty=1;}
static void pointer(){int x=fe.fb.pointer_x,y=fe.fb.pointer_y;unsigned phase=fe.fb.pointer_phase;fe.elapsed=0;
 if(phase==PXA_POINTER_CANCEL){arduboy_pxa_release();++fe.seq;trace("cancel");fe.dirty=1;return;}
 if(phase==PXA_POINTER_UP){arduboy_pxa_pointer(0,1);++fe.seq;trace("up");fe.dirty=1;return;}
 if(phase!=PXA_POINTER_DOWN&&phase!=PXA_POINTER_MOVE)return;
 if(fe.paused){if(phase!=PXA_POINTER_DOWN)return;if(x>=108&&x<292&&y>=120&&y<154){arduboy_pxa_release();fe.last_host=0;fe.paused=0;++fe.seq;trace("continue");fe.dirty=1;}else if(x>=108&&x<292&&y>=174&&y<208)command(0);return;}
 if(phase==PXA_POINTER_DOWN&&y>=8&&y<38){if(x>=8&&x<128)command(0);else if(x>=140&&x<260)command(1);else if(x>=272&&x<392)command(2);return;}
 if(arduboy_pxa_state.fault)return;arduboy_pxa_pointer(touch_mask(x,y),0);++fe.seq;trace(phase==PXA_POINTER_DOWN?"down":"move");fe.dirty=1;
}
extern "C" int32_t pxa_app_start(const uint8_t*config,uint32_t size){memset(&fe,0,sizeof fe);fe.generation=1;fe.paused=1;fe.dirty=1;fe.fb.width=FE_WIDTH;fe.fb.height=FE_HEIGHT;fe.fb.pixels=pixels;fe.fb.upload=upload;fe.fb.upload_bytes=sizeof upload;fe.fb.palette[0]=0;fe.fb.palette[1]=0xffff;fe.fb.palette[2]=0x2104;fe.fb.palette[3]=0x0340;fe.fb.palette[4]=0xf800;return pxfb_start(&fe.fb,config,size,16);}
extern "C" int32_t pxa_app_on_event(const uint8_t*bytes,uint32_t size){uint32_t previous_buttons=fe.fb.buttons;uint32_t kind=pxfb_event(&fe.fb,bytes,size);if(kind&PXFB_ERROR){pxa_log_write(4,"Arduboy UI failed");__builtin_trap();}
 if(!fe.started){if(kind&PXFB_TICK){arduboy_pxa_boot((uint32_t)fe.fb.timestamp_us);fe.started=1;fe.last_host=0;trace("start");fe.dirty=1;}if(fe.started)draw();return kind?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;}
 fe.elapsed=0;
 if(kind&PXFB_POINTER)pointer();
 if(kind&PXFB_CONTROLLER){uint32_t pressed=fe.fb.buttons&~previous_buttons;
 if(fe.paused&&(pressed&(PXA_CONTROLLER_A|PXA_CONTROLLER_START))){arduboy_pxa_release();fe.last_host=0;fe.paused=0;fe.dirty=1;}
 else if(!fe.paused&&(pressed&PXA_CONTROLLER_START)){arduboy_pxa_release();fe.last_host=0;fe.paused=1;fe.dirty=1;}
 else if(!fe.paused&&!arduboy_pxa_state.fault)arduboy_pxa_controller(controller_mask(fe.fb.buttons));++fe.seq;trace("controller");}
 if(kind&PXFB_LIFECYCLE){arduboy_pxa_release();fe.last_host=0;++fe.seq;trace("lifecycle");fe.dirty=1;}
 if(kind&PXFB_BACK){arduboy_pxa_release();fe.last_host=0;fe.paused=1;++fe.seq;trace("back");fe.dirty=1;}
 if((kind&PXFB_TICK)&&!fe.paused&&!arduboy_pxa_state.fault){uint64_t now=fe.fb.timestamp_us;if(fe.last_host&&now>=fe.last_host)fe.elapsed=(uint32_t)(now/1000u-fe.last_host/1000u);fe.last_host=now;arduboy_pxa_advance(fe.elapsed);trace("tick");fe.dirty=1;}
 if((kind&PXFB_READY)||fe.dirty)draw();return kind?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
extern "C" void pxa_app_stop(uint32_t reason){(void)reason;arduboy_pxa_release();fe.last_host=0;if(fe.started)trace("stop");pxfb_stop(&fe.fb);}
extern "C" void __wrap___assert_fail(const char*,const char*,unsigned,const char*){pxa_log_write(4,"Arduboy assertion");__builtin_trap();}
extern "C" void __wrap_abort(){pxa_log_write(4,"Arduboy abort");__builtin_trap();}
extern "C" void __wrap_exit(int){pxa_log_write(4,"Arduboy exit");__builtin_trap();}
