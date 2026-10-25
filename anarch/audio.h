/* SPDX-License-Identifier: CC0-1.0
 * PXA adapter for Anarch's original CC0 sound effects. Background music is disabled.
 * Released under CC0 1.0 plus a waiver of all other intellectual-property rights.
 */
#ifndef ANARCH_AUDIO_H
#define ANARCH_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ANARCH_AUDIO_RATE = 8000,
    ANARCH_AUDIO_VOICE_COUNT = 4,
    ANARCH_AUDIO_SOUND_COUNT = 6,
    ANARCH_AUDIO_MUSIC_OFF = 0,
    ANARCH_AUDIO_MUSIC_ON = 1,
    ANARCH_AUDIO_MUSIC_NEXT = 2
};

/* Reset to silent, unpaused effect playback. */
void anarch_audio_reset(void);

/* Compatibility hook for the upstream game and existing saved options.
 * All commands are ignored: this edition plays sound effects only. */
void anarch_audio_set_music(uint8_t command);

/* Play sound 0..5 at linear volume 0..255. At most four sounds overlap;
 * admission replaces the oldest active sound when all voices are occupied.
 * Invalid/zero-volume requests and requests while paused are ignored. */
void anarch_audio_play_sound(uint8_t sound_id, uint8_t volume);

/* Discard effects and filter history. */
void anarch_audio_clear_effects(void);

/* Entering pause discards effects/filter history. Paused rendering is silent.
 * Resuming accepts new sound effects without restoring discarded ones. */
void anarch_audio_set_paused(uint8_t paused);

/* Generate signed 16-bit mono PCM at ANARCH_AUDIO_RATE, with bounded gains,
 * DC suppression, and saturation. No allocation, IO, or platform calls.
 * Native int16_t output is little-endian on PXA's wasm target. Rendering
 * advances state: retain pending samples across partial device writes.
 * NULL/zero-size calls do nothing. Call all functions from one thread. */
void anarch_audio_render(int16_t *out, size_t frames);

#ifdef __cplusplus
}
#endif
#endif
