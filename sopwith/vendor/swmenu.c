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
// Menu system for main menu and options menus
//

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "pcsound.h"
#include "timer.h"
#include "video.h"

#include "sw.h"
#include "swconf.h"
#include "swend.h"
#include "swmain.h"
#include "swmenu.h"
#include "swsound.h"
#include "swtext.h"
#include "swtitle.h"

#define CONFIG_SETTING_COLUMN 28

static const char menukeys[] = "1234567890ABCDEFGHIJKL";

static void ChangeKeyBinding(const struct menuitem *item)
{
	const struct conf_option *opt;
	const char *config_name = item->user_data;
	int key;

	Vid_ClearBuf();

	swcolor(3);
	swposcur(10, 5);
	swputs("Press the new key for: ");

	swcolor(2);
	swposcur(14, 7);
	swputs(item->label);

	swcolor(1);
	swposcur(1, 22);
	swputs("   ESC - Cancel");

	Vid_Update();

	while ((key = Vid_GetKey()) == 0) {
		Timer_Sleep(50);

		swsndupdate();
		if (ctlbreak()) {
			return;
		}
	}

	if (!strcasecmp(Vid_KeyName(key), "Escape")) {
		return;
	}

	opt = ConfOptionByName(config_name);
	if (opt == NULL) {
		return;
	}
	*opt->value.i = key;
}

enum menu_action ToggleConfigOption(const struct menuitem *item)
{
	const char *config_name = item->user_data;
	const struct conf_option *opt;

	opt = ConfOptionByName(config_name);
	if (opt == NULL) {
		return MENU_ACTION_NONE;
	}
	switch (opt->type) {
	case CONF_BOOL:
		*opt->value.b = !*opt->value.b;
		break;
	case CONF_INT:
		// TODO: This should not be a special case.
		if (!strcasecmp(opt->name, "conf_video_palette")) {
			*opt->value.i =
			    (*opt->value.i + 1) % Vid_GetNumVideoPalettes();
			Vid_SetVideoPalette(*opt->value.i);
		}
		break;
	case CONF_KEY:
		ChangeKeyBinding(item);
		break;
	default:
		break;
	}

	// reset the screen if we need to
	if (opt->value.b == &vid_fullscreen) {
		Vid_Reset();
	}

	swsaveconf();

	return MENU_ACTION_NONE;
}

void FullscreenBackground(void *_title)
{
	const char *title = _title;
	int title_len = strlen(title), x = 19 - title_len / 2;

	// Planes on the menus look decorative but serve a real
	// purpose: they give you an impression of what the game will
	// look like under the different palettes.
	Vid_DispSymbol(x * 8 - 32, SCR_HGHT - 10, &symbol_plane[0].sym[0],
	               FACTION_PLAYER1);
	Vid_DispSymbol((x + title_len) * 8 + 16, SCR_HGHT - 10,
	               &symbol_plane[0].sym[6], FACTION_PLAYER2);

	swcolor(2);
	swposcur(x, 2);
	swputs(title);

	swcolor(1);

	swposcur(1, 22);
	swputs("   ESC - Exit Menu");

	swposcur(0, 5);
}

static void DrawConfigOption(const struct menuitem *item)
{
	const struct conf_option *opt;
	int x, y;

	GetCursorPosition(&x, &y);
	if (x >= CONFIG_SETTING_COLUMN) {
		++y;
	}
	swposcur(CONFIG_SETTING_COLUMN, y);

	opt = ConfOptionByName(item->user_data);
	if (opt == NULL) {
		return;
	}
	switch (opt->type) {
	case CONF_BOOL:
		swputs(*opt->value.b ? "on" : "off");
		break;
	case CONF_INT:
		if (!strcasecmp(opt->name, "conf_video_palette")) {
			swputs(Vid_GetVideoPaletteName(*opt->value.i));
		}
		break;
	case CONF_KEY:
		swputs(Vid_KeyName(*opt->value.i));
		break;
	default:
		break;
	}
}

