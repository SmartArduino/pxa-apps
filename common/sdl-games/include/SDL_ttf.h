/* Small SDL_ttf surface API backed by stb_truetype. GPL-3.0-or-later.
 * Font rasterisation happens while constructing cached game UI labels. */
#pragma once
#include <SDL.h>
typedef struct PxaSdlFont TTF_Font;
#define TTF_STYLE_NORMAL 0
#define TTF_STYLE_BOLD 1
#define TTF_GetError SDL_GetError
int TTF_Init(void);
int TTF_WasInit(void);
void TTF_Quit(void);
TTF_Font *TTF_OpenFont(const char *path,int points);
TTF_Font *TTF_OpenFontRW(SDL_RWops *rw,int close,int points);
void TTF_CloseFont(TTF_Font *font);
void TTF_SetFontStyle(TTF_Font *font,int style);
int TTF_FontHeight(const TTF_Font *font);
int TTF_SizeUTF8(TTF_Font *font,const char *text,int *width,int *height);
SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *font,const char *text,SDL_Color color);
