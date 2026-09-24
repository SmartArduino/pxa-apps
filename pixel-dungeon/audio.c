#include "audio.h"

#include "pxa_audio.h"
#include "pxa_log.h"
#include "pxa_permission.h"

#include "game.h"

#define PD_AUDIO_PERMISSION_REQUEST UINT32_C(0x50440001)
#define PD_AUDIO_OPEN_REQUEST UINT32_C(0x50440002)
#define PD_AUDIO_GRAPH_REQUEST UINT32_C(0x50440003)
#define PD_AUDIO_QUERY_REQUEST UINT32_C(0x50440004)
#define PD_AUDIO_GRAPH_GAIN_DB_Q8 (-2 * 256)
#define PD_AUDIO_MAX_FRAMES_PER_TICK 1u
#define PD_MUSIC_GAIN_Q8 (-7 * 256)
#define PD_MUSIC_START_GAIN_Q8 (-40 * 256)
#define PD_MUSIC_FADE_STEP_Q8 (2 * 256)
#define PD_SFX_EDGE_SAMPLES (PD_SFX_RATE / 250u)

static const char *const music_files[PD_MUSIC_COUNT + 1] = {
    "assets/music/sewers_1.ogg",
    "assets/music/prison_1.ogg",
    "assets/music/caves_1.ogg",
    "assets/music/city_1.ogg",
    "assets/music/halls_1.ogg",
    "assets/music/theme_1.ogg",
};

static const char *const sound_files[PD_SFX_COUNT] = {
    "assets/sfx/hit.pcm", "assets/sfx/miss.pcm",
    "assets/sfx/hit_crush.pcm", "assets/sfx/death.pcm",
    "assets/sfx/drink.pcm", "assets/sfx/eat.pcm",
    "assets/sfx/gold.pcm", "assets/sfx/descend.pcm",
    "assets/sfx/door_open.pcm", "assets/sfx/trap.pcm",
    "assets/sfx/levelup.pcm", "assets/sfx/item.pcm",
    "assets/sfx/read.pcm", "assets/sfx/shatter.pcm",
    "assets/sfx/unlock.pcm", "assets/sfx/health_warn.pcm",
    "assets/sfx/step.pcm", "assets/sfx/grass.pcm",
    "assets/sfx/trample.pcm", "assets/sfx/water.pcm",
};

static uint8_t g_payload[96];

static int32_t clamp_sample(int32_t value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return value;
}

static int16_t gain_from_db(int16_t db_q8) {
    int32_t gain = 256;
    for (int32_t decibel = -db_q8 / 256; decibel > 0; --decibel)
        gain = gain * 228 / 256;
    return (int16_t)gain;
}

/* 8-bit unsigned PCM with linear interpolation and 16.16 rate conversion. */
static int16_t sample_voice(const uint8_t *pcm, uint32_t bytes,
                            uint32_t position, uint32_t step, int16_t gain_q8,
                            uint32_t *next_position) {
    const uint32_t index = position >> 16;
    const uint32_t fraction = position & 0xFFFFu;
    int32_t first;
    int32_t second;
    int32_t value;
    if (index >= bytes) {
        *next_position = 0;
        return 0;
    }
    first = (int32_t)pcm[index] - 128;
    second = index + 1u < bytes ? (int32_t)pcm[index + 1u] - 128 : first;
    value = first + (int32_t)(((int64_t)(second - first) * fraction) >> 16);
    value = value * 256; /* 8-bit to 16-bit */
    value = (value * gain_q8) >> 8;
    *next_position = position + step;
    return (int16_t)clamp_sample(value);
}

