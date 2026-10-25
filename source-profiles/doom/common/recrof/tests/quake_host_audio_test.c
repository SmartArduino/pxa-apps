/* SPDX-License-Identifier: GPL-2.0-or-later */
#undef NDEBUG
#include "pxa/audio_mix.h"
#include "pxa/wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { int left, right; } portable_samplepair_t;
typedef struct { int leftvol, rightvol; } channel_t;
#include "quake_original_mix.h"
enum { TARGET = 2048, SOURCE = 16384, SPANS = 64 };
static _Alignas(8) uint8_t memory[32768];
static portable_samplepair_t expected[1024];
static uint32_t rng = 97;
static uint32_t random32(void) { return rng = rng * 1664525u + 1013904223u; }
int main(void) {
    unsigned cases = 0;
    for (unsigned trial = 0; trial < 8192; ++trial) {
        memset(memory, 0, sizeof(memory)); memset(expected, 0, sizeof(expected));
        unsigned count = 1 + trial % 1024, voices = 1 + trial % 8;
        unsigned first = trial % 7;
        if (count + first > 1024) first = 0;
        uint8_t header[16] = {0};
        pxa_write_u32(header, TARGET); pxa_write_u32(header + 4, 1024);
        pxa_write_u32(header + 8, SPANS); pxa_write_u16(header + 12, voices);
        for (unsigned voice = 0; voice < voices; ++voice) {
            uint8_t *samples = memory + SOURCE + voice * 1024;
            for (unsigned n = 0; n < count; ++n) samples[n] = random32() >> 24;
            channel_t channel = {(int)(trial % 256), (int)(random32() % 256)};
            original(expected + first, &channel, (signed char *)samples, count);
            uint8_t *span = memory + SPANS + voice * 32;
            pxa_write_u32(span, SOURCE + voice * 1024);
            pxa_write_u32(span + 4, count); pxa_write_u32(span + 12, 65536);
            pxa_write_u32(span + 16, first); pxa_write_u32(span + 20, count);
            pxa_write_u16(span + 24, channel.leftvol & ~7);
            pxa_write_u16(span + 26, channel.rightvol & ~7);
            span[28] = PXA_AUDIO_MIX_S8; span[29] = 1;
        }
        assert(!pxa_audio_mix_pcm(header, sizeof(header), memory, sizeof(memory)));
        for (unsigned n = 0; n < 1024; ++n) {
            assert((int32_t)pxa_read_u32(memory + TARGET + n * 8) == expected[n].left);
            assert((int32_t)pxa_read_u32(memory + TARGET + n * 8 + 4) == expected[n].right);
        }
        ++cases;
    }
    printf("PASS: %u comparisons with current Quake kernel; 1..8 voices, "
           "1..1024 frames, all gain quantization buckets, stereo accumulators\n", cases);
}
