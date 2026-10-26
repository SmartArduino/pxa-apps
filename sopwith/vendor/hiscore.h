//
// Copyright(C) 2025 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//
//
// High score table
//

#ifndef __HISCORE_H__
#define __HISCORE_H__

#include "sw.h"

extern void DrawHighScoreTable(void);
extern void LoadHighScoreTable(void);
extern void SaveHighScoreTable(void);
extern bool NewHighScore(score_t *s);
extern void InitHighScores(void);

#endif /* #ifndef __HISCORE_H__ */
