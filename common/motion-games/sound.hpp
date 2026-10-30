// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/app.hpp>
#include <pxa/audio.hpp>
#include <pxa/permission.hpp>
#include "codec.hpp"
#include <algorithm>
#include <cstdio>

namespace motion {
enum class Cue {click, paper, bamboo, dice, settle, favorite, star, win, countdown, go, boost, impact, lap};

// Short original procedural sounds. Host effect tracks keep latency bounded;
// a busy admission drops the obsolete cue instead of queueing it for later.
struct Sounds {
 pxa::Context* context=nullptr;
 std::optional<pxa::Permission> permission;
 std::optional<pxa::AudioSession> session;
 bool enabled=true,suspended=false,denied=false,saving=false,save_pending=false;
 unsigned accepted=0,dropped=0;bool traced=false;uint64_t next_engine=0;
 static constexpr unsigned preference_size=16;
 static constexpr auto engine_track=pxa::EffectTrack::track0;
 static constexpr auto game_track=pxa::EffectTrack::track1;
 static constexpr auto ui_track=pxa::EffectTrack::track2;

 pxa::Task<void> open(pxa::Context& ctx){
  context=&ctx;std::array<std::byte,preference_size> data{};
  auto size=co_await ctx.storage().get("motion-sound-v1",data);
  if(size&&*size==data.size()&&get32(data.data())==0x53465831&&unsigned(data[4])<=1&&get32(data.data()+12)==checksum(std::span(data).first(12))){
   bool valid=true;for(unsigned i=5;i<12;++i)valid&=data[i]==std::byte{};if(valid)enabled=bool(data[4]);}
  // Muting persists independently of gameplay saves. If access was never
  // granted, a muted launch does not ask for a permission it will not use.
  if(!enabled){auto allowed=co_await ctx.permissions().check("audio.playback","media");if(!allowed||!*allowed)co_return pxa::Result<void>{};}
  auto grant=co_await ctx.permissions().acquire("audio.playback","media");
  if(!grant){denied=true;report("Motion sound unavailable");co_return pxa::Result<void>{};}
  permission.emplace(std::move(*grant));auto opened=co_await ctx.audio().open(*permission);
  if(!opened){permission.reset();report("Motion sound unavailable");co_return pxa::Result<void>{};}
  auto configured=co_await opened->graph(0);if(!configured){permission.reset();report("Motion sound graph unavailable");co_return pxa::Result<void>{};}
  session.emplace(std::move(*opened));report("Motion sound ready");co_return pxa::Result<void>{};
 }
 void report(std::string_view text){if(context)(void)context->log().write(pxa::LogLevel::info,text);}
 bool audible()const{return enabled&&!suspended&&context&&context->foreground()&&session.has_value();}
 const char* label(bool compact=false)const{
  if(!enabled)return compact?"音效关":"音效：关闭";
  if(!session)return compact?"待授权":"音效：未授权";
  return compact?"音效开":"音效：开启";
 }
 void stop_engine(){next_engine=0;if(session)(void)session->stop_effect(engine_track);}
 void stop(){next_engine=0;if(session){(void)session->stop_effect(engine_track);(void)session->stop_effect(game_track);(void)session->stop_effect(ui_track);}}
 void suspend(){suspended=true;stop();}
 void resume(){suspended=false;next_engine=0;}
 void event(const pxa::Event& event){auto revoked=pxa::decode_permission_revoked(event);if(revoked&&revoked->name=="audio.playback"){stop();session.reset();permission.reset();denied=true;report("Motion sound revoked");}}
 pxa::Task<void> persist(){while(save_pending){save_pending=false;std::array<std::byte,preference_size> data{};put32(data.data(),0x53465831);data[4]=std::byte(enabled);put32(data.data()+12,checksum(std::span(data).first(12)));auto saved=co_await context->storage().set("motion-sound-v1",data);if(!saved){report("Motion sound preference save failed");break;}}saving=false;co_return pxa::Result<void>{};}
 void toggle(){enabled=!enabled;if(!enabled)stop();else play(Cue::click);save_pending=true;if(!saving&&context){saving=true;if(!context->tasks().start(persist()))saving=false;}report(enabled?"Motion sound enabled=1":"Motion sound enabled=0");}
 static pxa::Tone tone(unsigned hz,unsigned ms,int db,pxa::Waveform waveform=pxa::Waveform::sine,unsigned delay=0,unsigned release=12){return {uint16_t(hz),uint16_t(ms),int16_t(db*256),waveform,3,uint16_t(std::min(ms,release)),uint16_t(delay)};}
 void submit(std::span<const pxa::Tone> tones,pxa::EffectTrack track){
  if(!audible())return;auto sent=session->effect(tones,track);if(sent){if(!accepted)report("Motion sound playing");++accepted;}else{if(!dropped){char message[80];std::snprintf(message,sizeof message,"Motion sound effect failed error=%d",int(sent.error()));report(message);}++dropped;}
 }
 void play(Cue cue,unsigned variation=0){
  if(!traced){traced=true;char state[120];std::snprintf(state,sizeof state,"Motion sound state enabled=%u suspended=%u foreground=%u available=%u",enabled,suspended,context&&context->foreground(),session.has_value());report(state);}
  if(!audible())return;
  std::array<pxa::Tone,5> notes{};unsigned n=0;auto track=game_track;
  using W=pxa::Waveform;
  switch(cue){
   case Cue::click:notes[n++]=tone(880,24,-30,W::triangle);track=ui_track;break;
   case Cue::paper:notes[n++]=tone(1800,32,-34,W::noise,0,24);track=ui_track;break;
   case Cue::bamboo:notes[n++]=tone(620+variation%4*80,32,-25,W::triangle,0,27);notes[n++]=tone(1700,22,-34,W::noise,2,18);break;
   case Cue::dice:notes[n++]=tone(240+variation%3*90,28,-27,W::triangle,0,23);notes[n++]=tone(1100,28,-32,W::noise,0,22);break;
   case Cue::settle:notes[n++]=tone(659,180,-24,W::sine,0,135);notes[n++]=tone(880,200,-26,W::sine,65,155);notes[n++]=tone(1109,230,-29,W::sine,140,180);break;
   case Cue::favorite:notes[n++]=tone(1175,105,-27,W::sine,0,80);notes[n++]=tone(1568,130,-30,W::sine,55,100);track=ui_track;break;
   case Cue::star:notes[n++]=tone(659+variation%3*220,100,-25,W::sine,0,70);notes[n++]=tone(1319,145,-29,W::sine,55,110);break;
   case Cue::win:for(unsigned i=0;i<4;++i){constexpr unsigned hz[]={784,988,1175,1568};notes[n++]=tone(hz[i],210,-25,W::sine,i*85,155);}break;
   case Cue::countdown:notes[n++]=tone(variation==1?660:440,70,-26,W::sine,0,24);break;
   case Cue::go:notes[n++]=tone(880,160,-23,W::sine,0,70);notes[n++]=tone(1175,190,-28,W::sine,45,100);break;
   case Cue::boost:notes[n++]=tone(330,80,-29,W::triangle,0,45);notes[n++]=tone(660,100,-28,W::triangle,65,65);notes[n++]=tone(990,130,-30,W::sine,130,85);break;
   case Cue::impact:notes[n++]=tone(110,65,-26,W::triangle,0,55);notes[n++]=tone(600,35,-33,W::noise,0,28);break;
   case Cue::lap:notes[n++]=tone(784,130,-27,W::sine,0,90);notes[n++]=tone(1047,150,-27,W::sine,85,110);break;
  }
  submit(std::span(notes).first(n),track);
 }
 void engine(unsigned speed,uint64_t now){
  if(!audible()||now<next_engine)return;next_engine=now+200000;
  unsigned hz=85+std::min(speed,100u)*2;std::array<pxa::Tone,2> notes{tone(hz,230,-36,pxa::Waveform::triangle,0,30),tone(hz*2,230,-44,pxa::Waveform::sine,0,30)};submit(notes,engine_track);
 }
};
}
