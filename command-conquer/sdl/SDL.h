/* Minimal SDL.h stand-in for the ESP32-S3 port: the game is built as an "SDL2"
 * build for its key codes (USB HID usages), but SDL itself is not used. */
#ifndef SDL_h_
#define SDL_h_
#include "SDL_scancode.h"
#endif
