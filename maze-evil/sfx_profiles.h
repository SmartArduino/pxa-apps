#ifndef MAZE_EVIL_SFX_PROFILES_H
#define MAZE_EVIL_SFX_PROFILES_H

#include <stdint.h>

/* Generated from maze-evil's maze-break_sfx_profiles.hpp. */
enum { WAVE_SINE = 1, WAVE_SQUARE = 2, WAVE_TRIANGLE = 3, WAVE_NOISE = 4 };

typedef struct {
    uint8_t waveform;
    uint16_t frequency_hz;
    uint16_t duration_ms;
    uint16_t volume_per_mille;
    uint16_t attack_ms;
    uint16_t release_ms;
    uint16_t delay_ms;
} tone_spec_t;

static const tone_spec_t kShotgun[] = {
    {WAVE_NOISE, 0, 220, 236, 2, 160, 0},
    {WAVE_SINE, 110, 120, 292, 2, 90, 0},
    {WAVE_SINE, 55, 200, 236, 20, 140, 40},
};

static const tone_spec_t kEmptyClick[] = {
    {WAVE_NOISE, 0, 25, 50, 1, 18, 0},
    {WAVE_SQUARE, 600, 25, 110, 1, 18, 0},
};

static const tone_spec_t kImpAlert[] = {
    {WAVE_TRIANGLE, 150, 180, 218, 30, 40, 0},
    {WAVE_TRIANGLE, 120, 160, 218, 20, 40, 160},
    {WAVE_TRIANGLE, 95, 200, 204, 20, 150, 300},
};

static const tone_spec_t kImpFireball[] = {
    {WAVE_NOISE, 0, 260, 82, 10, 200, 0},
    {WAVE_SINE, 720, 90, 165, 5, 30, 0},
    {WAVE_SINE, 420, 90, 165, 5, 30, 80},
    {WAVE_SINE, 170, 160, 165, 5, 120, 160},
};

static const tone_spec_t kImpMelee[] = {
    {WAVE_SQUARE, 190, 90, 154, 3, 30, 0},
    {WAVE_SQUARE, 90, 120, 154, 3, 90, 80},
    {WAVE_NOISE, 0, 120, 69, 3, 90, 0},
};

static const tone_spec_t kImpPain[] = {
    {WAVE_TRIANGLE, 330, 90, 194, 4, 20, 0},
    {WAVE_TRIANGLE, 200, 100, 194, 4, 70, 80},
};

static const tone_spec_t kImpDeath[] = {
    {WAVE_TRIANGLE, 240, 180, 239, 5, 30, 0},
    {WAVE_TRIANGLE, 150, 200, 239, 5, 30, 170},
    {WAVE_TRIANGLE, 70, 360, 239, 5, 260, 360},
    {WAVE_NOISE, 0, 700, 48, 5, 500, 0},
};

static const tone_spec_t kFireballExplode[] = {
    {WAVE_NOISE, 0, 300, 168, 2, 240, 0},
    {WAVE_SINE, 160, 120, 259, 2, 80, 0},
    {WAVE_SINE, 60, 200, 233, 20, 160, 60},
};

static const tone_spec_t kPlayerPain[] = {
    {WAVE_SQUARE, 230, 110, 178, 3, 30, 0},
    {WAVE_SQUARE, 105, 130, 178, 3, 100, 100},
};

static const tone_spec_t kPickupHealth[] = {
    {WAVE_SQUARE, 660, 70, 120, 3, 20, 0},
    {WAVE_SQUARE, 880, 70, 120, 3, 20, 70},
    {WAVE_SQUARE, 1320, 180, 120, 3, 120, 140},
};

static const tone_spec_t kPickupAmmo[] = {
    {WAVE_SQUARE, 440, 60, 120, 3, 20, 0},
    {WAVE_SQUARE, 660, 150, 120, 3, 100, 60},
};

static const tone_spec_t kDoorOpen[] = {
    {WAVE_SQUARE, 52, 300, 88, 40, 40, 0},
    {WAVE_SQUARE, 72, 300, 88, 40, 120, 280},
    {WAVE_TRIANGLE, 104, 300, 64, 40, 40, 0},
    {WAVE_TRIANGLE, 144, 300, 64, 40, 120, 280},
    {WAVE_NOISE, 0, 580, 32, 40, 200, 0},
};

static const tone_spec_t kDoorClose[] = {
    {WAVE_SQUARE, 72, 300, 51, 40, 40, 0},
    {WAVE_SQUARE, 50, 260, 51, 40, 60, 280},
    {WAVE_NOISE, 0, 540, 18, 40, 100, 0},
    {WAVE_SINE, 60, 120, 148, 2, 90, 550},
};

static const tone_spec_t kExitSealed[] = {
    {WAVE_SQUARE, 200, 160, 112, 3, 40, 0},
    {WAVE_SQUARE, 150, 320, 112, 3, 200, 180},
};

static const tone_spec_t kWin[] = {
    {WAVE_SQUARE, 330, 130, 104, 3, 30, 0},
    {WAVE_SQUARE, 392, 130, 104, 3, 30, 130},
    {WAVE_SQUARE, 494, 130, 104, 3, 30, 260},
    {WAVE_SQUARE, 659, 130, 104, 3, 30, 390},
    {WAVE_SQUARE, 659, 600, 104, 10, 400, 520},
    {WAVE_TRIANGLE, 330, 600, 160, 10, 400, 520},
};

static const tone_spec_t kDie[] = {
    {WAVE_TRIANGLE, 300, 300, 188, 10, 40, 0},
    {WAVE_TRIANGLE, 180, 350, 188, 10, 40, 280},
    {WAVE_TRIANGLE, 55, 700, 188, 10, 500, 600},
    {WAVE_SINE, 150, 400, 163, 10, 100, 0},
    {WAVE_SINE, 30, 900, 163, 10, 600, 400},
};

#endif
