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
//        swtitle  -      SW perform animation on the title screen
//

#include <ctype.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

#include "video.h"

#include "hiscore.h"
#include "sw.h"
#include "swasynio.h"
#include "swconf.h"
#include "swend.h"
#include "swgames.h"
#include "swgrpha.h"
#include "swinit.h"
#include "swmain.h"
#include "swmenu.h"
#include "swsound.h"
#include "swsymbol.h"
#include "swtext.h"
#include "swtitle.h"
#include "timer.h"

// How many milliseconds from when the title screen appears to when the high
// score table gets shown?
#define HIGH_SCORE_PERIOD 10000

#define X_OFFSET ((SCR_WDTH / 2) - 160)

// If a text element has this magic string value, it is (1) drawn centered and
// (2) replaced by a string that shows the program version.
#define MAGIC_VERSION "__VERSION__"

struct title_element {
	enum {
		TITLE_GROUND,
		TITLE_TEXT,
		TITLE_SYMBOL,
		TITLE_LINE
	} type;
	union {
		struct {
			int color;
			int x, y;
			char *text;
			enum text_align align;
		} text;
		struct {
			int faction;
			int x, y;
			sopsym_t *sym;
		} sym;
		struct {
			GRNDTYPE *ground;
			unsigned int ground_len;
			ground_render_t render;
		} ground;
		struct {
			int color;
			int x1, y1;
			int x2, y2;
		} line;
	} v;
};

static struct title_element default_elements[] = {
    {TITLE_GROUND, {.ground = {original_ground + 367, 600}}},
    {TITLE_TEXT, {.text = {2, 20, 2, "SDL", TEXT_ALIGN_CENTER}}},
    {TITLE_TEXT, {.text = {3, 20, 4, "S O P W I T H", TEXT_ALIGN_CENTER}}},
    {TITLE_TEXT, {.text = {3, 20, 6, MAGIC_VERSION, TEXT_ALIGN_CENTER}}},
    {TITLE_TEXT, {.text = {3, 0, 9, "(c) 1984, 1985, 1987"}}},
    {TITLE_TEXT, {.text = {1, 21, 9, "BMB"}}},
    {TITLE_TEXT, {.text = {3, 25, 9, "Compuscience"}}},
    {TITLE_TEXT, {.text = {3, 0, 10, "(c) 1984-2000 David L. Clark"}}},
    {TITLE_TEXT,
     {.text = {3, 0, 11, "(c) 2001-2024 Simon Howard, Jesse Smith"}}},
    {TITLE_TEXT, {.text = {3, 4, 12, "Distributed under the"}}},
    {TITLE_TEXT, {.text = {1, 26, 12, "GNU"}}},
    {TITLE_TEXT, {.text = {3, 30, 12, "GPL"}}},
    {TITLE_SYMBOL, {.sym = {1, 40, 19, &symbol_plane[0].sym[0]}}},
    {TITLE_SYMBOL, {.sym = {2, 130, 119, &symbol_plane[1].sym[6]}}},
    {TITLE_SYMBOL, {.sym = {2, 23, 108, &symbol_targets[3].sym[0]}}},
    {TITLE_SYMBOL, {.sym = {1, 212, 108, &symbol_ox[0].sym[0]}}},
    {TITLE_LINE, {.line = {2, 280, 34, 280, 34}}},
    {TITLE_LINE, {.line = {2, 280, 29, 280, 29}}},
    {TITLE_LINE, {.line = {2, 280, 24, 280, 24}}},
    {TITLE_LINE, {.line = {2, 280, 19, 280, 19}}},
    {TITLE_LINE, {.line = {2, 280, 14, 280, 14}}},
    {TITLE_LINE, {.line = {2, 280, 9, 280, 9}}},
    {TITLE_SYMBOL, {.sym = {2, 270, 39, &symbol_plane_hit[0].sym[0]}}},
};

static struct title_element *title_elements = default_elements;
static int title_elements_len = arrlen(default_elements);
static int title_screen_start;

static void FreeTitleElement(struct title_element *el)
{
	switch (el->type) {
	case TITLE_TEXT:
		free(el->v.text.text);
		break;
	case TITLE_GROUND:
		free(el->v.ground.ground);
		break;
	default:
		break;
	}
}

// Clear all title screen elements. Starts the process of defining a new
// title screen in a mod file.
void ClearTitleScreen(void)
{
	static bool first_clear = true;
	int i;

	if (!first_clear) {
		for (i = 0; i < title_elements_len; ++i) {
			FreeTitleElement(&title_elements[i]);
		}
		free(title_elements);
	}

	title_elements = NULL;
	title_elements_len = 0;
	first_clear = false;
}

static struct title_element *AddTitleElement(void)
{
	++title_elements_len;
	title_elements = checked_realloc(
	    title_elements, title_elements_len * sizeof(struct title_element));
	return title_elements + title_elements_len - 1;
}

