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

#ifndef __SWTITLE_H__
#define __SWTITLE_H__

#include "sw.h"

enum text_align {
	TEXT_ALIGN_LEFT,
	TEXT_ALIGN_CENTER,
	TEXT_ALIGN_RIGHT
};

extern void clrprmpt(void);
extern bool ctlbreak(void);
extern void getgamemode(void);
extern void swtitln(void);
extern void swtitlf(void);

void ClearTitleScreen(void);
void AddTitleText(char *text, int x, int y, int color, enum text_align align);
void AddTitleGround(GRNDTYPE *ground, unsigned int len, ground_render_t render);
void AddTitleSymbol(sopsym_t *sym, int x, int y, faction_t faction);
void AddTitleLine(int x1, int y1, int x2, int y2, int color);

#endif
