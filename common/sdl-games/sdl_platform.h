/* Shared presentation path for SDL software windows and software GL. */
#pragma once
#include <stdint.h>
void port_sdl_present_rgb565(const uint16_t *pixels,int width,int height,int pitch);
void port_sdl_pace(void);
