#include "audio.h"

#include "pxa_audio.h"
#include "pxa_permission.h"
#include "rc_math.h"
#include "sfx_profiles.h"
#include "world.h"

#define AUDIO_PERMISSION_REQUEST UINT32_C(0x4D5A0001)
#define AUDIO_OPEN_REQUEST UINT32_C(0x4D5A0002)
#define AUDIO_GRAPH_REQUEST UINT32_C(0x4D5A0003)
#define AUDIO_SAMPLE_RATE 16000u
#define AUDIO_FRAME_US 20000u
#define AUDIO_QUEUE_CAPACITY 12u
/* Keep a cushion in the provider's 12-frame queue so a long render callback
 * cannot drain it; the 33 ms game tick tops the queue back up. */
#define AUDIO_TARGET_FRAMES 6u
#define AUDIO_MAX_FRAMES_PER_TICK 6u
#define AUDIO_QUERY_INTERVAL_TICKS 15u
#define AUDIO_GRAPH_GAIN_DB_Q8 (-256)

static void zero_bytes(uint8_t *data, uint32_t size) {
    uint32_t index;
    for (index = 0; index < size; ++index) {
        data[index] = 0;
    }
}

static int16_t waveform_sample(audio_voice_t *voice, const int16_t *sine) {
    switch (voice->waveform) {
        case WAVE_SINE:
            return sine[voice->phase >> 24u];
        case WAVE_SQUARE:
            return (voice->phase & 0x80000000u) == 0u ? 32767 : -32768;
        case WAVE_TRIANGLE: {
            const uint32_t position = voice->phase >> 16u;
            return position < 32768u
                       ? (int16_t)((int32_t)(position * 2u) - 32768)
                       : (int16_t)(98303 - (int32_t)(position * 2u));
        }
        case WAVE_NOISE:
            voice->noise = voice->noise * 1664525u + 1013904223u;
            return (int16_t)(voice->noise >> 16u);
        default:
            return 0;
    }
}

static int16_t soft_limit(int32_t sample) {
    const uint32_t threshold = 24576u;
    const uint32_t headroom = (uint32_t)32767 - threshold;
    const int negative = sample < 0;
    uint32_t magnitude =
        (uint32_t)(negative ? -(int64_t)sample : (int64_t)sample);
    if (magnitude > threshold) {
        const uint32_t excess = magnitude - threshold;
        magnitude = threshold + (uint32_t)((uint64_t)excess * headroom /
                                           (excess + headroom));
    }
    return (int16_t)(negative ? -(int32_t)magnitude : (int32_t)magnitude);
}

static void start_voice(game_audio_t *audio, uint8_t waveform,
                        uint16_t frequency_hz, uint16_t duration_ms,
                        uint16_t volume_per_mille, uint16_t attack_ms,
                        uint16_t release_ms) {
    int index;
    audio_voice_t *voice = 0;
    for (index = 0; index < AUDIO_MAX_VOICES; ++index) {
        if (!audio->voices[index].active) {
            voice = &audio->voices[index];
            break;
        }
    }
    if (voice == 0) {
        return;
    }
    voice->waveform = waveform;
    voice->phase = 0;
    voice->phase_step =
        (uint32_t)((uint64_t)frequency_hz * (1ULL << 32u) /
                   (uint64_t)AUDIO_SAMPLE_RATE);
    voice->total_frames = (uint32_t)duration_ms * AUDIO_SAMPLE_RATE / 1000u;
    voice->remaining_frames = voice->total_frames;
    voice->attack_frames = (uint32_t)attack_ms * AUDIO_SAMPLE_RATE / 1000u;
    voice->release_frames = (uint32_t)release_ms * AUDIO_SAMPLE_RATE / 1000u;
    voice->volume_per_mille = volume_per_mille;
    voice->noise = 0x51a9e21du ^ (uint32_t)frequency_hz ^ duration_ms;
    voice->active = 1;
}

