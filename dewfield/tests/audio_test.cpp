// SPDX-License-Identifier: MIT
#include "pxa_audio.h"
#include "pxa_permission.h"
#include "../sound_design.hpp"
#include <cassert>
#include <vector>
#include <cstring>
#include <cstdio>
static uint8_t packet[1024];
static constexpr uint64_t SOUND_PERMISSION=1,SOUND_OPEN=2,SOUND_GAIN=3;
static bool running=true,background=false,paused=false,exit_requested=false;
static uint64_t fake_now=2000000;
static uint64_t now_us(){return fake_now;}
struct Call{uint32_t op;std::vector<uint8_t> bytes;};
static std::vector<Call> calls;
static bool legacy=false;
extern "C" int32_t pxa_call(uint32_t,uint64_t,const uint8_t*,uint32_t){return 0;}
extern "C" int32_t pxa_io(uint64_t,uint32_t op,uint8_t*data,uint32_t length){
 calls.push_back({op,{data,data+length}});
 if(op==PXA_AUDIO_IO_PLAY_EFFECT&&legacy)return PXA_STATUS_UNSUPPORTED;
 if(op==PXA_AUDIO_IO_PLAY_MUSIC)pxa_store_u64(data,0x100000007ull);
 return length;
}
#include "../sound.inc"
int main(){
 sound_ready=true;sound_session=0x100000001ull;
 play_effect(dewfield_audio::Sfx::Axe);assert(calls.size()==1&&calls.back().op==PXA_AUDIO_IO_PLAY_EFFECT&&calls.back().bytes[0]==2&&calls.back().bytes[1]==2);
 play_effect(dewfield_audio::Sfx::Axe);assert(calls.size()==1);fake_now+=100000;
 legacy=true;play_effect(dewfield_audio::Sfx::Pick);assert(!effect_packets&&calls.size()==5);
 for(size_t i=2;i<calls.size();++i)assert(calls[i].op==PXA_AUDIO_IO_PLAY_TONE);
 calls.clear();effects_wanted=false;play_effect(dewfield_audio::Sfx::Eat);assert(calls.empty());effects_wanted=true;background=true;play_effect(dewfield_audio::Sfx::Gift);assert(calls.empty());background=false;
 music_wanted=true;wanted_music=0;service_sound();assert(music_instance&&playing_music==0&&calls.back().op==PXA_AUDIO_IO_PLAY_MUSIC);
 wanted_music=3;service_sound();assert(playing_music==3&&calls[calls.size()-2].op==PXA_AUDIO_IO_CONTROL_MUSIC&&calls.back().op==PXA_AUDIO_IO_PLAY_MUSIC);
 paused=true;service_sound();assert(music_paused&&calls.back().op==PXA_AUDIO_IO_CONTROL_MUSIC);paused=false;service_sound();assert(!music_paused);
 music_wanted=false;service_sound();assert(!music_instance);
 auto n=calls.size();fake_now+=200000;play_effect(dewfield_audio::Sfx::Eat);assert(calls.size()==n+3); // SFX stay on with BGM off.
 for(auto &call:calls)assert(call.op!=PXA_AUDIO_IO_WRITE&&call.op!=PXA_AUDIO_IO_MIX_PCM);
 puts("PASS: official audio wire payloads, bounded cooldown, legacy tone fallback, independent effects/BGM, scene score replacement, pause/resume and no Guest PCM writes");
}
