//
// Copyright(C) 2001-2005 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//
//
// System-independent video code
//

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "video.h"

// this should be in a header somewhere
#define SBAR_HGHT 19
#define VRAMSIZE  (SCR_HGHT * vid_pitch)

static const uint8_t color_mappings[][4] = {
    {0, 3, 3, 3}, // All-white                     - FACTION_NONE?
    {0, 1, 2, 3}, // Cyan fuselage, magenta wings  - FACTION_PLAYER1
    {0, 2, 1, 3}, // Magenta fuselage, cyan wings  - FACTION_PLAYER2
    // New colors:
    {0, 1, 3, 2}, // Cyan fuselage, white wings    - FACTION_PLAYER3
    {0, 2, 3, 1}, // Magenta fuselage, white wings - FACTION_PLAYER4
    {0, 3, 1, 2}, // White fuselage, cyan wings    - FACTION_PLAYER5
    {0, 3, 2, 1}, // White fuselage, magenta wings - FACTION_PLAYER6
    // Now we're getting into boring territory...
    {0, 1, 1, 3}, // All-cyan                      - FACTION_PLAYER7
    {0, 2, 2, 3}, // All-magenta                   - FACTION_PLAYER8
};

uint8_t *vid_vram;
unsigned int vid_pitch;

int keysdown[NUM_KEYS];

int Vid_GetGameKeys(void)
{
	int i, c = 0;

	// empty input buffer, get new key state
	while (Vid_GetKey())
		;

	if (Vid_GetCtrlBreak()) {
		c |= K_BREAK;
	}
	if (keysdown[KEY_FLIP]) {
		keysdown[KEY_FLIP] = 0;
		c |= K_FLIP;
	}
	if (keysdown[KEY_PULLUP]) {
		c |= K_FLAPU;
	}
	if (keysdown[KEY_PULLDOWN]) {
		c |= K_FLAPD;
	}
	if (keysdown[KEY_ACCEL]) {
		// smooth acceleration -- Jesse
		// keysdown[KEY_ACCEL] = 0;
		c |= K_ACCEL;
	}
	if (keysdown[KEY_DECEL]) {
		// smooth deacceleration -- Jesse
		// keysdown[KEY_DECEL] = 0;
		c |= K_DEACC;
	}
	if (keysdown[KEY_SOUND]) {
		keysdown[KEY_SOUND] = 0;
		c |= K_SOUND;
	}
	if (keysdown[KEY_BOMB]) {
		c |= K_BOMB;
	}
	if (keysdown[KEY_FIRE]) {
		c |= K_SHOT;
	}
	if (keysdown[KEY_HOME]) {
		c |= K_HOME;
	}
	if (keysdown[KEY_MISSILE]) {
		keysdown[KEY_MISSILE] = 0;
		c |= K_MISSILE;
	}
	if (keysdown[KEY_STARBURST]) {
		keysdown[KEY_STARBURST] = 0;
		c |= K_STARBURST;
	}

	// clear bits in key array
	for (i = 0; i < NUM_KEYS; ++i) {
		keysdown[i] &= ~KEYDOWN_WAS_PRESSED;
	}

	return c;
}

// Draw w columns of ground, starting at the given ground pointer.
void Vid_DispGround(GRNDTYPE *gptr, int xstart, int w)
{
	GRNDTYPE *g = gptr;
	uint8_t *sptr;
	int x, y;
	int hr, hc, hl;

	sptr = vid_vram + xstart;
	y = SCR_HGHT - 1;

	hc = *g;
	hc = clamp_max(*g, SCR_HGHT - 1);
	++g;
	hl = hc;

	// We keep a rolling window of three consecutive ground heights.
	// Although 'x' is the current column we're drawing, 'g' always points
	// one column ahead; we look one pixel both ahead (hr) and behind (hl).
	// We then draw a vertical line from lowest of the three to the current
	// height. This approach always draws the correct ground height for
	// that column. It also correctly handles a number of corner cases:
	// left-facing cliff, right-facing cliff, and pixel-wide bump/pillar.
	for (x = 0; x < w - 1; ++x) {
		hr = clamp_max(*g, SCR_HGHT - 1);
		++g;
		if (y > hl) {
			sptr += vid_pitch * (y - hl);
			y = hl;
		}
		if (y > hr) {
			sptr += vid_pitch * (y - hr);
			y = hr;
		}
		if (y >= hc) {
			sptr += vid_pitch * (y - hc + 1);
			y = hc - 1;
		}
		while (y < hc) {
			y++;
			sptr -= vid_pitch;
			*sptr ^= 0x3;
		}
		hl = hc;
		hc = hr;
		++sptr;
	}

	// The above loop always looks one entry ahead, but we never want to
	// read beyond the end of the ground array. So the above loop doesn't
	// draw the furthest-right pixel; we do that here.
	sptr += (y - hc) * (int) vid_pitch;
	*sptr ^= 0x3;
}

