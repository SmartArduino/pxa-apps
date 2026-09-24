#ifndef PXA_GAME_SFX_H
#define PXA_GAME_SFX_H

#include "pxa_audio.h"
#include "pxa_game_music.h"
#include "pxa_permission.h"

#define PXA_GAME_SFX_PERMISSION_REQUEST UINT32_C(0x50475801)
#define PXA_GAME_SFX_OPEN_REQUEST UINT32_C(0x50475802)
#define PXA_GAME_SFX_GRAPH_REQUEST UINT32_C(0x50475803)
#define PXA_GAME_SFX_SAMPLE_RATE 16000u
#define PXA_GAME_SFX_FRAME_SAMPLES 320u
#define PXA_GAME_SFX_FRAME_US 20000u
#define PXA_GAME_SFX_PREFILL_FRAMES 4u
#define PXA_GAME_SFX_MAX_FRAMES_PER_TICK 4u
#define PXA_GAME_SFX_EFFECT_VOICES 3u
#define PXA_GAME_SFX_SCORE_VOICES 19u
#define PXA_GAME_SFX_MUSIC_GAIN_Q10 4096u
#define PXA_GAME_SFX_DUCK_HIGH 384u
#define PXA_GAME_SFX_DUCK_LOW 640u
#define PXA_GAME_SFX_GRAPH_GAIN_DB_Q8 (-256)
#define PXA_GAME_SFX_LIMIT_THRESHOLD 24576u

enum {
    PXA_GAME_SFX_OFF = 0,
    PXA_GAME_SFX_WAIT_PERMISSION,
    PXA_GAME_SFX_WAIT_OPEN,
    PXA_GAME_SFX_WAIT_GRAPH,
    PXA_GAME_SFX_READY,
    PXA_GAME_SFX_UNAVAILABLE,
};

enum {
    PXA_GAME_SFX_TAP = 0,
    PXA_GAME_SFX_ACTION,
    PXA_GAME_SFX_SCORE,
    PXA_GAME_SFX_ALERT,
    PXA_GAME_SFX_FIRE,
    PXA_GAME_SFX_HIT,
    PXA_GAME_SFX_WIN,
    PXA_GAME_SFX_LOSE,
    PXA_GAME_SFX_PADDLE,
    PXA_GAME_SFX_CHARGE,
    PXA_GAME_SFX_EXPLODE,
    PXA_GAME_SFX_JUMP,
    PXA_GAME_SFX_PLACE,
    PXA_GAME_SFX_COLLECT,
    PXA_GAME_SFX_BITE,
    PXA_GAME_SFX_DEATH,
    PXA_GAME_SFX_BRICK_RED,
    PXA_GAME_SFX_BRICK_ORANGE,
    PXA_GAME_SFX_BRICK_YELLOW,
    PXA_GAME_SFX_BRICK_GREEN,
    PXA_GAME_SFX_BRICK_CYAN,
    PXA_GAME_SFX_BRICK_PURPLE,
    PXA_GAME_SFX_KIND_COUNT,
};

typedef struct {
    uint16_t phase;
    uint16_t samples_left;
    uint16_t samples_total;
    uint8_t kind;
} pxa_game_sfx_voice_t;

typedef struct {
    uint16_t phase;
    uint16_t note_step;
    uint32_t note_sample;
    uint32_t note_samples;
    uint16_t gain;
    uint8_t timbre;
} pxa_game_sfx_score_voice_t;

typedef struct {
    uint32_t permission_handle;
    uint32_t session_handle;
    const pxa_game_music_song_t* music_song;
    uint64_t music_tick_us;
    uint32_t music_remainder_us;
    uint16_t melody_phase;
    uint16_t bass_phase;
    uint16_t harmony_phase;
    uint16_t kick_phase;
    uint16_t melody_note_step;
    uint16_t melody_note_sample;
    uint16_t melody_note_samples;
    uint16_t melody_note_index;
    uint16_t score_position;
    uint16_t score_event_index;
    uint16_t music_tick_sample;
    uint16_t music_tick_samples;
    uint32_t noise_state;
    uint8_t state;
    uint8_t music_tick;
    uint8_t music_clock_started;
    uint8_t music_theme;
    pxa_game_sfx_voice_t effects[PXA_GAME_SFX_EFFECT_VOICES];
    pxa_game_sfx_score_voice_t score_voices[PXA_GAME_SFX_SCORE_VOICES];
    uint8_t payload[96];
    int16_t frame[PXA_GAME_SFX_FRAME_SAMPLES];
} pxa_game_sfx_t;