static void mix_frame(pd_audio_t *audio) {
    const uint32_t samples =
        audio->frame_samples == 0 ? 320u : audio->frame_samples;
    for (uint32_t index = 0; index < samples; ++index) {
        int32_t mixed = 0;
        if (audio->music_track < PD_MUSIC_COUNT && !audio->music_host &&
            audio->state == PD_AUDIO_READY) {
            const pd_music_bank_t *track = &pd_music_bank[audio->music_track];
            int16_t value;
            if (audio->music_position >> 16 >= track->bytes)
                audio->music_position = 0;
            value = sample_voice(track->pcm, track->bytes, audio->music_position,
                                 audio->music_step, audio->music_gain_q8,
                                 &audio->music_position);
            mixed += value;
        }
        for (int voice = 0; voice < PD_AUDIO_MAX_VOICES; ++voice) {
            pd_voice_t *slot = &audio->voices[voice];
            if (!slot->active) continue;
            const uint32_t source_index = slot->position >> 16;
            const uint32_t edge_samples = PD_SFX_EDGE_SAMPLES;
            const uint32_t attack = source_index + 1u;
            const uint32_t release = slot->bytes - source_index;
            uint32_t envelope = edge_samples;
            if (attack < envelope) envelope = attack;
            if (release < envelope) envelope = release;
            const int32_t sample = sample_voice(
                slot->pcm, slot->bytes, slot->position, slot->step,
                slot->gain_q8, &slot->position);
            mixed += sample * (int32_t)envelope / (int32_t)edge_samples;
            if (slot->position >> 16 >= slot->bytes) slot->active = 0;
        }
        audio->frame[index] = (int16_t)clamp_sample(mixed);
    }
}

void pd_audio_init(pd_audio_t *audio) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    uint8_t packet[96];
    for (uint32_t index = 0; index < sizeof(*audio); ++index)
        ((uint8_t *)audio)[index] = 0;
    audio->music_track = 0xFF;
    audio->music_pending = 0xFF;
    audio->music_gain_q8 = gain_from_db(PD_MUSIC_GAIN_Q8);
    audio->state = PD_AUDIO_WAIT_PERMISSION;
    if (!pxa_permission_acquire(
            PD_AUDIO_PERMISSION_REQUEST, permission_name,
            sizeof(permission_name) - 1u, permission_scope,
            sizeof(permission_scope) - 1u, g_payload, sizeof(g_payload), packet,
            sizeof(packet))) {
        audio->state = PD_AUDIO_UNAVAILABLE;
    }
}

static void open_session(pd_audio_t *audio) {
    uint8_t packet[96];
    if (audio->permission_handle == 0) {
        audio->state = PD_AUDIO_UNAVAILABLE;
        return;
    }
    if (!pxa_audio_open_media(PD_AUDIO_OPEN_REQUEST, audio->permission_handle,
                              g_payload, sizeof(g_payload), packet,
                              sizeof(packet))) {
        audio->state = PD_AUDIO_UNAVAILABLE;
    }
}

static void commit_graph(pd_audio_t *audio) {
    uint8_t packet[96];
    if (audio->session_handle == 0) return;
    (void)pxa_audio_commit_speaker_graph(
        PD_AUDIO_GRAPH_REQUEST, audio->session_handle, PD_AUDIO_GRAPH_GAIN_DB_Q8,
        120, 0, 128, g_payload, sizeof(g_payload), packet, sizeof(packet));
}

static void start_music_asset(pd_audio_t *audio, uint8_t track) {
    uint8_t command[64];
    const char *path = music_files[track];
    uint16_t size = 0;
    while (path[size] != '\0') ++size;
    audio->music_track = track;
    audio->music_position = 0;
    audio->music_asset_gain_db_q8 = PD_MUSIC_START_GAIN_Q8;
    audio->music_host = pxa_audio_play_asset(
        audio->session_handle, path, size, 1, PD_MUSIC_START_GAIN_Q8,
        command, sizeof(command)) > 0;
    if (!audio->music_host && track < PD_MUSIC_COUNT)
        (void)pxa_log_info("pd: full music unavailable; using PCM fallback");
    else
        (void)pxa_log_info("pd: full original music loaded");
}

