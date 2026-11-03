/* SPDX-License-Identifier: MIT. Public PXA C ABI transport, no desktop emulation. */
#include <stdint.h>
#include <string.h>
#include "pxa_framebuffer.h"
#include "pxa_port_log.h"
#include "ui.h"
static unsigned char image[400*416],upload[PXA_RASTER_UPLOAD_HEADER_BYTES+400*416];
static struct { pxfb_t fb; opr_ui ui; uint32_t ticks; } port;
static void trace(const char *event) {
    const opr_session *s=&port.ui.session; const OPR_GameState *g=&s->game;
    pxlog_t l;pxlog_init(&l);pxlog_text(&l,"OPR ");pxlog_text(&l,event);pxlog_field(&l,"scene",port.ui.scene);
    pxlog_text(&l," seed=");pxlog_uint(&l,s->seed);pxlog_field(&l,"turn",s->actions);pxlog_field(&l,"floor",g->player.floor);pxlog_field(&l,"x",g->player.x);pxlog_field(&l,"y",g->player.y);
    pxlog_field(&l,"hp",g->player.hp);pxlog_field(&l,"lvl",g->player.level);pxlog_field(&l,"xp",g->player.xp);pxlog_field(&l,"combat",g->inCombat);pxlog_field(&l,"pot",g->player.potions);pxlog_field(&l,"paused",s->paused);pxlog_field(&l,"win",g->victory);pxlog_field(&l,"dead",g->gameOver);pxlog_field(&l,"hash",(int32_t)opr_state_hash(g));pxlog_send(&l);
}
int32_t pxa_app_start(const uint8_t *cfg,uint32_t n) {
    memset(&port,0,sizeof(port));opr_ui_init(&port.ui);
    port.fb.width=400;port.fb.height=416;port.fb.pixels=image;port.fb.upload=upload;port.fb.upload_bytes=sizeof(upload);
    for(unsigned i=0;i<16;i++)port.fb.palette[i]=opr_ui_palette[i];opr_ui_render(&port.ui,image);
    trace("start");return pxfb_start(&port.fb,cfg,n,32);
}
int32_t pxa_app_on_event(const uint8_t *data,uint32_t n) {
    uint32_t f=pxfb_event(&port.fb,data,n);if(f&PXFB_ERROR)return PXA_STATUS_INTERNAL;
    if(f&PXFB_LIFECYCLE){opr_ui_background(&port.ui,port.fb.background);port.fb.buttons=0;trace(port.fb.background?"background":"foreground");}
    if(f&PXFB_BACK){opr_ui_back(&port.ui);trace("back");}
    if(f&PXFB_CONTROLLER){opr_ui_controller(&port.ui,port.fb.buttons);trace("controller");}
    if((f&PXFB_POINTER)&&port.fb.pointer_phase==PXA_POINTER_DOWN){opr_ui_pointer(&port.ui,port.fb.pointer_x,port.fb.pointer_y);trace("pointer");}
    if(f&PXFB_TICK){port.ticks++;if((port.ticks&31)==0)trace("tick");}
    if((f&(PXFB_READY|PXFB_TICK|PXFB_POINTER|PXFB_CONTROLLER|PXFB_BACK|PXFB_LIFECYCLE))&&port.fb.ready){opr_ui_render(&port.ui,image);if(pxfb_present(&port.fb)<0)return PXA_STATUS_INTERNAL;}
    return f?PXA_EVENT_HANDLED:PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(uint32_t reason){(void)reason;trace("stop");pxfb_stop(&port.fb);memset(&port.ui,0,sizeof(port.ui));}