static inline int16_t pxa_game_sfx_triangle(uint16_t phase) {
    const uint16_t ramp = (phase & UINT16_C(0x8000)) != 0
                              ? (uint16_t)(UINT16_MAX - phase)
                              : phase;
    return (int16_t)(((int32_t)ramp - 16384) * 2);
}

static inline int16_t pxa_game_sfx_sine(uint16_t phase) {
    const int32_t x = (int16_t)phase;
    const uint32_t magnitude = (uint32_t)(x < 0 ? -(int64_t)x : x);
    int32_t sample = (int32_t)((int64_t)4 * x * (32768u - magnitude) / 32768);
    const uint32_t sample_magnitude =
        (uint32_t)(sample < 0 ? -(int64_t)sample : sample);

    /* Correct the parabolic approximation to closely follow a sine wave. */
    const int32_t correction =
        (int32_t)((int64_t)sample * sample_magnitude / 32768) - sample;
    sample += (int32_t)((int64_t)7373 * correction / 32768);
    if (sample > INT16_MAX)
        return INT16_MAX;
    if (sample < INT16_MIN)
        return INT16_MIN;
    return (int16_t)sample;
}

static inline int32_t pxa_game_sfx_scale_q10(int32_t sample,
                                              uint16_t gain_q10) {
    return (int32_t)((int64_t)sample * gain_q10 / 1024);
}

static inline int16_t pxa_game_sfx_noise(uint32_t* state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return (int16_t)(*state >> 16);
}

static inline int16_t pxa_game_sfx_square(uint16_t phase) {
    return (phase & UINT16_C(0x8000)) != 0 ? INT16_MAX : INT16_MIN;
}

static inline int16_t pxa_game_sfx_soft_limit(int32_t sample) {
    const uint32_t limit_headroom =
        (uint32_t)INT16_MAX - PXA_GAME_SFX_LIMIT_THRESHOLD;
    const int negative = sample < 0;
    uint32_t magnitude =
        (uint32_t)(negative ? -(int64_t)sample : (int64_t)sample);
    if (magnitude > PXA_GAME_SFX_LIMIT_THRESHOLD) {
        const uint32_t excess = magnitude - PXA_GAME_SFX_LIMIT_THRESHOLD;
        magnitude = PXA_GAME_SFX_LIMIT_THRESHOLD +
                    (uint32_t)((uint64_t)excess * limit_headroom /
                               (excess + limit_headroom));
    }
    return (int16_t)(negative ? -(int32_t)magnitude : (int32_t)magnitude);
}

static inline uint8_t pxa_game_sfx_priority(uint8_t kind) {
    static const uint8_t priorities[PXA_GAME_SFX_KIND_COUNT] = {
        1, 2, 3, 4, 2, 2, 5, 5, 1, 3, 5,
        2, 2, 3, 2, 5,
        1, 1, 1, 1, 1, 1,
    };
    return kind < PXA_GAME_SFX_KIND_COUNT ? priorities[kind] : 1;
}

static inline uint16_t pxa_game_sfx_envelope(uint32_t sample, uint32_t length,
                                              uint16_t peak) {
    const uint32_t attack_samples = 32u;
    const uint32_t release_samples = 128u;
    if (sample >= length)
        return 0;
    if (sample < attack_samples)
        return (uint16_t)((uint32_t)sample * peak / attack_samples);
    if (length - sample < release_samples)
        return (uint16_t)((uint32_t)(length - sample) * peak / release_samples);
    return peak;
}

static inline void pxa_game_sfx_load_note(pxa_game_sfx_t* sfx,
                                          const pxa_game_music_song_t* song) {
    const pxa_game_music_note_t* note = &song->melody[sfx->melody_note_index];
    const uint8_t eighths = note->eighths != 0 ? note->eighths : 1;
    sfx->melody_note_step = note->phase_step;
    sfx->melody_note_sample = 0;
    sfx->melody_note_samples = (uint16_t)(sfx->music_tick_samples * eighths);
    sfx->melody_phase = 0;
}

