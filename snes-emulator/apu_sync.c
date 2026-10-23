/* PXA SNES synchronization. Core license: vendor/snes9x/src/LICENSE. */
#include "vendor/snes9x/snes9x.h"
#include "apu_sync.h"

#ifdef PXA_SNES_EVENT_SYNC
long snes_apu_checkpoint;
long snes_apu_boundaries[SNES_APU_CHECKPOINTS];
unsigned snes_apu_count;

void snes_apu_reset(void) {
    snes_apu_count = 0;
    snes_apu_checkpoint = 0;
}

long snes_apu_idle_deadline(long instruction_start) {
    /* An SPC idle hint stops future CPU synchronization calls, but the old
     * inner while loop finishes its CURRENT CPU boundary. Recover that exact
     * boundary instead of either stopping early or running to the whole batch.
     * Boundaries stay ordered because scanline cycle rebasing flushes first. */
    unsigned lo = 0, hi = snes_apu_count;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        if (snes_apu_boundaries[mid] < instruction_start)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < snes_apu_count ? snes_apu_boundaries[lo] : snes_apu_checkpoint;
}

void snes_apu_flush(void) {
    if (!snes_apu_count)
        return;
    snes_apu_checkpoint = snes_apu_boundaries[snes_apu_count - 1];
    if (IAPU.APUExecuting && APU.Cycles <= snes_apu_checkpoint)
        APUExecute(true, &snes_apu_checkpoint);
    snes_apu_count = 0;
}
#endif
