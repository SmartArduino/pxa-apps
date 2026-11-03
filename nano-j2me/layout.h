/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdint.h>
typedef struct { int x,y,w,h; } nano_rect;
typedef struct {
    int width,height,scale;
    nano_rect safe;
} nano_layout;
int nano_canvas_size(int display_w,int display_h,int *w,int *h);
nano_rect nano_safe_rect(int w,int h,unsigned shape,const unsigned inset[4],const unsigned radius[4]);
nano_rect nano_fit(nano_rect area,int w,int h);
int nano_inside(nano_rect r,int x,int y);
int nano_clamp(int n,int low,int high);