static inline void pxa_game_sfx_load_score_events(
    pxa_game_sfx_t* sfx, const pxa_game_music_song_t* song) {
    while (sfx->score_event_index < song->score_length) {
        const pxa_game_music_event_t* event =
            &song->score[sfx->score_event_index];
        pxa_game_sfx_score_voice_t* voice = NULL;
        if (event->start_eighth != sfx->score_position)
            break;
        ++sfx->score_event_index;
        for (uint8_t index = 0; index < PXA_GAME_SFX_SCORE_VOICES; ++index)
            if (sfx->score_voices[index].note_samples == 0) {
                voice = &sfx->score_voices[index];
                break;
            }
        if (voice == NULL)
            continue;
        voice->phase = 0;
        voice->note_step = event->phase_step;
        voice->note_sample = 0;
        voice->note_samples = (uint32_t)sfx->music_tick_samples *
                              (event->eighths != 0 ? event->eighths : 1);
        voice->gain = (uint16_t)event->gain * 10u;
        voice->timbre = event->timbre;
    }
}

static inline int16_t pxa_game_sfx_score_tone(
    pxa_game_sfx_t* sfx, const pxa_game_sfx_score_voice_t* voice) {
    const int32_t sine = pxa_game_sfx_sine(voice->phase);
    const int32_t triangle = pxa_game_sfx_triangle(voice->phase);
    switch (voice->timbre) {
        case PXA_MUSIC_TIMBRE_PIANO:
            return (int16_t)(sine * 3 / 4 +
                             (int32_t)pxa_game_sfx_sine(
                                 (uint16_t)(voice->phase * 2u)) / 4);
        case PXA_MUSIC_TIMBRE_BASS:
            return (int16_t)(sine * 7 / 8 + triangle / 8);
        case PXA_MUSIC_TIMBRE_SAX: {
            const int16_t vibrato =
                (int16_t)(pxa_game_sfx_sine((uint16_t)(voice->note_sample * 19u)) /
                          80);
            const uint16_t phase = (uint16_t)(voice->phase + vibrato);
            return (int16_t)((int32_t)pxa_game_sfx_sine(phase) * 3 / 5 +
                             (int32_t)pxa_game_sfx_triangle(phase) * 2 / 5);
        }
        case PXA_MUSIC_TIMBRE_STRINGS: {
            const uint16_t detuned_phase =
                (uint16_t)((uint32_t)voice->phase * 1016u / 1024u);
            return (int16_t)(sine * 3 / 8 +
                             (int32_t)pxa_game_sfx_sine(detuned_phase) * 3 / 8 +
                             triangle / 4);
        }
        case PXA_MUSIC_TIMBRE_PIZZICATO:
            return (int16_t)(triangle * 3 / 4 + sine / 4);
        case PXA_MUSIC_TIMBRE_SINE:
            return (int16_t)sine;
        case PXA_MUSIC_TIMBRE_LEAD:
            return (int16_t)(sine * 3 / 4 + triangle / 4);
        case PXA_MUSIC_TIMBRE_KICK:
            return (int16_t)(sine * 7 / 8 + triangle / 8);
        case PXA_MUSIC_TIMBRE_SNARE:
            return pxa_game_sfx_noise(&sfx->noise_state);
        case PXA_MUSIC_TIMBRE_CLICK:
            return (int16_t)((int32_t)pxa_game_sfx_noise(&sfx->noise_state) / 2 +
                             triangle / 2);
        default:
            return (int16_t)sine;
    }
}

static inline uint16_t pxa_game_sfx_score_gain(
    const pxa_game_sfx_score_voice_t* voice, uint32_t note_gate) {
    uint32_t effective_gate = note_gate;
    if (voice->timbre == PXA_MUSIC_TIMBRE_KICK && effective_gate > 1400u)
        effective_gate = 1400u;
    else if (voice->timbre == PXA_MUSIC_TIMBRE_SNARE && effective_gate > 800u)
        effective_gate = 800u;
    else if (voice->timbre == PXA_MUSIC_TIMBRE_CLICK && effective_gate > 320u)
        effective_gate = 320u;
    uint16_t gain = pxa_game_sfx_envelope(voice->note_sample, effective_gate,
                                          voice->gain);
    if (voice->timbre == PXA_MUSIC_TIMBRE_PIANO) {
        gain = (uint16_t)((uint64_t)gain *
                          (voice->note_samples * 3u - voice->note_sample * 2u) /
                          (voice->note_samples * 3u));
    } else if (voice->timbre == PXA_MUSIC_TIMBRE_PIZZICATO) {
        gain = (uint16_t)((uint64_t)gain *
                          (voice->note_samples - voice->note_sample) /
                          voice->note_samples);
    } else if (voice->timbre == PXA_MUSIC_TIMBRE_STRINGS &&
               voice->note_sample < 640u) {
        const uint16_t attack_gain =
            (uint16_t)((uint32_t)voice->gain * voice->note_sample / 640u);
        if (gain > attack_gain)
            gain = attack_gain;
    }
    return gain;
}

