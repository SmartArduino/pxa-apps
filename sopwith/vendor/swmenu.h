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

enum menu_action {
	MENU_ACTION_NONE,
	MENU_ACTION_RETURN,
};

struct menuitem {
	char key;
	char *label;
	enum menu_action (*callback)(const struct menuitem *item);
	const void *user_data;
};

struct menu {
	void (*draw_background)(void *);
	void *draw_background_data;
	const struct menuitem *items;
};

enum menu_action RunMenu(const struct menu *menu);
enum menu_action SubMenu(const struct menuitem *item);
void FullscreenBackground(void *title);
enum menu_action ToggleConfigOption(const struct menuitem *item);

// TODO: clang-format is currently disabled for this one comment because of a
// mismatch in output with the version found in Github Actions:
// clang-format off
#define CONFIG_OPTION(label, config_name)                                      \
	{'1', label, ToggleConfigOption, config_name}
// clang-format on
