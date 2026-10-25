/* GPL-2.0-or-later. Optional diagnostics; absent from release hot paths. */
#pragma once
#include "emu_profile.h"
#ifdef PXA_GBA_PROFILE
#include <stdint.h>
extern uint32_t gba_profile_instructions[2][16];
#define GBA_PROFILE_INSTRUCTION(thumb, pc) (++gba_profile_instructions[thumb][((pc) >> 24) & 15])
#else
#define GBA_PROFILE_INSTRUCTION(thumb, pc) ((void)0)
#endif
