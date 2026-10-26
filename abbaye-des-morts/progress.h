/* Persistent original-game checkpoints. GPL-3.0-or-later. */
#pragma once
#include "vendor/src/structs.h"
int pxa_progress_load(struct hero *hero,unsigned stage[25][22][32],unsigned *room);
void pxa_progress_update(const struct hero *hero,const unsigned stage[25][22][32]);
void pxa_progress_clear(void);
