/* SPDX-License-Identifier: GPL-3.0-or-later
 * Event-driven PXA frontend retaining upstream Meritous C gameplay and assets.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "SDL.h"
#include "mt_platform.h"
#include "pxa_framebuffer.h"
#include "assets.h"

#define GAME_W 640
#define GAME_H 480
#define PORT_H 528
#define ASYNC_STACK_BYTES 2097152
static uint8_t pixels[GAME_W*PORT_H],upload[sizeof(pixels)+PXA_RASTER_UPLOAD_HEADER_BYTES];
static pxfb_t fb;
static uint8_t async_stack[ASYNC_STACK_BYTES] __attribute__((aligned(16)));
static struct {uint32_t pos,end;} async_data;
static int suspended,rewinding,started,completed;
static uint32_t deadline,log_next;
static SDL_Event events[64];
static unsigned events_read,events_write;
static SDLKey held=SDLK_UNKNOWN;
/* Callbacks and Host-invoked malloc/free must not reuse a suspended game's C
 * linear stack. Asyncify saves Wasm locals but does not copy this memory. */
uint32_t mt_game_stack_pointer;
void mt_call_game(void);
uint32_t mt_get_stack_pointer(void);
static char *game_args[]={"meritous",NULL};
extern int player_x,player_y,player_room,magic_circuit,circuit_release;
extern int game_running,game_paused,voluntary_exit;
extern int total_enemies,killed_enemies;
extern unsigned char font_data[128][8][8];
int meritous_main(int argc,char **argv);

FILE *mt_fopen(const char *name,const char *mode){
    if(mode[0]=='r')for(unsigned i=0;i<sizeof(mt_assets)/sizeof(mt_assets[0]);i++){
        const struct mt_asset *a=&mt_assets[i];
        if(!strcmp(name,a->name)&&!a->w)return fmemopen((void*)a->pixels,a->bytes,"rb");
    }
    return fopen(name,mode);
}

__attribute__((import_module("asyncify"),import_name("start_unwind"))) void async_start_unwind(void*);
__attribute__((import_module("asyncify"),import_name("stop_unwind"))) void async_stop_unwind(void);
__attribute__((import_module("asyncify"),import_name("start_rewind"))) void async_start_rewind(void*);
__attribute__((import_module("asyncify"),import_name("stop_rewind"))) void async_stop_rewind(void);

