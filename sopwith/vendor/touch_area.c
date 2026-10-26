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
// Functions for handling the "touch area" that contains controls for
// playing on a touch screen.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sw.h"
#include "swinit.h"
#include "swmain.h"
#include "swtext.h"
#include "video.h"

#define MARGIN              4
#define BUTTON_WIDTH        87
#define BUTTON_HEIGHT       36
#define SMALL_BUTTON_WIDTH  39 /* single character button */
#define SMALL_BUTTON_HEIGHT 30

static bool button_pressed[16];
static const struct touch_button game_buttons[] = {
    {TOUCH_BUTTON_GAME_KEY, " \x18", 4, 3, KEY_PULLDOWN},
    {TOUCH_BUTTON_LABEL, "Stick", 3, 7},
    {TOUCH_BUTTON_GAME_KEY, " \x19", 4, 11, KEY_PULLUP},
    {TOUCH_BUTTON_GAME_KEY, "Flip", 4, 17, KEY_FLIP},

    {TOUCH_BUTTON_GAME_KEY, " \x18", 18, 3, KEY_ACCEL},
    {TOUCH_BUTTON_LABEL, "Throttle", 16, 7},
    {TOUCH_BUTTON_GAME_KEY, " \x19", 18, 11, KEY_DECEL},
    {TOUCH_BUTTON_GAME_KEY, "Home", 18, 17, KEY_HOME},

    /*
    {TOUCH_BUTTON_GAME_KEY, "Missile",  20,  7, KEY_MISSILE},
    {TOUCH_BUTTON_GAME_KEY, "Flare",    20, 10, KEY_STARBURST},
    */

    {TOUCH_BUTTON_CLOSE, "Close", 32, 3},
    {TOUCH_BUTTON_GAME_KEY, "Fire", 32, 11, KEY_FIRE},
    {TOUCH_BUTTON_GAME_KEY, "Bomb", 32, 17, KEY_BOMB},

    {TOUCH_BUTTON_END},
};

static const struct touch_button *curr_buttons = game_buttons;

static void GetButtonPos(const struct touch_button *b, int *x, int *y, int *w,
                         int *h)
{
	if (strlen(b->label) == 1) {
		*x = b->x * 8 - 15;
		*y = b->y * 8 - 11;
		*w = SMALL_BUTTON_WIDTH;
		*h = SMALL_BUTTON_HEIGHT;
	} else {
		*x = b->x * 8 - 28;
		*y = b->y * 8 - 14;
		*w = BUTTON_WIDTH;
		*h = BUTTON_HEIGHT;
	}
}

static void DrawTouchArea(const struct touch_button *buttons)
{
	int i, cx;

	Vid_Box(0, SCR_HGHT - 3 - MARGIN, SCR_WDTH,
	        TOUCH_AREA_HEIGHT - MARGIN - 3, 0);

	for (i = 0; buttons[i].type != TOUCH_BUTTON_END; i++) {
		const struct touch_button *b = &buttons[i];

		cx = b->x;

		if (b->type == TOUCH_BUTTON_LABEL) {
			swcolor(3);
		} else {
			int x, y, w, h;
			bool pressed =
			    i < arrlen(button_pressed) && button_pressed[i];
			GetButtonPos(b, &x, &y, &w, &h);
			Vid_Box(x, SCR_HGHT - y, w, h, 3);
			Vid_Box(x + 1, SCR_HGHT - y - 1, w - 2, h - 2,
			        pressed ? 2 : 1);
			swcolor(0);
		}

		swposcur(cx, b->y);
		swputs(b->label);
	}
}

const struct touch_button *Vid_GetTouchButton(int x, int y)
{
	int i, bx, by, bw, bh;

	for (i = 0; curr_buttons[i].type != TOUCH_BUTTON_END; i++) {
		const struct touch_button *b = &curr_buttons[i];
		GetButtonPos(b, &bx, &by, &bw, &bh);

		if (b->type != TOUCH_BUTTON_LABEL && x >= bx && y >= by &&
		    x < bx + bw && y < by + bh) {
			return b;
		}
	}

	return NULL;
}

void Vid_TouchButtonPress(const struct touch_button *b, bool pressed)
{
	int i;

	for (i = 0; curr_buttons[i].type != TOUCH_BUTTON_END; i++) {
		if (b == &curr_buttons[i]) {
			button_pressed[i] = pressed;
			return;
		}
	}
}

void Vid_DrawTouchControls(void)
{
	DrawTouchArea(curr_buttons);
}

void Vid_ShowTouchGameControls(void)
{
	memset(button_pressed, 0, sizeof(button_pressed));
	curr_buttons = game_buttons;
}

void Vid_ShowTouchKeys(const char *keys)
{
	static struct touch_button buttons[27], *b;
	static char button_labels[27][2];
	int i, x = 2, y = 8, key;

	memset(button_pressed, 0, sizeof(button_pressed));

	buttons[0].type = TOUCH_BUTTON_KEYPRESS;
	buttons[0].label = "ESC";
	buttons[0].x = 4;
	buttons[0].y = 3;
	buttons[0].param = 27;

	buttons[1].type = TOUCH_BUTTON_CLOSE;
	buttons[1].label = "Close";
	buttons[1].x = 32;
	buttons[1].y = 3;

	for (i = 0; i < arrlen(buttons) - 3 && keys[i] != '\0'; i++) {
		b = &buttons[i + 2];
		key = toupper(keys[i]);
		b->type = TOUCH_BUTTON_KEYPRESS;
		b->label = button_labels[i];
		b->x = x;
		b->y = y;
		b->param = key;
		x += 5;
		if (x >= 38) {
			x = 2;
			y += 4;
		}

		snprintf(button_labels[i], sizeof(button_labels[i]), "%c", key);
	}

	buttons[i + 2].type = TOUCH_BUTTON_END;

	curr_buttons = buttons;
}
