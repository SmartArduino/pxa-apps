#ifndef VOXEL_CRAFT_SFX_H
#define VOXEL_CRAFT_SFX_H

#include "pxa_audio.h"
#include "pxa_permission.h"

#define VOXEL_SFX_PERMISSION_REQUEST UINT32_C(0x56435801)
#define VOXEL_SFX_OPEN_REQUEST UINT32_C(0x56435802)
#define VOXEL_SFX_GRAPH_REQUEST UINT32_C(0x56435803)
#define VOXEL_SFX_SAMPLE_RATE 16000u
#define VOXEL_SFX_GRAPH_GAIN_DB_Q8 (-320)
#define VOXEL_BGM_GAIN_DB_Q8 (-14 * 256)

enum {
    VOXEL_SFX_OFF = 0,
    VOXEL_SFX_WAIT_PERMISSION,
    VOXEL_SFX_WAIT_OPEN,
    VOXEL_SFX_WAIT_GRAPH,
    VOXEL_SFX_READY,
    VOXEL_SFX_UNAVAILABLE,
};

enum {
    VOXEL_SFX_MINE = 0,
    VOXEL_SFX_BREAK,
    VOXEL_SFX_PLACE,
    VOXEL_SFX_ATTACK,
    VOXEL_SFX_HIT,
    VOXEL_SFX_JUMP,
    VOXEL_SFX_KIND_COUNT,
};

typedef struct {
    uint32_t permission_handle;
    uint32_t session_handle;
    uint8_t state;
    uint8_t bgm_pending;
    uint8_t bgm_active;
    uint8_t payload[96];
} voxel_sfx_t;

static inline void voxel_sfx_start(voxel_sfx_t *sfx, uint8_t *packet,
                                   size_t packet_capacity) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    if (sfx == NULL || packet == NULL || sfx->state != VOXEL_SFX_OFF) return;
    sfx->state = VOXEL_SFX_WAIT_PERMISSION;
    if (!pxa_permission_acquire(VOXEL_SFX_PERMISSION_REQUEST, permission_name,
                                sizeof(permission_name) - 1, permission_scope,
                                sizeof(permission_scope) - 1, sfx->payload,
                                sizeof(sfx->payload), packet,
                                packet_capacity)) {
        sfx->state = VOXEL_SFX_UNAVAILABLE;
    }
}

static inline int voxel_sfx_handle_event(voxel_sfx_t *sfx,
                                         const pxa_event_t *event,
                                         uint8_t *packet,
                                         size_t packet_capacity) {
    if (sfx == NULL || event == NULL || packet == NULL) return 0;
    if (event->service == PXA_SERVICE_PERMISSION &&
        event->opcode == PXA_PERMISSION_ACQUIRE &&
        event->request_id == VOXEL_SFX_PERMISSION_REQUEST) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(event, &result) ||
            result.status != PXA_STATUS_OK) {
            sfx->state = VOXEL_SFX_UNAVAILABLE;
            return 1;
        }
        sfx->permission_handle = result.handle;
        sfx->state = VOXEL_SFX_WAIT_OPEN;
        if (!pxa_audio_open_media(VOXEL_SFX_OPEN_REQUEST,
                                  sfx->permission_handle, sfx->payload,
                                  sizeof(sfx->payload), packet,
                                  packet_capacity)) {
            sfx->state = VOXEL_SFX_UNAVAILABLE;
        }
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_OPEN_SESSION &&
        event->request_id == VOXEL_SFX_OPEN_REQUEST) {
        pxa_audio_open_result_t result;
        if (!pxa_audio_parse_open(event, &result) ||
            result.status != PXA_STATUS_OK ||
            result.sample_rate != VOXEL_SFX_SAMPLE_RATE ||
            result.channels != 1 || result.frame_ms != 20) {
            sfx->state = VOXEL_SFX_UNAVAILABLE;
            return 1;
        }
        sfx->session_handle = result.session_handle;
        sfx->state = VOXEL_SFX_WAIT_GRAPH;
        if (!pxa_audio_commit_speaker_graph(
                VOXEL_SFX_GRAPH_REQUEST, sfx->session_handle,
                VOXEL_SFX_GRAPH_GAIN_DB_Q8, 1500, 256, 256, sfx->payload,
                sizeof(sfx->payload), packet, packet_capacity)) {
            sfx->state = VOXEL_SFX_UNAVAILABLE;
        }
        return 1;
    }
    if (event->service == PXA_SERVICE_AUDIO &&
        event->opcode == PXA_AUDIO_COMMIT_GRAPH &&
        event->request_id == VOXEL_SFX_GRAPH_REQUEST) {
        int32_t status;
        if (!pxa_audio_parse_status(event, PXA_AUDIO_COMMIT_GRAPH, &status) ||
            status != PXA_STATUS_OK) {
            sfx->state = VOXEL_SFX_UNAVAILABLE;
        } else {
            sfx->state = VOXEL_SFX_READY;
            sfx->bgm_pending = 1;
        }
        return 1;
    }
    return 0;
}

static inline void voxel_sfx_play(voxel_sfx_t *sfx, uint8_t kind) {
    static const uint16_t frequency_hz[VOXEL_SFX_KIND_COUNT] = {
        640, 140, 480, 900, 180, 700,
    };
    static const uint16_t duration_ms[VOXEL_SFX_KIND_COUNT] = {
        50, 120, 70, 60, 90, 80,
    };
    static const int16_t gain_db_q8[VOXEL_SFX_KIND_COUNT] = {
        -7 * 256, -5 * 256, -8 * 256, -8 * 256, -6 * 256, -8 * 256,
    };
    static const uint16_t attack_ms[VOXEL_SFX_KIND_COUNT] = {
        2, 1, 2, 2, 2, 4,
    };
    static const uint16_t release_ms[VOXEL_SFX_KIND_COUNT] = {
        20, 80, 30, 30, 60, 50,
    };
    static const uint8_t waveform[VOXEL_SFX_KIND_COUNT] = {
        PXA_AUDIO_TONE_TRIANGLE, PXA_AUDIO_TONE_NOISE,
        PXA_AUDIO_TONE_SQUARE, PXA_AUDIO_TONE_NOISE,
        PXA_AUDIO_TONE_SQUARE, PXA_AUDIO_TONE_SINE,
    };
    if (sfx == NULL || sfx->state != VOXEL_SFX_READY ||
        kind >= VOXEL_SFX_KIND_COUNT) {
        return;
    }
    (void)pxa_audio_play_tone_enveloped(
        sfx->session_handle, waveform[kind], frequency_hz[kind],
        duration_ms[kind], gain_db_q8[kind], attack_ms[kind],
        release_ms[kind], 0);
}

static inline void voxel_sfx_tick(voxel_sfx_t *sfx,
                                  const pxa_event_t *event) {
    (void)event;
    if (sfx != NULL && sfx->state == VOXEL_SFX_READY &&
        sfx->bgm_pending) {
        static const char path[] = "assets/bgm.ogg";
        const int32_t result = pxa_audio_play_asset(
            sfx->session_handle, path, sizeof(path) - 1u, 1,
            VOXEL_BGM_GAIN_DB_Q8, sfx->payload, sizeof(sfx->payload));
        if (result > 0) {
            sfx->bgm_pending = 0;
            sfx->bgm_active = 1;
        } else if (result != PXA_STATUS_WOULD_BLOCK) {
            sfx->bgm_pending = 0;
        }
    }
}

#endif
