/* SPDX-License-Identifier: GPL-2.0-or-later
 * Only the SDL 1.2 types/key values consumed by the original game. */
#ifndef PXA_BLOCKOUT_SDL_H
#define PXA_BLOCKOUT_SDL_H
#include <stdio.h>
#include <stdint.h>
typedef union { int type; } SDL_Event;
enum { SDLK_BACKSPACE=8, SDLK_RETURN=13, SDLK_ESCAPE=27, SDLK_SPACE=32,
 SDLK_UP=273, SDLK_DOWN=274, SDLK_RIGHT=275, SDLK_LEFT=276,
 SDLK_INSERT=277, SDLK_HOME=278, SDLK_END=279, SDLK_PAGEUP=280,
 SDLK_PAGEDOWN=281, SDLK_F1=282, SDLK_LAST=323 };
#endif
