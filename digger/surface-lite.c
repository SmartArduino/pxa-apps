/* SPDX-License-Identifier: MIT. Independent index8 Surface subset for PXA. */
#include "surface-lite.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static SDL_Surface *make_surface(void *pixels,int w,int h,int pitch,int own)
{
  SDL_Surface *s;
  if(w<=0||h<=0||pitch<w||w>8192||h>8192||(size_t)pitch*(size_t)h>33554432u)return NULL;
  s=(SDL_Surface *)calloc(1,sizeof *s);
  if(!s)return NULL;
  s->format=(SDL_PixelFormat *)calloc(1,sizeof *s->format);
  if(s->format)s->format->palette=(SDL_Palette *)calloc(1,sizeof *s->format->palette);
  if(s->format&&s->format->palette){
    s->format->palette->ncolors=256;
    s->format->palette->colors=(SDL_Color *)malloc(256*sizeof(SDL_Color));
  }
  if(!s->format||!s->format->palette||!s->format->palette->colors){SDL_FreeSurface(s);return NULL;}
  memset(s->format->palette->colors,255,256*sizeof(SDL_Color));
  s->w=w;s->h=h;s->pitch=pitch;s->owns_pixels=own;
  s->pixels=own?calloc((size_t)pitch,(size_t)h):pixels;
  if(!s->pixels){SDL_FreeSurface(s);return NULL;}
  return s;
}
SDL_Surface *SDL_CreateRGBSurface(unsigned f,int w,int h,int d,uint32_t r,uint32_t g,uint32_t b,uint32_t a)
{
  (void)f;(void)r;(void)g;(void)b;(void)a;
  if(d!=8||w<=0||w>8192)return NULL;
  return make_surface(NULL,w,h,(w+3)&~3,1);
}
SDL_Surface *SDL_CreateRGBSurfaceFrom(void *p,int w,int h,int d,int pitch,uint32_t r,uint32_t g,uint32_t b,uint32_t a)
{
  (void)r;(void)g;(void)b;(void)a;
  return d==8?make_surface(p,w,h,pitch,0):NULL;
}
void SDL_FreeSurface(SDL_Surface *s)
{
  if(!s)return;
  if(s->owns_pixels)free(s->pixels);
  if(s->format){
    if(s->format->palette){free(s->format->palette->colors);free(s->format->palette);}
    free(s->format);
  }
  free(s);
}
int SDL_SetPaletteColors(SDL_Palette *p,const SDL_Color *c,int first,int count)
{
  if(!p||!c||first<0||count<0||first>p->ncolors||count>p->ncolors-first)return -1;
  memcpy(p->colors+first,c,(size_t)count*sizeof *c);
  return 0;
}
static uint8_t nearest(const SDL_Color *pal,int n,SDL_Color color)
{
  unsigned best=UINT_MAX;int hit=0;
  for(int i=0;i<n;i++){
    int r=(int)color.r-pal[i].r,g=(int)color.g-pal[i].g,b=(int)color.b-pal[i].b;
    unsigned d=(unsigned)(r*r+g*g+b*b);
    if(d<best){best=d;hit=i;if(!d)break;}
  }
  return (uint8_t)hit;
}
int SDL_BlitSurface(SDL_Surface *src,const SDL_Rect *sr,SDL_Surface *dst,SDL_Rect *dr)
{
  int sx=sr?sr->x:0,sy=sr?sr->y:0,w=sr?sr->w:src?src->w:0,h=sr?sr->h:src?src->h:0;
  int dx=dr?dr->x:0,dy=dr?dr->y:0;
  uint8_t map[256];int same;
  if(!src||!dst||!src->pixels||!dst->pixels)return -1;
  if(sx<0){dx-=sx;w+=sx;sx=0;}if(sy<0){dy-=sy;h+=sy;sy=0;}
  if(sx+w>src->w)w=src->w-sx;if(sy+h>src->h)h=src->h-sy;
  if(dx<0){sx-=dx;w+=dx;dx=0;}if(dy<0){sy-=dy;h+=dy;dy=0;}
  if(dx+w>dst->w)w=dst->w-dx;if(dy+h>dst->h)h=dst->h-dy;
  if(w<0)w=0;if(h<0)h=0;
  if(dr){dr->x=dx;dr->y=dy;dr->w=w;dr->h=h;}
  if(!w||!h)return 0;
  same=src->format->palette==dst->format->palette||
       (src->format->palette->ncolors==dst->format->palette->ncolors&&
        !memcmp(src->format->palette->colors,dst->format->palette->colors,
          (size_t)src->format->palette->ncolors*sizeof(SDL_Color)));
  if(!same)for(int i=0;i<256;i++)map[i]=nearest(dst->format->palette->colors,dst->format->palette->ncolors,src->format->palette->colors[i]);
  int begin=0,end=h,step=1;
  if(src->pixels==dst->pixels&&dy>sy){begin=h-1;end=-1;step=-1;}
  for(int y=begin;y!=end;y+=step){
    uint8_t *s=(uint8_t *)src->pixels+(sy+y)*src->pitch+sx;
    uint8_t *d=(uint8_t *)dst->pixels+(dy+y)*dst->pitch+dx;
    if(same)memmove(d,s,(size_t)w);
    else for(int x=0;x<w;x++)d[x]=map[s[x]];
  }
  return 0;
}

