/* MPL-2.0. Dynamic geometry and two-buffer composition against independent scaling. */
#include "video_renderer.h"
#include "controls.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static const unsigned screens[][2]={{200,160},{296,240},{320,240},{240,320},{480,480},{800,480}};
static const unsigned sources[][4]={{159,143,10,9},{257,225,4,3},{160,144,10,9},{256,192,4,3},{256,224,4,3},{320,240,4,3},{512,478,4,3}};
int main(void){
 emu_core_defaults();
 uint16_t *source=malloc(523u*512u*2u);assert(source);
 for(unsigned n=0;n<sizeof(screens)/sizeof(*screens);++n){
  unsigned w=screens[n][0],h=screens[n][1],stride=w+7;
  size_t bytes=(stride*h+64)*2u;
  uint16_t *out[2]={calloc(1,bytes),calloc(1,bytes)},*reference=calloc(1,bytes);
  emu_video_renderer_t r;emu_controls_t *c=malloc(sizeof(*c));assert(out[0]&&out[1]&&reference&&c);
  emu_video_init(&r,w,h,(emu_display_rect_t){3,4,w-6,h-8});emu_video_set_stride(&r,stride);
  emu_controls_init(c,w,h);c->stride=stride;
  for(unsigned mode=0;mode<sizeof(sources)/sizeof(*sources);++mode){
   unsigned sw=sources[mode][0],sh=sources[mode][1],ss=sw+11;
   emu_core_info.width=sw;emu_core_info.height=sh;emu_core_info.stride=ss;
   emu_core_info.aspect_num=sources[mode][2];emu_core_info.aspect_den=sources[mode][3];
   emu_video_sync(&r);assert(r.view_width && r.view_height);
   assert(r.view_x+r.view_width<=w && r.view_y+r.view_height<=h);
   for(size_t i=0;i<(size_t)ss*sh;++i)source[i]=(uint16_t)(i*109+73);
   memset(out[0],0,bytes);memset(out[1],0,bytes);
   for(unsigned frame=0;frame<24;++frame){
    unsigned index=frame&1,pressed=frame>=12 && frame<18?EMU_PAD_A|EMU_PAD_UP:0;
    unsigned force=frame<2 || frame==12 || frame==13 || frame==18 || frame==19;
    if(frame>=8 && frame<12)source[(frame*13%sh)*ss+(frame*7%sw)]^=0x5c73;
    uint8_t dirty[EMU_DISPLAY_MAX_HEIGHT];
    unsigned pixels=emu_video_render_cached(&r,out[index],source,index,force,dirty);
    if(frame>=2 && frame<8)assert(!pixels);
    emu_display_rect_t view={r.view_x,r.view_y,r.view_width,r.view_height};
    emu_controls_clear_bars_rows(c,out[index],&view,dirty);emu_controls_draw_rows(c,out[index],pressed,dirty);
    memset(reference,0,bytes);
    for(unsigned y=0;y<r.view_height;++y)for(unsigned x=0;x<r.view_width;++x)
     reference[(r.view_y+y)*stride+r.view_x+x]=source[(y*sh/r.view_height)*ss+x*sw/r.view_width];
    emu_controls_draw(c,reference,pressed);
    assert(!memcmp(out[index],reference,bytes));
   }
  }
  emu_video_deinit(&r);emu_video_deinit(&r);free(c);free(out[0]);free(out[1]);free(reference);
 }
 free(source);puts("PASS: dynamic geometry, hires, aspect fit, two buffers, controls, strides and six screen sizes");
}
