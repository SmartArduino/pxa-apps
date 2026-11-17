// SPDX-License-Identifier: MIT
#pragma once
#include "../common/motion-games/sound.hpp"
namespace iron {
// Prepared effects and streamed music live on the Host, outside the game heap.
struct GameAudio:motion::Sounds {
 inline static constexpr const char* files[]={"ui","order","rifle","tank","artillery","rocket","impact","build","ready","upgrade"};
 std::array<std::optional<pxa::Asset>,10> clips;
 std::array<uint64_t,6> next_effect{};
 uint64_t milliseconds=0,music_instance=0,retry_music=0;int music_track=-1;float music_gain=-40;
 bool music_ready=false,music_paused=false,opening=false;unsigned accepted_clips=0;
 pxa::Task<void> prepare(){
  unsigned loaded=0;for(unsigned i=0;i<clips.size();++i){if(!session)break;char path[80];std::snprintf(path,sizeof path,"assets/audio/%s.pcm",files[i]);auto clip=co_await context->assets().load(pxa::AssetKind::audio,path);if(clip){clips[i].emplace(std::move(*clip));++loaded;}else{char text[100];std::snprintf(text,sizeof text,"Iron Dawn audio prepare %s error=%d",files[i],int(clip.error()));report(text);}}
  char text[80];std::snprintf(text,sizeof text,"Iron Dawn generated effects ready=%u/10",loaded);report(text);co_return pxa::Result<void>{};
 }
 pxa::Task<void> open(pxa::Context& ctx){auto result=co_await motion::Sounds::open(ctx);if(session)co_await prepare();co_return result;}
 pxa::Task<void> reopen(){
  opening=true;auto grant=co_await context->permissions().acquire("audio.playback","media");if(grant){permission.emplace(std::move(*grant));auto audio=co_await context->audio().open(*permission);if(audio){auto graph=co_await audio->graph(0);if(graph){session.emplace(std::move(*audio));denied=false;co_await prepare();}}}opening=false;co_return pxa::Result<void>{};
 }
 void stop_clips(){if(session)for(unsigned i=0;i<6;++i)(void)session->control_sound(pxa::SoundTrack(i),pxa::MusicAction::stop);}
 void stop(){stop_clips();motion::Sounds::stop();if(session)(void)session->control_music(pxa::MusicAction::stop);music_instance=0;music_track=-1;music_ready=music_paused=false;}
 void suspend(){stop_clips();motion::Sounds::suspend();if(session&&music_instance){(void)session->control_music(pxa::MusicAction::pause);music_paused=true;}}
 void resume(){motion::Sounds::resume();}
 void toggle(){enabled=!enabled;if(!enabled)stop();else if(!session&&!opening){opening=true;auto task=context->tasks().start(reopen());if(!task)opening=false;}else play(motion::Cue::click);save_pending=true;if(!saving&&context){saving=true;if(!context->tasks().start(persist()))saving=false;}report(enabled?"Iron Dawn audio enabled=1":"Iron Dawn audio enabled=0");}
 void effect(unsigned id){
  if(!audible()||id>=clips.size())return;unsigned track=id==2?0:id>=3&&id<=5?1:id==6?2:id<=1?3:id==7?4:5;
  if(milliseconds<next_effect[track])return;next_effect[track]=milliseconds+(track==0?85:track==1?130:track==2?180:track==3?65:250);
  if(!clips[id]){motion::Sounds::play(id==6?motion::Cue::impact:id>=8?motion::Cue::star:motion::Cue::click);return;}
  auto sent=session->sound_track(*clips[id],pxa::SoundTrack(track),false,(id<=1?-15:id==4||id==6?-12:-10)*256);if(sent){if(!accepted_clips)report("Iron Dawn generated effect playing");++accepted_clips;}
 }
 void play(motion::Cue cue,unsigned=0){using C=motion::Cue;effect(cue==C::impact?6:cue==C::star||cue==C::win?9:cue==C::settle?8:cue==C::dice?3:0);}
 void play_event(unsigned cue){switch(cue){case 1:effect(1);break;case 2:case 8:effect(3);break;case 3:effect(6);break;case 4:effect(9);break;case 5:case 11:effect(8);break;case 6:effect(2);break;case 7:effect(5);break;case 9:effect(4);break;case 10:effect(7);break;}}
 void event(const pxa::Event& e){
  motion::Sounds::event(e);if(!session){music_instance=0;music_track=-1;music_ready=false;for(auto& clip:clips)clip.reset();return;}
  auto played=pxa::decode_playback(e);if(!played||played->session!=session->handle()||played->instance!=music_instance)return;
  if(played->state==pxa::PlaybackState::ready){music_ready=true;report(music_track?"Iron Dawn battle music ready":"Iron Dawn command music ready");}
  else if(played->state!=pxa::PlaybackState::replaced){music_instance=0;music_ready=false;retry_music=milliseconds+5000;if(played->state==pxa::PlaybackState::error)report("Iron Dawn music decode failed");}
 }
 void tick(uint64_t now,float dt,bool battle,bool paused){
  milliseconds=now/1000;if(!audible())return;
  if(paused){if(!music_paused){stop_clips();if(music_instance)(void)session->control_music(pxa::MusicAction::pause);music_paused=true;}return;}
  if(music_paused){if(music_instance)(void)session->control_music(pxa::MusicAction::resume);music_paused=false;}
  int target=battle?1:0;
  if(music_instance&&music_track!=target){music_gain=std::max(-44.f,music_gain-dt*48);(void)session->control_music(pxa::MusicAction::gain,int16_t(music_gain*256));if(music_gain<=-44){(void)session->control_music(pxa::MusicAction::stop);music_instance=0;music_ready=false;}}
  if(!music_instance&&milliseconds>=retry_music){auto music=session->music(target?"assets/audio/battle.ogg":"assets/audio/command.ogg",true,-40*256);if(music){music_instance=*music;music_track=target;music_gain=-40;music_ready=false;}else{retry_music=milliseconds+5000;report("Iron Dawn music start deferred");}}
  if(music_instance&&music_ready&&music_track==target){float gain=std::min(battle?-12.f:-10.f,music_gain+dt*18);if(gain!=music_gain){if(session->control_music(pxa::MusicAction::gain,int16_t(gain*256)))music_gain=gain;}}
 }
};
}
