#include "jump3d_audio.h"

#include "jump3d_audio_data.h"

/* The bank order and the public clip enum are authored separately, so pin them
 * together at compile time. */
#define J3_AUDIO_PIN(name, index, samples, looping) \
    _Static_assert(J3_CLIP_##name == (index), "clip order drifted: " #name);
J3_AUDIO_BANK(J3_AUDIO_PIN)
#undef J3_AUDIO_PIN

/* Standard IMA ADPCM step table (index 0..88). */
static const int16_t k_step_table[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
    19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
    5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

static const int8_t k_index_table[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

/* Sample rate conversion: the stored clips are J3_AUDIO_CLIP_RATE_HZ and the
 * session runs at J3_AUDIO_SAMPLE_RATE. */
#define J3_AUDIO_STEP_Q16 \
    ((uint32_t)(((uint64_t)J3_AUDIO_CLIP_RATE_HZ << 16) / \
                J3_AUDIO_SAMPLE_RATE))

/* Gain ramp per output sample: 4096 / 8 = 512 samples, about 32 ms from silence
 * to full scale at 16 kHz. Ramping per sample (rather than per 20 ms frame) is
 * what removes the click when the charge sound is released. */
#define J3_AUDIO_RAMP_STEP 8u

static int16_t j3_audio_step(j3_audio_voice_t *voice, uint8_t nibble) {
    const int32_t step = k_step_table[voice->step_index];
    int32_t delta = step >> 3;
    if ((nibble & 4u) != 0u) delta += step;
    if ((nibble & 2u) != 0u) delta += step >> 1;
    if ((nibble & 1u) != 0u) delta += step >> 2;
    if ((nibble & 8u) != 0u) delta = -delta;
    voice->predictor += delta;
    if (voice->predictor > 32767) voice->predictor = 32767;
    if (voice->predictor < -32768) voice->predictor = -32768;
    /* The index table can subtract one at the bottom of the range; clamping
     * through a signed value keeps quiet passages quiet instead of wrapping to
     * the coarsest step, which is what made them sound raspy. */
    {
        int32_t next = (int32_t)voice->step_index +
                       k_index_table[nibble & 7u];
        if (next < 0) next = 0;
        if (next > 88) next = 88;
        voice->step_index = (uint16_t)next;
    }
    return (int16_t)voice->predictor;
}

static uint8_t j3_audio_nibble(const j3_audio_voice_t *voice) {
    const uint32_t word = voice->clip->words[voice->position >> 3];
    const uint8_t shift = (uint8_t)((voice->position & 7u) * 4u);
    return (uint8_t)((word >> shift) & 0xFu);
}

void j3_audio_stop(j3_audio_t *audio, uint8_t channel) {
    if (audio == NULL || channel >= J3_CHANNEL_COUNT) return;
    /* Ramp to silence instead of cutting, then release the voice. */
    audio->voices[channel].requested = 0;
    audio->voices[channel].target_gain_q12 = 0;
}

void j3_audio_stop_all(j3_audio_t *audio) {
    uint8_t channel;
    for (channel = 0; channel < J3_CHANNEL_COUNT; ++channel)
        j3_audio_stop(audio, channel);
}

void j3_audio_play(j3_audio_t *audio, uint8_t channel, uint8_t clip,
                   uint16_t gain_q12, uint8_t loop) {
    j3_audio_voice_t *voice;
    if (audio == NULL || channel >= J3_CHANNEL_COUNT || clip >= J3_AUDIO_CLIP_COUNT)
        return;
    voice = &audio->voices[channel];
    voice->clip = &j3_audio_bank[clip];
    voice->position = 0;
    voice->phase = 0;
    voice->step_q16 = J3_AUDIO_STEP_Q16;
    voice->predictor = 0;
    voice->step_index = 0;
    voice->previous = 0;
    voice->current = 0;
    /* A fresh voice starts from silence so percussive rests do not click. */
    if (voice->active == 0u) voice->gain_q12 = 0;
    voice->target_gain_q12 = gain_q12 > J3_GAIN_FULL ? J3_GAIN_FULL : gain_q12;
    voice->loop = loop != 0u ? 1u : 0u;
    voice->finished = 0;
    voice->requested = 1;
    voice->active = voice->clip->samples == 0u ? 0u : 1u;
}

uint8_t j3_audio_channel_clip(const j3_audio_t *audio, uint8_t channel) {
    if (audio == NULL || channel >= J3_CHANNEL_COUNT ||
        audio->voices[channel].clip == NULL)
        return (uint8_t)J3_AUDIO_CLIP_COUNT;
    return (uint8_t)(audio->voices[channel].clip - j3_audio_bank);
}

int j3_audio_channel_active(const j3_audio_t *audio, uint8_t channel) {
    if (audio == NULL || channel >= J3_CHANNEL_COUNT) return 0;
    return audio->voices[channel].active != 0u;
}

/* Decodes the next source sample and advances the clip position. */
static void j3_audio_advance(j3_audio_voice_t *voice) {
    const uint8_t nibble = j3_audio_nibble(voice);
    voice->previous = voice->current;
    voice->current = j3_audio_step(voice, nibble);
    if (++voice->position >= voice->clip->samples) {
        if (voice->loop != 0u) {
            voice->position = 0;
            voice->predictor = 0;
            voice->step_index = 0;
        } else {
            voice->finished = 1;
        }
    }
}

/* Smooth saturating limiter: transparent below the knee (about -1.3 dBFS) and
 * asymptotic to full scale above it, so only genuine overlaps are shaped. */
static int32_t j3_audio_limit(int32_t value) {
    const int32_t knee = 28800;
    const int32_t range = 32767 - knee;
    int32_t magnitude = value < 0 ? -value : value;
    if (magnitude > knee) {
        magnitude = knee + range - (range * range) / (magnitude - knee + range);
        if (magnitude > 32767) magnitude = 32767;
    }
    return value < 0 ? -magnitude : magnitude;
}

static void j3_audio_mix(j3_audio_t *audio) {
    int any_effect = 0;
    uint8_t channel;
    int index;
    for (channel = J3_CHANNEL_LAND; channel <= J3_CHANNEL_BONUS; ++channel)
        if (audio->voices[channel].active != 0u) any_effect = 1;
    for (channel = 0; channel < J3_CHANNEL_COUNT; ++channel) {
        j3_audio_voice_t *voice = &audio->voices[channel];
        if (channel == J3_CHANNEL_BGM && voice->requested != 0u)
            voice->target_gain_q12 =
                any_effect != 0 ? J3_GAIN_BGM_DUCKED : J3_GAIN_BGM;
        if (voice->active != 0u && voice->finished != 0u) {
            voice->requested = 0;
            voice->target_gain_q12 = 0;
        }
    }
    for (index = 0; index < (int)J3_AUDIO_FRAME_SAMPLES; ++index) {
        int32_t mix = 0;
        for (channel = 0; channel < J3_CHANNEL_COUNT; ++channel) {
            j3_audio_voice_t *voice = &audio->voices[channel];
            int32_t sample;
            if (voice->active == 0u) continue;
            /* Per-sample gain ramp: no steps, so no clicks. */
            if (voice->gain_q12 < voice->target_gain_q12) {
                const uint16_t next =
                    (uint16_t)(voice->gain_q12 + J3_AUDIO_RAMP_STEP);
                voice->gain_q12 = next > voice->target_gain_q12
                                      ? voice->target_gain_q12
                                      : next;
            } else if (voice->gain_q12 > voice->target_gain_q12) {
                const uint16_t difference =
                    voice->gain_q12 - voice->target_gain_q12;
                voice->gain_q12 =
                    difference > J3_AUDIO_RAMP_STEP
                        ? (uint16_t)(voice->gain_q12 - J3_AUDIO_RAMP_STEP)
                        : voice->target_gain_q12;
            }
            if (voice->gain_q12 == 0u) {
                if (voice->requested == 0u) voice->active = 0u;
                continue;
            }
            /* Linear interpolation between the two nearest source samples. */
            voice->phase += voice->step_q16;
            while (voice->phase >= 65536u) {
                voice->phase -= 65536u;
                if (voice->finished == 0u || voice->loop != 0u)
                    j3_audio_advance(voice);
            }
            sample = (int32_t)voice->previous +
                     (((int32_t)(voice->current - voice->previous) *
                       (int32_t)(voice->phase >> 8)) >> 8);
            mix += (sample * (int32_t)voice->gain_q12) >> 12;
        }
        audio->frame[index] = (int16_t)j3_audio_limit(mix);
    }
}

static int j3_audio_submit(j3_audio_t *audio) {
    int32_t result;
    if (audio->state != J3_AUDIO_READY) return 0;
    j3_audio_mix(audio);
    result = pxa_audio_write_pcm(audio->session_handle,
                                 (uint8_t *)audio->frame,
                                 sizeof(audio->frame));
    return result == (int32_t)sizeof(audio->frame);
}

void j3_audio_start(j3_audio_t *audio, uint8_t *packet, uint32_t capacity) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    if (audio == NULL || packet == NULL ||
        audio->state != J3_AUDIO_OFF)
        return;
    audio->state = J3_AUDIO_WAIT_PERMISSION;
    if (!pxa_permission_acquire(J3_AUDIO_PERMISSION_REQUEST, permission_name,
                                sizeof(permission_name) - 1u, permission_scope,
                                sizeof(permission_scope) - 1u, audio->payload,
                                sizeof(audio->payload), packet, capacity))
        audio->state = J3_AUDIO_UNAVAILABLE;
}

int j3_audio_handle_event(j3_audio_t *audio, const pxa_event_t *event,
                          uint8_t *packet, uint32_t capacity) {
    if (audio == NULL || event == NULL || packet == NULL) return 0;
    if (event->service == PXA_SERVICE_PERMISSION &&
        event->opcode == PXA_PERMISSION_ACQUIRE &&
        event->request_id == J3_AUDIO_PERMISSION_REQUEST) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(event, &result) ||
            result.status != PXA_STATUS_OK) {
            audio->state = J3_AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->permission_handle = result.handle;
        audio->state = J3_AUDIO_WAIT_OPEN;
        if (!pxa_audio_open_media(J3_AUDIO_OPEN_REQUEST,
                                  audio->permission_handle, audio->payload,
                                  sizeof(audio->payload), packet, capacity))
            audio->state = J3_AUDIO_UNAVAILABLE;
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_OPEN_SESSION &&
        event->request_id == J3_AUDIO_OPEN_REQUEST) {
        pxa_audio_open_result_t result;
        if (!pxa_audio_parse_open(event, &result) ||
            result.status != PXA_STATUS_OK ||
            result.sample_rate != J3_AUDIO_SAMPLE_RATE || result.channels != 1 ||
            result.frame_ms != 20) {
            audio->state = J3_AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->session_handle = result.session_handle;
        audio->state = J3_AUDIO_WAIT_GRAPH;
        if (!pxa_audio_commit_speaker_graph(J3_AUDIO_GRAPH_REQUEST,
                                            audio->session_handle, -256, 1500,
                                            256, 256, audio->payload,
                                            sizeof(audio->payload), packet,
                                            capacity))
            audio->state = J3_AUDIO_UNAVAILABLE;
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_COMMIT_GRAPH &&
        event->request_id == J3_AUDIO_GRAPH_REQUEST) {
        int32_t status;
        if (!pxa_audio_parse_status(event, PXA_AUDIO_COMMIT_GRAPH, &status) ||
            status != PXA_STATUS_OK) {
            audio->state = J3_AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->state = J3_AUDIO_READY;
        {
            uint8_t frame;
            for (frame = 0; frame < J3_AUDIO_PREFILL_FRAMES; ++frame)
                if (!j3_audio_submit(audio)) break;
        }
        return 1;
    }
    return 0;
}

void j3_audio_tick(j3_audio_t *audio, const pxa_event_t *event) {
    uint64_t timestamp_us;
    uint64_t elapsed_us;
    uint8_t frames = 0;
    if (audio == NULL || event == NULL || audio->state != J3_AUDIO_READY ||
        event->service != PXA_SERVICE_CLOCK || event->opcode != PXA_CLOCK_TICK ||
        event->payload_length != 8)
        return;
    timestamp_us = pxa_read_u64(event->payload);
    if (audio->tick_us == 0) {
        audio->tick_us = timestamp_us;
        frames = 2;
    } else {
        elapsed_us = timestamp_us - audio->tick_us;
        audio->tick_us = timestamp_us;
        if (elapsed_us > 100000u) elapsed_us = 100000u;
        audio->remainder_us += (uint32_t)elapsed_us;
        while (audio->remainder_us >= J3_AUDIO_FRAME_US &&
               frames < J3_AUDIO_MAX_FRAMES_PER_TICK) {
            audio->remainder_us -= J3_AUDIO_FRAME_US;
            ++frames;
        }
    }
    while (frames-- != 0)
        if (!j3_audio_submit(audio)) break;
}
