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
// Configuration code
//
// Save game settings to a configuration file
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

#define CONFIG_FILE_NAME "sopwith.cfg"

static const char *GetConfigFilename(void)
{
	static char *result = NULL;
	char *pref_path;
	size_t buflen;

	if (result != NULL) {
		return result;
	}

	pref_path = Vid_GetPrefPath();
	if (pref_path == NULL) {
		return NULL;
	}

	buflen = strlen(pref_path) + strlen(CONFIG_FILE_NAME) + 1;
	result = checked_malloc(buflen);
	snprintf(result, buflen, "%s%s", pref_path, CONFIG_FILE_NAME);
	return result;
}

static const struct conf_option confoptions[] = {
    {"conf_missiles", CONF_BOOL, {&conf_missiles}},
    {"conf_solidground", CONF_BOOL, {&conf_solidground}},
    {"conf_hudsplats", CONF_BOOL, {&conf_hudsplats}},
    {"conf_wounded", CONF_BOOL, {&conf_wounded}},
    {"conf_animals", CONF_BOOL, {&conf_animals}},
    {"conf_harrykeys", CONF_BOOL, {&conf_harrykeys}},
    {"conf_big_explosions", CONF_BOOL, {&conf_big_explosions}},
    {"conf_medals", CONF_BOOL, {&conf_medals}},
    {"vid_fullscreen", CONF_BOOL, {&vid_fullscreen}},
    {"snd_tinnyfilter", CONF_BOOL, {&snd_tinnyfilter}},
    {"conf_video_palette", CONF_INT, {&conf_video_palette}},

    {"key_accelerate", CONF_KEY, {&keybindings[KEY_ACCEL]}},
    {"key_decelerate", CONF_KEY, {&keybindings[KEY_DECEL]}},
    {"key_pullup", CONF_KEY, {&keybindings[KEY_PULLUP]}},
    {"key_pulldown", CONF_KEY, {&keybindings[KEY_PULLDOWN]}},
    {"key_flip", CONF_KEY, {&keybindings[KEY_FLIP]}},
    {"key_fire", CONF_KEY, {&keybindings[KEY_FIRE]}},
    {"key_dropbomb", CONF_KEY, {&keybindings[KEY_BOMB]}},
    {"key_home", CONF_KEY, {&keybindings[KEY_HOME]}},
    {"key_missile", CONF_KEY, {&keybindings[KEY_MISSILE]}},
    {"key_starburst", CONF_KEY, {&keybindings[KEY_STARBURST]}},

    {"controller_accelerate", CONF_INT, {&controller_bindings[KEY_ACCEL]}},
    {"controller_decelerate", CONF_INT, {&controller_bindings[KEY_DECEL]}},
    {"controller_pullup", CONF_INT, {&controller_bindings[KEY_PULLUP]}},
    {"controller_pulldown", CONF_INT, {&controller_bindings[KEY_PULLDOWN]}},
    {"controller_flip", CONF_INT, {&controller_bindings[KEY_FLIP]}},
    {"controller_fire", CONF_INT, {&controller_bindings[KEY_FIRE]}},
    {"controller_dropbomb", CONF_INT, {&controller_bindings[KEY_BOMB]}},
    {"controller_home", CONF_INT, {&controller_bindings[KEY_HOME]}},
};

static void Chomp(char *s)
{
	char *p;
	for (p = s + strlen(s) - 1; isspace(*p) && p > s; --p) {
		*p = '\0';
	}
}

const struct conf_option *ConfOptionByName(const char *name)
{
	int i;

	for (i = 0; i < arrlen(confoptions); ++i) {
		if (!strcasecmp(name, confoptions[i].name)) {
			return &confoptions[i];
		}
	}

	return NULL;
}