static inline void pxa_game_sfx_mix_frame(pxa_game_sfx_t* sfx) {
    static const uint16_t effect_steps[PXA_GAME_SFX_KIND_COUNT] = {
        1240, 1840, 2050, 620, 4100, 940, 2200, 1500, 1560, 700, 780,
        1050, 680, 2500, 760, 1700,
        1120, 1450, 1800, 2140, 2480, 2920,
    };
    static const int16_t effect_amplitudes[PXA_GAME_SFX_KIND_COUNT] = {
        3000, 5000, 6200, 7200, 4300, 5000, 7000, 7200, 5600, 4700, 8000,
        5600, 5900, 6200, 5700, 7600,
        3400, 3800, 4200, 4600, 5000, 5400,
    };
    const pxa_game_music_song_t* song = sfx->music_song != NULL
                                            ? sfx->music_song
                                            : pxa_game_music_song(sfx->music_theme);
    if (sfx->music_tick_samples == 0) {
        sfx->music_tick_samples = (uint16_t)(PXA_GAME_SFX_SAMPLE_RATE * 30u /
                                              song->bpm);
        if (song->score == NULL)
            pxa_game_sfx_load_note(sfx, song);
    }
    for (size_t index = 0; index < PXA_GAME_SFX_FRAME_SAMPLES; ++index) {
        if (song->score != NULL && sfx->music_tick_sample == 0)
            pxa_game_sfx_load_score_events(sfx, song);
        const uint8_t tick = sfx->music_tick;
        const uint16_t tick_sample = sfx->music_tick_sample;
        const uint16_t bass_step = song->bass[(tick / 2u) % 8u];
        const uint16_t beat_sample = (uint16_t)((tick % 2u) *
                                                 sfx->music_tick_samples + tick_sample);
        const uint16_t melody_gate = sfx->melody_note_samples >
                                             sfx->music_tick_samples / 8u
                                         ? (uint16_t)(sfx->melody_note_samples -
                                                      sfx->music_tick_samples / 8u)
                                         : sfx->melody_note_samples;
        const uint16_t melody_gain = pxa_game_sfx_envelope(
            sfx->melody_note_sample, melody_gate, 1200u);
        const uint16_t bass_gain = pxa_game_sfx_envelope(
            beat_sample, (uint16_t)(sfx->music_tick_samples * 3u / 2u), 680u);
        const uint16_t harmony_gain = (tick % 2u) != 0u
                                          ? pxa_game_sfx_envelope(
                                                tick_sample,
                                                (uint16_t)(sfx->music_tick_samples / 2u),
                                                300u)
                                          : 0u;
        int32_t mixed = 0;

        if (song->score != NULL) {
            for (uint8_t voice_index = 0; voice_index < PXA_GAME_SFX_SCORE_VOICES;
                 ++voice_index) {
                pxa_game_sfx_score_voice_t* voice = &sfx->score_voices[voice_index];
                if (voice->note_samples == 0)
                    continue;
                const uint32_t note_gate = voice->note_samples >
                                                   sfx->music_tick_samples / 8u
                                               ? voice->note_samples -
                                                     sfx->music_tick_samples / 8u
                                               : voice->note_samples;
                const uint16_t gain = pxa_game_sfx_score_gain(voice, note_gate);
                mixed += (int32_t)pxa_game_sfx_score_tone(sfx, voice) * gain /
                         32768;
                uint16_t note_step = voice->note_step;
                if (voice->timbre == PXA_MUSIC_TIMBRE_KICK &&
                    voice->note_sample < 1400u)
                    note_step = (uint16_t)(note_step +
                                           (1400u - voice->note_sample));
                voice->phase = (uint16_t)(voice->phase + note_step);
                if (++voice->note_sample >= voice->note_samples)
                    voice->note_samples = 0;
            }
        } else if (sfx->melody_note_step != PXA_MUSIC_REST) {
            mixed += (int32_t)pxa_game_sfx_sine(sfx->melody_phase) *
                     melody_gain / 32768;
        }
        mixed += (int32_t)pxa_game_sfx_sine(sfx->bass_phase) * bass_gain / 32768;
        mixed += (int32_t)pxa_game_sfx_sine(sfx->harmony_phase) * harmony_gain / 32768;
        if (song->score == NULL)
            sfx->melody_phase = (uint16_t)(sfx->melody_phase + sfx->melody_note_step);
        sfx->bass_phase = (uint16_t)(sfx->bass_phase + bass_step);
        sfx->harmony_phase = (uint16_t)(sfx->harmony_phase + bass_step * 3u);

        if ((song->kick_pattern & (UINT16_C(1) << tick)) != 0 && tick_sample < 480u) {
            const uint16_t kick_gain = pxa_game_sfx_envelope(tick_sample, 480u, 1150u);
            const uint16_t kick_step = (uint16_t)(380u + (480u - tick_sample) * 2u);
            mixed += (int32_t)pxa_game_sfx_sine(sfx->kick_phase) * kick_gain / 32768;
            sfx->kick_phase = (uint16_t)(sfx->kick_phase + kick_step);
        }
        if ((song->snare_pattern & (UINT16_C(1) << tick)) != 0 && tick_sample < 360u) {
            const uint16_t snare_gain = pxa_game_sfx_envelope(tick_sample, 360u, 780u);
            mixed += (int32_t)pxa_game_sfx_noise(&sfx->noise_state) * snare_gain / 32768;
        }
        if ((song->hat_pattern & (UINT16_C(1) << tick)) != 0 && tick_sample < 96u) {
            const uint16_t hat_gain = pxa_game_sfx_envelope(tick_sample, 96u, 300u);
            mixed += (int32_t)pxa_game_sfx_noise(&sfx->noise_state) * hat_gain / 32768;
        }
        uint16_t music_duck = 1024u;
        for (uint8_t voice_index = 0; voice_index < PXA_GAME_SFX_EFFECT_VOICES;
             ++voice_index)
            if (sfx->effects[voice_index].samples_left != 0) {
                const uint8_t priority = pxa_game_sfx_priority(
                    sfx->effects[voice_index].kind);
                if (priority >= 4u)
                    music_duck = PXA_GAME_SFX_DUCK_HIGH;
                else if (music_duck == 1024u)
                    music_duck = PXA_GAME_SFX_DUCK_LOW;
            }
        /* Leave headroom for effects and duck the score while they are active. */
        mixed = pxa_game_sfx_scale_q10(mixed, PXA_GAME_SFX_MUSIC_GAIN_Q10);
        mixed = pxa_game_sfx_scale_q10(mixed, music_duck);
        for (uint8_t voice_index = 0; voice_index < PXA_GAME_SFX_EFFECT_VOICES;
             ++voice_index) {
            pxa_game_sfx_voice_t* voice = &sfx->effects[voice_index];
            const uint8_t effect_kind = voice->kind < PXA_GAME_SFX_KIND_COUNT ?
                                            voice->kind : PXA_GAME_SFX_TAP;
            if (voice->samples_left != 0) {
                const uint16_t elapsed = (uint16_t)(voice->samples_total -
                                                    voice->samples_left);
                uint16_t envelope = 320;
                uint16_t step = effect_steps[effect_kind];
                const int32_t tone = (int32_t)pxa_game_sfx_triangle(voice->phase) *
                                     effect_amplitudes[effect_kind] / 32768;
                const int32_t pulse = (int32_t)pxa_game_sfx_square(voice->phase) *
                                      effect_amplitudes[effect_kind] / 32768;
                int32_t effect = tone;
                if (elapsed < 24)
                    envelope = (uint16_t)(elapsed * 13u);
                else if (voice->samples_left < 160)
                    envelope = (uint16_t)(voice->samples_left * 2u);

                if (effect_kind == PXA_GAME_SFX_SCORE ||
                    effect_kind == PXA_GAME_SFX_COLLECT ||
                    effect_kind == PXA_GAME_SFX_WIN) {
                    const uint16_t segment = (uint16_t)(elapsed / 320u);
                    step = (uint16_t)(step + segment *
                                      (effect_kind == PXA_GAME_SFX_COLLECT ? 620u : 410u));
                    effect += pulse / 5;
                } else if (effect_kind == PXA_GAME_SFX_JUMP) {
                    step = (uint16_t)(step + elapsed * 2u);
                    effect += pulse / 8;
                } else if (effect_kind == PXA_GAME_SFX_CHARGE) {
                    step = (uint16_t)(step + elapsed * 2u);
                    effect = tone * 3 / 4 + pulse / 4;
                } else if (effect_kind == PXA_GAME_SFX_FIRE) {
                    const uint16_t fall = (uint16_t)(elapsed * 9u);
                    step = fall < 3300u ? (uint16_t)(4100u - fall) : 800u;
                    effect = pulse * 3 / 4 +
                             (int32_t)pxa_game_sfx_noise(&sfx->noise_state) *
                                 effect_amplitudes[effect_kind] / 131072;
                    if (sfx->music_theme == PXA_GAME_SFX_THEME_PLANE) {
                        step = fall < 3700u ? (uint16_t)(4800u - fall) : 940u;
                        effect = pulse * 2 / 3 + tone / 4;
                    }
                } else if (effect_kind == PXA_GAME_SFX_HIT ||
                           effect_kind == PXA_GAME_SFX_PADDLE ||
                           effect_kind == PXA_GAME_SFX_PLACE ||
                           effect_kind == PXA_GAME_SFX_BITE) {
                    const int32_t noise =
                        (int32_t)pxa_game_sfx_noise(&sfx->noise_state) *
                        effect_amplitudes[effect_kind] / 32768;
                    effect = effect_kind == PXA_GAME_SFX_BITE
                                 ? pulse / 2 + noise / 2
                                 : tone * 3 / 4 + noise / 4;
                    if (effect_kind == PXA_GAME_SFX_PLACE)
                        step = (uint16_t)(680u + elapsed / 2u);
                    if (effect_kind == PXA_GAME_SFX_HIT &&
                        sfx->music_theme == PXA_GAME_SFX_THEME_PLANE) {
                        step = (uint16_t)(1800u + elapsed * 7u);
                        effect = tone / 2 + pulse / 3 + noise / 6;
                    }
                } else if (effect_kind == PXA_GAME_SFX_EXPLODE) {
                    const int32_t noise =
                        (int32_t)pxa_game_sfx_noise(&sfx->noise_state) *
                        effect_amplitudes[effect_kind] / 32768;
                    step = voice->samples_left > 2100u
                               ? 980u
                               : (uint16_t)(260u + voice->samples_left / 3u);
                    effect = noise * 3 / 4 + tone / 2;
                } else if (effect_kind == PXA_GAME_SFX_LOSE ||
                           effect_kind == PXA_GAME_SFX_DEATH) {
                    const int32_t noise =
                        (int32_t)pxa_game_sfx_noise(&sfx->noise_state) *
                        effect_amplitudes[effect_kind] / 32768;
                    step = (uint16_t)(300u + voice->samples_left / 2u);
                    effect = tone * 3 / 4 + noise / 3;
                } else if (effect_kind == PXA_GAME_SFX_ALERT) {
                    step = (elapsed / 240u) % 2u == 0u ? 720u : 1040u;
                    effect = pulse * 2 / 3;
                    if (sfx->music_theme == PXA_GAME_SFX_THEME_PLANE) {
                        step = (elapsed / 180u) % 3u == 0u ? 660u :
                               (elapsed / 180u) % 3u == 1u ? 940u : 1280u;
                        effect = pulse / 2 + tone / 4;
                    }
                }
                mixed += effect * envelope / 320;
                voice->phase = (uint16_t)(voice->phase + step);
                --voice->samples_left;
            }
        }
        sfx->frame[index] = pxa_game_sfx_soft_limit(mixed);
        if (song->score == NULL &&
            ++sfx->melody_note_sample >= sfx->melody_note_samples) {
            sfx->melody_note_index = (uint16_t)((sfx->melody_note_index + 1u) %
                                                song->melody_length);
            pxa_game_sfx_load_note(sfx, song);
        }
        if (++sfx->music_tick_sample >= sfx->music_tick_samples) {
            sfx->music_tick_sample = 0;
            sfx->music_tick = (uint8_t)((sfx->music_tick + 1u) % 16u);
            if (song->score != NULL &&
                ++sfx->score_position >= song->score_eighths) {
                sfx->score_position = 0;
                sfx->score_event_index = 0;
            }
        }
    }
}

