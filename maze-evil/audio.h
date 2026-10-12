#ifndef MAZE_EVIL_AUDIO_H
#define MAZE_EVIL_AUDIO_H

#include <stdint.h>

#include "pxa.h"
#include "pxa_audio.h"
#include "pxa_clock.h"
#include "pxa_permission.h"

enum {
    AUDIO_OFF = 0,
    AUDIO_WAIT_PERMISSION,
    AUDIO_WAIT_OPEN,
    AUDIO_WAIT_GRAPH,
    AUDIO_READY,
    AUDIO_UNAVAILABLE,
};

typedef struct {
    uint64_t permission_handle;
    uint64_t session_handle;
    uint8_t state;
} game_audio_t;

/* Requests audio.playback and opens a media session. */
void audio_init(game_audio_t *audio);

/* Drives the permission -> open -> graph handshake. Returns 1 when the event
 * belonged to the audio service. */
int audio_handle_event(game_audio_t *audio, const pxa_event_t *event,
                       uint8_t *packet, size_t packet_capacity);

/* Submits one atomic, parallel effect group through Audio 0.9. gain is 0..255. */
void audio_play(game_audio_t *audio, uint8_t sound_id, uint8_t gain);

void audio_stop(game_audio_t *audio);

#endif
