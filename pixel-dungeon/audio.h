#ifndef PD_AUDIO_H
#define PD_AUDIO_H

#include <stdint.h>

#include "audio_data.h"
#include "pxa.h"

/* The Host accepts 16 kHz mono signed 16-bit PCM frames; the mixer expands the
 * 8-bit banks from audio_data.h and resamples the music bed in the Guest. */
#define PD_AUDIO_MAX_VOICES 5
#define PD_AUDIO_FRAME_SAMPLES 960
#define PD_AUDIO_PENDING_SOUNDS 6
#define PD_MUSIC_TITLE PD_MUSIC_COUNT

enum {
    PD_AUDIO_IDLE = 0,
    PD_AUDIO_WAIT_PERMISSION,
    PD_AUDIO_WAIT_SESSION,
    PD_AUDIO_WAIT_GRAPH,
    PD_AUDIO_READY,
    PD_AUDIO_UNAVAILABLE,
};

typedef struct {
    const uint8_t *pcm;
    uint32_t bytes;
    uint32_t position; /* 16.16 fixed point source position */
    uint32_t step;     /* 16.16 fixed point rate conversion */
    int16_t gain_q8;
    uint8_t active;
} pd_voice_t;

typedef struct {
    uint8_t state;
    uint8_t music_track;
    uint8_t music_pending;
    uint8_t music_host;
    uint8_t sfx_host_reported;
    uint8_t music_fading_out;
    int16_t music_asset_gain_db_q8;
    int16_t music_gain_q8;
    uint32_t permission_handle;
    uint32_t session_handle;
    uint32_t sample_rate;
    uint16_t frame_samples;
    uint16_t frame_ms;
    uint32_t music_position;
    uint32_t music_step;
    uint8_t pending[PD_AUDIO_PENDING_SOUNDS];
    uint8_t pending_count;
    pd_voice_t voices[PD_AUDIO_MAX_VOICES];
    int16_t frame[PD_AUDIO_FRAME_SAMPLES];
} pd_audio_t;

void pd_audio_init(pd_audio_t *audio);
void pd_audio_handle_event(pd_audio_t *audio, const pxa_event_t *event,
                           uint8_t *packet, uint32_t packet_capacity);
/* Tops up the Host queue; call from the clock tick. */
void pd_audio_tick(pd_audio_t *audio);
void pd_audio_play(pd_audio_t *audio, uint8_t sfx);
void pd_audio_set_music(pd_audio_t *audio, uint8_t track);
void pd_audio_stop(pd_audio_t *audio);
int pd_audio_active(const pd_audio_t *audio);

#endif