static inline int pxa_game_sfx_submit_frame(pxa_game_sfx_t* sfx) {
    int32_t result;
    if (sfx == NULL || sfx->state != PXA_GAME_SFX_READY)
        return 0;
    pxa_game_sfx_mix_frame(sfx);
    result = pxa_audio_write_pcm(sfx->session_handle, (uint8_t*)sfx->frame,
                                 sizeof(sfx->frame));
    return result == (int32_t)sizeof(sfx->frame) || result == PXA_STATUS_WOULD_BLOCK;
}

static inline void pxa_game_sfx_start(pxa_game_sfx_t* sfx, uint8_t* packet,
                                      size_t packet_capacity) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    if (sfx == NULL || packet == NULL || sfx->state != PXA_GAME_SFX_OFF)
        return;
    sfx->state = PXA_GAME_SFX_WAIT_PERMISSION;
    if (!pxa_permission_acquire(PXA_GAME_SFX_PERMISSION_REQUEST, permission_name,
                                sizeof(permission_name) - 1, permission_scope,
                                sizeof(permission_scope) - 1, sfx->payload,
                                sizeof(sfx->payload), packet, packet_capacity))
        sfx->state = PXA_GAME_SFX_UNAVAILABLE;
}

static inline void pxa_game_sfx_reset_music(pxa_game_sfx_t* sfx) {
    if (sfx == NULL)
        return;
    sfx->melody_phase = 0;
    sfx->bass_phase = 0;
    sfx->harmony_phase = 0;
    sfx->kick_phase = 0;
    sfx->melody_note_step = 0;
    sfx->melody_note_sample = 0;
    sfx->melody_note_samples = 0;
    sfx->melody_note_index = 0;
    sfx->score_position = 0;
    sfx->score_event_index = 0;
    for (uint8_t index = 0; index < PXA_GAME_SFX_SCORE_VOICES; ++index)
        sfx->score_voices[index] = (pxa_game_sfx_score_voice_t){0};
    sfx->music_tick_sample = 0;
    sfx->music_tick_samples = 0;
    sfx->music_tick = 0;
}

