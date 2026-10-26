/* Exercise descriptor lifetime, LRU eviction and playing-voice pinning. */
#include "sound_cache_pxa.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static Mix_Chunk *active;
static int decodes;
void port_log(const char *format,...) {(void)format;}
int Mix_AllocateChannels(int n) {(void)n;return 8;}
int Mix_Playing(int channel) {return (channel==0 || channel<0) && active!=NULL;}
Mix_Chunk *Mix_GetChunk(int channel) {return channel==0?active:NULL;}
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *rw,int close) {
  assert(rw);if(close)SDL_RWclose(rw);
  Mix_Chunk *c=SDL_calloc(1,sizeof(*c));assert(c);
  c->allocated=1;c->alen=7000;c->abuf=SDL_calloc(1,c->alen);c->volume=MIX_MAX_VOLUME;assert(c->abuf);
  ++decodes;return c;
}
Mix_Chunk *Mix_LoadWAV(const char *path) { return Mix_LoadWAV_RW(SDL_RWFromFile(path,"rb"),1); }
void __real_Mix_FreeChunk(Mix_Chunk *c) {
  if(!c)return;if(c==active)active=NULL;
  if(c->allocated)SDL_free(c->abuf);SDL_free(c);
}
extern void __wrap_Mix_FreeChunk(Mix_Chunk *);
int main(int argc,char **argv) {
  assert(argc==2);Mix_Chunk *chunks[4];
  for(int i=0;i<4;++i){
    char path[1024];snprintf(path,sizeof(path),"%s/sfx-%d.ogg",argv[1],i);
    FILE *f=fopen(path,"w");assert(f);fclose(f);
    chunks[i]=pxa_sound_register(path);assert(chunks[i]);assert(!chunks[i]->abuf);
  }
  assert(pxa_sound_resolve(chunks[0]));active=chunks[0];
  assert(pxa_sound_resolve(chunks[1]));assert(decodes==2);
  assert(pxa_sound_resolve(chunks[2]));assert(chunks[0]->abuf);assert(!chunks[1]->abuf);
  Mix_Chunk *saved=chunks[1];assert(pxa_sound_resolve(saved));assert(saved==chunks[1]);
  assert(chunks[0]->abuf);assert(decodes==4);assert(pxa_sound_resolve(saved));assert(decodes==4);
  active=NULL;
  for(int i=0;i<4;++i)__wrap_Mix_FreeChunk(chunks[i]);
  puts("Sound cache stable pointers, LRU, active pinning and release passed");
}
