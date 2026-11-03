/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "input.h"
#include <string.h>
_Static_assert(NJ_KEY_COUNT<=64,"key mask capacity");
void nano_input_push(nano_input *s,nj_event event) {
    if(s->tail-s->head>=128){s->head=s->tail=0;s->quit=1;return;}
    s->queue[s->tail++%128]=event;
}
void nano_input_key(nano_input *s,unsigned key,int down) {
    if(!key || key>=NJ_KEY_COUNT)return;
    if(down){if(!s->refs[key]++)nano_input_push(s,(nj_event){.type=NJ_EV_KEY_DOWN,.key=key});}
    else if(s->refs[key] && !--s->refs[key])nano_input_push(s,(nj_event){.type=NJ_EV_KEY_UP,.key=key});
}
void nano_input_pad(nano_input *s,unsigned pad,uint32_t buttons,const uint8_t map[NANO_PAD_BUTTONS]) {
    if(pad>=2)return;uint64_t next=0,old=s->pads[pad];
    for(unsigned i=0;i<NANO_PAD_BUTTONS;++i)if((buttons&(1u<<i)) && map[i]>0 && map[i]<NJ_KEY_COUNT)next|=UINT64_C(1)<<map[i];
    for(unsigned k=1;k<NJ_KEY_COUNT;++k){uint64_t bit=UINT64_C(1)<<k;if((old^next)&bit)nano_input_key(s,k,(next&bit)!=0);}
    s->pads[pad]=next;
}
void nano_input_release(nano_input *s) {
    for(unsigned k=1;k<NJ_KEY_COUNT;++k)if(s->refs[k]){s->refs[k]=0;nano_input_push(s,(nj_event){.type=NJ_EV_KEY_UP,.key=k});}
    memset(s->pads,0,sizeof(s->pads));
}
int nano_input_poll(nano_input *s,nj_event *event) {
    if(s->quit){s->quit=0;*event=(nj_event){.type=NJ_EV_QUIT};return 1;}
    if(s->head==s->tail)return 0;*event=s->queue[s->head++%128];return 1;
}
