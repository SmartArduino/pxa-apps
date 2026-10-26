/* GPL-3.0-or-later. Store inventory/world changes only when a checkpoint,
 * life or quest flag changes, never perform flash I/O every rendered frame. */
#include "progress.h"
#include "port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#define SAVE_PATH "/data/progress.sav"
#define TEMP_PATH "/data/progress.tmp"
typedef struct { uint32_t magic,version,bytes,crc;struct hero hero;unsigned stage[25][22][32]; } Progress;
static int signature[13],have_signature;
static uint64_t retry_after;
static uint32_t crc32(const void *memory,size_t bytes) {
  static const uint32_t table[16]={0,0x1db71064,0x3b6e20c8,0x26d930ac,0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,
    0xedb88320,0xf00f9344,0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,0xa00ae278,0xbdbdf21c};
  const uint8_t *p=memory;uint32_t crc=~0u;
  for(size_t i=0;i<bytes;++i){crc^=p[i];crc=(crc>>4)^table[crc&15];crc=(crc>>4)^table[crc&15];}
  return ~crc;
}
static void fingerprint(const struct hero *hero,int out[13]) {
  memcpy(out,hero->checkpoint,4*sizeof(int));memcpy(out+4,hero->state,2*sizeof(int));
  memcpy(out+6,hero->flags,7*sizeof(int));
}
void pxa_progress_clear(void) { unlink(SAVE_PATH);unlink(TEMP_PATH);have_signature=0; }
int pxa_progress_load(struct hero *hero,unsigned stage[25][22][32],unsigned *room) {
  FILE *file=fopen(SAVE_PATH,"rb");if(!file)return 0;
  Progress *saved=malloc(sizeof(*saved));
  int ok=saved && fread(saved,sizeof(*saved),1,file)==1 && fgetc(file)==EOF;
  fclose(file);
  if(ok)ok=saved->magic==0x41584241u && saved->version==1 && saved->bytes==sizeof(*saved) &&
    saved->crc==crc32(&saved->hero,sizeof(*saved)-offsetof(Progress,hero)) &&
    saved->hero.checkpoint[0]>=0 && saved->hero.checkpoint[0]<25 &&
    saved->hero.checkpoint[1]>=0 && saved->hero.checkpoint[1]<256 &&
    saved->hero.checkpoint[2]>=0 && saved->hero.checkpoint[2]<192 &&
    saved->hero.state[0]>0 && saved->hero.state[0]<=99;
  if(ok) {
    *hero=saved->hero;memcpy(stage,saved->stage,sizeof(saved->stage));
    *room=hero->checkpoint[0];hero->x=hero->checkpoint[1];hero->y=hero->checkpoint[2];
    hero->jump=0;hero->height=0;hero->death=0;hero->temp=1;memset(hero->push,0,sizeof(hero->push));
    fingerprint(hero,signature);have_signature=1;port_log("PXA Abbaye checkpoint restored room=%u lives=%d",*room,hero->state[0]);
  } else port_log("PXA Abbaye checkpoint rejected; starting a new game");
  free(saved);return ok;
}
void pxa_progress_update(const struct hero *hero,const unsigned stage[25][22][32]) {
  if(hero->state[0]<=0){pxa_progress_clear();return;}
  if(hero->death || hero->flags[6])return;
  int next[13];fingerprint(hero,next);
  if(have_signature && !memcmp(next,signature,sizeof(next)))return;
  if(retry_after && port_time_us()<retry_after)return;
  Progress *saved=calloc(1,sizeof(*saved));if(!saved)return;
  saved->magic=0x41584241u;saved->version=1;saved->bytes=sizeof(*saved);saved->hero=*hero;
  memcpy(saved->stage,stage,sizeof(saved->stage));
  saved->crc=crc32(&saved->hero,sizeof(*saved)-offsetof(Progress,hero));
  FILE *file=fopen(TEMP_PATH,"wb");int ok=0;
  if(file){ok=fwrite(saved,sizeof(*saved),1,file)==1;if(fclose(file))ok=0;}
  if(ok && !port_replace(TEMP_PATH,SAVE_PATH)) {
    memcpy(signature,next,sizeof(next));have_signature=1;
    port_log("PXA Abbaye checkpoint saved room=%d lives=%d",hero->checkpoint[0],hero->state[0]);
    retry_after=0;
  } else {unlink(TEMP_PATH);retry_after=port_time_us()+30000000;port_log("PXA Abbaye checkpoint write failed");}
  free(saved);
}
