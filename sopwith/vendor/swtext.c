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
//        swtext - text processing.. input/output
//

#include <ctype.h>
#include <string.h>

#include "font.h"
#include "timer.h"
#include "video.h"

#include "swsound.h"
#include "swtext.h"
#include "swtitle.h"

#define BLINK_PERIOD 400 /* ms */

// sdh: emulate text display

static int cur_x = 0, cur_y = 0; // place we are writing text
static int cur_color;            // text color

// Unicode code points for the characters in font_data[]:
// https://en.wikipedia.org/wiki/Code_page_437
static const unsigned short cp437[] = {
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, // 0x
    0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, // 1x
    0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, // 2x
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, // 3x
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, // 4x
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, // 5x
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, // 6x
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, // 7x
    0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,

    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, // 8x
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, // 9x
    0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, // Ax
    0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, // Bx
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, // Cx
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, // Dx
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, // Ex
    0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, // Fx
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
};

// Change the variable to skip to the start of the next UTF-8 char:
static bool UTF8_NextChar(const char **p)
{
	if (**p == '\0') {
		return false;
	}
	do {
		++*p;
	} while ((**p & 0xc0) == 0x80);
	return **p != '\0';
}

// Equivalent of strlen() but returns count of Unicode characters:
size_t UTF8_Strlen(const char *p)
{
	size_t result = 0;

	while (*p != '\0') {
		++result;
		UTF8_NextChar(&p);
	}

	return result;
}

// Decode the UTF-8 char at the given location; returns 0 for failure:
static unsigned int UTF8_DecodeChar(const char *p)
{
	unsigned int nbytes, i, result;

	if ((*p & 0x80) == 0) {
		return *p;
	}

	if ((*p & 0xe0) == 0xc0) {
		result = *p & 0x1f;
		nbytes = 1;
	} else if ((*p & 0xf0) == 0xe0) {
		result = *p & 0x0f;
		nbytes = 2;
	} else if ((*p & 0xf8) == 0xf0) {
		result = *p & 0x07;
		nbytes = 3;
	} else {
		return 0;
	}

	++p;

	for (i = 0; i < nbytes; ++i) {
		if ((*p & 0xc0) != 0x80) {
			return 0;
		}
		result = (result << 6) | (*p & 0x3f);
		++p;
	}

	return result;
}

// Given a Unicode character, return an Extended ASCII (CP437) character
// number. Returns 0 if the character is not in CP437.
static unsigned int UnicodeToCP437(unsigned int u)
{
	unsigned int i;

	// The happy path:
	if (u < arrlen(cp437) && cp437[u] == u) {
		return u;
	}

	for (i = 0; i < arrlen(cp437); ++i) {
		if (cp437[i] == u) {
			return i;
		}
	}

	return 0;
}

// Draw the given Unicode character at the given coordinates.
static inline void drawchar(int x, int y, int c)
{
	uint8_t *p;
	int x2, y2;

	// We can only render those Unicode characters that are found in
	// the font_data[] array:
	c = UnicodeToCP437(c);
	if (c == 0) {
		return;
	}

	p = font_data + c * 8;

	for (y2 = 0; y2 < 8; ++y2) {
		int m = 0x80;

		for (x2 = 0; x2 < 8; ++x2) {
			if (p[y2] & m) {
				Vid_PlotPixel(x + x2, SCR_HGHT - 1 - (y + y2),
				              cur_color);
			}

			m >>= 1;
		}
	}
}

void swputc(int c)
{
	if (c == '\n') {
		cur_x = 0;
		++cur_y;
	} else {
		drawchar(cur_x * 8, cur_y * 8, c);
		++cur_x;
	}
}

void swputs(const char *sp)
{
	const char *s = sp;

	while (*s != '\0') {
		swputc(UTF8_DecodeChar(s));
		UTF8_NextChar(&s);
	}
}

void swgets(char *s, int max)
{
	int or_x = cur_x, or_y = cur_y;
	int erase_len = 0;

	Vid_StartTextInput();

	for (;;) {
		unsigned char c;

		// erase background from previous write
		Vid_Box(or_x * 8, SCR_HGHT - (or_y) * 8 - 1, erase_len * 8, 8,
		        0);

		cur_x = or_x;
		cur_y = or_y;
		erase_len = strlen(s) + 1;
		swputs(s);

		if (((Timer_GetMS() / BLINK_PERIOD) % 2) == 0) {
			swputc('_');
		}

		Vid_Update();

		// read all queued keypresses
		while ((c = Vid_GetChar()) != 0) {
			if (c != 0 && isprint(c) && strlen(s) < max) {
				s[strlen(s) + 1] = '\0';
				s[strlen(s)] = c;
			} else if (c == '\b') {
				// backspace
				if (strlen(s) > 0) {
					s[strlen(s) - 1] = '\0';
				}
			} else if (c == '\n') {
				break;
			} else if (c == 27) {
				s[0] = '\0';
				c = '\n';
				break;
			}
		}
		if (c == '\n') {
			break;
		} else if (ctlbreak()) {
			// TODO
		}
		swsndupdate();
		Timer_Sleep(50);
	}

	Vid_StopTextInput();
}

void swcolor(int a)
{
	cur_color = a;
}

void swposcur(int a, int b)
{
	cur_x = a;
	cur_y = b;
}

void GetCursorPosition(int *x, int *y)
{
	if (x != NULL) {
		*x = cur_x;
	}
	if (y != NULL) {
		*y = cur_y;
	}
}

//
// 2003-02-14: Code was checked into version control; no further entries
// will be added to this log.
//
// sdh 14/2/2003: change license header to GPL
// sdh 26/03/2002: change CGA_ to Vid_
// sdh 15/11/2001: dont thrash the processor while waiting for a keypress
// sdh 24/10/2001: fix auxdisp buffer code
// sdh 21/10/2001: rearranged headers, added cvs tags
// sdh 21/10/2001: reformatted with indent
//
// 87-03-09        Microsoft compiler.
// 84-07-23        Development
//