void AddTitleText(char *text, int x, int y, int color, enum text_align align)
{
	struct title_element *el = AddTitleElement();
	el->type = TITLE_TEXT;
	el->v.text.x = x;
	el->v.text.y = y;
	el->v.text.color = color;
	el->v.text.text = text;
	el->v.text.align = align;
}

void AddTitleGround(GRNDTYPE *ground, unsigned int len, ground_render_t render)
{
	struct title_element *el = AddTitleElement();
	el->type = TITLE_GROUND;
	el->v.ground.ground = ground;
	el->v.ground.ground_len = len;
	el->v.ground.render = render;
}

void AddTitleSymbol(sopsym_t *sym, int x, int y, faction_t faction)
{
	struct title_element *el = AddTitleElement();
	el->type = TITLE_SYMBOL;
	el->v.sym.sym = sym;
	el->v.sym.x = x;
	el->v.sym.y = y;
	el->v.sym.faction = faction;
}

void AddTitleLine(int x1, int y1, int x2, int y2, int color)
{
	struct title_element *el = AddTitleElement();
	el->type = TITLE_LINE;
	el->v.line.x1 = x1;
	el->v.line.y1 = y1;
	el->v.line.x2 = x2;
	el->v.line.y2 = y2;
	el->v.line.color = color;
}

static void DrawTitleScreenText(void)
{
	struct title_element *el;
	const char *text;
	int i, x;

	for (i = 0; i < title_elements_len; ++i) {
		el = &title_elements[i];
		if (el->type != TITLE_TEXT) {
			continue;
		}
		swcolor(el->v.text.color);

		x = el->v.text.x + X_OFFSET / 8;
		text = el->v.text.text;
		if (!strcmp(text, MAGIC_VERSION)) {
			text = "Version " PACKAGE_VERSION;
		}

		switch (el->v.text.align) {
		case TEXT_ALIGN_CENTER:
			x -= (UTF8_Strlen(text) + 1) / 2;
			break;
		case TEXT_ALIGN_RIGHT:
			x -= UTF8_Strlen(text);
			break;
		default:
			break;
		}
		swposcur(x, el->v.text.y);
		swputs(text);
	}
}

static void DrawLine(struct title_element *el)
{
	int x1 = el->v.line.x1, x2 = el->v.line.x2;
	int y1 = el->v.line.y1, y2 = el->v.line.y2;
	int cnt = imax(abs(x1 - x2), abs(y1 - y2)) + 1;
	int i;

	for (i = 0; i < cnt; ++i) {
		int j = cnt - i;
		int x = (x1 * i + x2 * j) / cnt;
		int y = (y1 * i + y2 * j) / cnt;
		Vid_PlotPixel(x, SCR_HGHT - 1 - y, el->v.line.color);
	}
}

static void DrawTitleScreenElements(void)
{
	struct title_element *el;
	int i, xoff;

	for (i = 0; i < title_elements_len; ++i) {
		el = &title_elements[i];
		switch (el->type) {
		case TITLE_SYMBOL:
			Vid_DispSymbol(el->v.sym.x + X_OFFSET,
			               SCR_HGHT - 1 - el->v.sym.y,
			               el->v.sym.sym, el->v.sym.faction);
			break;
		case TITLE_LINE:
			DrawLine(el);
			break;
		case TITLE_GROUND:
			// The logic here ensures that the ground is correctly
			// aligned even if the screen width is changed.
			xoff = (SCR_WDTH - (int) el->v.ground.ground_len) / 2;
			swground(el->v.ground.ground - imin(xoff, 0),
			         imax(xoff, 0),
			         imin(el->v.ground.ground_len, SCR_WDTH),
			         el->v.ground.render);
			break;
		default:
			break;
		}
	}
}

void swtitln(void)
{
	int i;

	sound(S_TITLE, 0, NULL);

	// clear the screen
	Vid_ClearBuf();

	DrawTitleScreenElements();

	// We show the title screen, but after five seconds it switches to the
	// high score table instead.
	i = Timer_GetMS() - title_screen_start;
	if ((i / HIGH_SCORE_PERIOD) % 2 == 0) {
		DrawTitleScreenText();
	} else {
		DrawHighScoreTable();
	}
}

void swtitlf(void)
{
	if (titleflg) {
		return;
	}

	sound(0, 0, NULL);
	swsound();
}

bool ctlbreak(void)
{
	return Vid_GetCtrlBreak();
}

// clear bottom of screen
void clrprmpt(void)
{
	int x, y;

	for (y = 0; y <= 43; ++y)
		for (x = 0; x < SCR_WDTH; ++x) {
			Vid_PlotPixel(x, y, 0);
		}

	swposcur(0, 20);
}