__attribute__((noinline)) static void yield_to_pxa(void) {
    if(rewinding){async_stop_rewind();rewinding=0;suspended=0;return;}
    /* Persist the live depth before Asyncify unwinds C prologues. Rewind must
     * restore this exact pointer, while frontend callbacks use their own stack. */
    mt_game_stack_pointer=mt_get_stack_pointer();
    suspended=1;async_start_unwind(&async_data);
}
void SDL_Delay(Uint32 ms){deadline=fb.time_ms+(ms?ms:1);yield_to_pxa();}
Uint32 SDL_GetTicks(void){return fb.time_ms;}
int SDL_PollEvent(SDL_Event *event){
    if(events_read==events_write)return 0;
    if(event)*event=events[events_read%64];events_read++;return 1;
}
static void queue(SDLKey key,int down){
    if(events_write-events_read>=64)return;
    SDL_Event *e=&events[events_write++%64];memset(e,0,sizeof(*e));
    e->type=down?SDL_KEYDOWN:SDL_KEYUP;e->key.state=down?SDL_PRESSED:SDL_RELEASED;
    e->key.keysym.sym=key;
}
static void release(void){if(held!=SDLK_UNKNOWN)queue(held,0);held=SDLK_UNKNOWN;}
static void pointer(void){
    if(fb.pointer_phase==PXA_POINTER_UP||fb.pointer_phase==PXA_POINTER_CANCEL){release();return;}
    if(fb.pointer_y<GAME_H||fb.pointer_x<0||fb.pointer_x>=GAME_W)return;
    static const SDLKey keys[10]={SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT,SDLK_SPACE,SDLK_RETURN,SDLK_TAB,SDLK_h,SDLK_p,SDLK_ESCAPE};
    SDLKey key=keys[fb.pointer_x/64];
    if(key==held)return;release();held=key;queue(key,1);
    char line[96];snprintf(line,sizeof(line),"Meritous TOUCH key=%d",key);pxa_log_write(PXA_LOG_LEVEL_INFO,line);
}
static void label(int x,int y,const char *s){
    for(;*s;s++,x+=8){unsigned c=(unsigned char)*s;
        if(c>=128)continue;
        for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)
            if(font_data[c][xx][yy]&&x+xx<GAME_W)pixels[(y+yy)*GAME_W+x+xx]=255;
    }
}
void mt_present(SDL_Surface *s){
    if(!fb.ready||!s)return;
    for(int y=0;y<GAME_H;y++)memcpy(pixels+y*GAME_W,(uint8_t*)s->pixels+y*s->pitch,GAME_W);
    memset(pixels+GAME_H*GAME_W,24,(PORT_H-GAME_H)*GAME_W);
    static const char *labels[10]={"UP","DOWN","LEFT","RIGHT","PSI","ENTER","MAP","HELP","PAUSE","EXIT"};
    for(int i=0;i<10;i++){
        for(int y=GAME_H;y<PORT_H;y++)pixels[y*GAME_W+i*64]=220;
        label(i*64+5,GAME_H+19,labels[i]);
    }
    int32_t status=pxfb_present(&fb);
    if(status<0){char msg[80];snprintf(msg,sizeof(msg),"Meritous render status=%d",status);pxa_log_write(PXA_LOG_LEVEL_ERROR,msg);}
    if(fb.time_ms>=log_next){
        log_next=fb.time_ms+500;char msg[180];
        snprintf(msg,sizeof(msg),"Meritous t=%u x=%d y=%d room=%d circuit=%d release=%d running=%d paused=%d exit=%d enemies=%d killed=%d",fb.time_ms,player_x,player_y,player_room,magic_circuit,circuit_release,game_running,game_paused,voluntary_exit,total_enemies,killed_enemies);
        pxa_log_write(PXA_LOG_LEVEL_INFO,msg);
    }
}
void mt_palette(SDL_Color *colors,int first,int count){
    for(int i=0;i<count;i++){
        SDL_Color c=colors[i];fb.palette[first+i]=(uint16_t)(((c.r>>3)<<11)|((c.g>>2)<<5)|(c.b>>3));
    }
    if(fb.ready)(void)pxa_raster_upload_palette_rgb565(fb.context,fb.palette,fb.upload,fb.upload_bytes);
}
/* Normal upstream exit returns control to PXA. A fatal upstream exit traps. */
void mt_exit(int status){
    if(status){char msg[90];snprintf(msg,sizeof(msg),"Meritous fatal exit=%d",status);pxa_log_write(PXA_LOG_LEVEL_ERROR,msg);__builtin_trap();}
    completed=1;
    for(;;)yield_to_pxa();
}
void exit(int status){mt_exit(status);__builtin_unreachable();}
void abort(void){__builtin_trap();}
__attribute__((noinline)) void mt_run_game(void){
    if(suspended){rewinding=1;async_start_rewind(&async_data);}
    (void)meritous_main(1,game_args);
    if(suspended)async_stop_unwind();else completed=1;
}
int32_t mt_app_start(const uint8_t *config,uint32_t length){
    memset(&fb,0,sizeof(fb));fb.pixels=pixels;fb.upload=upload;fb.upload_bytes=sizeof(upload);
    fb.width=GAME_W;fb.height=PORT_H;
    for(unsigned i=0;i<256;i++)fb.palette[i]=((i>>3)<<11)|((i>>2)<<5)|(i>>3);
    async_data.pos=(uint32_t)(uintptr_t)async_stack;async_data.end=async_data.pos+sizeof(async_stack);
    suspended=rewinding=started=completed=0;deadline=log_next=0;events_read=events_write=0;held=SDLK_UNKNOWN;
    pxa_log_write(PXA_LOG_LEVEL_INFO,"Meritous upstream 2b86412346ed; PXA touch frontend; silent; session-only saves");
    fb.pointer_controller=1;return pxfb_start(&fb,config,length,20);
}
int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t length){
    uint32_t flags=pxfb_event(&fb,bytes,length);
    if(flags&PXFB_ERROR)return fb.error;
    if(flags&PXFB_POINTER)pointer();
    if(flags&PXFB_LIFECYCLE)release();
    if(flags&PXFB_BACK){release();queue(SDLK_ESCAPE,1);queue(SDLK_ESCAPE,0);return PXA_EVENT_HANDLED;}
    if((flags&PXFB_READY)&&!started){started=1;mt_call_game();}
    else if((flags&PXFB_TICK)&&started&&!completed&&(int32_t)(fb.time_ms-deadline)>=0)mt_call_game();
    return flags?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(uint32_t reason){(void)reason;release();pxfb_stop(&fb);}