void pd_audio_handle_event(pd_audio_t *audio, const pxa_event_t *event,
                           uint8_t *packet, uint32_t packet_capacity) {
    (void)packet;
    (void)packet_capacity;
    if (event->service == PXA_SERVICE_PERMISSION) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(event, &result)) return;
        if (result.status != PXA_STATUS_OK) {
            audio->state = PD_AUDIO_UNAVAILABLE;
            (void)pxa_log_info("pd: audio permission denied");
            return;
        }
        audio->permission_handle = result.handle;
        audio->state = PD_AUDIO_WAIT_SESSION;
        open_session(audio);
        return;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_OPEN_SESSION) {
        pxa_audio_open_result_t opened;
        if (!pxa_audio_parse_open(event, &opened)) return;
        if (opened.status != PXA_STATUS_OK) {
            audio->state = PD_AUDIO_UNAVAILABLE;
            (void)pxa_log_info("pd: audio session unavailable");
            return;
        }
        audio->session_handle = opened.session_handle;
        audio->sample_rate = opened.sample_rate;
        audio->frame_ms = opened.frame_ms;
        audio->frame_samples = (uint16_t)(opened.sample_rate *
                                          (uint32_t)opened.frame_ms / 1000u);
        if (audio->frame_samples == 0) audio->frame_samples = 320;
        if (audio->frame_samples > PD_AUDIO_FRAME_SAMPLES)
            audio->frame_samples = PD_AUDIO_FRAME_SAMPLES;
        audio->music_step =
            (uint32_t)(((uint64_t)PD_MUSIC_RATE << 16) / opened.sample_rate);
        audio->state = PD_AUDIO_WAIT_GRAPH;
        commit_graph(audio);
        return;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_COMMIT_GRAPH) {
        int32_t status = 0;
        if (pxa_audio_parse_status(event, PXA_AUDIO_COMMIT_GRAPH, &status)) {
            if (status != PXA_STATUS_OK) {
                audio->state = PD_AUDIO_UNAVAILABLE;
                return;
            }
            audio->state = PD_AUDIO_READY;
            if (audio->music_track != 0xFF)
                start_music_asset(audio, audio->music_track);
            (void)pxa_log_info("pd: audio ready");
        }
        return;
    }
}

void pd_audio_play(pd_audio_t *audio, uint8_t sfx) {
    static const uint8_t bank[PD_SOUND_COUNT] = {
        PD_SFX_HIT, PD_SFX_MISS, PD_SFX_HIT_STRONG, PD_SFX_DEATH,
        PD_SFX_DRINK, PD_SFX_EAT, PD_SFX_GOLD, PD_SFX_ITEM,
        PD_SFX_READ, PD_SFX_DESCEND, PD_SFX_DOOR, PD_SFX_TRAP,
        PD_SFX_LEVELUP, PD_SFX_SHATTER, PD_SFX_UNLOCK, PD_SFX_HUNGRY,
        PD_SFX_STEP, PD_SFX_GRASS, PD_SFX_TRAMPLE, PD_SFX_WATER,
    };
    if (sfx >= PD_SOUND_COUNT) return;
    if (audio->pending_count < PD_AUDIO_PENDING_SOUNDS)
        audio->pending[audio->pending_count++] = bank[sfx];
}

void pd_audio_set_music(pd_audio_t *audio, uint8_t track) {
    if (track > PD_MUSIC_TITLE) {
        if (audio->music_host)
            (void)pxa_audio_control_asset(audio->session_handle,
                                          PXA_AUDIO_ASSET_STOP, 0);
        audio->music_host = 0;
        audio->music_track = 0xFF;
        audio->music_pending = 0xFF;
        return;
    }
    if (audio->music_track == track) {
        audio->music_pending = 0xFF;
        audio->music_fading_out = 0;
        return;
    }
    if (audio->music_pending == track) return;
    if (audio->music_host) {
        audio->music_pending = track;
        audio->music_fading_out = 1;
        return;
    }
    if (audio->state == PD_AUDIO_READY && audio->session_handle != 0) {
        start_music_asset(audio, track);
        return;
    }
    audio->music_track = track;
    audio->music_position = 0;
}

void pd_audio_stop(pd_audio_t *audio) {
    if (audio->music_host)
        (void)pxa_audio_control_asset(audio->session_handle,
                                      PXA_AUDIO_ASSET_STOP, 0);
    audio->music_host = 0;
    audio->music_track = 0xFF;
    for (int index = 0; index < PD_AUDIO_MAX_VOICES; ++index)
        audio->voices[index].active = 0;
    audio->pending_count = 0;
    audio->state = PD_AUDIO_UNAVAILABLE;
    if (audio->session_handle != 0) {
        (void)pxa_close_handle(audio->session_handle);
        audio->session_handle = 0;
    }
    if (audio->permission_handle != 0) {
        (void)pxa_close_handle(audio->permission_handle);
        audio->permission_handle = 0;
    }
}

int pd_audio_active(const pd_audio_t *audio) {
    return audio->state == PD_AUDIO_READY;
}

