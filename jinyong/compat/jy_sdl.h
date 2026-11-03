/* SPDX-License-Identifier: MIT */
#ifndef JY_SDL_H
#define JY_SDL_H
#include "SDL/SDL.h"
#include <stdint.h>
typedef struct {
 void *context;
 uint64_t (*now_us)(void *);
 void (*delay_us)(void *,uint64_t);
 int (*present)(void *,const SDL_Surface *,const SDL_Rect *,int);
 int (*poll_event)(void *,SDL_Event *); /* 1 event, 0 none, -1 error */
} jy_sdl_ops;
int jy_sdl_bind(const jy_sdl_ops *);
int jy_sdl_push_event(const SDL_Event *); /* Host OS event, including filter. */
int jy_sdl_copy_rgb24(const SDL_Surface *,unsigned char *,size_t);
void jy_sdl_surface_freed(SDL_Surface *);
/* Transfer malloc-compatible pixel storage to an existing borrowed surface. */
int jy_sdl_own_pixels(SDL_Surface *);
int jy_sdl_fail(const char *);
Uint32 jy_sdl_pixel(const SDL_Surface *,int,int);
void jy_sdl_put_pixel(SDL_Surface *,int,int,Uint32);
#endif
