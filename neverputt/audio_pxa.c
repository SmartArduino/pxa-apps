/* PXA SDL_mixer backend. GPL-3.0-or-later. */
#include <SDL_mixer.h>
#include <string.h>
#include "audio.h"
#include "config.h"
#include "fs_rwops.h"
#include "port.h"
static Mix_Music *music;
static struct { char name[256]; Mix_Chunk *chunk; unsigned used; } sounds[24];
static unsigned stamp;
static int ready,sound_volume=80,music_volume=64;
static char queued[256];static float queue_fade;
void audio_volume(int sound,int song) {
  sound_volume=SDL_clamp(sound,0,10)*MIX_MAX_VOLUME/10;
  music_volume=SDL_clamp(song,0,10)*MIX_MAX_VOLUME/10;
  if(ready){Mix_Volume(-1,sound_volume);Mix_VolumeMusic(music_volume);}
}
void audio_init(void) {
  if(ready)return;
  if(Mix_OpenAudio(22050,AUDIO_S16SYS,2,512)<0){port_log("Neverputt audio: %s",Mix_GetError());return;}
  ready=1;Mix_AllocateChannels(12);
  audio_volume(config_get_d(CONFIG_SOUND_VOLUME),config_get_d(CONFIG_MUSIC_VOLUME));
}
void audio_music_stop(void) {
  queued[0]=0;
  if(music){Mix_HaltMusic();Mix_FreeMusic(music);music=NULL;}
}
void audio_free(void) {
  audio_music_stop();if(!ready)return;
  Mix_HaltChannel(-1);
  for(unsigned i=0;i<24;++i){Mix_FreeChunk(sounds[i].chunk);memset(&sounds[i],0,sizeof(sounds[i]));}
  Mix_CloseAudio();ready=0;
}
void audio_play(const char *name,float gain) {
  if(!ready || !name || gain<=0)return;
  unsigned slot=24,oldest=~0u;
  for(unsigned i=0;i<24;++i){
    if(sounds[i].chunk && !strcmp(name,sounds[i].name)){slot=i;break;}
    int active=0;for(int c=0;c<12;++c)if(Mix_Playing(c) && Mix_GetChunk(c)==sounds[i].chunk)active=1;
    if(!active && sounds[i].used<oldest){oldest=sounds[i].used;slot=i;}
  }
  if(slot==24)return;
  if(!sounds[slot].chunk || strcmp(name,sounds[slot].name)){
    SDL_RWops *rw=fs_rwops_open(name,"r");if(!rw)return;
    Mix_Chunk *chunk=Mix_LoadWAV_RW(rw,1);if(!chunk)return;
    Mix_FreeChunk(sounds[slot].chunk);sounds[slot].chunk=chunk;
    SDL_strlcpy(sounds[slot].name,name,sizeof(sounds[slot].name));
  }
  sounds[slot].used=++stamp;
  int channel=Mix_PlayChannel(-1,sounds[slot].chunk,0);
  if(channel>=0)Mix_Volume(channel,(int)(sound_volume*SDL_clamp(gain,0.f,1.f)));
}
void audio_music_play(const char *name) {
  audio_music_stop();if(!ready || !name || !*name)return;
  SDL_RWops *rw=fs_rwops_open(name,"r");if(!rw)return;
  music=Mix_LoadMUS_RW(rw,1);
  if(music){Mix_VolumeMusic(music_volume);Mix_PlayMusic(music,-1);}
}
void audio_music_queue(const char *name,float seconds) {
  if(!name)return;
  SDL_strlcpy(queued,name,sizeof(queued));queue_fade=seconds;
  if(ready && Mix_PlayingMusic())Mix_FadeOutMusic((int)(seconds*1000));
}
void audio_music_fade_in(float seconds) {
  if(ready && music){Mix_VolumeMusic(music_volume);Mix_FadeInMusic(music,-1,(int)(seconds*1000));}
}
void audio_music_fade_out(float seconds) {if(ready)Mix_FadeOutMusic((int)(seconds*1000));}
void audio_music_fade_to(float seconds,const char *name) {audio_music_queue(name,seconds);}
void audio_timer(float dt) {
  (void)dt;
  if(ready && queued[0] && !Mix_PlayingMusic()){
    char name[sizeof(queued)];SDL_strlcpy(name,queued,sizeof(name));float fade=queue_fade;
    audio_music_play(name);if(fade>0)audio_music_fade_in(fade);
  }
}
