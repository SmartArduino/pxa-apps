/* PXA PNG/JPEG/BMP image bridge, using the upstream stb decoder. GPL-3.0-or-later. */
#pragma once
#include <SDL.h>
#define IMG_INIT_JPG 1
#define IMG_INIT_PNG 2
#define IMG_GetError SDL_GetError
#define IMG_SetError SDL_SetError
int IMG_Init(int flags);
void IMG_Quit(void);
SDL_Surface *IMG_Load(const char *path);
SDL_Surface *IMG_Load_RW(SDL_RWops *rw, int close);
SDL_Surface *IMG_LoadTyped_RW(SDL_RWops *rw, int close, const char *type);
SDL_Texture *IMG_LoadTexture(SDL_Renderer *renderer, const char *path);