// sdh 28/10/2001: solid ground function
void Vid_DispGround_Solid(GRNDTYPE *gptr, int xstart, int w)
{
	GRNDTYPE *g = gptr;
	uint8_t *sptr;
	int x, y;
	int gc;

	for (x = xstart, g = gptr; x < xstart + w; ++x) {
		gc = clamp_max(*g, SCR_HGHT - 1);
		++g;

		sptr = vid_vram + (SCR_HGHT - SBAR_HGHT - 1) * vid_pitch + x;

		for (y = gc - SBAR_HGHT + 1; y; --y) {
			*sptr ^= 3;
			sptr -= vid_pitch;
		}
	}
}

void Vid_PlotPixel(int x, int y, int clr)
{
	uint8_t *sptr = vid_vram + (SCR_HGHT - 1 - y) * vid_pitch + x;

	*sptr = clr & 3;
}

void Vid_XorPixel(int x, int y, int clr)
{
	uint8_t *sptr = vid_vram + (SCR_HGHT - 1 - y) * vid_pitch + x;

	*sptr ^= clr & 3;
}

// Returns the color of the fuselage for planes of a given faction.
// This is used for the colors shown for objects on the map.
// TODO: When the fuselage is white, we should probably return the wing
// color instead so that the player can be distinguished from the ground.
int Vid_FuselageColor(faction_t f)
{
	assert(f < arrlen(color_mappings));
	return color_mappings[f][1];
}

void Vid_DispSymbol(int x, int y, sopsym_t *symbol, faction_t clr)
{
	int left_skip = x < 0 ? -x : 0;
	uint8_t *dst =
	    vid_vram + (SCR_HGHT - 1 - y) * vid_pitch + x + left_skip;
	const uint8_t *src = symbol->data;
	int x1, y1;
	int w = symbol->w, h = symbol->h;
	const uint8_t *color_mapping;

	if (w == 1 && h == 1) {
		Vid_XorPixel(x, y, clr);
		return;
	}

	// Clip to right-side edge:
	if (x + w > SCR_WDTH) {
		w -= x + w - SCR_WDTH;
	}

	if (h > y + 1) {
		h = y + 1;
	}

	assert(clr < arrlen(color_mappings));
	color_mapping = color_mappings[clr];
	for (y1 = 0; y1 < h; ++y1) {
		uint8_t *dst2 = dst;
		const uint8_t *src2 = src + left_skip;

		for (x1 = left_skip; x1 < w; ++x1, ++dst2) {
			int i = *src2++;

			if (i) {
				*dst2 ^= color_mapping[i];
			}
		}
		src += symbol->w;
		dst += vid_pitch;
	}
}

// sdh 27/6/2002: box function for drawing filled boxes
void Vid_Box(int x, int y, int w, int h, int c)
{
	uint8_t *p = vid_vram + (SCR_HGHT - 1 - y) * vid_pitch + x;

	for (; h >= 0; --h, p += vid_pitch) {
		memset(p, c, w);
	}
}

// clear screen
void Vid_ClearBuf(void)
{
	memset(vid_vram, 0, VRAMSIZE);
}

//
// 2003-02-14: Code was checked into version control; no further entries
// will be added to this log.
//
// sdh 14/2/2003: change license header to GPL
// sdh 27/7/2002: remove collision detection code
// sdh 27/6/2002: move to new sopsym_t for symbols
// sdh 25/04/2002: rename vga_{pitch,vram} to vid_{pitch,vram}
// sdh 26/03/2002: split off platform specific drawing functions here
//                 replaced amiga drawing functions with these generic
//                 8 bit ones
// sdh 28/10/2001: get_type/set_type removed
// sdh 28/10/2001: moved auxdisp and auxdisp functions here
// sdh 24/10/2001: fix auxdisp buffer
// sdh 21/10/2001: use new obtype_t and obstate_t
// sdh 21/10/2001: rearranged headers, added cvs tags
// sdh 21/10/2001: added #define for solid ground (sopwith 1 style)
// sdh 21/10/2001: reformatted with indent, adjusted some code by hand
//                 to make more readable
// sdh 19/10/2001: removed extern definitions, these are in headers now
//                 shuffled some functions round to shut up the compiler
// sdh 18/10/2001: converted all functions to ANSI-style arguments
//
// 87-03-09        Microsoft compiler.
// 85-11-05        Atari
// 84-06-13        PCjr Speed-up
// 84-02-21        Development
//
