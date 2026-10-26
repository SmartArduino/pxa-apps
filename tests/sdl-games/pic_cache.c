/* A copied Pic value must keep using one texture; renderer changes invalidate. */
#include "pic_cache_pxa.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO)==0);
  SDL_Window *w=SDL_CreateWindow("test",0,0,32,32,0);assert(w);
  SDL_Renderer *r=SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);assert(r);
  Uint32 pixels[16]={0},other[16]={0};
  SDL_Texture *a=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,4,4);assert(a);
  assert(pxa_pic_store(pixels,a));
  for(int i=0;i<1000;++i){const void *copy=pixels;assert(pxa_pic_texture(copy)==a);}
  pxa_pic_release(pixels);assert(!pxa_pic_texture(pixels));
  SDL_Texture *b=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,4,4);assert(b);
  assert(pxa_pic_store(other,b));SDL_DestroyRenderer(r);
  assert(!pxa_pic_texture(other));SDL_DestroyWindow(w);SDL_Quit();
  puts("Pic cache reuse, release and renderer invalidation passed");
}
