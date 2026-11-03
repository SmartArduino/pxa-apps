// SPDX-License-Identifier: BSD-2-Clause
#include <new>
#include <cstring>
#include "ui.hpp"
extern "C" {
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
}
using namespace pxa_tt;
alignas(UI) static unsigned char ui_storage[sizeof(UI)];
static UI* ui;
static pxfb_t fb;
static uint8_t upload[PXA_RASTER_UPLOAD_HEADER_BYTES+Width*Height];
static uint32_t previous_buttons,previous_ms;
static unsigned constructors_ready;
namespace {struct ConstructorProbe{ConstructorProbe(){constructors_ready=1;}};ConstructorProbe constructor_probe;}
static void startup_log(const char*phase,int32_t status,const uint8_t*config,uint32_t n){
 pxa_ui_environment_t env{};int valid=pxa_ui_parse_start_environment(config,n,&env);
 pxlog_t l;pxlog_init(&l);pxlog_text(&l,"TRIAD BOOT phase=");pxlog_text(&l,phase);pxlog_field(&l,"status",status);
 pxlog_field(&l,"ctors",constructors_ready);pxlog_field(&l,"config_bytes",n);pxlog_field(&l,"env_valid",valid);
 pxlog_field(&l,"width",env.width);pxlog_field(&l,"height",env.height);
 if(constructors_ready){pxlog_field(&l,"null_id",tt::Card::null.id);pxlog_field(&l,"null_x",tt::BoardTile::null.position().x);}
 pxlog_send(&l);
}
static uint32_t checksum(){uint32_t h=2166136261u;for(auto b:ui->pixels)h=(h^b)*16777619u;return h;}
static void trace(const char*action){pxlog_t l;pxlog_init(&l);pxlog_text(&l,"TRIAD PXA action=");pxlog_text(&l,action);
 unsigned occupied=0;for(unsigned y=0;y<3;y++)for(unsigned x=0;x<3;x++)if(ui->game.board.get(x,y).id!=tt::BAD_CARD_ID)occupied|=1u<<(y*3+x);
 pxlog_field(&l,"generation",ui->game.generation);pxlog_field(&l,"stage",ui->game.stage);pxlog_field(&l,"moves",ui->game.moves);
 pxlog_field(&l,"turn",ui->game.turn);pxlog_field(&l,"you",ui->game.score[0]);pxlog_field(&l,"cpu",ui->game.score[1]);
 pxlog_field(&l,"captures",ui->game.captures);pxlog_field(&l,"ai",ui->game.ai_moves);pxlog_field(&l,"paused",ui->game.paused);
 pxlog_field(&l,"background",ui->game.background);pxlog_field(&l,"winner",ui->game.winner);
 pxlog_field(&l,"occupied",occupied);pxlog_field(&l,"selected",ui->selected);pxlog_field(&l,"mode",ui->game.mode);
 pxlog_field(&l,"hand0",ui->game.hands[0].size());pxlog_field(&l,"hand1",ui->game.hands[1].size());
 pxlog_field(&l,"cursor",ui->cursor);pxlog_field(&l,"focus",ui->board_focus);pxlog_field(&l,"hash",checksum());pxlog_send(&l);}
extern "C" int32_t pxa_app_start(const uint8_t*config,uint32_t n){
 startup_log("before-ui",0,config,n);
 if(!constructors_ready||tt::Card::null.id!=tt::BAD_CARD_ID||tt::BoardTile::null.position().x!=-1)return PXA_STATUS_PROTOCOL_ERROR;
 if(ui){ui->~UI();ui=nullptr;}ui=new(ui_storage)UI();memset(&fb,0,sizeof fb);previous_buttons=previous_ms=0;
 fb.width=Width;fb.height=Height;fb.pixels=ui->pixels;fb.upload=upload;fb.upload_bytes=sizeof upload;
 for(unsigned i=0;i<256;i++){unsigned r=((i>>5)&7)*255/7,g=((i>>2)&7)*255/7,b=(i&3)*255/3;fb.palette[i]=((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
 ui->draw();trace("start");int32_t status=pxfb_start(&fb,config,n,33);startup_log("pxfb-start",status,config,n);return status;
}
extern "C" int32_t pxa_app_on_event(const uint8_t*data,uint32_t n){
 if(!ui)return PXA_EVENT_UNHANDLED;uint32_t flags=pxfb_event(&fb,data,n);
 if(flags&PXFB_ERROR)return PXA_STATUS_INTERNAL;
 if(flags&PXFB_LIFECYCLE){ui->game.background=fb.background;previous_buttons=0;previous_ms=fb.time_ms;trace(fb.background?"background":"foreground");}
 if((flags&PXFB_BACK)&&ui->game.stage==Playing){ui->game.paused=!ui->game.paused;previous_buttons=0;previous_ms=fb.time_ms;trace(ui->game.paused?"pause":"resume");}
 if(!fb.background&&(flags&PXFB_POINTER)&&fb.pointer_phase==PXA_POINTER_DOWN){ui->pointer(fb.pointer_x,fb.pointer_y);trace("pointer");}
 if(!fb.background&&(flags&PXFB_CONTROLLER)){ui->buttons(fb.buttons&~previous_buttons);previous_buttons=fb.buttons;trace("controller");}
 if(flags&PXFB_TICK){unsigned before=ui->game.moves;uint32_t delta=fb.time_ms-previous_ms;previous_ms=fb.time_ms;if(delta>100)delta=100;ui->tick(delta);if(before!=ui->game.moves)trace("move");}
 if(fb.ready&&(flags&(PXFB_READY|PXFB_TICK|PXFB_POINTER|PXFB_CONTROLLER|PXFB_BACK|PXFB_LIFECYCLE))){ui->draw();if(pxfb_present(&fb)<0)return PXA_STATUS_INTERNAL;}
 return flags?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
extern "C" void pxa_app_stop(uint32_t reason){(void)reason;if(ui){trace("stop");ui->~UI();ui=nullptr;}previous_buttons=0;pxfb_stop(&fb);}
extern "C" void __wrap___assert_fail(const char*,const char*,unsigned,const char*){pxa_log_write(4,"TTcards core assertion");__builtin_trap();}
extern "C" void __wrap_abort(){pxa_log_write(4,"TTcards abort");__builtin_trap();}
extern "C" void __wrap_exit(int){pxa_log_write(4,"TTcards unexpected exit");__builtin_trap();}
// Fatal C++ runtime diagnostics use the Guest logger, avoiding stdio/futex/
// filesystem imports from libc++'s desktop error printer. These always trap.
extern "C" [[noreturn]] void __wrap___abort_message(const char*,...){pxa_log_write(4,"TTcards C++ runtime failure");__builtin_trap();}
extern "C" [[noreturn]] void __wrap___cxa_pure_virtual(){pxa_log_write(4,"TTcards pure virtual call");__builtin_trap();}
extern "C" [[noreturn]] void __wrap___cxa_deleted_virtual(){pxa_log_write(4,"TTcards deleted virtual call");__builtin_trap();}
extern "C" [[noreturn]] void __wrap__ZNSt3__222__libcpp_verbose_abortEPKcz(const char*,...){pxa_log_write(4,"TTcards libc++ failure");__builtin_trap();}