static int16_t voice_sample(audio_voice_t *voice, const int16_t *sine) {
    const uint32_t played_frames =
        voice->total_frames - voice->remaining_frames;
    uint32_t gain = voice->volume_per_mille;
    int32_t sample;
    if (voice->attack_frames != 0u && played_frames < voice->attack_frames) {
        gain = gain * played_frames / voice->attack_frames;
    }
    if (voice->release_frames != 0u &&
        voice->remaining_frames < voice->release_frames) {
        const uint32_t release_gain = voice->volume_per_mille *
                                      voice->remaining_frames /
                                      voice->release_frames;
        if (release_gain < gain) {
            gain = release_gain;
        }
    }
    sample = (int32_t)waveform_sample(voice, sine) * (int32_t)gain / 1000;
    voice->phase += voice->phase_step;
    --voice->remaining_frames;
    if (voice->remaining_frames == 0u) {
        voice->active = 0;
    }
    return (int16_t)sample;
}

static void mix_frame(game_audio_t *audio) {
    int index;
    for (index = 0; index < AUDIO_FRAME_SAMPLES; ++index) {
        int32_t mixed = 0;
        int voice_index;
        int slot;
        /* Fire delayed tones at the exact sample so layered SFX keep their
         * rhythm instead of quantising to the frame rate. */
        for (slot = 0; slot < AUDIO_MAX_SCHEDULED; ++slot) {
            audio_scheduled_t *scheduled = &audio->scheduled[slot];
            if (scheduled->active && scheduled->delay_frames == 0u) {
                scheduled->active = 0;
                start_voice(audio, scheduled->waveform,
                            scheduled->frequency_hz, scheduled->duration_ms,
                            scheduled->volume_per_mille,
                            scheduled->attack_ms, scheduled->release_ms);
            }
        }
        for (voice_index = 0; voice_index < AUDIO_MAX_VOICES; ++voice_index) {
            audio_voice_t *voice = &audio->voices[voice_index];
            if (voice->active) {
                mixed += voice_sample(voice, audio->sine);
            }
        }
        audio->frame[index] = soft_limit(mixed);
        for (slot = 0; slot < AUDIO_MAX_SCHEDULED; ++slot) {
            audio_scheduled_t *scheduled = &audio->scheduled[slot];
            if (scheduled->active && scheduled->delay_frames != 0u) {
                --scheduled->delay_frames;
            }
        }
    }
}

static int32_t submit_frame(game_audio_t *audio) {
    if (audio->state != AUDIO_READY) {
        return PXA_STATUS_BAD_STATE;
    }
    mix_frame(audio);
    return pxa_audio_write_pcm(audio->session_handle, (uint8_t *)audio->frame,
                               sizeof(audio->frame));
}

static const tone_spec_t *profile_for(uint8_t sound_id, uint32_t *count) {
    switch (sound_id) {
        case SND_SHOTGUN:
            *count = sizeof(kShotgun) / sizeof(kShotgun[0]);
            return kShotgun;
        case SND_EMPTY_CLICK:
            *count = sizeof(kEmptyClick) / sizeof(kEmptyClick[0]);
            return kEmptyClick;
        case SND_IMP_ALERT:
            *count = sizeof(kImpAlert) / sizeof(kImpAlert[0]);
            return kImpAlert;
        case SND_IMP_FIREBALL:
            *count = sizeof(kImpFireball) / sizeof(kImpFireball[0]);
            return kImpFireball;
        case SND_IMP_MELEE:
            *count = sizeof(kImpMelee) / sizeof(kImpMelee[0]);
            return kImpMelee;
        case SND_IMP_PAIN:
            *count = sizeof(kImpPain) / sizeof(kImpPain[0]);
            return kImpPain;
        case SND_IMP_DEATH:
            *count = sizeof(kImpDeath) / sizeof(kImpDeath[0]);
            return kImpDeath;
        case SND_FIREBALL_EXPLODE:
            *count = sizeof(kFireballExplode) / sizeof(kFireballExplode[0]);
            return kFireballExplode;
        case SND_PLAYER_PAIN:
            *count = sizeof(kPlayerPain) / sizeof(kPlayerPain[0]);
            return kPlayerPain;
        case SND_PICKUP_HEALTH:
            *count = sizeof(kPickupHealth) / sizeof(kPickupHealth[0]);
            return kPickupHealth;
        case SND_PICKUP_AMMO:
            *count = sizeof(kPickupAmmo) / sizeof(kPickupAmmo[0]);
            return kPickupAmmo;
        case SND_DOOR_OPEN:
            *count = sizeof(kDoorOpen) / sizeof(kDoorOpen[0]);
            return kDoorOpen;
        case SND_DOOR_CLOSE:
            *count = sizeof(kDoorClose) / sizeof(kDoorClose[0]);
            return kDoorClose;
        case SND_EXIT_SEALED:
            *count = sizeof(kExitSealed) / sizeof(kExitSealed[0]);
            return kExitSealed;
        case SND_WIN:
            *count = sizeof(kWin) / sizeof(kWin[0]);
            return kWin;
        case SND_DIE:
            *count = sizeof(kDie) / sizeof(kDie[0]);
            return kDie;
        default:
            *count = 0;
            return 0;
    }
}