static inline void pxa_game_sfx_set_theme(pxa_game_sfx_t* sfx, uint8_t theme) {
    if (sfx == NULL)
        return;
    sfx->music_song = NULL;
    sfx->music_theme = theme < PXA_GAME_SFX_THEME_COUNT ? theme :
                       PXA_GAME_SFX_THEME_BRICK;
    pxa_game_sfx_reset_music(sfx);
}

static inline void pxa_game_sfx_set_song(pxa_game_sfx_t* sfx,
                                         const pxa_game_music_song_t* song) {
    if (sfx == NULL || song == NULL || song->bpm == 0)
        return;
    sfx->music_song = song;
    pxa_game_sfx_reset_music(sfx);
}

static inline int pxa_game_sfx_handle_event(pxa_game_sfx_t* sfx,
                                            const pxa_event_t* event,
                                            uint8_t* packet,
                                            size_t packet_capacity) {
    if (sfx == NULL || event == NULL || packet == NULL)
        return 0;
    if (event->service == PXA_SERVICE_PERMISSION &&
        event->opcode == PXA_PERMISSION_ACQUIRE &&
        event->request_id == PXA_GAME_SFX_PERMISSION_REQUEST) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(event, &result) ||
            result.status != PXA_STATUS_OK) {
            sfx->state = PXA_GAME_SFX_UNAVAILABLE;
            return 1;
        }
        sfx->permission_handle = result.handle;
        sfx->state = PXA_GAME_SFX_WAIT_OPEN;
        if (!pxa_audio_open_media(PXA_GAME_SFX_OPEN_REQUEST, sfx->permission_handle,
                                  sfx->payload, sizeof(sfx->payload), packet,
                                  packet_capacity))
            sfx->state = PXA_GAME_SFX_UNAVAILABLE;
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_OPEN_SESSION &&
        event->request_id == PXA_GAME_SFX_OPEN_REQUEST) {
        pxa_audio_open_result_t result;
        if (!pxa_audio_parse_open(event, &result) || result.status != PXA_STATUS_OK ||
            result.sample_rate != PXA_GAME_SFX_SAMPLE_RATE || result.channels != 1 ||
            result.frame_ms != 20) {
            sfx->state = PXA_GAME_SFX_UNAVAILABLE;
            return 1;
        }
        sfx->session_handle = result.session_handle;
        sfx->state = PXA_GAME_SFX_WAIT_GRAPH;
        if (!pxa_audio_commit_speaker_graph(PXA_GAME_SFX_GRAPH_REQUEST,
                                            sfx->session_handle,
                                            PXA_GAME_SFX_GRAPH_GAIN_DB_Q8, 1500,
                                            256, 256, sfx->payload,
                                            sizeof(sfx->payload), packet,
                                            packet_capacity))
            sfx->state = PXA_GAME_SFX_UNAVAILABLE;
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_COMMIT_GRAPH &&
        event->request_id == PXA_GAME_SFX_GRAPH_REQUEST) {
        int32_t status;
        if (!pxa_audio_parse_status(event, PXA_AUDIO_COMMIT_GRAPH, &status) ||
            status != PXA_STATUS_OK)
            sfx->state = PXA_GAME_SFX_UNAVAILABLE;
        else {
            sfx->state = PXA_GAME_SFX_READY;
            for (uint8_t frame = 0; frame < PXA_GAME_SFX_PREFILL_FRAMES; ++frame)
                if (!pxa_game_sfx_submit_frame(sfx))
                    break;
        }
        return 1;
    }
    return 0;
}

