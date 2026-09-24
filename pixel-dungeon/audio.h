#ifndef PD_AUDIO_H
#define PD_AUDIO_H

#include <stdint.h>

#include "pxa.h"

#define PD_AUDIO_PENDING_SOUNDS 6

enum {
    PD_MUSIC_SEWERS = 0,
    PD_MUSIC_PRISON,
    PD_MUSIC_CAVES,
    PD_MUSIC_CITY,
    PD_MUSIC_HALLS,
    PD_MUSIC_COUNT,
    PD_MUSIC_TITLE = PD_MUSIC_COUNT,
};

enum {
    PD_AUDIO_IDLE = 0,
    PD_AUDIO_WAIT_PERMISSION,
    PD_AUDIO_WAIT_SESSION,
    PD_AUDIO_WAIT_GRAPH,
    PD_AUDIO_READY,
    PD_AUDIO_UNAVAILABLE,
};

typedef struct {
    uint8_t state;
    uint8_t music_track;
    uint8_t music_pending;
    uint8_t music_host;
    uint8_t sfx_host_reported;
    uint8_t music_fading_out;
    int16_t music_asset_gain_db_q8;
    uint32_t permission_handle;
    uint32_t session_handle;
    uint8_t pending[PD_AUDIO_PENDING_SOUNDS];
    uint8_t pending_count;
} pd_audio_t;

void pd_audio_init(pd_audio_t *audio);
void pd_audio_handle_event(pd_audio_t *audio, const pxa_event_t *event,
                           uint8_t *packet, uint32_t packet_capacity);
/* Dispatches queued effects and applies music fades. */
void pd_audio_tick(pd_audio_t *audio);
void pd_audio_play(pd_audio_t *audio, uint8_t sfx);
void pd_audio_set_music(pd_audio_t *audio, uint8_t track);
void pd_audio_stop(pd_audio_t *audio);
int pd_audio_active(const pd_audio_t *audio);

#endif
