/* Visible, indeterminate startup feedback; no guessed completion percentage. */
#pragma once
#include "framebuffer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static inline void port_loading_rect(uint16_t *p, int w, int h,
    int x, int y, int width, int height, uint16_t color) {
  int x0=x*w/320, y0=y*h/240, x1=(x+width)*w/320, y1=(y+height)*h/240;
  for(int row=y0;row<y1;++row)
    for(int col=x0;col<x1;++col) p[row*w+col]=color;
}
static inline void port_draw_loading(uint16_t *p, int w, int h,
    const char *title, const char *stage, uint64_t elapsed_us) {
  if(!p || w<=0 || h<=0) return;
  for(int i=0;i<w*h;++i) p[i]=0x0841;
  port_loading_rect(p,w,h,32,40,256,2,0x07e0);
  port_draw_text(p,w,h,32,58,title);
  port_draw_text(p,w,h,32,94,"Loading...");
  /* Bound text to the available width; stage strings are ordinary UI text. */
  char line[32];snprintf(line,sizeof(line),"%.30s",stage);
  port_draw_text(p,w,h,32,114,line);
  port_loading_rect(p,w,h,32,142,256,14,0x3186);
  unsigned offset=(unsigned)(elapsed_us/100000)%432;
  if(offset>216)offset=432-offset;
  port_loading_rect(p,w,h,32+(int)offset,144,40,10,0x07e0);
  unsigned seconds=(unsigned)(elapsed_us/1000000);
  if(seconds>99999)seconds=99999;
  snprintf(line,sizeof(line),"Elapsed: %u s",seconds);
  port_draw_text(p,w,h,32,178,line);
}
