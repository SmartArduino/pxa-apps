/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "controls.h"
#include "input.h"
#include "layout.h"
#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned labels;
static void label(uint16_t *p,int w,int h,const char *s) {
    ++labels;(void)s;
    for(int y=4;y<h-4;++y)for(int x=5;x<w-5;++x)if((x+y)%3==0)p[y*w+x]=0xffff;
}
static void layouts(void) {
    const unsigned displays[][2]={{160,160},{176,208},{240,240},{296,240},{296,222},{200,198},{320,240},{480,480},{800,480},{480,800},{1280,720},{1080,1920},{1920,1080}};
    nano_controls *controls=calloc(1,sizeof(*controls));assert(controls);
    for(unsigned d=0;d<sizeof(displays)/sizeof(*displays);++d) {
        int w,h,scale=nano_canvas_size(displays[d][0],displays[d][1],&w,&h);
        assert(scale && !(w&7) && !(h&1) && !((unsigned)w*h*2%64) && (unsigned)w*h<=153600);
        assert(w*scale<=(int)displays[d][0] && h*scale<=(int)displays[d][1]);
        size_t pixels=(size_t)w*h;
        uint16_t *allocation=malloc((pixels+32)*2),*image=allocation+16,*saved=malloc(pixels*2);assert(allocation && saved);
        for(unsigned shape=0;shape<3;++shape) {
            unsigned inset[4]={3,4,5,6},radius[4]={25,25,25,25};
            nano_rect safe=nano_safe_rect(w,h,shape,inset,radius),view=nano_fit(safe,240,320);
            assert(safe.w>0 && safe.h>0 && nano_inside((nano_rect){0,0,w,h},safe.x,safe.y));
            assert(view.x>=safe.x && view.y>=safe.y && view.x+view.w<=safe.x+safe.w && view.y+view.h<=safe.y+safe.h);
            assert(abs(view.w*320-view.h*240)<=320);
            for(unsigned numeric=0;numeric<2;++numeric)for(unsigned size=0;size<3;++size) {
                nano_settings s;nano_settings_default(&s);s.numeric=numeric;s.button_size=size;
                nano_controls_build(controls,safe,&s,1,label);
                assert(controls->count && controls->used<=NANO_CONTROL_MASK_BYTES);
                for(unsigned i=0;i<controls->count;++i) {
                    nano_rect r=controls->buttons[i].rect;
                    assert(r.x>=safe.x && r.y>=safe.y && r.x+r.w<=safe.x+safe.w && r.y+r.h<=safe.y+safe.h);
                }
                for(size_t i=0;i<pixels+32;++i)allocation[i]=0x6b4d;
                nano_controls_draw(controls,image,w,0,0);memcpy(saved,image,pixels*2);
                unsigned calls=labels,rebuilds=controls->cache_rebuilds;
                for(unsigned frame=0;frame<3;++frame) {
                    for(size_t i=0;i<pixels;++i)image[i]=0x6b4d;
                    nano_controls_draw(controls,image,w,0,0);
                    assert(!memcmp(saved,image,pixels*2));
                }
                assert(labels==calls && controls->cache_rebuilds==rebuilds);
                for(unsigned i=0;i<16;++i)assert(allocation[i]==0x6b4d && allocation[pixels+16+i]==0x6b4d);
                unsigned action;int button;
                assert(!nano_controls_hit(controls,-1,-1,&action,&button) && !action && button==-1);
                nano_button menu=controls->buttons[0];
                assert(!nano_controls_hit(controls,menu.rect.x+menu.rect.w/2,menu.rect.y+menu.rect.h/2,&action,&button));
                assert(action==NANO_ACTION_MENU && button==0);
            }
            nano_settings s;nano_settings_default(&s);nano_controls_build(controls,safe,&s,0,label);assert(controls->count==1);
        }
        free(saved);free(allocation);
    }
    free(controls);
}
static void expect(nano_input *input,unsigned type,unsigned key) {
    nj_event e;assert(nano_input_poll(input,&e));assert(e.type==type && e.key==key);
}
static void inputs(void) {
    nano_input input={0};nano_settings s;nano_settings_default(&s);nano_settings_phone_keys(&s);
    assert(s.map[0]==NJ_KEY_2 && s.map[4]==NJ_KEY_5 && s.map[6]==NANO_ACTION_MENU);
    nano_settings_default(&s);s.map[9]=NJ_KEY_FIRE;
    nano_input_pad(&input,0,(1u<<4)|(1u<<9),s.map);expect(&input,NJ_EV_KEY_DOWN,NJ_KEY_FIRE);
    nano_input_key(&input,NJ_KEY_FIRE,1);nano_input_pad(&input,1,1u<<4,s.map);
    nano_input_pad(&input,0,1u<<9,s.map);nano_input_pad(&input,0,0,s.map);nano_input_key(&input,NJ_KEY_FIRE,0);
    nj_event e;assert(!nano_input_poll(&input,&e));nano_input_pad(&input,1,0,s.map);expect(&input,NJ_EV_KEY_UP,NJ_KEY_FIRE);
    nano_input_pad(&input,0,1u<<4,s.map);expect(&input,NJ_EV_KEY_DOWN,NJ_KEY_FIRE);
    nano_input_release(&input);expect(&input,NJ_EV_KEY_UP,NJ_KEY_FIRE);
    s.map[4]=NJ_KEY_5;nano_input_pad(&input,0,1u<<4,s.map);expect(&input,NJ_EV_KEY_DOWN,NJ_KEY_5);
    nano_input_pad(&input,0,0,s.map);expect(&input,NJ_EV_KEY_UP,NJ_KEY_5);
    for(unsigned i=0;i<129;++i)nano_input_push(&input,(nj_event){.type=NJ_EV_KEY_DOWN,.key=NJ_KEY_0});
    expect(&input,NJ_EV_QUIT,0);
}
static int fail_replace(const char *a,const char *b){(void)a;(void)b;return -1;}
static void settings(const char *root) {
    nano_settings original,decoded,edited;nano_settings_default(&original);edited=original;
    edited.fps_cap=60;edited.heap_kib=3072;edited.width=360;edited.height=640;edited.map[4]=NJ_KEY_5;edited.alpha=6;
    static const uint8_t legacy[]={78,74,80,50,1,0,36,0,240,0,64,1,0,3,1,0,13,14,15,16,17,19,28,1,18,19,11,12,0,0,0,0,135,221,31,65};
    assert(nano_settings_decode(&decoded,legacy,sizeof(legacy)) && decoded.heap_kib==2048 && decoded.fps_cap==30);
    assert(!memcmp(&decoded,&original,sizeof(original)));
    static const uint8_t legacy2[]={78,74,80,50,2,0,36,0,240,0,64,1,0,3,1,0,13,14,15,16,17,19,28,1,18,19,11,12,0,12,0,0,196,35,219,74};
    assert(nano_settings_decode(&decoded,legacy2,sizeof(legacy2)) && decoded.heap_kib==3072 && decoded.fps_cap==30);
    uint8_t bytes[NANO_SETTINGS_BYTES];assert(nano_settings_encode(&edited,bytes));
    assert(nano_settings_decode(&decoded,bytes,sizeof(bytes)) && !memcmp(&decoded,&edited,sizeof(edited)));
    edited.heap_kib=2816;assert(nano_settings_encode(&edited,bytes));
    assert(nano_settings_decode(&decoded,bytes,sizeof(bytes)) && decoded.heap_kib==2816);
    edited.heap_kib=3072;assert(nano_settings_encode(&edited,bytes));
    bytes[16]^=1;assert(!nano_settings_decode(&decoded,bytes,sizeof(bytes)));
    assert(!nano_settings_decode(&decoded,bytes,sizeof(bytes)-1));
    edited.map[0]=255;assert(!nano_settings_encode(&edited,bytes));edited.map[0]=original.map[0];
    edited.heap_kib=3073;assert(!nano_settings_encode(&edited,bytes));edited.heap_kib=3072;
    edited.fps_cap=45;assert(!nano_settings_encode(&edited,bytes));
    edited.fps_cap=0;assert(!nano_settings_encode(&edited,bytes));edited.fps_cap=60;
    char path[1024];snprintf(path,sizeof(path),"%s/settings.njp",root);
    assert(nano_settings_save(&original,path,rename));assert(!nano_settings_save(&edited,path,fail_replace));
    assert(nano_settings_load(&decoded,path) && !memcmp(&decoded,&original,sizeof(original)));
    assert(nano_settings_save(&edited,path,rename));assert(nano_settings_load(&decoded,path) && !memcmp(&decoded,&edited,sizeof(edited)));
    remove(path);
}
int main(int argc,char **argv) {
    assert(argc==2);layouts();inputs();settings(argv[1]);
    puts("adaptive/round layouts, cached masks, multi-source keys, mapping and atomic settings passed");return 0;
}
