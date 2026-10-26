/* GPL-3.0-or-later; uses unmodified LGPL-2.1-or-later Nuked OPL3 in the guest.
 * Replaces the optional old non-commercial MAME dependency entirely. */
#include "opl_backend.h"
#include "vendor/nuked-opl3/opl3.h"
#include <stdlib.h>
#include <string.h>
static opl3_chip *opl;
static int gain=20;
int pxa_opl_init(int chips,int clock,int rate) {
  if(chips!=1 || clock!=3579545 || rate<=0)return -1;
  pxa_opl_shutdown();opl=calloc(1,sizeof(*opl));if(!opl)return -1;
  OPL3_Reset(opl,(uint32_t)rate);gain=20;return 0;
}
void pxa_opl_shutdown(void) { free(opl);opl=NULL; }
void pxa_opl_write(int chip,int address,int value,const int *volume) {
  if(!opl || chip || address<0 || address>255)return;
  if(volume)gain=*volume<0?0:*volume>20?20:*volume;
  OPL3_WriteReg(opl,(uint16_t)address,(uint8_t)value);
}
void pxa_opl_generate(int chip,int16_t *samples,int frames) {
  if(!samples || frames<=0)return;
  if(!opl || chip){memset(samples,0,(size_t)frames*2);return;}
  for(int i=0;i<frames;++i) {
    int16_t stereo[2];OPL3_GenerateResampled(opl,stereo);
    samples[i]=(int16_t)(((int)stereo[0]+stereo[1])*gain/40);
  }
}