static int16_t host_gain_db_q8(uint16_t volume_per_mille) {
    static const uint16_t thresholds[] = {
        708, 501, 355, 251, 178, 126, 89, 63, 45, 32, 22, 16, 11, 8,
    };
    uint32_t index;
    if (volume_per_mille >= 1000) return 0;
    for (index = 0; index < sizeof(thresholds) / sizeof(thresholds[0]);
         ++index) {
        if (volume_per_mille >= thresholds[index]) {
            return (int16_t)(-(int32_t)(index + 1u) * 3 * 256);
        }
    }
    return (int16_t)(-48 * 256);
}

static int32_t submit_host_tone(game_audio_t *audio, const tone_spec_t *spec,
                                uint8_t gain) {
    const uint16_t volume =
        (uint16_t)(((uint32_t)spec->volume_per_mille * gain + 127u) / 255u);
    const uint16_t frequency = spec->frequency_hz < 40u
                                   ? 40u
                                   : spec->frequency_hz;
    if (volume == 0) return PXA_STATUS_OK;
    return pxa_audio_play_tone_enveloped(
        audio->session_handle, (uint8_t)(spec->waveform - 1u), frequency,
        spec->duration_ms, host_gain_db_q8(volume), spec->attack_ms,
        spec->release_ms, spec->delay_ms);
}

static int audio_has_pcm_work(const game_audio_t *audio) {
    int index;
    for (index = 0; index < AUDIO_MAX_VOICES; ++index) {
        if (audio->voices[index].active) return 1;
    }
    for (index = 0; index < AUDIO_MAX_SCHEDULED; ++index) {
        if (audio->scheduled[index].active) return 1;
    }
    return 0;
}

void audio_init(game_audio_t *audio) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    uint8_t packet[64];
    int index;
    zero_bytes((uint8_t *)audio, (uint32_t)sizeof(*audio));
    for (index = 0; index < AUDIO_SINE_ENTRIES; ++index) {
        audio->sine[index] =
            (int16_t)(rc_sin(2.0F * RC_PI * (float)index /
                             (float)AUDIO_SINE_ENTRIES) *
                      32767.0F);
    }
    audio->state = AUDIO_WAIT_PERMISSION;
    if (!pxa_permission_acquire(
            AUDIO_PERMISSION_REQUEST, permission_name,
            sizeof(permission_name) - 1u, permission_scope,
            sizeof(permission_scope) - 1u, audio->payload,
            sizeof(audio->payload), packet, sizeof(packet))) {
        audio->state = AUDIO_UNAVAILABLE;
    }
}

