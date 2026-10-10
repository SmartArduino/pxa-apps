#include "audio.hpp"
#include "game.hpp"

namespace dungeon {
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

static const pd_sound_file_t sound_files[PD_SOUND_COUNT]{
    {"assets/sfx/hit.pcm", -3 * 256},
    {"assets/sfx/miss.pcm", -6 * 256},
    {"assets/sfx/hit_crush.pcm", -3 * 256},
    {"assets/sfx/death.pcm", -3 * 256},
    {"assets/sfx/drink.pcm", -4 * 256},
    {"assets/sfx/eat.pcm", -5 * 256},
    {"assets/sfx/gold.pcm", -5 * 256},
    {"assets/sfx/item.pcm", -6 * 256},
    {"assets/sfx/read.pcm", -6 * 256},
    {"assets/sfx/descend.pcm", -3 * 256},
    {"assets/sfx/door_open.pcm", -6 * 256},
    {"assets/sfx/trap.pcm", -3 * 256},
    {"assets/sfx/levelup.pcm", -3 * 256},
    {"assets/sfx/shatter.pcm", -4 * 256},
    {"assets/sfx/unlock.pcm", -4 * 256},
    {"assets/sfx/health_warn.pcm", -6 * 256},
    {"assets/sfx/step.pcm", -8 * 256},
    {"assets/sfx/grass.pcm", -7 * 256},
    {"assets/sfx/trample.pcm", -6 * 256},
    {"assets/sfx/water.pcm", -7 * 256},
    {"assets/sfx/hit_magic.pcm", -5 * 256},
    {"assets/sfx/lightning.pcm", -6 * 256},
    {"assets/sfx/blast.pcm", -5 * 256},
};


void pd_audio_init(pd_audio_t* audio,pxa::Context& context){audio->bank.start(context,-2*256,{120,0,128});}
void pd_audio_event(pd_audio_t* audio,const pxa::Event& event){
    auto played=pxa::decode_playback(event);
    if(!played||!audio->bank.session||played->session!=audio->bank.session->handle()||played->instance!=audio->instance)return;
    if(played->state==pxa::PlaybackState::ready)audio->ready=true;
    else{audio->ready=false;audio->instance=0;}
}
static void start_music(pd_audio_t* audio,uint8_t track){
    if(track>PD_MUSIC_TITLE||!audio->bank.session)return;
    auto music=audio->bank.session->music(music_files[track],true,-40*256);
    if(music){audio->instance=*music;audio->ready=false;audio->music_track=track;audio->music_pending=0xff;audio->fading_out=false;audio->gain=-40*256;}
}
void pd_audio_play(pd_audio_t* audio,uint8_t sound){if(sound<PD_SOUND_COUNT&&audio->pending_count<PD_AUDIO_PENDING_SOUNDS)audio->pending[audio->pending_count++]=sound;}
void pd_audio_set_music(pd_audio_t* audio,uint8_t track){
    if(track>PD_MUSIC_TITLE){if(audio->bank.session)(void)audio->bank.session->control_music(pxa::MusicAction::stop);audio->instance=0;audio->music_track=audio->music_pending=0xff;return;}
    if(audio->music_track==track&&(audio->instance||!audio->bank.session)){audio->music_pending=0xff;audio->fading_out=false;return;}
    if(audio->music_pending==track)return;
    audio->music_pending=track;audio->fading_out=audio->instance!=0;
}
int pd_audio_active(const pd_audio_t* audio){return audio->bank.session.has_value();}
void pd_audio_stop(pd_audio_t* audio){audio->pending_count=0;if(audio->bank.session)(void)audio->bank.session->control(pxa::MusicAction::pause);}
void pd_audio_tick(pd_audio_t* audio){
    if(!audio->bank.session)return;auto& session=*audio->bank.session;
    if(audio->music_pending==0xff&&!audio->instance&&audio->music_track<=PD_MUSIC_TITLE)audio->music_pending=audio->music_track;
    if(audio->music_pending!=0xff&&!audio->fading_out)start_music(audio,audio->music_pending);
    if(audio->instance&&audio->ready){int16_t next=audio->gain;
        if(audio->fading_out){next-=2*256;if(next<=-40*256){start_music(audio,audio->music_pending);next=audio->gain;}}
        else next=std::min<int>(next+2*256,-7*256);
        if(next!=audio->gain&&session.control_music(pxa::MusicAction::gain,next))audio->gain=next;
    }
    while(audio->pending_count){auto id=audio->pending[0];auto* sound=audio->bank.prepare(id,sound_files[id].path);if(!sound&&!audio->bank.failed[id])break;
        auto result=sound?session.sound(*sound,sound_files[id].gain_db_q8):pxa::Result<void>{std::unexpected(pxa::Error::not_found)};if(!result&&result.error()==pxa::Error::would_block)break;
        for(unsigned i=1;i<audio->pending_count;++i)audio->pending[i-1]=audio->pending[i];--audio->pending_count;
    }
}

} // namespace dungeon
