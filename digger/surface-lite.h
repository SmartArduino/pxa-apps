/* SPDX-License-Identifier: MIT. Independent index8 Surface subset for PXA. */
#ifndef DIGGER_SURFACE_LITE_H
#define DIGGER_SURFACE_LITE_H
#include <stdint.h>
/* Keep this subset separate from real SDL symbols used by native golden tests. */
#define SDL_CreateRGBSurface digger_surface_create
#define SDL_CreateRGBSurfaceFrom digger_surface_from
#define SDL_FreeSurface digger_surface_free
#define SDL_SetPaletteColors digger_surface_set_palette
#define SDL_BlitSurface digger_surface_blit
typedef struct { uint8_t r,g,b,a; } SDL_Color;
typedef struct { int ncolors; SDL_Color *colors; } SDL_Palette;
typedef struct { SDL_Palette *palette; } SDL_PixelFormat;
typedef struct SDL_Surface {
  int w,h,pitch;
  void *pixels;
  SDL_PixelFormat *format;
  int owns_pixels;
} SDL_Surface;
typedef struct { int x,y,w,h; } SDL_Rect;
SDL_Surface *SDL_CreateRGBSurface(unsigned flags,int w,int h,int depth,uint32_t r,uint32_t g,uint32_t b,uint32_t a);
SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels,int w,int h,int depth,int pitch,uint32_t r,uint32_t g,uint32_t b,uint32_t a);
void SDL_FreeSurface(SDL_Surface *);
int SDL_SetPaletteColors(SDL_Palette *,const SDL_Color *,int first,int count);
int SDL_BlitSurface(SDL_Surface *,const SDL_Rect *,SDL_Surface *,SDL_Rect *);
#endif