void audio_play(game_audio_t *audio, uint8_t sound_id, uint8_t gain) {
    uint32_t count = 0;
    const tone_spec_t *tones = profile_for(sound_id, &count);
    uint32_t index;
    if (tones == 0 || gain < 8u) {
        return;
    }
    if (audio->state == AUDIO_READY &&
        audio->tone_mode != AUDIO_TONE_MODE_PCM_FALLBACK) {
        int host_supported = 0;
        for (index = 0; index < count; ++index) {
            const int32_t status = submit_host_tone(audio, &tones[index], gain);
            if (status == PXA_STATUS_UNSUPPORTED) {
                if (!host_supported) {
                    audio->tone_mode = AUDIO_TONE_MODE_PCM_FALLBACK;
                    break;
                }
                return;
            }
            host_supported = 1;
        }
        if (audio->tone_mode != AUDIO_TONE_MODE_PCM_FALLBACK) {
            audio->tone_mode = AUDIO_TONE_MODE_HOST;
            return;
        }
    } else if (audio->state != AUDIO_READY) {
        audio->tone_mode = AUDIO_TONE_MODE_PCM_FALLBACK;
    }
    for (index = 0; index < count; ++index) {
        const tone_spec_t *spec = &tones[index];
        const uint16_t volume = (uint16_t)(((uint32_t)spec->volume_per_mille *
                                            gain + 127u) /
                                           255u);
        if (spec->delay_ms == 0u) {
            start_voice(audio, spec->waveform, spec->frequency_hz,
                        spec->duration_ms, volume, spec->attack_ms,
                        spec->release_ms);
            continue;
        }
        {
            int slot;
            for (slot = 0; slot < AUDIO_MAX_SCHEDULED; ++slot) {
                audio_scheduled_t *scheduled = &audio->scheduled[slot];
                if (!scheduled->active) {
                    scheduled->waveform = spec->waveform;
                    scheduled->frequency_hz = spec->frequency_hz;
                    scheduled->duration_ms = spec->duration_ms;
                    scheduled->volume_per_mille = volume;
                    scheduled->attack_ms = spec->attack_ms;
                    scheduled->release_ms = spec->release_ms;
                    scheduled->delay_frames =
                        (uint32_t)spec->delay_ms * AUDIO_SAMPLE_RATE / 1000u;
                    scheduled->active = 1;
                    break;
                }
            }
        }
    }
}

void audio_tick(game_audio_t *audio, const pxa_event_t *event) {
    uint64_t timestamp_us;
    uint64_t elapsed_us;
    uint32_t consumed;
    uint32_t to_write;
    if (audio->state != AUDIO_READY ||
        audio->tone_mode != AUDIO_TONE_MODE_PCM_FALLBACK || event == 0 ||
        event->service != PXA_SERVICE_CLOCK ||
        event->opcode != PXA_CLOCK_TICK || event->payload_length != 8) {
        return;
    }
    timestamp_us = pxa_read_u64(event->payload);
    if (!audio->clock_started) {
        audio->clock_started = 1;
        audio->tick_us = timestamp_us;
    } else {
        elapsed_us = timestamp_us - audio->tick_us;
        audio->tick_us = timestamp_us;
        if (elapsed_us > 100000u) {
            elapsed_us = 100000u;
        }
        consumed = (uint32_t)(elapsed_us / AUDIO_FRAME_US);
        if (consumed >= audio->queued_frames) {
            audio->queued_frames = 0;
        } else {
            audio->queued_frames -= consumed;
        }
    }

    if (!audio_has_pcm_work(audio)) return;

    /* Top the provider queue back up to the target cushion. */
    if (audio->queued_frames < AUDIO_TARGET_FRAMES) {
        to_write = AUDIO_TARGET_FRAMES - audio->queued_frames;
        if (to_write > AUDIO_MAX_FRAMES_PER_TICK) {
            to_write = AUDIO_MAX_FRAMES_PER_TICK;
        }
        while (to_write-- != 0u) {
            const int32_t status = submit_frame(audio);
            if (status == PXA_STATUS_WOULD_BLOCK) {
                audio->queued_frames = AUDIO_QUEUE_CAPACITY;
                break;
            }
            if (status != (int32_t)sizeof(audio->frame)) {
                break;
            }
            ++audio->queued_frames;
        }
    }

    if (++audio->ticks_since_query >= AUDIO_QUERY_INTERVAL_TICKS &&
        !audio->query_pending) {
        audio->ticks_since_query = 0;
        ++audio->query_request;
        if (audio->query_request == 0u) {
            audio->query_request = 1u;
        }
        if (pxa_audio_query_state(audio->query_request, audio->session_handle,
                                  audio->payload, sizeof(audio->payload))) {
            audio->query_pending = 1;
        }
    }
}

