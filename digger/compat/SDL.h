/* SPDX-License-Identifier: MIT. Only the clock calls used by original sdl_timer.c. */
#ifndef DIGGER_PXA_SDL_CLOCK_H
#define DIGGER_PXA_SDL_CLOCK_H
#include <stdint.h>
uint32_t SDL_GetTicks(void);
void SDL_Delay(uint32_t milliseconds);
#endif

