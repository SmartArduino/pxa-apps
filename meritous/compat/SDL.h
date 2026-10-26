/* Minimal SDL 1.2 surface and event API used by the Meritous PXA port. */
#ifndef MERITOUS_COMPAT_SDL_H
#define MERITOUS_COMPAT_SDL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;

typedef enum SDLKey {
    SDLK_UNKNOWN = 0,
    SDLK_TAB = 9,
    SDLK_RETURN = 13,
    SDLK_ESCAPE = 27,
    SDLK_SPACE = 32,
    SDLK_a = 'a',
    SDLK_d = 'd',
    SDLK_h = 'h',
    SDLK_j = 'j',
    SDLK_k = 'k',
    SDLK_m = 'm',
    SDLK_n = 'n',
    SDLK_p = 'p',
    SDLK_s = 's',
    SDLK_w = 'w',
    SDLK_UP = 273,
    SDLK_DOWN = 274,
    SDLK_RIGHT = 275,
    SDLK_LEFT = 276
} SDLKey;

typedef enum SDLMod {
    KMOD_NONE = 0
} SDLMod;

typedef struct SDL_keysym {
    Uint8 scancode;
    SDLKey sym;
    SDLMod mod;
    Uint16 unicode;
} SDL_keysym;

typedef struct SDL_KeyboardEvent {
    Uint8 type;
    Uint8 which;
    Uint8 state;
    SDL_keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_QuitEvent {
    Uint8 type;
} SDL_QuitEvent;

typedef union SDL_Event {
    Uint8 type;
    SDL_KeyboardEvent key;
    SDL_QuitEvent quit;
} SDL_Event;

#define SDL_KEYDOWN 2
#define SDL_KEYUP 3
#define SDL_QUIT 12
#define SDL_RELEASED 0
#define SDL_PRESSED 1

typedef struct SDL_Rect {
    Sint16 x, y;
    Uint16 w, h;
} SDL_Rect;

typedef struct SDL_Color {
    Uint8 r, g, b, unused;
} SDL_Color;

typedef struct SDL_Palette {
    int ncolors;
    SDL_Color *colors;
} SDL_Palette;

typedef struct SDL_PixelFormat {
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint8 Rloss, Gloss, Bloss, Aloss;
    Uint8 Rshift, Gshift, Bshift, Ashift;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint32 colorkey;
    Uint8 alpha;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    Uint16 pitch;
    void *pixels;
    SDL_Rect clip_rect;
} SDL_Surface;

#define SDL_SWSURFACE 0x00000000u
#define SDL_SRCCOLORKEY 0x00001000u
#define SDL_RLEACCEL 0x00004000u
#define SDL_FULLSCREEN 0x80000000u
#define SDL_LOGPAL 0x01
#define SDL_PHYSPAL 0x02

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                  int depth, Uint32 rmask, Uint32 gmask,
                                  Uint32 bmask, Uint32 amask);
SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags);
void SDL_FreeSurface(SDL_Surface *surface);
int SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect,
                    SDL_Surface *dst, SDL_Rect *dstrect);
int SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
int SDL_SetColorKey(SDL_Surface *surface, Uint32 flags, Uint32 key);
int SDL_SetPalette(SDL_Surface *surface, int flags, SDL_Color *colors,
                   int firstcolor, int ncolors);
void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y,
                    Uint32 w, Uint32 h);
void SDL_WM_SetCaption(const char *title, const char *icon);
void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask);
int SDL_SaveBMP(SDL_Surface *surface, const char *filename);
void SDL_Quit(void);

/* The event/timing bridge is implemented by the PXA guest entry point. */
void SDL_Delay(Uint32 ms);
Uint32 SDL_GetTicks(void);
int SDL_PollEvent(SDL_Event *event);

#ifdef __cplusplus
}
#endif
#endif
