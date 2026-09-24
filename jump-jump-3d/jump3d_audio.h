#ifndef JUMP3D_AUDIO_H
#define JUMP3D_AUDIO_H

#include <stdint.h>

#include "pxa.h"
#include "pxa_audio.h"
#include "pxa_permission.h"

/* Clip order matches j3_audio_bank in the generated jump3d_audio_data.h. */
enum {
    J3_CLIP_SCALE_INTRO = 0,
    J3_CLIP_SCALE_LOOP,
    J3_CLIP_SUCCESS,
    J3_CLIP_POP,
    J3_CLIP_COMBO1,
    J3_CLIP_COMBO2,
    J3_CLIP_COMBO3,
    J3_CLIP_COMBO4,
    J3_CLIP_COMBO5,
    J3_CLIP_COMBO6,
    J3_CLIP_COMBO7,
    J3_CLIP_COMBO8,
    J3_CLIP_FALL,
    J3_CLIP_FALL_2,
    J3_CLIP_START,
    J3_CLIP_SING,
    J3_CLIP_STORE,
    J3_CLIP_WATER,
    J3_CLIP_ICON,
    J3_CLIP_COUNT
};

/* One clip per channel, so a new effect always replaces the previous one of
 * the same kind instead of piling up voices. The charge swell and its sustain
 * loop run on separate channels because the original layers them. */
enum {
    J3_CHANNEL_CHARGE = 0, /* scale_intro: the charge swell */
    J3_CHANNEL_SUSTAIN,    /* scale_loop: the sustained charge loop */
    J3_CHANNEL_LAND,       /* success, fall, restart */
    J3_CHANNEL_COMBO,      /* the rising centre-hit notes */
    J3_CHANNEL_POP,        /* the next block dropping in */
    J3_CHANNEL_BONUS,      /* music box, store, manhole */
    J3_CHANNEL_BGM,        /* background music, ducked by effects */
    J3_CHANNEL_COUNT
};

#define J3_AUDIO_SAMPLE_RATE 16000u
#define J3_AUDIO_FRAME_SAMPLES 320u
#define J3_AUDIO_FRAME_US 20000u
#define J3_AUDIO_PREFILL_FRAMES 4u
#define J3_AUDIO_MAX_FRAMES_PER_TICK 4u

#define J3_AUDIO_PERMISSION_REQUEST UINT32_C(0x4a334101)
#define J3_AUDIO_OPEN_REQUEST UINT32_C(0x4a334102)
#define J3_AUDIO_GRAPH_REQUEST UINT32_C(0x4a334103)

/* Linear gains in Q12 (4096 = unity). */
#define J3_GAIN_FULL UINT16_C(4096)
#define J3_GAIN_LOUD UINT16_C(3300)
#define J3_GAIN_SOFT UINT16_C(2400)
#define J3_GAIN_QUIET UINT16_C(1500)
#define J3_GAIN_BGM UINT16_C(1150)
#define J3_GAIN_BGM_DUCKED UINT16_C(430)

enum {
    J3_AUDIO_OFF = 0,
    J3_AUDIO_WAIT_PERMISSION,
    J3_AUDIO_WAIT_OPEN,
    J3_AUDIO_WAIT_GRAPH,
    J3_AUDIO_READY,
    J3_AUDIO_UNAVAILABLE
};

typedef struct {
    const uint32_t *words;
    uint32_t samples;
    uint8_t looping;
} j3_audio_clip_t;

typedef struct {
    const j3_audio_clip_t *clip;
    uint32_t position;      /* index of the newer decoded sample */
    uint32_t phase;         /* Q16 fraction between previous and current */
    uint32_t step_q16;      /* clip rate / session rate in Q16 */
    int32_t predictor;
    uint16_t step_index;
    int16_t previous;       /* sample at position - 1 */
    int16_t current;        /* sample at position */
    uint16_t gain_q12;      /* current gain, Q12 (0..4096) */
    uint16_t target_gain_q12;
    uint8_t active;         /* still mixing */
    uint8_t requested;      /* asked to play; clears when the fade ends */
    uint8_t loop;
    uint8_t finished;       /* ran past the end of a one shot */
} j3_audio_voice_t;

typedef struct {
    uint32_t permission_handle;
    uint32_t session_handle;
    uint64_t tick_us;
    uint32_t remainder_us;
    uint16_t queued_frames;
    uint8_t state;
    uint8_t payload[96];
    j3_audio_voice_t voices[J3_CHANNEL_COUNT];
    int16_t frame[J3_AUDIO_FRAME_SAMPLES];
} j3_audio_t;

/* Requests the optional playback permission and opens the media session. */
void j3_audio_start(j3_audio_t *audio, uint8_t *packet, uint32_t capacity);

/* Handles permission/audio responses. Returns 1 when the event was consumed. */
int j3_audio_handle_event(j3_audio_t *audio, const pxa_event_t *event,
                          uint8_t *packet, uint32_t capacity);

/* Mixes and submits frames for a clock tick. */
void j3_audio_tick(j3_audio_t *audio, const pxa_event_t *event);

/* Starts `clip` on `channel`. `loop` repeats it until stopped or replaced. */
void j3_audio_play(j3_audio_t *audio, uint8_t channel, uint8_t clip,
                   uint16_t gain_q12, uint8_t loop);
void j3_audio_stop(j3_audio_t *audio, uint8_t channel);
void j3_audio_stop_all(j3_audio_t *audio);
int j3_audio_channel_active(const j3_audio_t *audio, uint8_t channel);

/* Clip index currently loaded in `channel`, or J3_CLIP_COUNT when idle. */
uint8_t j3_audio_channel_clip(const j3_audio_t *audio, uint8_t channel);

#endif
