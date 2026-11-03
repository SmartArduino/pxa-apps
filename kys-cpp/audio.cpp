#include "Audio.h"
#include "GameUtil.h"
#include <algorithm>
// Decode only the current music and requested effects. All audio belongs to
// the guest and reaches the Host through the shared SDL/PXA PCM bridge.
Audio::Audio() {
    Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3);
    if (Mix_OpenAudio(16000,AUDIO_S16SYS,2,512)<0) LOG("Audio open failed: {}\n",SDL_GetError());
    Mix_AllocateChannels(16);
    init();
}
Audio::~Audio() {
    Mix_HaltMusic();Mix_HaltChannel(-1);
    if(current_music_)Mix_FreeMusic(current_music_);
    for(auto c:asound_)if(c)Mix_FreeChunk(c);
    for(auto c:esound_)if(c)Mix_FreeChunk(c);
    for(auto [id,c]:voice_)if(c)Mix_FreeChunk(c);
    Mix_CloseAudio();Mix_Quit();
}
void Audio::init() {current_music_=nullptr;current_sound_=nullptr;asound_.resize(100);esound_.resize(100);}
MUSIC Audio::loadMusic(const std::string& path) {return Mix_LoadMUS(path.c_str());}
WAV Audio::loadWav(const std::string& path) {return Mix_LoadWAV(path.c_str());}
void Audio::saveCurrentMusicPosition() {
    if(current_music_index_>=0 && current_music_) {
        double s=Mix_GetMusicPosition(current_music_);
        if(s>=0)music_positions_[current_music_index_]=Sint64(s*1000);
    }
}
void Audio::playMusic(int num) {
    if(num==current_music_index_ && Mix_PlayingMusic())return;
    saveCurrentMusicPosition();Mix_HaltMusic();
    if(current_music_)Mix_FreeMusic(current_music_);
    current_music_=nullptr;current_music_index_=-1;
    if(num<0)return;
    for(const char* ext:{"ogg","mp3","wav"}) {
        auto path=std::format("{}music/{}.{}",GameUtil::PATH(),num,ext);
        if(filefunc::fileExist(path) && (current_music_=loadMusic(path)))break;
    }
    if(!current_music_) {LOG("Music {} unavailable; MIDI requires offline rendering\n",num);return;}
    current_music_index_=num;playMusic(current_music_);
    if(music_positions_.contains(num))Mix_SetMusicPosition(music_positions_[num]/1000.0);
}
void Audio::playMusic(MUSIC m) {if(m){Mix_VolumeMusic(volume_*MIX_MAX_VOLUME/100);Mix_FadeInMusic(m,-1,50);}}
void Audio::playWav(WAV w,int volume,int channel) {
    if(!w)return;
    if(volume<0 || volume>100)volume=volume_wav_;
    Mix_VolumeChunk(w,volume*MIX_MAX_VOLUME/100);Mix_PlayChannel(channel,w,0);current_sound_=w;
}
void Audio::playASound(int num,int v) {
    if(num<0 || num>=asound_.size())return;
    if(!asound_[num])asound_[num]=loadWav(std::format("{}sound/atk{:02}.wav",GameUtil::PATH(),num));
    playWav(asound_[num],v);
}
void Audio::playESound(int num,int v) {
    if(num<0 || num>=esound_.size())return;
    if(!esound_[num])esound_[num]=loadWav(std::format("{}sound/e{:02}.wav",GameUtil::PATH(),num));
    playWav(esound_[num],v);
}
void Audio::playVoice(int num,int v) {
    stopWav();
    if(!voice_.contains(num)) {
        auto path=std::format("{}voice/{}.wav",GameUtil::PATH(),num);
        if(!filefunc::fileExist(path))path=std::format("{}voice/{}.mp3",GameUtil::PATH(),num);
        voice_[num]=loadWav(path);
    }
    playWav(voice_[num],v,voice_channel_);
}
void Audio::pauseMusic(){Mix_PauseMusic();}
void Audio::resumeMusic(){Mix_ResumeMusic();}
void Audio::stopMusic(){saveCurrentMusicPosition();Mix_HaltMusic();current_music_index_=-1;}
void Audio::stopWav(){Mix_HaltChannel(voice_channel_);}
void Audio::setVolume(int v){volume_=std::clamp(v,0,100);Mix_VolumeMusic(volume_*MIX_MAX_VOLUME/100);}
