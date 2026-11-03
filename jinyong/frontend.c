/* SPDX-License-Identifier: MIT. Original PXA host for the full S60 Jin Yong core. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
#include "compat/jy_sdl.h"
#include "fs/jy_fs.h"
#include "pxa_fs_backend.h"
#include "protected_bridge.h"
#include "guest_calls.h"
#include "lua.h"
#include "lauxlib.h"
extern int jy_game_main(int,char **);
enum {W=400,H=416,GW=320,GH=240,Q=128};
static uint8_t image[W*H],upload[PXFB_UPLOAD_BYTES];
/* Compact the RGB snapshot in place after palette selection. The next SDL
 * presentation overwrites it completely; UI-only redraws reuse its indices. */
static uint8_t rgb24[GW*GH*3];
static uint16_t color_keys[1024],color_values[1024];
static struct {
 pxfb_t fb;lua_State *vm;SDL_Event input[Q];unsigned input_head,input_count;
 uint64_t game_us,credit_us,wall_us,observe_us,next_trace_us;unsigned frames,drives,polls;uint32_t controller_buttons;
 uint32_t hash;int phase,started,in_game,paused,dirty,got_image,fault,touch_key,observer_busy,packed,palette_fallback;
 int status,tick,x,y,scene,menu,lua_kib;
 int ui_dialog,ui_current,ui_key_press,last_key,delivered_key;
 unsigned observed_frames;int observed_status;
} fe;
static const uint8_t letters[36][5]={
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},
 {0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
 {0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},
 {6,0x49,0x49,0x29,0x1e},{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
 {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},
 {0x7f,9,9,9,1},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},
 {0,0x41,0x7f,0x41,0},{0x20,0x40,0x40,0x3f,1},{0x7f,8,0x14,0x22,0x41},
 {0x7f,0x40,0x40,0x40,0x40},{0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},
 {0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},
 {0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},
 {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},
 {0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}
};
static uint16_t rgb565(unsigned r,unsigned g,unsigned b){return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));}
static uint32_t hash_bytes(const uint8_t *p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
static void rect(int x,int y,int w,int h,uint8_t c){for(int j=y;j<y+h;j++)if(j>=0&&j<H)for(int i=x;i<x+w;i++)if(i>=0&&i<W)image[j*W+i]=c;}
static void text(int x,int y,const char *s,int scale,uint8_t c){for(;*s;s++,x+=6*scale){unsigned k=*s>='0'&&*s<='9'?*s-'0':*s>='A'&&*s<='Z'?10+*s-'A':36;if(k<36)for(int i=0;i<5;i++)for(int j=0;j<7;j++)if(letters[k][i]&(1<<j))rect(x+i*scale,y+j*scale,scale,scale,c);}}
static void button(int x,int y,int w,const char *s,uint8_t bg,uint8_t fg){rect(x,y,w,30,bg);text(x+(w-(int)strlen(s)*6)/2,y+11,s,1,fg);}
static void trace(const char *action){
 pxlog_t l;jy_pxa_fs_stats io;jy_pxa_fs_get_stats(&io);pxlog_init(&l);pxlog_text(&l,"JY action=");pxlog_text(&l,action);
 pxlog_field(&l,"frames",fe.frames);pxlog_field(&l,"phase",fe.phase);pxlog_field(&l,"paused",fe.paused);pxlog_field(&l,"hash",fe.hash);
 pxlog_field(&l,"fault",jy_fault_code());pxlog_field(&l,"lua_kib",fe.lua_kib);
 pxlog_field(&l,"ms",fe.game_us/1000);pxlog_field(&l,"status",fe.status);pxlog_field(&l,"tick",fe.tick);pxlog_field(&l,"x",fe.x);pxlog_field(&l,"y",fe.y);pxlog_field(&l,"scene",fe.scene);pxlog_field(&l,"menu",fe.menu);
 pxlog_field(&l,"io",io.requests);pxlog_field(&l,"pending",jy_pxa_fs_pending());pxlog_field(&l,"anchors",jy_anchor_count());pxlog_field(&l,"ioerr",io.last_error);pxlog_field(&l,"host_busy",jy_host_busy_count());pxlog_field(&l,"host_error",jy_host_last_error());pxlog_field(&l,"read_bytes",io.asset_bytes);pxlog_field(&l,"async_peak",jy_saved_peak());pxlog_field(&l,"anchor_peak",jy_anchor_bytes_peak());pxlog_send(&l);
}
void jy_port_lua_state(lua_State *L){fe.vm=L;}
void jy_port_protected_error(int code){pxlog_t l;pxlog_init(&l);pxlog_text(&l,"JY protected throw");pxlog_field(&l,"code",code);pxlog_send(&l);}
void jy_port_debug(const char *message){pxlog_t l;pxlog_init(&l);pxlog_text(&l,"JY script: ");pxlog_text(&l,message);pxlog_send(&l);}
void jy_port_lua_result(lua_State *L,const char *stage,int result){
 if(!result)return;pxlog_t l;pxlog_init(&l);pxlog_text(&l,"JY action=lua-error stage=");pxlog_text(&l,stage);pxlog_field(&l,"code",result);pxlog_text(&l," message=");const char *message=lua_tostring(L,-1);pxlog_text(&l,message?message:"non-string Lua error");pxlog_send(&l);
}
static void input_trace(const char *action,int key,int down){
 pxlog_t l;pxlog_init(&l);pxlog_text(&l,"JY INPUT action=");pxlog_text(&l,action);pxlog_field(&l,"key",key);pxlog_field(&l,"down",down);pxlog_field(&l,"ms",fe.game_us/1000);pxlog_field(&l,"dialog",fe.ui_dialog);pxlog_field(&l,"current",fe.ui_current);pxlog_field(&l,"lua_key",fe.ui_key_press);pxlog_send(&l);
}
void jy_port_key_result(int key){fe.last_key=key;input_trace("returned",key,1);}
static void cfg(lua_State *L,const char *k,int value){lua_pushinteger(L,value);lua_setfield(L,-2,k);}
void jy_port_config(lua_State *L){
 int top=lua_gettop(L);lua_getglobal(L,"CONFIG");
 if(lua_istable(L,-1)){
  cfg(L,"Type",0);cfg(L,"Width",GW);cfg(L,"Height",GH);cfg(L,"bpp",32);cfg(L,"FullScreen",0);cfg(L,"EnableSound",0);cfg(L,"Debug",0);
  cfg(L,"MAXCacheNum",256);cfg(L,"CleanMemory",1);cfg(L,"LoadFullS",0);cfg(L,"LoadMMapType",2);cfg(L,"PreLoadPicGrp",0);
 }lua_settop(L,top);
}
static int table_number(lua_State *L,const char *table,const char *key,int fallback){int top=lua_gettop(L),r=fallback;lua_pushstring(L,table);lua_rawget(L,LUA_GLOBALSINDEX);if(lua_istable(L,-1)){lua_pushstring(L,key);lua_rawget(L,-2);if(lua_isnumber(L,-1))r=(int)lua_tointeger(L,-1);}lua_settop(L,top);return r;}
static int base_number(lua_State *L,const char *key){int top=lua_gettop(L),r=-999;lua_pushliteral(L,"JY");lua_rawget(L,LUA_GLOBALSINDEX);if(lua_istable(L,-1)){lua_pushliteral(L,"Base");lua_rawget(L,-2);if(lua_istable(L,-1)||lua_isuserdata(L,-1)){lua_pushstring(L,key);lua_gettable(L,-2);if(lua_isnumber(L,-1))r=(int)lua_tointeger(L,-1);}}lua_settop(L,top);return r;}
static int observe_lua(lua_State *L){
 fe.status=table_number(L,"JY","Status",-999);fe.tick=table_number(L,"JY","MyTick",-1);fe.scene=table_number(L,"JY","SubScene",-999);
 /* Reading Base invokes the original file-backed metatable. Only refresh
  * coordinates after a presentation/status change; idle diagnostics must not
  * evict the game's compressed-page cache on every input poll. */
 if(fe.observed_frames!=fe.frames||fe.observed_status!=fe.status){
  fe.x=base_number(L,"\xc8\xcbX1");fe.y=base_number(L,"\xc8\xcbY1");
  fe.observed_frames=fe.frames;fe.observed_status=fe.status;
 }
 fe.lua_kib=lua_gc(L,LUA_GCCOUNT,0);fe.menu=0;
 int dialog=0,selection=-999,key_press=-999;
 lua_Debug d;for(int i=0;i<80&&lua_getstack(L,i,&d);i++)if(lua_getinfo(L,"n",&d)&&d.name){
  if(!strcmp(d.name,"MMenu"))fe.menu=1;
  if(!strcmp(d.name,"ShowMenu2")){
   dialog=2;
   for(int j=1;j<100;j++){
    const char *name=lua_getlocal(L,&d,j);if(!name)break;
    if(lua_isnumber(L,-1)){if(!strcmp(name,"current"))selection=(int)lua_tointeger(L,-1);else if(!strcmp(name,"keyPress"))key_press=(int)lua_tointeger(L,-1);}
    lua_pop(L,1);
   }
  }
 }
 if(dialog!=fe.ui_dialog||selection!=fe.ui_current||key_press!=fe.ui_key_press){
  fe.ui_dialog=dialog;fe.ui_current=selection;fe.ui_key_press=key_press;
  pxlog_t l;pxlog_init(&l);pxlog_text(&l,"JY UI");pxlog_field(&l,"dialog",dialog);pxlog_field(&l,"current",selection);pxlog_field(&l,"lua_key",key_press);pxlog_field(&l,"last_key",fe.last_key);pxlog_field(&l,"delivered",fe.delivered_key);pxlog_field(&l,"ms",fe.game_us/1000);pxlog_send(&l);
 }
 return 0;
}
/* Only called on the original active game C stack. Metatable reads can do
 * real file I/O and suspend; the outer Host callback only prints this cache. */
static void observe(void){
 if(!fe.in_game||!fe.vm||fe.observer_busy||fe.game_us<fe.observe_us)return;
 fe.observe_us=fe.game_us+100000;fe.observer_busy=1;lua_State *L=fe.vm;int top=lua_gettop(L);
 lua_pushcfunction(L,observe_lua);if(lua_pcall(L,0,0,0))fe.scene=-998;lua_settop(L,top);fe.observer_busy=0;
}
static void pace(void){
 if(!fe.in_game||++fe.polls<128)return;fe.polls=0;
 if(fe.credit_us){uint64_t n=fe.credit_us<1000?fe.credit_us:1000;fe.credit_us-=n;fe.game_us+=n;}else jy_suspend(0);
}
static uint64_t host_now(void *ctx){(void)ctx;pace();return fe.game_us+1000;}
static void host_delay(void *ctx,uint64_t us){
 (void)ctx;if(!us)us=1000;
 while(us){uint64_t n=us<fe.credit_us?us:fe.credit_us;fe.game_us+=n;fe.credit_us-=n;us-=n;if(us)jy_suspend(us);}
}
static int host_poll(void *ctx,SDL_Event *out){
 (void)ctx;observe();pace();if(!fe.input_count)return 0;*out=fe.input[fe.input_head];fe.input_head=(fe.input_head+1)%Q;fe.input_count--;fe.delivered_key=out->key.keysym.sym;input_trace("delivered",out->key.keysym.sym,out->type==SDL_KEYDOWN);return 1;
}
static int host_present(void *ctx,const SDL_Surface *surface,const SDL_Rect *rects,int count){
 (void)ctx;(void)rects;(void)count;if(surface->w!=GW||surface->h!=GH)return -1;
 if(jy_sdl_copy_rgb24(surface,rgb24,GW*3))return -1;
 fe.frames++;fe.hash=hash_bytes(rgb24,sizeof rgb24);fe.got_image=fe.dirty=1;fe.packed=0;observe();
 /* Return to the event pump after every real original presentation. */
 jy_suspend(0);return 0;
}
static void enqueue_key(int key,int down){
 if(key<=0||key>=SDLK_LAST||fe.input_count==Q)return;SDL_Event *e=&fe.input[(fe.input_head+fe.input_count++)%Q];memset(e,0,sizeof *e);e->type=down?SDL_KEYDOWN:SDL_KEYUP;e->key.state=down?SDL_PRESSED:SDL_RELEASED;e->key.keysym.sym=(SDLKey)key;input_trace("queued",key,down);
}
static void release_keys(void){fe.controller_buttons=0;static const int keys[]={SDLK_LEFT,SDLK_RIGHT,SDLK_UP,SDLK_DOWN,SDLK_RETURN,SDLK_SPACE,SDLK_ESCAPE,SDLK_y,SDLK_n,SDLK_TAB};fe.input_head=fe.input_count=0;for(unsigned i=0;i<sizeof keys/sizeof keys[0];i++)enqueue_key(keys[i],0);fe.touch_key=0;}
static int key_at(int x,int y){static const int a[]={SDLK_LEFT,SDLK_RIGHT,SDLK_UP,SDLK_DOWN,SDLK_RETURN},b[]={SDLK_SPACE,SDLK_ESCAPE,SDLK_y,SDLK_n,SDLK_TAB};if(x<8||x>=392)return 0;int k=(x-8)/78;if(k>=5||(x-8)%78>=72)return 0;if(y>=344&&y<374)return a[k];if(y>=380&&y<410)return b[k];return 0;}
static unsigned color_slot(uint16_t c){
 unsigned k=((uint32_t)c*40503u)&1023u;
 while(color_values[k]&&color_keys[k]!=c)k=(k+1)&1023u;
 return k;
}
static void render(void){
 int fallback=fe.palette_fallback;
 if(!fe.got_image){fe.fb.palette[0]=rgb565(9,12,20);fe.fb.palette[1]=rgb565(35,43,59);fe.fb.palette[2]=0xffff;}
 if(fe.got_image&&!fe.packed){
  unsigned colors=3;fallback=0;memset(color_values,0,sizeof color_values);
  fe.fb.palette[0]=rgb565(9,12,20);fe.fb.palette[1]=rgb565(35,43,59);fe.fb.palette[2]=0xffff;
  /* Choose the palette before modifying any RGB input, so the RGB332 fallback
     remains byte-identical when a frame contains over 253 distinct colors. */
  for(unsigned i=0;i<GW*GH;i++){
   const uint8_t *p=rgb24+i*3;uint16_t c=rgb565(p[0],p[1],p[2]);unsigned slot=color_slot(c);
   if(!color_values[slot]){if(colors==256){fallback=1;break;}color_keys[slot]=c;color_values[slot]=(uint16_t)colors;fe.fb.palette[colors++]=c;}
  }
  if(fallback)for(unsigned i=0;i<256;i++)fe.fb.palette[i]=rgb565(((i>>5)&7)*255/7,((i>>2)&7)*255/7,(i&3)*255/3);
  for(unsigned i=0;i<GW*GH;i++){
   const uint8_t *p=rgb24+i*3;
   uint8_t c=fallback?(p[0]&224)|((p[1]>>3)&28)|(p[2]>>6):(uint8_t)color_values[color_slot(rgb565(p[0],p[1],p[2]))];
   rgb24[i]=c;
  }
  fe.packed=1;fe.palette_fallback=fallback;
 }
 uint8_t bg=0,button_bg=fallback?36:1,fg=fallback?255:2;rect(0,0,W,H,bg);
 if(fe.got_image)for(int y=0;y<300;y++)for(int x=0;x<400;x++)image[(y+40)*W+x]=rgb24[(y*GH/300)*GW+x*GW/400];
 else text(130,172,"LOADING",3,fg);
 button(8,4,100,"MENU",button_bg,fg);button(284,4,108,fe.paused?"RESUME":"PAUSE",button_bg,fg);
 static const char *a[]={"LEFT","RIGHT","UP","DOWN","ENTER"},*b[]={"SPACE","ESC","Y","N","TAB"};for(int i=0;i<5;i++){button(8+i*78,344,72,a[i],button_bg,fg);button(8+i*78,380,72,b[i],button_bg,fg);}
 if(fe.paused){rect(116,148,168,48,button_bg);text(146,166,"PAUSED",2,fg);}
 if(fe.fault){rect(68,148,264,48,button_bg);text(92,166,"ENGINE STOPPED",2,fg);}
 else if(fe.phase==JY_DONE){rect(68,148,264,48,button_bg);text(92,166,"HOME TO CLOSE",2,fg);}
 fe.dirty=0;
}
static void game_entry(void){static char name[]="jinyong-pxa";static char *argv[]={name,NULL};int rc=jy_game_main(1,argv);jy_finish(rc);}
static unsigned host_sequence,drive_sequence;
static uint64_t monotonic_us(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return (uint64_t)t.tv_sec*1000000u+(uint64_t)t.tv_nsec/1000u;}
/* Bound startup diagnostics; these calls only run with the engine unwound. */
static void boundary(const char *stage,unsigned seq,uint32_t kind,uint32_t operation,int result){
 if(seq>80)return;
 pxlog_t l;pxlog_init(&l);pxlog_text(&l,"JY boundary=");pxlog_text(&l,stage);
 pxlog_field(&l,"seq",seq);pxlog_field(&l,"kind",kind);pxlog_field(&l,"operation",operation);pxlog_field(&l,"result",result);pxlog_send(&l);
}
/* The only root scheduler: it must remain outside Asyncify's saved graph. */
__attribute__((noinline)) static void jy_front_drive(void){
 /* A bounded import batch may leave work for the next event. Do not rewind
  * its continuation until the Host result has actually been produced. */
 if(fe.paused||fe.fb.background||fe.fault||fe.phase==JY_DONE||jy_host_call_pending()||(jy_pxa_fs_pending()&&!jy_host_result_ready()))return;
 unsigned seq=++drive_sequence;boundary("resume-begin",seq,0,0,fe.phase);
 fe.in_game=1;int phase;
 if(!fe.started){fe.started=1;phase=jy_coroutine_begin(game_entry);}else phase=jy_coroutine_resume();
 unsigned n=0;while((phase==JY_CHECKPOINT||phase==JY_THROW)&&n++<4096)phase=jy_coroutine_resume();
 fe.in_game=0;fe.phase=phase;fe.drives++;boundary("resume-end",seq,0,0,phase);
 if(phase<0){fe.fault=1;trace("coroutine-fault");}else if(phase==JY_DONE){pxlog_t done;pxlog_init(&done);pxlog_text(&done,"JY finished");pxlog_field(&done,"code",jy_finish_code());pxlog_send(&done);trace("engine-returned");fe.dirty=1;}
}
int32_t pxa_app_start(const uint8_t *config,uint32_t n){
 host_sequence=drive_sequence=0;
 jy_host_calls_reset();
 memset(&fe,0,sizeof fe);fe.status=fe.x=fe.y=fe.scene=-999;fe.tick=-1;fe.phase=JY_IDLE;fe.dirty=1;
 fe.fb.width=W;fe.fb.height=H;fe.fb.pixels=image;fe.fb.upload=upload;fe.fb.upload_bytes=sizeof upload;
 if(jy_pxa_fs_bind())return PXA_STATUS_INTERNAL;
 jy_sdl_ops ops={NULL,host_now,host_delay,host_present,host_poll};if(jy_sdl_bind(&ops))return PXA_STATUS_INTERNAL;
 render();int32_t rc=pxfb_start(&fe.fb,config,n,16);if(!rc){pxlog_t l;pxlog_init(&l);pxlog_text(&l,"JY action=start");pxlog_field(&l,"screen_w",fe.fb.screen_width);pxlog_field(&l,"screen_h",fe.fb.screen_height);pxlog_send(&l);}return rc;
}
int32_t pxa_app_on_event(const uint8_t *data,uint32_t n){
 pxa_event_t event;int io_event=pxa_parse_event(data,n,&event)?jy_pxa_fs_handle_event(&event):0;
 uint32_t flags=pxfb_event(&fe.fb,data,n);if(flags&PXFB_ERROR)return PXA_STATUS_INTERNAL;
 if(flags&PXFB_LIFECYCLE){release_keys();fe.credit_us=0;fe.wall_us=0;fe.dirty=1;trace(fe.fb.background?"background":"foreground");}
 if((flags&PXFB_CONTROLLER)&&!fe.paused&&!fe.fb.background){
  static const uint32_t masks[]={PXA_CONTROLLER_UP,PXA_CONTROLLER_DOWN,PXA_CONTROLLER_LEFT,PXA_CONTROLLER_RIGHT,
   PXA_CONTROLLER_A|PXA_CONTROLLER_R,PXA_CONTROLLER_B|PXA_CONTROLLER_START,PXA_CONTROLLER_L,PXA_CONTROLLER_X,PXA_CONTROLLER_Y,PXA_CONTROLLER_SELECT};
  static const int keys[]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT,SDLK_RETURN,SDLK_ESCAPE,SDLK_SPACE,SDLK_y,SDLK_n,SDLK_TAB};
  for(unsigned i=0;i<sizeof keys/sizeof keys[0];i++)if(!!(fe.controller_buttons&masks[i])!=!!(fe.fb.buttons&masks[i]))enqueue_key(keys[i],!!(fe.fb.buttons&masks[i]));
  fe.controller_buttons=fe.fb.buttons;
 }
 if(flags&PXFB_BACK){fe.paused=!fe.paused;release_keys();fe.credit_us=0;fe.dirty=1;trace(fe.paused?"pause":"resume");}
 if(flags&PXFB_POINTER){int x=fe.fb.pointer_x,y=fe.fb.pointer_y,down=fe.fb.pointer_phase==PXA_POINTER_DOWN;
  if(down&&y>=4&&y<34&&x>=284&&x<392){fe.paused=!fe.paused;release_keys();fe.credit_us=0;trace(fe.paused?"pause":"resume");}
  else if(!fe.paused){
   if(down&&y>=4&&y<34&&x>=8&&x<108){enqueue_key(SDLK_ESCAPE,1);enqueue_key(SDLK_ESCAPE,0);}
   else if(down){
    /* Each virtual button is one activation, like MENU and the native
     * reference's key pulse. A held pointer must not become the original
     * game's 30ms keyboard autorepeat. MOVE/UP never create another key. */
    int key=key_at(x,y);if(key){enqueue_key(key,1);enqueue_key(key,0);}
   }
   trace("pointer");
  }fe.dirty=1;
 }
 if(flags&PXFB_TICK){uint64_t now=fe.fb.timestamp_us,delta=fe.wall_us&&now>fe.wall_us?now-fe.wall_us:16000;fe.wall_us=now;jy_pxa_fs_tick(now);if(delta>100000)delta=100000;if(!fe.paused&&!fe.fb.background){fe.credit_us+=delta;if(fe.credit_us>100000)fe.credit_us=100000;}}
 if(fe.fb.ready&&(io_event||(flags&(PXFB_READY|PXFB_TICK|PXFB_POINTER|PXFB_BACK|PXFB_LIFECYCLE))))jy_front_drive();
 /* Keep fast imports together, but return after a slow filesystem operation.
  * Real wall time, rather than the virtual game clock, bounds the batch. */
 int host_work=0;
 uint64_t began=monotonic_us();
 for(unsigned i=0;i<8&&jy_host_call_pending();i++){
  uint32_t kind,operation;jy_host_describe(&kind,&operation);unsigned seq=++host_sequence;
  boundary("host-begin",seq,kind,operation,0);
  int dispatched=jy_host_dispatch();host_work|=dispatched;boundary("host-end",seq,kind,operation,jy_host_last_result());
  if(!dispatched||!began||monotonic_us()-began>=2000)break;
  jy_front_drive();
  if(monotonic_us()-began>=2000)break;
 }
 if((flags&PXFB_TICK)&&fe.wall_us>=fe.next_trace_us){fe.next_trace_us=fe.wall_us+500000;trace("tick");}
 if(fe.fb.ready&&(fe.dirty||(flags&(PXFB_READY|PXFB_BACK|PXFB_POINTER|PXFB_LIFECYCLE)))){
  render();
  /* The common framebuffer installs a palette once at context creation.
   * Our original software screen uses a fresh palette for each snapshot. The
   * SDK helper submits this palette itself; do not submit it a second time. */
  if(pxa_raster_upload_palette_rgb565(fe.fb.context,fe.fb.palette,fe.fb.upload,
      fe.fb.upload_bytes)!=(int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES+512u))return PXA_STATUS_INTERNAL;
  if(pxfb_present(&fe.fb)<0)return PXA_STATUS_INTERNAL;
 }
 return flags||io_event||host_work?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(void){jy_coroutine_cancel();fe.in_game=0;fe.vm=NULL;jy_host_calls_reset();jy_pxa_fs_cancel();trace("stop");jy_fs_reset();SDL_Quit();pxfb_stop(&fe.fb);}
