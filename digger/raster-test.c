/* SPDX-License-Identifier: MIT. Native comparison against real SDL2 surfaces. */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surface-lite.h"
#include "hardware.h"
#include "alpha.h"
const uint8_t *digger_pixels640(void);
const uint16_t *digger_palette565(void);
void digger_raster_reset(void);
const uint8_t *ref_digger_pixels640(void);
const uint16_t *ref_digger_palette565(void);
void ref_digger_raster_reset(void);
void ref_vgainit(void);void ref_vgaclear(void);void ref_vgapal(int16_t);
void ref_vgainten(int16_t);void ref_vgatitle(void);
void ref_vgaputim(int16_t,int16_t,int16_t,int16_t,int16_t);
void ref_vgawrite(int16_t,int16_t,int16_t,int16_t);
void ref_vgageti(int16_t,int16_t,uint8_t*,int16_t,int16_t);
void ref_vgaputi(int16_t,int16_t,uint8_t*,int16_t,int16_t);
int16_t ref_vgagetpix(int16_t,int16_t);
void raster_reference_free(void *);
#include "raster-fixture.h"
void gettitle(unsigned char *out){memcpy(out,raster_original_title,sizeof raster_original_title);}
static unsigned checks,frames,readbacks,draws;
void digger_port_draw(void){draws++;}
static void need(int ok,const char *what){
 checks++;if(!ok){fprintf(stderr,"check%u failed: %s\n",checks,what);abort();}
}
static void same(const char *what){
 need(!memcmp(digger_pixels640(),ref_digger_pixels640(),640u*400u),what);
 need(!memcmp(digger_palette565(),ref_digger_palette565(),16u*sizeof(uint16_t)),"palette565");
 frames++;
}
int main(void){
 vgainit();ref_vgainit();same("initial zero surface");
 vgatitle();ref_vgatitle();same("original title");
 need(!memcmp(digger_pixels640(),raster_original_title,256000),"exact original decompressed title");
 const int pos[][2]={{120,80},{-3,-2},{315,193},{-40,199},{330,210}};
 for(int pal=0;pal<2;pal++)for(int intensity=0;intensity<2;intensity++){
  vgapal(pal);ref_vgapal(pal);vgainten(intensity);ref_vgainten(intensity);same("all four original palettes");
  for(int ch=0;ch<120;ch++)for(unsigned p=0;p<sizeof(pos)/sizeof(*pos);p++){
   vgaputim(pos[p][0],pos[p][1],ch,raster_sprite_dims[ch][0],raster_sprite_dims[ch][1]);
   ref_vgaputim(pos[p][0],pos[p][1],ch,raster_sprite_dims[ch][0],raster_sprite_dims[ch][1]);
   same("original actor/mask + clipped blit");
  }
  for(int ch=0;ch<128;ch++)for(int color=1;color<=3;color++){
   int x=(ch*13)%340-10,y=(ch*7)%210-5;
   vgawrite(x,y,ch,color);ref_vgawrite(x,y,ch,color);
   same("original glyph/recolour/clipping");
  }
 }
 for(int y=0;y<200;y++)for(int x=0;x<=316;x+=4){
  need(vgagetpix(x,y)==ref_vgagetpix(x,y),"all valid collision readback footprints");readbacks++;
 }
 const int bad[][2]={{-1,0},{0,-1},{317,199},{319,199},{320,0},{0,200},{-32768,199},{32767,32767}};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)need(vgagetpix(bad[i][0],bad[i][1])==255,"invalid footprint safely returns wall mask");
 SDL_Surface *saved=NULL;void *ref_saved=NULL;
 vgageti(-4,-5,(uint8_t *)&saved,80,200);
 ref_vgageti(-4,-5,(uint8_t *)&ref_saved,80,200);
 vgaclear();ref_vgaclear();same("clear exact zero");
 vgaputi(-4,-5,(uint8_t *)&saved,80,200);
 ref_vgaputi(-4,-5,(uint8_t *)&ref_saved,80,200);same("clipped background get and put");
 SDL_FreeSurface(saved);raster_reference_free(ref_saved);
 digger_raster_reset();ref_digger_raster_reset();
 need(!digger_pixels640()&&!ref_digger_pixels640(),"resource reset");
 printf("{\"passed\":true,\"checks\":%u,\"full_frame_comparisons\":%u,\"valid_collision_reads\":%u,\"sprite_frames\":120,\"palettes\":4,\"original_title_bytes\":256000,\"Guest_executed\":false}\n",checks,frames,readbacks);
 return 0;
}

