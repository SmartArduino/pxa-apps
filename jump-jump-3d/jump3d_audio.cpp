#include "jump3d_audio.hpp"
#include "jump3d_audio_data.hpp"

namespace jump {
static_assert(J3_CHANNEL_BGM <= pxa::AudioSession::sound_track_count);
static int16_t gain_db(uint16_t gain) {
    // Authored Q12 levels, expressed as dB Q8 without a Guest math library.
    if (gain >= J3_GAIN_FULL) return 0;
    if (gain >= J3_GAIN_LOUD) return -2*256;
    if (gain >= J3_GAIN_SOFT) return -5*256;
    if (gain >= J3_GAIN_QUIET) return -9*256;
    if (gain >= J3_GAIN_BGM) return -11*256;
    if (gain) return -20*256;
    return -60*256;
}

// The 13-second music-box clip shares the Host's bounded asset cache with
// fonts and short effects. Release inactive prepared effects before loading
// it; merely replacing one LRU slot leaves the other seven assets pinned.
static void release_idle_sounds(j3_audio_t* audio, uint8_t wanted) {
    for (unsigned slot=0;slot<audio->bank.sounds.size();++slot) {
        if(!audio->bank.sounds[slot] || audio->bank.ids[slot]==wanted)continue;
        bool active=false;
        for(unsigned channel=0;channel<J3_CHANNEL_BGM;++channel)
            active|=audio->voices[channel].clip==audio->bank.ids[slot] &&
                    j3_audio_channel_active(audio,uint8_t(channel));
        if(!active)audio->bank.sounds[slot].reset();
    }
}
void j3_audio_play(j3_audio_t *audio,uint8_t channel,uint8_t clip,uint16_t gain,uint8_t loop) {
    if (!audio || channel>=J3_CHANNEL_COUNT || clip>=J3_CLIP_COUNT ||
        (channel==J3_CHANNEL_BGM && clip!=J3_CLIP_ICON) ||
        (channel!=J3_CHANNEL_BGM && clip==J3_CLIP_ICON)) return;
    j3_audio_voice_t *v=&audio->voices[channel];
    v->clip=clip; v->gain_q12=gain; v->loop=loop!=0; v->requested=1;
    // The current Host voice continues until the replacement is ready.
    v->started=0; v->ends_us=0; v->stop_pending=0;
}
void j3_audio_stop(j3_audio_t *audio,uint8_t channel) {
    if (!audio || channel>=J3_CHANNEL_COUNT) return;
    audio->voices[channel].requested=0;
    audio->voices[channel].stop_pending=1;
    audio->voices[channel].started=0;
    if(channel==J3_CHANNEL_BONUS && audio->voices[channel].clip==J3_CLIP_SING){
        audio->bank.release(J3_CLIP_SING);
        release_idle_sounds(audio,J3_CLIP_COUNT);
    }
}
void j3_audio_stop_all(j3_audio_t *audio) {
    if (!audio) return;
    for (unsigned i=0;i<J3_CHANNEL_COUNT;++i) j3_audio_stop(audio,(uint8_t)i);
    // Shutdown calls this immediately before Guest exit, so send stops now.
    j3_audio_tick(audio,audio->tick_us);
}
int j3_audio_channel_active(const j3_audio_t *audio,uint8_t channel) {
    if (!audio || channel>=J3_CHANNEL_COUNT) return 0;
    const j3_audio_voice_t *v=&audio->voices[channel];
    return v->requested && (!v->started || v->loop || audio->tick_us<v->ends_us);
}
uint8_t j3_audio_channel_clip(const j3_audio_t *audio,uint8_t channel) {
    return j3_audio_channel_active(audio,channel) ? audio->voices[channel].clip : J3_CLIP_COUNT;
}


void j3_audio_start(j3_audio_t* audio,pxa::Context& context) {
    audio->state=J3_AUDIO_WAIT_PERMISSION;audio->bank.start(context,-256,{1500,256,256});
}
static void music_failed(j3_audio_t* audio,pxa::Error error) {
    auto& voice=audio->voices[J3_CHANNEL_BGM];
    voice.started=0;audio->music_ready=false;audio->music_instance=0;
    const bool retry=voice.requested && !voice.stop_pending &&
        arcade::transient_audio_error(error) && audio->music_retries<3;
    if(retry){voice.ends_us=audio->tick_us+(uint64_t(200000)<<audio->music_retries);++audio->music_retries;}
    else voice.requested=0;
    char message[112];std::snprintf(message,sizeof(message),"J3CPP music error=%d retry=%u",int(error),retry?audio->music_retries:0);
    if(audio->bank.context)(void)audio->bank.context->log().write(pxa::LogLevel::warning,message);
}
void j3_audio_event(j3_audio_t* audio,const pxa::Event& event) {
    auto played=pxa::decode_playback(event);
    if(!played||!audio->bank.session||played->session!=audio->bank.session->handle()||played->instance!=audio->music_instance)return;
    if(played->state==pxa::PlaybackState::ready){audio->music_ready=true;audio->music_retries=0;}
    else if(played->state==pxa::PlaybackState::error)music_failed(audio,static_cast<pxa::Error>(played->status));
    else{audio->music_ready=false;audio->music_instance=0;audio->voices[J3_CHANNEL_BGM].started=0;audio->voices[J3_CHANNEL_BGM].requested=0;}
}
void j3_audio_tick(j3_audio_t* audio,uint64_t now) {
    if(audio->state==J3_AUDIO_PAUSED)
        for(auto& voice:audio->voices)if(voice.started)voice.ends_us+=now;
    audio->tick_us=now;
    if(!audio->bank.session){if(audio->bank.unavailable)audio->state=J3_AUDIO_UNAVAILABLE;return;}
    audio->state=J3_AUDIO_READY;auto& session=*audio->bank.session;
    // Stop old Host voices before a replacement load asks the cache to evict
    // their assets. Track order must not let charge precede a bonus stop.
    for(unsigned i=0;i<J3_CHANNEL_COUNT;++i){auto& v=audio->voices[i];
        const auto track=static_cast<pxa::SoundTrack>(i);
        if(v.stop_pending){auto result=i==J3_CHANNEL_BGM?session.control_music(pxa::MusicAction::stop):session.control_sound(track,pxa::MusicAction::stop);
            if(result){v.stop_pending=0;if(i==J3_CHANNEL_BGM){audio->music_instance=0;audio->music_ready=false;}}}
    }
    for(unsigned i=0;i<J3_CHANNEL_COUNT;++i){auto& v=audio->voices[i];
        const auto track=static_cast<pxa::SoundTrack>(i);
        if(v.stop_pending)continue;
        if(!v.requested)continue;
        if(v.started){if(!v.loop&&now>=v.ends_us){v.requested=0;v.started=0;}continue;}
        pxa::Result<void> result;
        if(i==J3_CHANNEL_BGM){
            if(now<v.ends_us)continue;
            auto music=session.music(j3_audio_bank[v.clip].path,v.loop,gain_db(v.gain_q12));
            if(music){audio->music_instance=*music;audio->music_ready=false;audio->music_gain_q12=v.gain_q12;}
            else{music_failed(audio,music.error());continue;}
        }else{
            if(v.clip==J3_CLIP_SING)release_idle_sounds(audio,J3_CLIP_SING);
            auto* asset=audio->bank.prepare(v.clip,j3_audio_bank[v.clip].path);if(!asset)continue;
            result=session.sound_track(*asset,track,v.loop,gain_db(v.gain_q12));}
        if(result){v.started=1;v.ends_us=now+uint64_t(j3_audio_bank[v.clip].samples)*1000000/J3_AUDIO_SAMPLE_RATE;}
        else if(result.error()!=pxa::Error::would_block)v.requested=0;
    }
    if(audio->voices[J3_CHANNEL_BGM].started&&audio->music_ready){uint16_t gain=J3_GAIN_BGM;
        for(unsigned i=J3_CHANNEL_LAND;i<=J3_CHANNEL_BONUS;++i)if(j3_audio_channel_active(audio,i))gain=J3_GAIN_BGM_DUCKED;
        if(gain!=audio->music_gain_q12&&session.control_music(pxa::MusicAction::gain,gain_db(gain)))audio->music_gain_q12=gain;
    }
    if(audio->voices[J3_CHANNEL_BONUS].clip!=J3_CLIP_SING ||
       !j3_audio_channel_active(audio,J3_CHANNEL_BONUS))audio->bank.release(J3_CLIP_SING);
    constexpr uint8_t preload[]={J3_CLIP_SCALE_INTRO,J3_CLIP_SCALE_LOOP,J3_CLIP_SUCCESS,J3_CLIP_POP};
    if(audio->preload_index<std::size(preload)&&audio->bank.prepare(preload[audio->preload_index],j3_audio_bank[preload[audio->preload_index]].path))++audio->preload_index;
}

void j3_audio_pause(j3_audio_t* audio) {
    if(!audio || audio->state==J3_AUDIO_PAUSED)return;
    // Release gameplay effects, retaining the music stream and its position.
    for(unsigned i=0;i<J3_CHANNEL_BGM;++i)j3_audio_stop(audio,uint8_t(i));
    j3_audio_tick(audio,audio->tick_us);
    if(audio->bank.session){
        for(auto& voice:audio->voices)if(voice.started)
            voice.ends_us=voice.ends_us>audio->tick_us?voice.ends_us-audio->tick_us:0;
        audio->state=J3_AUDIO_PAUSED;
    }
    audio->bank.pause();
}
void j3_audio_resume(j3_audio_t* audio) {if(audio)audio->bank.resume();}

} // namespace jump