static void DrawMenu(const struct menu *menu, int selected)
{
	const struct menuitem *items = menu->items;
	int i, y, keynum;
	char buttons[32];
	int num_buttons = 0;

	Vid_ClearBuf();
	swposcur(0, 0);

	if (menu->draw_background != NULL) {
		menu->draw_background(menu->draw_background_data);
	}

	swcolor(3);

	for (i = 0, keynum = 0; items[i].label != NULL; ++i) {
		char *prefix, *suffix;
		char buf[40];
		int key;

		if (strlen(items[i].label) == 0) {
			swputs("\n");
			continue;
		}

		key = items[i].key;
		prefix = i == 0 ? "Key:" : "";
		suffix = "";

		if (key == '1') {
			key = menukeys[keynum];
			++keynum;
			suffix = ":";
		}
		// If we have a game controller connected, we show a
		// cursor and allow navigation using the D-pad.
		if (Vid_HaveController()) {
			prefix = i == selected ? "   \x1a" : "";
			swcolor(i == selected ? 2 : 3);
		} else if (strstr(items[i].label, ">>>")) {
			swcolor(2);
		} else {
			swcolor(3);
		}
		snprintf(buf, sizeof(buf), "%-5s%c - %s%s", prefix, key,
		         items[i].label, suffix);

		GetCursorPosition(NULL, &y);
		swposcur(0, y);
		swputs(buf);

		if (num_buttons < sizeof(buttons) - 1) {
			buttons[num_buttons] = key;
			++num_buttons;
		}
		if (strlen(items[i].label) == 0) {
			continue;
		}
		if (items[i].callback == ToggleConfigOption) {
			DrawConfigOption(&items[i]);
		}
		swputs("\n");
	}

	buttons[num_buttons] = '\0';
	Vid_ShowTouchKeys(buttons);

	Vid_Update();
}

static const struct menuitem *MenuItemForKey(const struct menu *menu, int key)
{
	const struct menuitem *items = menu->items;
	int i, keynum;

	if (key <= 0) {
		return NULL;
	}

	for (i = 0, keynum = 0; items[i].label != NULL; ++i) {
		int itemkey;
		if (items[i].key == 0) {
			continue;
		} else if (items[i].key == '1') {
			itemkey = menukeys[keynum];
			++keynum;
		} else {
			itemkey = items[i].key;
		}
		if (key == itemkey) {
			return &items[i];
		}
	}

	return NULL;
}

static bool MenuKeypress(const struct menu *menu, enum menu_action *result)
{
	const struct menuitem *pressed;
	enum menu_action a;
	int key;

	key = Vid_GetChar();
	if (key == 0) {
		return true;
	}
	if (key == 27) {
		*result = MENU_ACTION_NONE;
		return false;
	}

	key = toupper(key);

	// check if a number has been pressed for a menu option
	pressed = MenuItemForKey(menu, key);
	if (pressed == NULL) {
		return true;
	}
	if (pressed->callback == NULL) {
		return true;
	}

	a = pressed->callback(pressed);
	if (a == MENU_ACTION_RETURN) {
		*result = MENU_ACTION_RETURN;
		return false;
	}

	return true;
}

static int MoveCursor(const struct menu *menu, int orig_selected, int direction)
{
	const struct menuitem *items = menu->items;
	int selected = orig_selected;

	for (;;) {
		selected += direction;
		if (selected < 0 || items[selected].label == NULL) {
			break;
		}
		if (strlen(items[selected].label) > 0) {
			return selected;
		}
	}

	// Reached top or bottom of list.
	return orig_selected;
}

static bool ControllerPress(const struct menu *menu, enum menu_action *result,
                            int *selected)
{
	const struct menuitem *i;
	enum menu_action a;

	switch (Vid_ControllerMenuKey()) {
	case MENUKEY_START:
		i = &menu->items[*selected];
		a = i->callback(i);
		if (a == MENU_ACTION_RETURN) {
			*result = MENU_ACTION_RETURN;
			return false;
		}
		return true;

	case MENUKEY_BACK:
		*result = MENU_ACTION_NONE;
		return false;

	case MENUKEY_UP:
		*selected = MoveCursor(menu, *selected, -1);
		return true;

	case MENUKEY_DOWN:
		*selected = MoveCursor(menu, *selected, 1);
		return true;

	default:
		return true;
	}
}

// Present the given menu to the user. Returns zero if escape was pushed
// to exit the menu, or if a >jump item was chosen, it returns the key
// binding associated with it.
enum menu_action RunMenu(const struct menu *menu)
{
	enum menu_action result;
	int selected = 0;

	// Clear out any controller presses from before the menu opened.
	Vid_ControllerMenuKey();

	for (;;) {
		DrawMenu(menu, selected);

		if (ctlbreak()) {
			swend(NULL, false);
		}

		if (!MenuKeypress(menu, &result) ||
		    !ControllerPress(menu, &result, &selected)) {
			return result;
		}

		// Wait for a keypress:
		Timer_Sleep(50);
	}
}

enum menu_action SubMenu(const struct menuitem *item)
{
	return RunMenu(item->user_data);
}
