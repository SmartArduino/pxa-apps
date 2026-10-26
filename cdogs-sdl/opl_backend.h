/* PXA Wolf import audio adapter, GPL-3.0-or-later. */
#pragma once
#include <stdint.h>
int pxa_opl_init(int chips,int clock,int rate);
void pxa_opl_shutdown(void);
void pxa_opl_write(int chip,int address,int value,const int *volume);
void pxa_opl_generate(int chip,int16_t *samples,int frames);
