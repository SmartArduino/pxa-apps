/* SPDX-License-Identifier: MIT. Private native-only oracle for original microTD (uTD).
 * Never linked into a Guest. No Guest load, seed, or state-write interface.
 * Original utd.ino and SAF core execute for every recorded FE transition.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#define SAF_PLATFORM_NATIVE_TEST
#include "utd.ino"

static uint8_t screen[4096], covered[4096], save_store[32], input;
static uint32_t sound_calls, save_calls;
static void SAF_FE_drawPixel(uint8_t x,uint8_t y,uint8_t c) {
  assert(x<64 && y<64); screen[(unsigned)y*64+x]=c;covered[(unsigned)y*64+x]=1;
}
static void SAF_FE_playSound(uint8_t n) {assert(n<SAF_SOUNDS);sound_calls++;}
static void SAF_FE_save(uint8_t i,uint8_t d) {assert(i<32);save_store[i]=d;save_calls++;}
static uint8_t SAF_FE_load(uint8_t i) {assert(i<32);return save_store[i];}
static uint8_t SAF_FE_buttonPressed(uint8_t b) {assert(b<7);return !!(input&(1u<<b));}
static const char *SAF_FE_extension(const char *s) {(void)s;return "";}

static void put8(uint8_t **p,uint8_t v) {*(*p)++=v;}
static void put16(uint8_t **p,uint16_t v) {put8(p,(uint8_t)v);put8(p,(uint8_t)(v>>8));}
static void put32(uint8_t **p,uint32_t v) {put16(p,(uint16_t)v);put16(p,(uint16_t)(v>>16));}
static uint8_t get8(const uint8_t **p) {return *(*p)++;}
static uint16_t get16(const uint8_t **p) {uint16_t lo=get8(p);return lo|((uint16_t)get8(p)<<8);}
static uint32_t get32(const uint8_t **p) {uint32_t lo=get16(p);return lo|((uint32_t)get16(p)<<16);}

static void put_pointer(uint8_t **p,const uint8_t *ptr,unsigned role) {
  uint8_t tag=0;uint16_t offset=0;
  if(ptr) {
    uintptr_t v=(uintptr_t)ptr;
    const uint8_t *base=role==0?UTD_ramImage:UTD_ramMask;
    if(role<2 && v>=(uintptr_t)base && v<=(uintptr_t)base+10) {
      tag=(uint8_t)(role+1);offset=(uint16_t)(v-(uintptr_t)base);
    } else if(role==0 && v>=(uintptr_t)UTD_logo && v<=(uintptr_t)UTD_logo+sizeof UTD_logo) {
      tag=3;offset=(uint16_t)(v-(uintptr_t)UTD_logo);
    } else assert(!"Unknown original renderer pointer");
  }
  put8(p,tag);put16(p,offset);
}
static const uint8_t *get_pointer(const uint8_t **p,unsigned role) {
  uint8_t tag=get8(p);uint16_t offset=get16(p);
  if(tag==0){assert(offset==0);return NULL;}
  if(role<2 && tag==role+1){assert(offset<=10);return (role==0?UTD_ramImage:UTD_ramMask)+offset;}
  if(role==0 && tag==3){assert(offset<=sizeof UTD_logo);return UTD_logo+offset;}
  assert(!"Unknown original renderer pointer tag");return NULL;
}
/* Common SAF45 + UTD574 + renderer scratch18; explicit LE, no padding/ptr. */
unsigned qa_wire(uint8_t *out) {
  uint8_t *p=out;
  put32(&p,_SAF_frame);put8(&p,_SAF_currentRandom);
  memcpy(p,_SAF_buttonStates,7);p+=7;
  memcpy(p,_SAF_saveMemory,32);p+=32;put8(&p,_SAF_saveMemoryLoaded);
  put8(&p,UTD_gameState);put8(&p,UTD_mapIndex);put8(&p,UTD_sound);
  put8(&p,UTD_backColor);put8(&p,UTD_menuItem);put8(&p,UTD_cameraPos);
  put8(&p,UTD_cursorPos);put8(&p,(uint8_t)UTD_lives);
  put8(&p,UTD_updateCounter);put8(&p,UTD_creepCount);
  put16(&p,UTD_money);put16(&p,UTD_round);
  memcpy(p,UTD_map,34);p+=34;
  for(unsigned i=0;i<128;i++)put16(&p,UTD_mapTiles[i]);
  for(unsigned i=0;i<50;i++) {
    put8(&p,UTD_creeps[i].typeFreeze);put8(&p,UTD_creeps[i].healthLives);
    put8(&p,UTD_creeps[i].pathStart);put16(&p,(uint16_t)UTD_creeps[i].pathPosition);
  }
  memcpy(p,UTD_ramImage,10);p+=10;memcpy(p,UTD_ramMask,10);p+=10;
  put_pointer(&p,_SAF_drawnImage.image,0);put_pointer(&p,_SAF_drawnImage.binaryMask,1);put_pointer(&p,_SAF_drawnImage.palette,2);
  const uint8_t scratch[]={_SAF_drawnImage.mode,_SAF_drawnImage.transparentColor,_SAF_drawnImage.binaryLine,_SAF_drawnImage.binaryMaskLine,_SAF_drawnImage.binaryPosition,_SAF_drawnImage.binaryColor1,_SAF_drawnImage.binaryColor2,_SAF_drawnImage.rleCount,_SAF_drawnImage.rleLastColor};
  memcpy(p,scratch,9);p+=9;assert(p-out==637);return 637;
}
static void restore_wire(const uint8_t *in) {
  const uint8_t *p=in;
  _SAF_frame=get32(&p);_SAF_currentRandom=get8(&p);
  memcpy(_SAF_buttonStates,p,7);p+=7;memcpy(_SAF_saveMemory,p,32);p+=32;
  _SAF_saveMemoryLoaded=get8(&p);
  UTD_gameState=get8(&p);UTD_mapIndex=get8(&p);UTD_sound=get8(&p);
  UTD_backColor=get8(&p);UTD_menuItem=get8(&p);UTD_cameraPos=get8(&p);
  UTD_cursorPos=get8(&p);UTD_lives=(int8_t)get8(&p);
  UTD_updateCounter=get8(&p);UTD_creepCount=get8(&p);
  UTD_money=get16(&p);UTD_round=get16(&p);
  memcpy(UTD_map,p,34);p+=34;
  for(unsigned i=0;i<128;i++)UTD_mapTiles[i]=get16(&p);
  for(unsigned i=0;i<50;i++) {
    UTD_creeps[i].typeFreeze=get8(&p);UTD_creeps[i].healthLives=get8(&p);
    UTD_creeps[i].pathStart=get8(&p);UTD_creeps[i].pathPosition=(int16_t)get16(&p);
  }
  memcpy(UTD_ramImage,p,10);p+=10;memcpy(UTD_ramMask,p,10);p+=10;
  _SAF_drawnImage.image=get_pointer(&p,0);_SAF_drawnImage.binaryMask=get_pointer(&p,1);_SAF_drawnImage.palette=get_pointer(&p,2);
  _SAF_drawnImage.mode=get8(&p);_SAF_drawnImage.transparentColor=get8(&p);
  _SAF_drawnImage.binaryLine=get8(&p);_SAF_drawnImage.binaryMaskLine=get8(&p);
  _SAF_drawnImage.binaryPosition=get8(&p);_SAF_drawnImage.binaryColor1=get8(&p);
  _SAF_drawnImage.binaryColor2=get8(&p);_SAF_drawnImage.rleCount=get8(&p);
  _SAF_drawnImage.rleLastColor=get8(&p);assert(p-in==637);
}
unsigned qa_screen(uint8_t *out) {memcpy(out,screen,4096);return 4096;}
unsigned qa_save(uint8_t *out) {memcpy(out,save_store,32);return 32;}
unsigned qa_snapshot(uint8_t *out) {
  uint8_t *p=out;qa_wire(p);p+=637;memcpy(p,screen,4096);p+=4096;
  memcpy(p,save_store,32);p+=32;put8(&p,input);
  put32(&p,sound_calls);put32(&p,save_calls);assert(p-out==4774);return 4774;
}
/* Planner restoration is restricted to this private native instance. */
void qa_restore(const uint8_t *in) {
  const uint8_t *p=in;restore_wire(p);p+=637;memcpy(screen,p,4096);p+=4096;
  memcpy(save_store,p,32);p+=32;input=get8(&p);
  sound_calls=get32(&p);save_calls=get32(&p);assert(p-in==4774);
}
void qa_release(void) {input=0;memset(_SAF_buttonStates,0,7);}
void qa_draw(void) {
  memset(covered,0,4096);UTD_draw();
  for(unsigned i=0;i<4096;i++)assert(covered[i]);
}
void qa_boot(void) {
  /* Reset this native process to the official declaration values. */
  memset(UTD_creeps,0,sizeof UTD_creeps);memset(UTD_mapTiles,0,sizeof UTD_mapTiles);
  memset(UTD_map,0,sizeof UTD_map);memset(UTD_ramImage,0,10);memset(UTD_ramMask,0,10);
  UTD_ramImage[0]=UTD_ramImage[1]=8;UTD_ramMask[0]=UTD_ramMask[1]=8;
  UTD_gameState=UTD_mapIndex=UTD_sound=UTD_menuItem=UTD_cameraPos=UTD_cursorPos=0;
  UTD_lives=0;UTD_money=UTD_round=0;UTD_updateCounter=UTD_creepCount=0;
  UTD_backColor=SAF_COLOR_WHITE;
  _SAF_frame=0;_SAF_currentRandom=0;_SAF_saveMemoryLoaded=0;
  memset(_SAF_saveMemory,0,32);memset(save_store,0,32);memset(screen,0,4096);
  memset(&_SAF_drawnImage,0,sizeof _SAF_drawnImage);
  sound_calls=save_calls=0;qa_release();SAF_FE_init();qa_draw();
}
int qa_tick(unsigned mask) {
  assert(mask<128);input=(uint8_t)mask;memset(covered,0,4096);
  int result=SAF_FE_loop();assert(result==1);
  for(unsigned i=0;i<4096;i++)assert(covered[i]);
  assert(UTD_creepCount<=50 && UTD_cursorPos<128 && UTD_cameraPos<=8);
  return result;
}
void qa_new(void) {qa_release();SAF_FE_init();qa_draw();}
void qa_restart(void) {assert(UTD_mapIndex<6);qa_release();UTD_loadMap(UTD_mapIndex);qa_draw();}
uint32_t qa_sound_calls(void) {return sound_calls;}
uint32_t qa_save_calls(void) {return save_calls;}
void qa_rgb(unsigned colour,uint8_t *out) {
  assert(colour<256);SAF_colorToRGB((uint8_t)colour,out,out+1,out+2);
}
