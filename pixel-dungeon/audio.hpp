#ifndef PD_AUDIO_H_CPP
#define PD_AUDIO_H_CPP

#include <stdint.h>




#include "../common/cpp/audio.hpp"

namespace dungeon {

#define PD_AUDIO_PENDING_SOUNDS 6
#define PD_AUDIO_SOUND_SLOTS 8

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

struct pd_audio_t {
    arcade::AudioBank bank;
    uint8_t music_track=0xff,music_pending=0xff,pending_count=0;
    uint8_t pending[PD_AUDIO_PENDING_SOUNDS]{};
    int16_t gain=-40*256;
    uint64_t instance=0;
    bool ready=false,fading_out=false;
};
void pd_audio_init(pd_audio_t*,pxa::Context&);
void pd_audio_event(pd_audio_t*,const pxa::Event&);
void pd_audio_tick(pd_audio_t*);
void pd_audio_play(pd_audio_t*,uint8_t);
void pd_audio_set_music(pd_audio_t*,uint8_t);
void pd_audio_stop(pd_audio_t*);
int pd_audio_active(const pd_audio_t*);
} // namespace dungeon
#endif
