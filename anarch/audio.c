/* SPDX-License-Identifier: CC0-1.0
 * PXA adapter for Anarch's original CC0 sound effects. Background music is disabled.
 * Released under CC0 1.0 plus a waiver of all other intellectual-property rights.
 */
#include "audio.h"

/* Keep the original audio content in this one translation unit. The standalone
 * sound header needs these two definitions normally provided by game.h. */
#define SFG_PROGRAM_MEMORY static const
#define SFG_PROGRAM_MEMORY_U8(address) (*(address))
#include "third_party/anarch/sounds.h"

typedef struct {
    uint16_t position;
    uint8_t sound_id;
    uint8_t volume;
} anarch_audio_voice_t;

static anarch_audio_voice_t voices[ANARCH_AUDIO_VOICE_COUNT];
static uint8_t paused;
static int64_t dc_q16;

void anarch_audio_clear_effects(void) {
    for (unsigned i = 0; i < ANARCH_AUDIO_VOICE_COUNT; ++i) {
        voices[i].position = SFG_SFX_SAMPLE_COUNT;
        voices[i].sound_id = 0;
        voices[i].volume = 0;
    }
    dc_q16 = 0;
}

void anarch_audio_reset(void) {
    anarch_audio_clear_effects();
    paused = 0;
}

void anarch_audio_set_music(uint8_t command) {
    /* Keep the upstream hook and saved sound options compatible. The PXA
     * edition plays effects only, regardless of the saved music setting. */
    (void)command;
}

void anarch_audio_play_sound(uint8_t sound_id, uint8_t volume) {
    if (sound_id >= ANARCH_AUDIO_SOUND_COUNT || volume == 0 || paused)
        return;

    unsigned selected = 0;
    uint16_t oldest_position = 0;
    for (unsigned i = 0; i < ANARCH_AUDIO_VOICE_COUNT; ++i) {
        if (voices[i].position >= SFG_SFX_SAMPLE_COUNT || voices[i].volume == 0) {
            selected = i;
            break;
        }
        /* All effects are equally long, so greatest progress means oldest.
         * A tie deterministically chooses the first voice. */
        if (voices[i].position > oldest_position) {
            oldest_position = voices[i].position;
            selected = i;
        }
    }
    voices[selected].position = 0;
    voices[selected].sound_id = sound_id;
    voices[selected].volume = volume;
}

void anarch_audio_set_paused(uint8_t value) {
    if (value && !paused)
        anarch_audio_clear_effects();
    paused = value != 0;
}

void anarch_audio_render(int16_t *out, size_t frames) {
    if (out == NULL)
        return;

    for (size_t frame = 0; frame < frames; ++frame) {
        if (paused) {
            out[frame] = 0;
            continue;
        }

        /* Each effect uses 3/16 of unsigned-eight-bit-to-S16 conversion
         * before its own volume. Four simultaneous effects stay bounded. */
        int32_t mix = 0;
        for (unsigned i = 0; i < ANARCH_AUDIO_VOICE_COUNT; ++i) {
            anarch_audio_voice_t *voice = &voices[i];
            if (voice->position < SFG_SFX_SAMPLE_COUNT && voice->volume != 0) {
                int32_t sample = (int32_t)SFG_GET_SFX_SAMPLE(
                    voice->sound_id, voice->position) - 128;
                ++voice->position;
                mix += (sample * voice->volume * 48) / 255;
            }
        }

        /* One-pole DC estimate, coefficient 1/256 (~5 Hz at 8 kHz). Q16
         * history preserves small changes; multiplication avoids signed-shift
         * undefined behavior. int64_t keeps the transient arithmetic bounded. */
        dc_q16 += ((int64_t)mix * 65536 - dc_q16) / 256;
        int32_t filtered = mix - (int32_t)(dc_q16 / 65536);
        if (filtered > 32767)
            filtered = 32767;
        else if (filtered < -32768)
            filtered = -32768;
        out[frame] = (int16_t)filtered;
    }
}
