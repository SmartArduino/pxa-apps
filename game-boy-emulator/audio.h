/* Streaming gnuboy PCM conversion. GPL-2.0-or-later. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#define GB_AUDIO_STAGE_RATE 32768u
#define GB_AUDIO_OUT_MAX 2048u
#define GB_AUDIO_STAGE_MAX 1024u
typedef struct {
    int16_t previous[2];
    unsigned have_previous;
    uint32_t phase, step, rate;
    unsigned channels;
    unsigned fade_remaining, fade_total;
} gb_audio_t;
void gb_audio_reset(gb_audio_t *a);
void gb_audio_configure(gb_audio_t *a,unsigned rate,unsigned channels);
void gb_audio_start(gb_audio_t *a,unsigned rate,unsigned channels);
/* 32768 Hz interleaved stereo to the negotiated Host format. */
unsigned gb_audio_convert(gb_audio_t *a,const int16_t *src,size_t frames,int16_t *dst,unsigned capacity);

/* Exact-rate stereo borrows the core buffer after the one-time start ramp.
 * scratch_capacity is in stereo frames; NULL indicates insufficient space. */
const int16_t *gb_audio_native(gb_audio_t *a,const int16_t *src,size_t frames,
                              int16_t *scratch,size_t scratch_capacity);
