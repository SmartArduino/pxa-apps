/* SPDX-License-Identifier: MIT
 * Explicit closed/no-device SDL_mixer provider.
 * Original engine's supported EnableSound=0 path retains all game logic.
 * No load/play call pretends to succeed; no audio resource is created.
 */
#include "SDL_mixer.h"
static int music_volume=MIX_MAX_VOLUME;
int Mix_OpenAudio(int frequency,Uint16 format,int channels,int chunksize){
 (void)frequency;(void)format;(void)channels;(void)chunksize;
 SDL_SetError("No audio device configured for this PXA port");return -1;
}
void Mix_CloseAudio(void){}
int Mix_QuerySpec(int *frequency,Uint16 *format,int *channels){
 if(frequency)*frequency=0;if(format)*format=0;if(channels)*channels=0;return 0;
}
Mix_Music *Mix_LoadMUS(const char *file){(void)file;SDL_SetError("Audio device is closed");return NULL;}
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *source,int freesrc){
 if(source&&freesrc)SDL_RWclose(source);SDL_SetError("Audio device is closed");return NULL;
}
int Mix_PlayMusic(Mix_Music *music,int loops){(void)music;(void)loops;SDL_SetError("Audio device is closed");return -1;}
int Mix_PlayChannelTimed(int channel,Mix_Chunk *chunk,int loops,int ticks){(void)channel;(void)chunk;(void)loops;(void)ticks;SDL_SetError("Audio device is closed");return -1;}
int Mix_HaltMusic(void){return 0;}
void Mix_FreeMusic(Mix_Music *music){if(music)SDL_SetError("No mixer-owned music exists");}
void Mix_FreeChunk(Mix_Chunk *chunk){if(chunk)SDL_SetError("No mixer-owned chunk exists");}
void Mix_HookMusic(void (*fn)(void*,Uint8*,int),void *arg){(void)fn;(void)arg;}
int Mix_VolumeMusic(int volume){int old=music_volume;if(volume>=0)music_volume=volume>MIX_MAX_VOLUME?MIX_MAX_VOLUME:volume;return old;}
int Mix_VolumeChunk(Mix_Chunk *chunk,int volume){if(!chunk)return -1;int old=chunk->volume;if(volume>=0)chunk->volume=(Uint8)(volume>MIX_MAX_VOLUME?MIX_MAX_VOLUME:volume);return old;}
