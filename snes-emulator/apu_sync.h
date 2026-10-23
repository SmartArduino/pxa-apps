/* PXA SNES synchronization. Core license: vendor/snes9x/src/LICENSE. */
#pragma once

#ifdef PXA_SNES_EVENT_SYNC
#include <stdbool.h>

#define SNES_APU_CHECKPOINTS 256u
extern long snes_apu_checkpoint;
extern long snes_apu_boundaries[SNES_APU_CHECKPOINTS];
extern unsigned snes_apu_count;

void snes_apu_flush(void);
void snes_apu_reset(void);
long snes_apu_idle_deadline(long instruction_start);

/* The original core synchronizes before every CPU opcode. Record those times
 * cheaply, then catch up before a port access, DMA, shutdown or scanline event.
 * CPU.Cycles is long in this core (32 bits on Wasm). The bounded history costs
 * 1 KiB there and also handles unusually long sequences without an event.
 * Keep this in the CPU loop: another function call per opcode defeats batching. */
#define SNES_APU_DEFER()                                                                           \
    do {                                                                                           \
        if (IAPU.APUExecuting) {                                                                   \
            if (snes_apu_count == SNES_APU_CHECKPOINTS)                                            \
                snes_apu_flush();                                                                  \
            snes_apu_boundaries[snes_apu_count++] = CPU.Cycles;                                    \
        }                                                                                          \
    } while (0)
#else
#define SNES_APU_DEFER() APU_EXECUTE()
#define snes_apu_flush() ((void)0)
#define snes_apu_reset() ((void)0)
#endif
