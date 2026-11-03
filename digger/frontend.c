/* SPDX-License-Identifier: MIT. PXA host for the real local Digger engine. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
#include "def.h"
#include "game.h"
#include "digger.h"
#include "scores.h"
#include "input.h"
#include "asyncify-driver.h"
#include "digger-host.h"
#include "font.h"
enum{W=400,H=416,GX=40,GY=56};
static uint8_t image[W*H],upload[PXA_RASTER_UPLOAD_HEADER_BYTES+W*H];
static struct{pxfb_t fb;uint64_t now_us,limit_us,host_us,release_at;uint32_t frames,draws,generation;int touch,paused,dirty,new_stage;uint32_t buttons;}fe;
static uint16_t rgb(int r,int g,int b){return(uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));}
static void rect(int x,int y,int w,int h,uint8_t c){for(int j=y;j<y+h;j++)if(j>=0&&j<H)for(int i=x;i<x+w;i++)if(i>=0&&i<W)image[j*W+i]=c;}
static void text(int x,int y,const char*s,int scale,uint8_t c){for(;*s;++s,x+=6*scale){unsigned k=*s>='0'&&*s<='9'?*s-'0':*s>='A'&&*s<='Z'?10+*s-'A':36;if(k<36)for(int i=0;i<5;i++)for(int j=0;j<7;j++)if(letters[k][i]&(1<<j))rect(x+i*scale,y+j*scale,scale,scale,c);}}
static void button(int x,int y,int w,int h,const char*s){rect(x,y,w,h,241);text(x+(w-(int)strlen(s)*12)/2,y+(h-14)/2,s,2,242);}
static uint32_t checksum(const uint8_t*p,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static void trace(const char *s){const uint8_t*p=digger_pixels640();pxlog_t l;pxlog_init(&l);pxlog_text(&l,"DIGGER ");pxlog_text(&l,s);pxlog_field(&l,"clock",(int32_t)(fe.now_us/1000));pxlog_field(&l,"ticks",dgstate.ticks);pxlog_field(&l,"running",gamerunning());pxlog_field(&l,"x",diggerx(0));pxlog_field(&l,"y",diggery(0));pxlog_field(&l,"score",gettscore(0));pxlog_field(&l,"lives",getlives(0));pxlog_field(&l,"ftime",dgstate.ftime);pxlog_field(&l,"raster",p?(int32_t)checksum(p,640*400):0);pxlog_field(&l,"saved",digger_coroutine_saved_bytes());pxlog_send(&l);}
void digger_port_draw(void){fe.draws++;fe.dirty=1;}
uint64_t digger_port_time_us(void){return fe.now_us;}
void digger_port_delay_us(uint32_t us){
 fe.now_us+=us;
 if(fe.release_at&&fe.now_us>=fe.release_at){digger_host_key(40,0);fe.release_at=0;}
 if(fe.new_stage==1&&!gamerunning()){
  digger_host_release();digger_host_key(40,1);fe.release_at=fe.now_us+160000;fe.new_stage=0;fe.generation++;
 }
 if(fe.now_us>fe.limit_us){uint64_t wait=fe.now_us-fe.limit_us;digger_coroutine_yield(wait>UINT32_MAX?UINT32_MAX:(uint32_t)wait);}
}
uint32_t SDL_GetTicks(void){return(uint32_t)(fe.now_us/1000);}
void SDL_Delay(uint32_t ms){digger_port_delay_us(ms>UINT32_MAX/1000?UINT32_MAX:ms*1000);}
static void input_update(void){
 const int scan[]={82,81,80,79,58,44};for(unsigned i=0;i<6;i++)digger_host_key(scan[i],!!(fe.buttons&(1u<<i))||fe.touch==scan[i]);
 if(fe.touch==40)digger_host_key(40,1);else if(!fe.release_at)digger_host_key(40,0);
}
static void release_input(void){digger_host_release();fe.buttons=0;fe.touch=-1;fe.fb.buttons=0;}
static void new_game(void){release_input();fe.paused=0;
 if(gamerunning()){digger_host_key(67,1);fe.new_stage=1;}
 else{digger_host_key(40,1);fe.release_at=fe.now_us+160000;fe.generation++;}
}
static int key_at(int x,int y){if(y>=300&&y<334){if(x<125)return 40;if(x<260)return 44;return 67;}
 if(x>=40&&x<88&&y>=344&&y<372)return 82;if(x>=40&&x<88&&y>=380&&y<410)return 81;
 if(x>=0&&x<40&&y>=364&&y<394)return 80;if(x>=88&&x<132&&y>=364&&y<394)return 79;
 if(x>=180&&x<280&&y>=344&&y<410)return 58;return -1;}
static int32_t present(void){
 /* The engine changes VGA intensity/palette during play. Refresh the Host's
    palette as well as its pixels; the shared adapter initializes it once. */
 int32_t status=pxa_raster_upload_palette_rgb565(fe.fb.context,fe.fb.palette,fe.fb.upload,fe.fb.upload_bytes);
 if(status==PXA_STATUS_WOULD_BLOCK)return 0;
 if(status!=532)return status<0?status:PXA_STATUS_INTERNAL;
 return pxfb_present(&fe.fb);
}
static void render(void){memset(image,240,sizeof image);const uint8_t*p=digger_pixels640();const uint16_t*pal=digger_palette565();
 for(int i=0;i<16;i++)fe.fb.palette[i]=pal[i];fe.fb.palette[255]=0xffff;
 if(p)for(int y=0;y<200;y++)for(int x=0;x<320;x++)image[(GY+y)*W+GX+x]=p[(y*2)*640+x*2];
 button(8,8,120,30,"NEW");button(140,8,120,30,"TITLE");button(272,8,120,30,fe.paused?"CONT":"MENU");
 text(120,270,"DIGGER",2,242);button(8,300,112,34,"START");button(130,300,122,34,"PAUSE");button(264,300,128,34,"QUIT");
 button(40,344,48,28,"U");button(40,382,48,28,"D");button(0,364,40,30,"L");button(88,364,44,30,"R");button(180,346,100,62,"FIRE");
 if(fe.paused){rect(100,100,200,80,241);text(128,126,"PAUSED",3,242);}fe.dirty=0;
}
__attribute__((noinline))static int digger_drive(uint64_t delta){
 fe.limit_us+=delta;for(unsigned n=0;n<2048;n++){
  int st=digger_coroutine_status();
  if(st==DIGGER_COROUTINE_IDLE)st=digger_coroutine_start();
  else if(st==DIGGER_COROUTINE_SUSPENDED){if(fe.now_us>fe.limit_us)break;st=digger_coroutine_resume();}
  if(st<0)return 0;if(st==DIGGER_COROUTINE_FINISHED){fe.paused=1;break;}
  if(fe.now_us>fe.limit_us)break;
 }fe.frames++;return 1;
}
int32_t pxa_app_start(const uint8_t*cfg,uint32_t n){
 memset(&fe,0,sizeof fe);fe.touch=-1;fe.fb.width=W;fe.fb.height=H;fe.fb.pixels=image;fe.fb.upload=upload;fe.fb.upload_bytes=sizeof upload;
 fe.fb.palette[240]=rgb(12,14,22);fe.fb.palette[241]=rgb(38,44,55);fe.fb.palette[242]=0xffff;
 if(!digger_drive(1))return PXA_STATUS_INTERNAL;render();return pxfb_start(&fe.fb,cfg,n,16);
}
int32_t pxa_app_on_event(const uint8_t*data,uint32_t n){
 uint32_t f=pxfb_event(&fe.fb,data,n);if(f&PXFB_ERROR)return PXA_STATUS_INTERNAL;
 if(f&PXFB_LIFECYCLE){release_input();fe.host_us=0;trace(fe.fb.background?"background":"foreground");}
 if(f&PXFB_BACK){fe.paused=!fe.paused;release_input();trace(fe.paused?"pause":"continue");}
 if(f&PXFB_CONTROLLER){fe.buttons=fe.fb.buttons;input_update();trace("controller");}
 if(f&PXFB_POINTER){int x=fe.fb.pointer_x,y=fe.fb.pointer_y,down=fe.fb.pointer_phase==PXA_POINTER_DOWN;
  if(down&&y>=8&&y<38){
   if(x>=8&&x<128){new_game();trace("new");}
   else if(x>=140&&x<260){release_input();if(gamerunning())digger_host_key(67,1);fe.new_stage=0;trace("title");}
   else if(x>=272&&x<392){fe.paused=!fe.paused;release_input();trace(fe.paused?"pause":"continue");}
  }else if(!fe.paused){fe.touch=(fe.fb.pointer_phase==PXA_POINTER_UP||fe.fb.pointer_phase==PXA_POINTER_CANCEL)?-1:key_at(x,y);input_update();trace("pointer");}
 }
 if(f&PXFB_TICK){uint64_t now=fe.fb.timestamp_us,delta=fe.host_us&&now>fe.host_us?now-fe.host_us:16000;fe.host_us=now;if(delta>100000)delta=100000;
  if(!fe.paused&&!fe.fb.background&&!digger_drive(delta))return PXA_STATUS_INTERNAL;if((fe.frames&15)==0)trace("tick");}
 if((f&(PXFB_READY|PXFB_TICK|PXFB_BACK|PXFB_POINTER|PXFB_LIFECYCLE))&&fe.fb.ready){render();if(present()<0)return PXA_STATUS_INTERNAL;}
 return f?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(uint32_t reason){(void)reason;release_input();(void)digger_coroutine_cancel();digger_raster_reset();digger_sessionfs_reset();pxfb_stop(&fe.fb);}
void __wrap___assert_fail(const char*a,const char*b,unsigned c,const char*d){(void)a;(void)b;(void)c;(void)d;pxa_log_write(4,"Digger assertion");__builtin_trap();}
void __wrap_abort(void){pxa_log_write(4,"Digger abort");__builtin_trap();}
void __wrap_exit(int n){(void)n;pxa_log_write(4,"Digger unexpected exit");__builtin_trap();}