int audio_handle_event(game_audio_t *audio, const pxa_event_t *event,
                       uint8_t *packet, size_t packet_capacity) {
    if (event->service == PXA_SERVICE_PERMISSION &&
        event->opcode == PXA_PERMISSION_ACQUIRE &&
        event->request_id == AUDIO_PERMISSION_REQUEST) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(event, &result) ||
            result.status != PXA_STATUS_OK) {
            audio->state = AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->permission_handle = result.handle;
        audio->state = AUDIO_WAIT_OPEN;
        if (!pxa_audio_open_media(AUDIO_OPEN_REQUEST, audio->permission_handle,
                                  audio->payload, sizeof(audio->payload),
                                  packet, packet_capacity)) {
            audio->state = AUDIO_UNAVAILABLE;
        }
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_OPEN_SESSION &&
        event->request_id == AUDIO_OPEN_REQUEST) {
        pxa_audio_open_result_t result;
        if (!pxa_audio_parse_open(event, &result) ||
            result.status != PXA_STATUS_OK ||
            result.sample_rate != AUDIO_SAMPLE_RATE || result.channels != 1 ||
            result.frame_ms != 20) {
            audio->state = AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->session_handle = result.session_handle;
        audio->state = AUDIO_WAIT_GRAPH;
        if (!pxa_audio_commit_speaker_graph(
                AUDIO_GRAPH_REQUEST, audio->session_handle,
                AUDIO_GRAPH_GAIN_DB_Q8, 1500, 256, 256, audio->payload,
                sizeof(audio->payload), packet, packet_capacity)) {
            audio->state = AUDIO_UNAVAILABLE;
        }
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_COMMIT_GRAPH &&
        event->request_id == AUDIO_GRAPH_REQUEST) {
        int32_t status;
        if (!pxa_audio_parse_status(event, PXA_AUDIO_COMMIT_GRAPH, &status) ||
            status != PXA_STATUS_OK) {
            audio->state = AUDIO_UNAVAILABLE;
        } else {
            audio->state = AUDIO_READY;
            if (audio->tone_mode == AUDIO_TONE_MODE_PCM_FALLBACK) {
                uint32_t frame;
                for (frame = 0; frame < AUDIO_TARGET_FRAMES; ++frame) {
                    if (submit_frame(audio) != (int32_t)sizeof(audio->frame)) {
                        break;
                    }
                    ++audio->queued_frames;
                }
            }
        }
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_QUERY_STATE && audio->query_pending &&
        event->request_id == audio->query_request) {
        pxa_audio_state_result_t result;
        audio->query_pending = 0;
        if (pxa_audio_parse_state(event, &result) &&
            result.status == PXA_STATUS_OK) {
            audio->queued_frames =
                result.queued_samples / AUDIO_FRAME_SAMPLES;
        }
        return 1;
    }
    return 0;
}

void audio_stop(game_audio_t *audio) {
    int index;
    for (index = 0; index < AUDIO_MAX_VOICES; ++index) {
        audio->voices[index].active = 0;
    }
    for (index = 0; index < AUDIO_MAX_SCHEDULED; ++index) {
        audio->scheduled[index].active = 0;
    }
    if (audio->session_handle != 0) {
        (void)pxa_close_handle(audio->session_handle);
        audio->session_handle = 0;
    }
    audio->state = AUDIO_OFF;
}
