/* SPDX-License-Identifier: CC0-1.0
 * Native behavioral tests for Anarch's effects-only mixer.
 * Released under CC0 1.0 plus a waiver of all other intellectual-property rights.
 * Build audio.c with this file using -fsanitize=address,undefined.
 * Put generated binaries and logs under local/artifacts.
 */
#include "../audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int16_t first[8192], second[8192];
static int silent(const int16_t *samples, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (samples[i] != 0) return 0;
    return 1;
}
static uint64_t energy(const int16_t *samples, size_t count) {
    uint64_t result = 0;
    for (size_t i = 0; i < count; ++i)
        result += (int64_t)samples[i] * samples[i];
    return result;
}
static uint64_t hash_samples(uint64_t hash, const int16_t *samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint16_t sample = (uint16_t)samples[i];
        hash = (hash ^ (sample & 255)) * UINT64_C(1099511628211);
        hash = (hash ^ (sample >> 8)) * UINT64_C(1099511628211);
    }
    return hash;
}
static void test_effects(void) {
    uint64_t hashes[ANARCH_AUDIO_SOUND_COUNT];
    anarch_audio_reset();
    anarch_audio_render(first, 8192);
    assert(silent(first, 8192));
    for (uint8_t sound = 0; sound < ANARCH_AUDIO_SOUND_COUNT; ++sound) {
        anarch_audio_reset();
        anarch_audio_play_sound(sound, 255);
        anarch_audio_render(first, 2048);
        assert(energy(first, 2048) > 1000000);
        hashes[sound] = hash_samples(0, first, 2048);
        for (uint8_t previous = 0; previous < sound; ++previous)
            assert(hashes[sound] != hashes[previous]);
        anarch_audio_render(first, 8192);
        assert(silent(first + 8192 - 256, 256));
    }
    anarch_audio_reset();
    anarch_audio_play_sound(0, 0);
    anarch_audio_play_sound(ANARCH_AUDIO_SOUND_COUNT, 255);
    anarch_audio_play_sound(255, 255);
    anarch_audio_render(first, 8192);
    assert(silent(first, 8192));
    anarch_audio_reset();
    anarch_audio_play_sound(0, 255);
    anarch_audio_render(first, 2048);
    uint64_t full_energy = energy(first, 2048);
    anarch_audio_reset();
    anarch_audio_play_sound(0, 128);
    anarch_audio_render(first, 2048);
    uint64_t half_energy = energy(first, 2048);
    assert(half_energy > full_energy / 5 && half_energy < full_energy / 3);
    anarch_audio_clear_effects();
    anarch_audio_render(first, 8192);
    assert(silent(first, 8192));
}
static void test_voice_bound(void) {
    anarch_audio_reset();
    for (uint8_t sound = 0; sound < 5; ++sound)
        anarch_audio_play_sound(sound, 255);
    anarch_audio_render(first, 4096);
    anarch_audio_reset();
    anarch_audio_play_sound(4, 255);
    anarch_audio_play_sound(1, 255);
    anarch_audio_play_sound(2, 255);
    anarch_audio_play_sound(3, 255);
    anarch_audio_render(second, 4096);
    assert(memcmp(first, second, 4096 * sizeof(first[0])) == 0);
}
static void test_pause_resume(void) {
    anarch_audio_reset();
    anarch_audio_play_sound(0, 255);
    anarch_audio_set_paused(1);
    anarch_audio_play_sound(1, 255);
    anarch_audio_render(first, 8192);
    assert(silent(first, 8192));
    anarch_audio_set_paused(0);
    anarch_audio_render(first, 8192);
    assert(silent(first, 8192));
    anarch_audio_play_sound(1, 255);
    anarch_audio_render(first, 2048);
    assert(energy(first, 2048) > 1000000);
}
static uint64_t render_effects(size_t chunk) {
    uint64_t hash = UINT64_C(14695981039346656037);
    anarch_audio_reset();
    anarch_audio_set_music(ANARCH_AUDIO_MUSIC_ON);
    anarch_audio_play_sound(0, 255);
    anarch_audio_play_sound(3, 190);
    for (size_t offset = 0; offset < 32768;) {
        size_t count = 32768 - offset;
        if (count > chunk) count = chunk;
        anarch_audio_render(first, count);
        hash = hash_samples(hash, first, count);
        offset += count;
    }
    return hash;
}
static void test_music_disabled(void) {
    const uint8_t commands[] = {ANARCH_AUDIO_MUSIC_ON, ANARCH_AUDIO_MUSIC_NEXT,
        ANARCH_AUDIO_MUSIC_ON, ANARCH_AUDIO_MUSIC_OFF, 255};
    anarch_audio_reset();
    for (unsigned round = 0; round < 16; ++round)
        for (unsigned i = 0; i < sizeof(commands); ++i) {
            anarch_audio_set_music(commands[i]);
            anarch_audio_render(first, 8192);
            assert(silent(first, 8192));
        }
    /* Ignored music commands must not truncate or reset a live effect. */
    anarch_audio_reset();
    anarch_audio_play_sound(2, 255);
    anarch_audio_render(first, 4096);
    anarch_audio_reset();
    anarch_audio_play_sound(2, 255);
    for (unsigned i = 0; i < sizeof(commands); ++i)
        anarch_audio_set_music(commands[i]);
    anarch_audio_render(second, 4096);
    assert(memcmp(first, second, 4096 * sizeof(first[0])) == 0);
    assert(render_effects(160) == render_effects(113));
}
static void test_noop_calls(void) {
    anarch_audio_reset();
    anarch_audio_play_sound(0, 255);
    anarch_audio_render(first, 4096);
    anarch_audio_reset();
    anarch_audio_play_sound(0, 255);
    anarch_audio_render(NULL, 8192);
    anarch_audio_render(second, 0);
    anarch_audio_render(second, 4096);
    assert(memcmp(first, second, 4096 * sizeof(first[0])) == 0);
}
int main(void) {
    test_effects();
    test_voice_bound();
    test_pause_resume();
    test_music_disabled();
    test_noop_calls();
    puts("Anarch audio: six original effects, no background music, volume, voice bound, pause and chunk invariance passed.");
    return 0;
}