static void ParseConfigLine(const char *config_file, int lineno, char *line)
{
	char *name, *value, *p;
	int key;
	const struct conf_option *opt;

	p = line;
	Chomp(p);

	// skip whitespace and discard comments.
	while (*p != '\0' && isspace(*p)) {
		++p;
	}
	if (*p == '#') {
		return;
	}
	if (*p == '\0') {
		return;
	}

	name = p;
	for (; *p != '\0' && !isspace(*p); ++p)
		;

	if (*p == '\0') {
		fprintf(stderr, "swloadconf: %s:%d: malformed line: no value\n",
		        config_file, lineno);
		return;
	}

	*p++ = '\0';
	for (; isspace(*p); ++p)
		;
	value = p;

	opt = ConfOptionByName(name);
	if (opt == NULL) {
		fprintf(stderr,
		        "swloadconf: %s:%d: unknown config option '%s'\n",
		        config_file, lineno, name);
		return;
	}

	switch (opt->type) {
	case CONF_BOOL:
		*opt->value.b = atoi(value) != 0;
		break;
	case CONF_INT:
		if (!strcasecmp(opt->name, "conf_video_palette")) {
			// If an invalid video palette number was loaded, use
			// palette 0
			if (atoi(value) >= (Vid_GetNumVideoPalettes())) {
				*opt->value.i = 0;
			} else {
				*opt->value.i = atoi(value);
			}
		} else {
			*opt->value.i = atoi(value);
		}
		break;
	case CONF_KEY:
		if (sscanf(value, "%d", &key) == 1) {
			*opt->value.i = key;
		}
		break;
	default:
		break;
	}
}

//
// load config file
//
// ugly but it works
//

void swloadconf(void)
{
	const char *config_file = GetConfigFilename();
	FILE *fs;
	char inbuf[128];
	int lineno = 0;

	if (config_file == NULL) {
		return;
	}

	fs = fopen(config_file, "r");

	if (fs == NULL) {
		// It isn't an error if the config file doesn't exist yet.
		if (errno != ENOENT) {
			fprintf(stderr, "swloadconf: failed to open %s: %s\n",
			        config_file, strerror(errno));
		}
		return;
	}

	while (!feof(fs)) {
		fgets(inbuf, sizeof(inbuf), fs);
		++lineno;
		ParseConfigLine(config_file, lineno, inbuf);
	}

	fclose(fs);
}

//
// swsaveconf
//
// save config file
//

void swsaveconf(void)
{
	const char *config_file = GetConfigFilename();
	FILE *fs;
	int i;

	if (config_file == NULL) {
		return;
	}

	fs = fopen(config_file, "w");

	if (fs == NULL) {
		fprintf(stderr, "swsaveconf: failed to open %s: %s\n",
		        config_file, strerror(errno));
		return;
	}

	fprintf(fs, "# Configuration file for " PACKAGE_NAME "\n"
	            "# Created by " PACKAGE_STRING "\n\n");

	for (i = 0; i < arrlen(confoptions); ++i) {
		fprintf(fs, "%-25s ", confoptions[i].name);
		switch (confoptions[i].type) {
		case CONF_BOOL:
			fprintf(fs, "%d", *confoptions[i].value.b);
			break;
		case CONF_INT:
		case CONF_KEY:
			fprintf(fs, "%d", *confoptions[i].value.i);
			break;
		default:
			fprintf(fs, "?");
			break;
		}
		fprintf(fs, "\n");
	}

	fprintf(fs, "\n\n");

	fclose(fs);
}

static const struct menuitem keys_menu_items[] = {
    CONFIG_OPTION("Accelerate", "key_accelerate"),
    CONFIG_OPTION("Decelerate", "key_decelerate"),
    CONFIG_OPTION("Pull up", "key_pullup"),
    CONFIG_OPTION("Pull down", "key_pulldown"),
    CONFIG_OPTION("Flip", "key_flip"),
    CONFIG_OPTION("Fire machine gun", "key_fire"),
    CONFIG_OPTION("Drop bomb", "key_dropbomb"),
    CONFIG_OPTION("Navigate home", "key_home"),
    {0, NULL},
};

static const struct menu keys_menu = {
    FullscreenBackground,
    "OPTIONS",
    keys_menu_items,
};

static const struct menuitem options_menu_items[] = {
#ifndef NO_FULLSCREEN
    CONFIG_OPTION("Run fullscreen", "vid_fullscreen"),
#endif
    CONFIG_OPTION("Video palette", "conf_video_palette"),
    CONFIG_OPTION("Solid ground", "conf_solidground"),
    CONFIG_OPTION("HUD splats", "conf_hudsplats"),
    CONFIG_OPTION("Wounded planes", "conf_wounded"),
    CONFIG_OPTION("Oxen and birds", "conf_animals"),
    CONFIG_OPTION("Big oil tank explosions", "conf_big_explosions"),
    CONFIG_OPTION("Medals", "conf_medals"),
    CONFIG_OPTION("Harry keys mode", "conf_harrykeys"),
    {0, ""},
    {'K', "Key bindings >>>", SubMenu, &keys_menu},
    {0, NULL},
};

const struct menu options_menu = {
    FullscreenBackground,
    "OPTIONS > KEY BINDINGS",
    options_menu_items,
};