void pd_audio_tick(pd_audio_t *audio) {
    if (audio->state != PD_AUDIO_READY || audio->session_handle == 0) return;
    if (audio->music_host) {
        int16_t gain = audio->music_asset_gain_db_q8;
        if (audio->music_fading_out) {
            gain -= PD_MUSIC_FADE_STEP_Q8;
            if (gain <= PD_MUSIC_START_GAIN_Q8) {
                (void)pxa_audio_control_asset(audio->session_handle,
                                              PXA_AUDIO_ASSET_STOP, 0);
                audio->music_host = 0;
                audio->music_fading_out = 0;
                start_music_asset(audio, audio->music_pending);
                audio->music_pending = 0xFF;
                gain = audio->music_asset_gain_db_q8;
            }
        } else if (gain < PD_MUSIC_GAIN_Q8) {
            gain += PD_MUSIC_FADE_STEP_Q8;
            if (gain > PD_MUSIC_GAIN_Q8) gain = PD_MUSIC_GAIN_Q8;
        }
        if (audio->music_host && gain != audio->music_asset_gain_db_q8) {
            (void)pxa_audio_control_asset(audio->session_handle,
                                          PXA_AUDIO_ASSET_SET_GAIN, gain);
            audio->music_asset_gain_db_q8 = gain;
        }
    }
    /* Start queued effects first so a tick never delays a new sound. */
    while (audio->pending_count > 0) {
        const uint8_t sfx = audio->pending[0];
        int slot = -1;
        for (int index = 0; index < PD_AUDIO_MAX_VOICES; ++index) {
            if (!audio->voices[index].active) {
                slot = index;
                break;
            }
        }
        for (int index = 0; index + 1 < audio->pending_count; ++index)
            audio->pending[index] = audio->pending[index + 1];
        --audio->pending_count;
        {
            uint8_t command[64];
            const char *path = sound_files[sfx];
            uint16_t size = 0;
            while (path[size] != '\0') ++size;
            if (pxa_audio_play_asset(audio->session_handle, path, size, 0,
                                     pd_sfx_bank[sfx].gain_db_q8,
                                     command, sizeof(command)) > 0) {
                if (!audio->sfx_host_reported) {
                    audio->sfx_host_reported = 1;
                    (void)pxa_log_info("pd: sound effects playing in host mixer");
                }
                continue;
            }
        }
        if (slot < 0) continue;
        audio->voices[slot].pcm = pd_sfx_bank[sfx].pcm;
        audio->voices[slot].bytes = pd_sfx_bank[sfx].bytes;
        audio->voices[slot].position = 0;
        audio->voices[slot].step =
            (uint32_t)(((uint64_t)PD_SFX_RATE << 16) / audio->sample_rate);
        audio->voices[slot].gain_q8 = gain_from_db(pd_sfx_bank[sfx].gain_db_q8);
        audio->voices[slot].active = 1;
    }
    if (audio->music_host) {
        int has_fallback_voice = 0;
        for (int voice = 0; voice < PD_AUDIO_MAX_VOICES; ++voice)
            has_fallback_voice |= audio->voices[voice].active;
        if (!has_fallback_voice) return;
    }
    for (uint32_t frame = 0; frame < PD_AUDIO_MAX_FRAMES_PER_TICK; ++frame) {
        const uint32_t bytes = (uint32_t)audio->frame_samples * 2u;
        uint32_t voice_positions[PD_AUDIO_MAX_VOICES];
        uint8_t voice_active[PD_AUDIO_MAX_VOICES];
        const uint32_t music_position = audio->music_position;
        int32_t written;
        for (int voice = 0; voice < PD_AUDIO_MAX_VOICES; ++voice) {
            voice_positions[voice] = audio->voices[voice].position;
            voice_active[voice] = audio->voices[voice].active;
        }
        mix_frame(audio);
        written = pxa_audio_write_pcm(audio->session_handle,
                                      (uint8_t *)audio->frame, bytes);
        if (written != (int32_t)bytes) {
            audio->music_position = music_position;
            for (int voice = 0; voice < PD_AUDIO_MAX_VOICES; ++voice) {
                audio->voices[voice].position = voice_positions[voice];
                audio->voices[voice].active = voice_active[voice];
            }
            break;
        }
    }
}
