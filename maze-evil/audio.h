#ifndef MAZE_EVIL_AUDIO_H
#define MAZE_EVIL_AUDIO_H

#include <stdint.h>

#include "pxa.h"

#define AUDIO_MAX_VOICES 16
#define AUDIO_MAX_SCHEDULED 24
#define AUDIO_FRAME_SAMPLES 320
#define AUDIO_SINE_ENTRIES 256

typedef struct {
    uint8_t waveform;
    uint32_t phase;
    uint32_t phase_step;
    uint32_t total_frames;
    uint32_t remaining_frames;
    uint32_t attack_frames;
    uint32_t release_frames;
    uint16_t volume_per_mille;
    uint32_t noise;
    uint8_t active;
} audio_voice_t;

typedef struct {
    uint8_t waveform;
    uint16_t frequency_hz;
    uint16_t duration_ms;
    uint16_t volume_per_mille;
    uint16_t attack_ms;
    uint16_t release_ms;
    uint32_t delay_frames; /* sample-accurate start delay */
    uint8_t active;
} audio_scheduled_t;

enum {
    AUDIO_OFF = 0,
    AUDIO_WAIT_PERMISSION,
    AUDIO_WAIT_OPEN,
    AUDIO_WAIT_GRAPH,
    AUDIO_READY,
    AUDIO_UNAVAILABLE,
};

enum {
    AUDIO_TONE_MODE_UNKNOWN = 0,
    AUDIO_TONE_MODE_HOST,
    AUDIO_TONE_MODE_PCM_FALLBACK,
};

typedef struct {
    uint32_t permission_handle;
    uint32_t session_handle;
    uint64_t tick_us;
    uint32_t queued_frames; /* estimate of the provider queue depth */
    uint32_t ticks_since_query;
    uint32_t query_request;
    uint32_t query_pending;
    uint8_t state;
    uint8_t clock_started;
    uint8_t tone_mode;
    int16_t sine[AUDIO_SINE_ENTRIES];
    int16_t frame[AUDIO_FRAME_SAMPLES];
    audio_voice_t voices[AUDIO_MAX_VOICES];
    audio_scheduled_t scheduled[AUDIO_MAX_SCHEDULED];
    uint8_t payload[96];
} game_audio_t;

/* Requests audio.playback and opens a media session. */
void audio_init(game_audio_t *audio);

/* Drives the permission -> open -> graph handshake. Returns 1 when the event
 * belonged to the audio service. */
int audio_handle_event(game_audio_t *audio, const pxa_event_t *event,
                       uint8_t *packet, size_t packet_capacity);

/* Starts every tone of a sound profile; `gain` is 0..255. Host tone commands
 * are preferred; the local mixer is retained for older Hosts. */
void audio_play(game_audio_t *audio, uint8_t sound_id, uint8_t gain);

/* Pumps the PCM compatibility path only while it has active sound. */
void audio_tick(game_audio_t *audio, const pxa_event_t *event);

void audio_stop(game_audio_t *audio);

#endif
