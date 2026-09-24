#include "audio.h"

#include "pxa_audio.h"
#include "pxa_log.h"
#include "pxa_permission.h"

#include "game.h"

#define PD_AUDIO_PERMISSION_REQUEST UINT32_C(0x50440001)
#define PD_AUDIO_OPEN_REQUEST UINT32_C(0x50440002)
#define PD_AUDIO_GRAPH_REQUEST UINT32_C(0x50440003)
#define PD_AUDIO_GRAPH_GAIN_DB_Q8 (-2 * 256)
#define PD_MUSIC_GAIN_Q8 (-7 * 256)
#define PD_MUSIC_START_GAIN_Q8 (-40 * 256)
#define PD_MUSIC_FADE_STEP_Q8 (2 * 256)

static const char *const music_files[PD_MUSIC_COUNT + 1] = {
    "assets/music/sewers_1.ogg",
    "assets/music/prison_1.ogg",
    "assets/music/caves_1.ogg",
    "assets/music/city_1.ogg",
    "assets/music/halls_1.ogg",
    "assets/music/theme_1.ogg",
};

typedef struct {
    const char *path;
    int16_t gain_db_q8;
} pd_sound_file_t;

static const pd_sound_file_t sound_files[PD_SOUND_COUNT] = {
    [PD_SOUND_HIT] = {"assets/sfx/hit.pcm", -3 * 256},
    [PD_SOUND_MISS] = {"assets/sfx/miss.pcm", -6 * 256},
    [PD_SOUND_HIT_STRONG] = {"assets/sfx/hit_crush.pcm", -3 * 256},
    [PD_SOUND_DEATH] = {"assets/sfx/death.pcm", -3 * 256},
    [PD_SOUND_DRINK] = {"assets/sfx/drink.pcm", -4 * 256},
    [PD_SOUND_EAT] = {"assets/sfx/eat.pcm", -5 * 256},
    [PD_SOUND_GOLD] = {"assets/sfx/gold.pcm", -5 * 256},
    [PD_SOUND_ITEM] = {"assets/sfx/item.pcm", -6 * 256},
    [PD_SOUND_READ] = {"assets/sfx/read.pcm", -6 * 256},
    [PD_SOUND_DESCEND] = {"assets/sfx/descend.pcm", -3 * 256},
    [PD_SOUND_DOOR] = {"assets/sfx/door_open.pcm", -6 * 256},
    [PD_SOUND_TRAP] = {"assets/sfx/trap.pcm", -3 * 256},
    [PD_SOUND_LEVELUP] = {"assets/sfx/levelup.pcm", -3 * 256},
    [PD_SOUND_SHATTER] = {"assets/sfx/shatter.pcm", -4 * 256},
    [PD_SOUND_UNLOCK] = {"assets/sfx/unlock.pcm", -4 * 256},
    [PD_SOUND_HUNGRY] = {"assets/sfx/health_warn.pcm", -6 * 256},
    [PD_SOUND_STEP] = {"assets/sfx/step.pcm", -8 * 256},
    [PD_SOUND_GRASS] = {"assets/sfx/grass.pcm", -7 * 256},
    [PD_SOUND_TRAMPLE] = {"assets/sfx/trample.pcm", -6 * 256},
    [PD_SOUND_WATER] = {"assets/sfx/water.pcm", -7 * 256},
};

static uint8_t g_payload[96];

void pd_audio_init(pd_audio_t *audio) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    uint8_t packet[96];
    for (uint32_t index = 0; index < sizeof(*audio); ++index)
        ((uint8_t *)audio)[index] = 0;
    audio->music_track = 0xFF;
    audio->music_pending = 0xFF;
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
    audio->music_asset_gain_db_q8 = PD_MUSIC_START_GAIN_Q8;
    audio->music_host = pxa_audio_play_asset(
        audio->session_handle, path, size, 1, PD_MUSIC_START_GAIN_Q8,
        command, sizeof(command)) > 0;
    if (!audio->music_host)
        (void)pxa_log_info("pd: packaged music unavailable");
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
    }
}

void pd_audio_play(pd_audio_t *audio, uint8_t sfx) {
    if (sfx >= PD_SOUND_COUNT) return;
    if (audio->pending_count < PD_AUDIO_PENDING_SOUNDS)
        audio->pending[audio->pending_count++] = sfx;
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
}

void pd_audio_stop(pd_audio_t *audio) {
    if (audio->music_host)
        (void)pxa_audio_control_asset(audio->session_handle,
                                      PXA_AUDIO_ASSET_STOP, 0);
    audio->music_host = 0;
    audio->music_track = 0xFF;
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
    while (audio->pending_count > 0) {
        const uint8_t sound = audio->pending[0];
        const pd_sound_file_t *asset = &sound_files[sound];
        uint8_t command[64];
        uint16_t size = 0;
        while (asset->path[size] != '\0') ++size;
        for (int index = 1; index < audio->pending_count; ++index)
            audio->pending[index - 1] = audio->pending[index];
        --audio->pending_count;
        if (pxa_audio_play_asset(audio->session_handle, asset->path, size, 0,
                                 asset->gain_db_q8, command,
                                 sizeof(command)) > 0) {
            if (audio->sfx_host_reported != 1) {
                audio->sfx_host_reported = 1;
                (void)pxa_log_info("pd: sound effects playing in host mixer");
            }
        } else if (audio->sfx_host_reported != 2) {
            audio->sfx_host_reported = 2;
            (void)pxa_log_info("pd: packaged sound effect unavailable");
        }
    }
}