static inline void pxa_game_sfx_play(pxa_game_sfx_t* sfx, uint8_t kind) {
    static const uint8_t effect_frames[PXA_GAME_SFX_KIND_COUNT] = {
        1, 2, 5, 5, 2, 2, 8, 8, 2, 7, 10,
        5, 4, 5, 4, 10,
        2, 2, 2, 2, 2, 2,
    };
    const uint8_t effect_kind = kind < PXA_GAME_SFX_KIND_COUNT ? kind :
                                PXA_GAME_SFX_TAP;
    const uint8_t effect_priority = pxa_game_sfx_priority(effect_kind);
    uint8_t voice_index = 0;
    uint8_t found_free_voice = 0;
    uint8_t lowest_priority = UINT8_MAX;
    uint16_t shortest_remaining = UINT16_MAX;
    if (sfx == NULL || sfx->state != PXA_GAME_SFX_READY)
        return;
    for (uint8_t index = 0; index < PXA_GAME_SFX_EFFECT_VOICES; ++index) {
        if (sfx->effects[index].samples_left == 0) {
            voice_index = index;
            found_free_voice = 1;
            break;
        }
        const uint8_t priority = pxa_game_sfx_priority(sfx->effects[index].kind);
        if (priority < lowest_priority ||
            (priority == lowest_priority &&
             sfx->effects[index].samples_left < shortest_remaining)) {
            lowest_priority = priority;
            shortest_remaining = sfx->effects[index].samples_left;
            voice_index = index;
        }
    }
    if (!found_free_voice && effect_priority < lowest_priority)
        return;
    if (sfx->noise_state == 0)
        sfx->noise_state = UINT32_C(0x9E3779B9);
    sfx->effects[voice_index].kind = effect_kind;
    sfx->effects[voice_index].phase = 0;
    sfx->effects[voice_index].samples_total = (uint16_t)(effect_frames[effect_kind] *
                                                          PXA_GAME_SFX_FRAME_SAMPLES);
    sfx->effects[voice_index].samples_left = sfx->effects[voice_index].samples_total;
}

