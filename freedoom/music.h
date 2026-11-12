// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PXA_FREEDOOM_MUSIC_H
#define PXA_FREEDOOM_MUSIC_H
#include <stddef.h>
#include <stdint.h>
#define FD_MUSIC_RATE 11025
#define FD_MUSIC_BLOCK 512
/* Borrows immutable WAD bytes until the next start/stop. No heap allocations. */
int fd_music_init(const void *genmidi, size_t bytes);
int fd_music_start(const void *midi, size_t bytes, int looping);
void fd_music_stop(void);
/* Advances even at volume zero, freezes while paused. Saturates SFX + music. */
void fd_music_mix(int16_t *stereo, unsigned frames, unsigned volume, int paused);
uint64_t fd_music_position(void);
unsigned fd_music_loops(void);
unsigned fd_music_faults(void);
unsigned fd_music_peak(void);
#endif
