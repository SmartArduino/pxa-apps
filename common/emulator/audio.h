/* Streaming console PCM conversion. GPL-2.0-or-later. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "core.h"
#define EMU_AUDIO_STAGE_RATE (emu_core_audio_rate())
#define EMU_AUDIO_OUT_MAX 2048u
#define EMU_AUDIO_STAGE_MAX 1024u
typedef struct {
    int16_t previous[2];
    unsigned have_previous;
    uint32_t phase, step, rate;
    unsigned channels;
    unsigned fade_remaining, fade_total;
} emu_audio_t;
void emu_audio_reset(emu_audio_t *a);
void emu_audio_configure(emu_audio_t *a,unsigned rate,unsigned channels);
void emu_audio_start(emu_audio_t *a,unsigned rate,unsigned channels);
/* Native-rate interleaved stereo to the negotiated Host format. */
unsigned emu_audio_convert(emu_audio_t *a,const int16_t *src,size_t frames,int16_t *dst,unsigned capacity);
