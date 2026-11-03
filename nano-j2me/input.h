/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "settings.h"
typedef struct {
    nj_event queue[128];
    unsigned head,tail,quit;
    uint16_t refs[NJ_KEY_COUNT];
    uint64_t pads[2];
} nano_input;
void nano_input_push(nano_input *s,nj_event event);
void nano_input_key(nano_input *s,unsigned key,int down);
void nano_input_pad(nano_input *s,unsigned pad,uint32_t buttons,const uint8_t map[NANO_PAD_BUTTONS]);
void nano_input_release(nano_input *s);
int nano_input_poll(nano_input *s,nj_event *event);
