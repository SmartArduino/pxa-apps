/**
 * @file cnc_platform.h
 * @brief Interfaces between the ESP32-S3 platform layer files of the
 * Command & Conquer port (cnc_*.cpp) and the C board support code.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */
#ifndef CNC_PLATFORM_H
#define CNC_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Game resolution: DOS low resolution mode, with the height of the panel (more
// map is shown). A 320x200 screen would be stretched to the 320x240 panel.
#define GAME_WIDTH 320
#define GAME_HEIGHT 240

// ---- cnc_video.cpp ----
// Starts the display task. displayInit() must have been called.
void videoInit(void);
// Boot / error console, drawn on its own screen until the game sets its video
// mode
void videoConsolePrint(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
// Shows the console screen again (fatal errors after the game started)
void videoShowConsole(void);
// Full screen progress display (shown until videoHideConsole())
void videoProgress(const char *title, const char *detail, const char *note,
                   int percent);
// Back to the game screen (if the game has set its video mode)
void videoHideConsole(void);
// Mouse cursor position, in game coordinates
void videoSetMouse(float x, float y);
void videoMoveMouse(float dx, float dy);
void videoGetMouse(int *x, int *y);
// LCD (touch) coordinates -> game coordinates
void videoLcdToGame(int lx, int ly, int *gx, int *gy);
// Frames sent to the panel (statistics)
uint32_t videoGetFrameCount(void);
// Screen shown (game page or console) and its palette (768 bytes, 6-bit VGA
// components)
bool videoGetScreen(const uint8_t **pixels, int *w, int *h,
                    const uint8_t **palette);

// ---- cnc_input.cpp ----
void inputInit(void);
// Injected input (debug console), game coordinates. They block for the duration
// of the gesture.
void inputInjectClick(int x, int y, bool right);
void inputInjectDrag(int x0, int y0, int x1, int y1);
void inputInjectKey(int scancode);
void inputInjectScroll(int dx, int dy);

// ---- cnc_debug.cpp ----
void debugInit(void);

// ---- cnc_sound.cpp ----
// Statistics: number of audio buffer underruns
uint32_t soundGetUnderruns(void);

#ifdef __cplusplus
}
#endif
#endif
