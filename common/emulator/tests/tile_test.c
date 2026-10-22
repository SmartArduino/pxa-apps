/* Exercise the actual private decoder against independent four-plane pixels. */
#include "core.h"
#include "../engines/smsplus/render.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
 uint8_t *rom=calloc(1,EMU_ROM_CAPACITY(n));assert(rom&&fread(rom,1,n,f)==n);fclose(f);assert(!emu_core_load(rom,n));
 for(unsigned phase=0;phase<4;++phase){
  if(!phase)vdp_reset();
  for(unsigned i=0;i<0x4000;++i){
   uint8_t value=phase==0?0:(uint8_t)((i*109u+(i>>4)*37u+phase*193u)^(i>>7));
   vdp.addr=i;vdp.code=1;
   if(phase%3==0)vdp_write(0,value);else if(phase%3==1)gg_vdp_write(0,value);else tms_write(0,value);
  }
  for(unsigned name=0;name<512;++name)for(unsigned y=0;y<8;++y)for(unsigned flip=0;flip<4;++flip){
   unsigned attr=name|((flip&1)?0x200:0)|((flip&2)?0x400:0),sy=(flip&2)?y^7:y;
   for(unsigned hit=0;hit<2;++hit){const uint8 *p=tile_get(attr,y);
    for(unsigned x=0;x<8;++x){unsigned bit=(flip&1)?x:7-x,expected=0;
     for(unsigned plane=0;plane<4;++plane)expected|=((vdp.vram[name*32+sy*4+plane]>>bit)&1)<<plane;
     assert(p[x]==expected);
    }
   }
  }
 }
 emu_core_close();free(rom);puts("PASS: all 512 patterns, rows, both flips, cache hits and live VRAM changes");
}
