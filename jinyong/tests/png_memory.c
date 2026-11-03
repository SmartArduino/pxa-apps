/* SPDX-License-Identifier: MIT */
#include "jy_sdl.h"
#include "SDL/SDL_image.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void *__real_malloc(size_t);
extern void __real_free(void *);
typedef union {max_align_t align;size_t bytes;} header;
static size_t live,peak,limit=2u*1024u*1024u;
/* The PNG oracle reads independent host files; private FS is tested separately. */
FILE *jy_fs_fopen(const char *path,const char *mode){return fopen(path,mode);}
int jy_fs_fclose(FILE *file){return fclose(file);}
void *__wrap_malloc(size_t n){
 if(n>limit-live||n>SIZE_MAX-sizeof(header))return NULL;
 header *p=__real_malloc(sizeof(header)+n);if(!p)return NULL;p->bytes=n;
 live+=n;if(live>peak)peak=live;return p+1;
}
void __wrap_free(void *p){if(p){header *h=(header*)p-1;live-=h->bytes;__real_free(h);}}
void *__wrap_calloc(size_t n,size_t size){if(size&&n>SIZE_MAX/size)return NULL;size_t bytes=n*size;void *p=__wrap_malloc(bytes);if(p)memset(p,0,bytes);return p;}
void *__wrap_realloc(void *old,size_t bytes){if(!old)return __wrap_malloc(bytes);if(!bytes){__wrap_free(old);return NULL;}void *p=__wrap_malloc(bytes);if(!p)return NULL;header *h=(header*)old-1;memcpy(p,old,h->bytes<bytes?h->bytes:bytes);__wrap_free(old);return p;}
int main(int argc,char **argv){
 assert(argc==5);SDL_Surface *s=IMG_Load(argv[1]);assert(s&&s->w==640&&s->h==480&&s->format->BytesPerPixel==3);
 for(int y=0;y<480;y+=7)for(int x=0;x<640;x+=11){Uint8 r,g,b,a;SDL_GetRGBA(jy_sdl_pixel(s,x,y),s->format,&r,&g,&b,&a);assert(r==(x%8)*30&&g==(y%8)*30&&b==((x/80+y/80)%8)*30&&a==255);}
 SDL_FreeSurface(s);assert(!live);
 s=IMG_Load(argv[2]);assert(s&&s->w==2&&s->h==2&&s->format->BytesPerPixel==4);
 for(int y=0;y<2;y++)for(int x=0;x<2;x++){Uint8 r,g,b,a;SDL_GetRGBA(jy_sdl_pixel(s,x,y),s->format,&r,&g,&b,&a);assert(r==20+x&&g==40+y&&b==60&&a==(x+y*2)*85);}
 SDL_FreeSurface(s);assert(!live);
 limit=1024u*1024u;peak=0;
 for(int i=3;i<=4;i++){
  s=IMG_Load(argv[i]);assert(s);assert(s->w==(i==3?640:2)&&s->h==(i==3?480:2));
  for(int y=0;y<s->h;y+=i==3?7:1)for(int x=0;x<s->w;x+=i==3?11:1){Uint8 r,g,b,a;SDL_GetRGBA(jy_sdl_pixel(s,x,y),s->format,&r,&g,&b,&a);if(i==3)assert(r==(x%8)*30&&g==(y%8)*30&&b==((x/80+y/80)%8)*30&&a==255);else assert(r==20+x&&g==40+y&&b==60&&a==(x+y*2)*85);}
  SDL_FreeSurface(s);assert(!live);
 }
 unsigned char invalid[16]={'J','Y','P','I',255,255,255,127,1,0,0,0,3,0,0,0};
 assert(!IMG_Load_RW(SDL_RWFromConstMem(invalid,sizeof invalid),1));assert(!live);
 unsigned char truncated[17]={'J','Y','P','I',2,0,0,0,2,0,0,0,3,0,0,0,1};
 assert(!IMG_Load_RW(SDL_RWFromConstMem(truncated,sizeof truncated),1));assert(!live);
 printf("PASS RGB/RGBA pixels, alpha, owned buffers and 640x480 predecoded pixels below 1MiB: peak=%zu\n",peak);return 0;
}