static void TitleScreenBackground(void *unused)
{
	swtitln();
	clrprmpt();
}

static bool gethost(void)
{
	static char addrbuf[50];

	clrprmpt();

	swputs("Enter Remote Hostname/IP:\n");
	swgets(addrbuf, sizeof(addrbuf) - 3);
	asynhost = addrbuf;

	return strcmp(addrbuf, "") != 0;
}

#ifdef TCPIP
static enum menu_action StartNetgameListen(const struct menuitem *item)
{
	playmode = PLAYMODE_ASYNCH;
	asynmode = ASYN_LISTEN;
	return MENU_ACTION_RETURN;
}

static enum menu_action StartNetgameConnect(const struct menuitem *item)
{
	if (gethost()) {
		playmode = PLAYMODE_ASYNCH;
		asynmode = ASYN_CONNECT;
		return MENU_ACTION_RETURN;
	} else {
		return MENU_ACTION_NONE;
	}
}

static const struct menuitem netgame_menu_items[] = {
    {'L', "listen for connection", StartNetgameListen},
    {'C', "connect to remote host", StartNetgameConnect},
    {0, NULL},
};

static const struct menu netgame_menu = {
    TitleScreenBackground,
    NULL,
    netgame_menu_items,
};
#endif

static enum menu_action StartNovice(const struct menuitem *item)
{
	playmode = PLAYMODE_NOVICE;
	return MENU_ACTION_RETURN;
}

static enum menu_action StartExpert(const struct menuitem *item)
{
	playmode = PLAYMODE_SINGLE;
	return MENU_ACTION_RETURN;
}

static const struct menuitem single_player_menu_items[] = {
    {'N', "novice player", StartNovice},
    {'E', "expert player", StartExpert},
    {0, NULL},
};

static const struct menu single_player_menu = {
    TitleScreenBackground,
    NULL,
    single_player_menu_items,
};

static enum menu_action StartVsComputer(const struct menuitem *item)
{
	playmode = PLAYMODE_COMPUTER;
	return MENU_ACTION_RETURN;
}

#ifdef __EMSCRIPTEN__
static enum menu_action OpenManual(const struct menuitem *item)
{
	emscripten_run_script("openManual()");
	return MENU_ACTION_NONE;
}

static enum menu_action InstallApp(const struct menuitem *item)
{
	emscripten_run_script("promptForInstall()");
	return MENU_ACTION_NONE;
}
#endif

#ifndef NO_EXIT
static enum menu_action QuitGame(const struct menuitem *item)
{
	exit(0);
	return MENU_ACTION_NONE;
}
#endif

static const struct menuitem main_menu_items[] = {
    {'S', "single player", SubMenu, &single_player_menu},
    {'C', "single player versus computer", StartVsComputer},
#ifdef TCPIP
    {'N', "network game", SubMenu, &netgame_menu},
#endif
    {'O', "game options", SubMenu, &options_menu},
#ifdef __EMSCRIPTEN__
    {'M', "open manual", OpenManual},
    {'I', "install as app", InstallApp},
#endif
#ifndef NO_EXIT
    {'Q', "quit game", QuitGame},
#endif
    {0, NULL},
};

static const struct menu main_menu = {
    TitleScreenBackground,
    NULL,
    main_menu_items,
};

void getgamemode(void)
{
	title_screen_start = Timer_GetMS();
	playmode = PLAYMODE_UNSET;
	while (playmode == PLAYMODE_UNSET) {
		RunMenu(&main_menu);
	}
}

//
// 2003-02-14: Code was checked into version control; no further entries
// will be added to this log.
//
// sdh 14/2/2003: change license header to GPL
// sdh 27/06/2002: move to new sopsym_t for symbols,
//                 remove references to symwdt, symhgt
// sdh 28/04/2002: Centering of title screen on non-320 pixel wide screens
// sdh 26/03/2002: change CGA_ to Vid_
// sdh 16/11/2001: TCPIP #define to disable TCP/IP support
// sdh 29/10/2001: moved options menu into swconf.c
// sdh 29/10/2001: harrykeys
// sdh 28/10/2001: rearranged title/menu stuff a bit
//                 added options menu
// sdh 21/10/2001: rearranged headers, added cvs tags
// sdh 19/10/2001: moved all the menus here
// sdh 19/10/2001: removed extern definitions, these are now in headers
// sdh 18/10/2001: converted all functions to ANSI-style arguments
// sdh ??/10/2001: added #define to control whether we use the classic
//                 title screen or the "network edition" one
//
// 2000-10-29      Copyright update.
// 99-01-24        1999 copyright.
// 96-12-27        New network version.
// 87-04-01        Version 7.F15
// 87-03-11        Title reformatting.
// 87-03-10        Microsoft compiler.
// 84-02-02        Development
//
