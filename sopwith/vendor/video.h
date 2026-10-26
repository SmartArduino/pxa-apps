//
// Copyright(C) 1984-2000 David L. Clark
// Copyright(C) 2001-2005 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//
//
// CGA Video Interface
//

#ifndef __VIDEO_H__
#define __VIDEO_H__

#include <stdint.h>

#include "sw.h"

#define TOUCH_AREA_HEIGHT 160

enum gamekey {
	KEY_UNKNOWN,
	KEY_PULLUP,
	KEY_PULLDOWN,
	KEY_FLIP,
	KEY_BOMB,
	KEY_FIRE,
	KEY_HOME,
	KEY_MISSILE,
	KEY_STARBURST,
	KEY_ACCEL,
	KEY_DECEL,
	KEY_SOUND,
	NUM_KEYS,
};

enum menukey {
	MENUKEY_NONE,
	MENUKEY_START,
	MENUKEY_BACK,
	MENUKEY_UP,
	MENUKEY_DOWN,
};

// Which keys are currently down or have been pressed since the last call
// to Vid_GetGameKeys:

#define KEYDOWN_KEYBOARD    (1 << 0)
#define KEYDOWN_WAS_PRESSED (1 << 1)
#define KEYDOWN_TOUCH       (1 << 2)
#define KEYDOWN_GAMEPAD     (1 << 3)

extern int keysdown[NUM_KEYS];
extern int keybindings[NUM_KEYS];
extern int controller_bindings[NUM_KEYS];

extern bool vid_fullscreen;
extern uint8_t *vid_vram;
extern unsigned int vid_pitch;

// init/shutdown

extern void Vid_Init(void);
extern void Vid_Reset(void);

// update screen

extern void Vid_Update(void);

extern bool Vid_GetCtrlBreak(void);

// video palette

void Vid_SetVideoPalette(int palette);
const char *Vid_GetVideoPaletteName(int palette);
int Vid_GetNumVideoPalettes(void);

// keyboard functions

extern int Vid_GetKey(void);
extern int Vid_GetChar(void);
extern int Vid_GetGameKeys(void);
extern const char *Vid_KeyName(int key);

// In text input mode, characters returned from Vid_GetChar() are "fully
// baked" - with shifting applied etc. Text input mode also pops up the
// on-screen keyboard if appropriate.
void Vid_StartTextInput(void);
void Vid_StopTextInput(void);

// drawing routines

void Vid_Box(int x, int y, int w, int h, int c);

// draw ground

extern void Vid_DispGround(GRNDTYPE *gptr, int xstart, int w);
extern void Vid_DispGround_Solid(GRNDTYPE *gptr, int xstart, int w);

// draw a pixel

extern void Vid_PlotPixel(int x, int y, int clr);
extern void Vid_XorPixel(int x, int y, int clr);

// draw a symbol

extern void Vid_DispSymbol(int x, int y, sopsym_t *symbol, faction_t f);
extern int Vid_FuselageColor(faction_t f);

// clear screen

void Vid_ClearBuf(void);

char *Vid_GetPrefPath(void);

void ErrorExit(char *s, ...);

enum touch_button_type {
	TOUCH_BUTTON_END,
	TOUCH_BUTTON_LABEL,
	TOUCH_BUTTON_GAME_KEY,
	TOUCH_BUTTON_KEYPRESS,
	TOUCH_BUTTON_CLOSE,
};

struct touch_button {
	enum touch_button_type type;
	const char *label;
	int x, y;
	int param;
};

extern void Vid_DrawTouchControls(void);
extern const struct touch_button *Vid_GetTouchButton(int x, int y);
extern void Vid_TouchButtonPress(const struct touch_button *b, bool pressed);
extern void Vid_ShowTouchGameControls(void);
extern void Vid_ShowTouchKeys(const char *keys);

enum menukey Vid_ControllerMenuKey(void);
bool Vid_HaveController(void);
const char *Vid_ControllerButtonName(enum gamekey key);

#endif
