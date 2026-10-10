#ifndef JUMP3D_AUDIO_H_CPP
#define JUMP3D_AUDIO_H_CPP

#include <stdint.h>




#include "../common/cpp/audio.hpp"

namespace jump {

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
#define J3_AUDIO_SOUND_SLOTS 8u

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
    J3_AUDIO_UNAVAILABLE,
    J3_AUDIO_PAUSED
};

typedef struct {
    const char *path;
    uint32_t samples;
    uint8_t looping;
} j3_audio_clip_t;

typedef struct {
    uint64_t ends_us;
    uint16_t gain_q12;
    uint8_t clip, loop, requested, started, stop_pending;
} j3_audio_voice_t;

struct j3_audio_t {
    arcade::AudioBank bank;
    j3_audio_voice_t voices[J3_CHANNEL_COUNT]{};
    uint64_t tick_us=0,music_instance=0;
    uint16_t music_gain_q12=0;
    uint8_t state=J3_AUDIO_OFF,preload_index=0;
    bool music_ready=false;
};
void j3_audio_start(j3_audio_t*,pxa::Context&);
void j3_audio_event(j3_audio_t*,const pxa::Event&);
void j3_audio_tick(j3_audio_t*,uint64_t);
void j3_audio_play(j3_audio_t*,uint8_t,uint8_t,uint16_t,uint8_t);
void j3_audio_stop(j3_audio_t*,uint8_t);
void j3_audio_stop_all(j3_audio_t*);
void j3_audio_pause(j3_audio_t*);
void j3_audio_resume(j3_audio_t*);
int j3_audio_channel_active(const j3_audio_t*,uint8_t);
uint8_t j3_audio_channel_clip(const j3_audio_t*,uint8_t);
} // namespace jump
#endif
