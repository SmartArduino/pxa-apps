/* SPDX-License-Identifier: MIT. Observe actual SDK transport and tile payloads. */
#include "../pxa_framebuffer.h"
#include <assert.h>
#include <stdio.h>
static uint8_t pixels[800*552],scratch[PXFB_UPLOAD_BYTES];
static uint8_t tiles[16][256*256];
static unsigned widths[16],heights[16],uploads,submits;
static int blocked;
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t length) {
    (void)route;(void)token;(void)data;(void)length;return 0;
}
int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t *data,uint32_t length) {
    assert(handle==1);
    if(blocked){blocked=0;return PXA_STATUS_WOULD_BLOCK;}
    if(op==PXA_GAME_RENDER_IO_UPLOAD){
        assert(length>=20&&pxa_load_u32(data)==PXA_RASTER_UPLOAD_MAGIC);
        if(data[8]==PXA_RASTER_UPLOAD_TEXTURE_INDEX8){
            unsigned slot=data[9];assert(slot<16);
            widths[slot]=pxa_load_u16(data+12);heights[slot]=pxa_load_u16(data+14);
            assert(length==20+widths[slot]*heights[slot]);
            memcpy(tiles[slot],data+20,length-20);++uploads;
        }
    }else {assert(op==PXA_GAME_RENDER_IO_SUBMIT);++submits;}
    return (int32_t)length;
}
int main(void) {
    pxfb_t fb={0};fb.pixels=pixels;fb.upload=scratch;fb.upload_bytes=sizeof(scratch);
    fb.context=1;fb.ready=1;fb.palette[1]=0xffff;
    const unsigned sources[][2]={{400,416},{296,240},{800,552}};
    const unsigned screens[][2]={{320,240},{296,240},{800,480}};
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){
        fb.width=sources[a][0];fb.height=sources[a][1];
        for(unsigned y=0;y<fb.height;++y)for(unsigned x=0;x<fb.width;++x)pixels[y*fb.width+x]=(x*13+y*19)&255;
        pxa_ui_environment_t env={0};env.width=screens[b][0];env.height=screens[b][1];env.density_q16=131072;env.safe_insets[0]=3;env.safe_insets[3]=5;
        assert(pxfb_environment(&fb,&env));uploads=0;
        assert(pxfb_present(&fb)>=0);assert(!fb.pending);
        unsigned slot=0;
        for(unsigned y=0;y<fb.viewport.height;y+=256)for(unsigned x=0;x<fb.viewport.width;x+=256,++slot)
            for(unsigned row=0;row<heights[slot];++row)for(unsigned col=0;col<widths[slot];++col){
                unsigned sx=(uint64_t)(x+col)*fb.width/fb.viewport.width;
                unsigned sy=(uint64_t)(y+row)*fb.height/fb.viewport.height;
                assert(tiles[slot][row*widths[slot]+col]==pixels[sy*fb.width+sx]);
            }
        assert(uploads==slot);
    }
    unsigned before=submits;blocked=1;assert(pxfb_present(&fb)==0&&fb.pending);assert(submits==before);
    assert(pxfb_present(&fb)>=0&&!fb.pending&&submits==before+1);
    fb.pointer_controller=1;fb.cursor_x=fb.width/2;fb.cursor_y=fb.height/2;
    fb.buttons=PXA_CONTROLLER_RIGHT|PXA_CONTROLLER_A;
    assert(pxfb_cursor(&fb,0,1)&PXFB_POINTER);assert(fb.pointer_phase==PXA_POINTER_DOWN);
    int x=fb.cursor_x;assert(!pxfb_cursor(&fb,fb.buttons,0));
    fb.timestamp_us=200000;assert(pxfb_cursor(&fb,fb.buttons,0)&PXFB_POINTER);assert(fb.cursor_x>x);
    uint32_t previous=fb.buttons;fb.buttons=0;assert(pxfb_cursor(&fb,previous,1)&PXFB_POINTER);assert(fb.pointer_phase==PXA_POINTER_UP);
    uint32_t crc=0;for(unsigned i=0;i<fb.width*fb.height;++i)crc=crc*33+pixels[i];
    assert(pxfb_present(&fb)>=0);uint32_t after=0;for(unsigned i=0;i<fb.width*fb.height;++i)after=after*33+pixels[i];assert(crc==after);
    fb.background=1;before=submits;assert(pxfb_present(&fb)==0&&submits==before);
    /* Product hosts deliver controller state to root node 1, even when
     * pointer input belongs to a child canvas. Exercise the real SDK codec. */
    uint8_t value[32]={0},packet[128];uint32_t length=0;
    pxa_store_u32(value,1);pxa_store_u32(value+4,1);pxa_store_u32(value+8,1);
    pxa_store_u16(value+12,PXA_UI_EVENT_CONTROLLER_STATE_KIND);
    value[25]=1;pxa_store_u32(value+28,PXA_CONTROLLER_A);
    assert(pxa_build_message(packet,sizeof(packet),PXA_UI_SERVICE,PXA_UI_EVENT,0,value,sizeof(value),&length));
    fb.pointer_controller=0;fb.buttons=0;fb.background=0;
    assert(pxfb_event(&fb,packet,length)&PXFB_CONTROLLER);assert(fb.buttons==PXA_CONTROLLER_A);
    value[25]=0;pxa_store_u32(value+28,0);
    assert(pxa_build_message(packet,sizeof(packet),PXA_UI_SERVICE,PXA_UI_EVENT,0,value,sizeof(value),&length));
    assert(pxfb_event(&fb,packet,length)&PXFB_CONTROLLER);assert(fb.buttons==0);
    puts("PASS: SDK uploads, pixel scaling, seams, retry, cursor drag and untouched source pixels");
}
