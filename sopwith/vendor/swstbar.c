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
// Status bar code
//

#include "video.h"

#include "swinit.h"
#include "swmain.h"
#include "swtext.h"

static void dispribbonrow(int *ribbonid, int ribbons_nr, int y)
{
	int i;
	int offset = 50 + ((3 - ribbons_nr) * 4);

	for (i = 0; i < ribbons_nr; i++) {
		int ribbon_id = ribbonid[i];

		Vid_DispSymbol(offset, y, &symbol_ribbon[ribbon_id].sym[0],
		               FACTION_PLAYER1);
		offset += 8;
	}
}

static void dispmedals(OBJECTS *ob)
{
	static const int medal_widths[3] = {10, 8, 8};
	static const int medal_offsets[3] = {0, -1, -1};
	int medal_offset = 50;
	int i;

	for (i = 0; i < ob->ob_score.medals_nr; i++) {
		int medal_id = ob->ob_score.medals[i];

		Vid_DispSymbol(medal_offset + medal_offsets[medal_id], 11,
		               &symbol_medal[medal_id].sym[0], FACTION_PLAYER1);
		medal_offset += medal_widths[medal_id];
	}

	if (ob->ob_score.ribbons_nr <= 3) {
		dispribbonrow(ob->ob_score.ribbons, ob->ob_score.ribbons_nr,
		              15);
	} else {
		dispribbonrow(ob->ob_score.ribbons, 3, 16);
		dispribbonrow(ob->ob_score.ribbons + 3,
		              ob->ob_score.ribbons_nr - 3, 14);
	}
}

static void dispscore(OBJECTS *ob)
{
	char buf[10];
	int x;

	Vid_Box(0, 16, 48 + 32, 16, 0);

	// We adjust position for large scores to not overwrite the medals.
	if (!in_range(-9999, ob->ob_score.score, 99999)) {
		// Wow?
		x = 0;
	} else if (!in_range(-999, ob->ob_score.score, 9999)) {
		x = 1;
	} else {
		x = 2;
	}
	swposcur(x, 24);
	swcolor(ob->ob_clr);
	snprintf(buf, sizeof(buf), "%d", ob->ob_score.score);
	swputs(buf);
}

static void dispgge(int x, int cury, int maxy, int clr)
{
	int y;

	cury = clamp_max(9, cury * 10 / maxy - 1);
	for (y = 0; y <= cury; ++y) {
		Vid_PlotPixel(x, y, clr);
	}
	for (; y <= 9; ++y) {
		Vid_PlotPixel(x, y, 0);
	}
}

static void dispgauges(OBJECTS *ob)
{
	int x = GAUGEX;
	int sep = conf_missiles ? 3 : 5;

	// crashes/lives
	dispgge(x += sep, maxcrash - ob->ob_crashcnt, maxcrash, 1);

	// fuel
	dispgge(x += sep, ob->ob_life >> 4, MAXFUEL >> 4, 1);

	// bombs
	dispgge(x += sep, ob->ob_bombs, MAXBOMBS, 2);

	// bullets
	dispgge(x += sep, ob->ob_rounds, MAXROUNDS, 3);

	if (conf_missiles) {

		// missiles
		dispgge(x += sep, ob->ob_missiles, MAXMISSILES, 1);

		// starburst (flares)
		dispgge(x += sep, ob->ob_bursts, MAXBURSTS, 2);
	}
}

static void dispmapobjects(void)
{
	int mapx, mapy, x, y, groundy;
	OBJECTS *ob;

	for (ob = objtop; ob; ob = ob->ob_next) {
		if (!ob->ob_onmap || ob->ob_x < 0 ||
		    ob->ob_x >= currgame->gm_max_x) {
			continue;
		}

		mapx = ob->ob_x + (ob->ob_symbol->w / 2);

		// We usually draw the dot at the map location of the
		// center of the sprite. However, if the object is
		// sitting on the ground (eg. target) we want to make
		// sure it is never floating above the ground.
		groundy = ground[ob->ob_x];
		if (ob->ob_y - ob->ob_symbol->h <= groundy) {
			// on ground
			mapy = ob->ob_y - ob->ob_symbol->h + 8;
		} else {
			mapy = ob->ob_y - (ob->ob_symbol->h / 2);
		}
		x = SCR_CENTR + mapx / WRLD_RSX;
		y = mapy / WRLD_RSY;

		if (y < SCR_MNSH - 1) {
			Vid_PlotPixel(x, y, Vid_FuselageColor(ob->ob_clr));
		}
	}
}

static void dispmap(void)
{
	int x, y, dx, maxh, sx;

	dx = 0;
	sx = SCR_CENTR;

	maxh = 0;
	y = 0;

	// draw ground
	for (x = 0; x < currgame->gm_max_x; ++x) {
		maxh = imax(maxh, ground[x]);

		++dx;

		if (dx == WRLD_RSX) {
			maxh /= WRLD_RSY;
			if (maxh == y) {
				Vid_PlotPixel(sx, maxh, 7);
			} else if (maxh > y) {
				for (++y; y <= maxh; ++y) {
					Vid_PlotPixel(sx, y, 7);
				}
			} else {
				for (--y; y >= maxh; --y) {
					Vid_PlotPixel(sx, y, 7);
				}
			}
			y = maxh;
			Vid_PlotPixel(sx, 0, 11);
			++sx;
			dx = maxh = 0;
		}
	}

	// map border
	maxh = MAX_Y / WRLD_RSY;
	for (y = 0; y <= maxh; ++y) {
		Vid_PlotPixel(SCR_CENTR, y, 11);
		Vid_PlotPixel(sx, y, 11);
	}

	dispmapobjects();

	// border of status bar
	for (x = 0; x < SCR_WDTH; ++x) {
		Vid_PlotPixel(x, SCR_MNSH + 2, 7);
	}
}

void dispstatusbar(void)
{
	dispmap();
	dispscore(consoleplayer);
	dispgauges(consoleplayer);

	if (conf_medals) {
		dispmedals(consoleplayer);
	}
}