static inline void pxa_game_sfx_tick(pxa_game_sfx_t* sfx,
                                     const pxa_event_t* event) {
    uint64_t timestamp_us;
    uint64_t elapsed_us;
    uint8_t frames = 0;
    if (sfx == NULL || event == NULL || sfx->state != PXA_GAME_SFX_READY ||
        event->service != PXA_SERVICE_CLOCK || event->opcode != PXA_CLOCK_TICK ||
        event->payload_length != 8)
        return;
    timestamp_us = pxa_read_u64(event->payload);
    if (!sfx->music_clock_started) {
        sfx->music_clock_started = 1;
        sfx->music_tick_us = timestamp_us;
        frames = 2;
    } else {
        elapsed_us = timestamp_us - sfx->music_tick_us;
        sfx->music_tick_us = timestamp_us;
        if (elapsed_us > 100000)
            elapsed_us = 100000;
        sfx->music_remainder_us += (uint32_t)elapsed_us;
        while (sfx->music_remainder_us >= PXA_GAME_SFX_FRAME_US &&
               frames < PXA_GAME_SFX_MAX_FRAMES_PER_TICK) {
            sfx->music_remainder_us -= PXA_GAME_SFX_FRAME_US;
            ++frames;
        }
    }
    while (frames-- != 0)
        if (!pxa_game_sfx_submit_frame(sfx))
            break;
}

#endif
