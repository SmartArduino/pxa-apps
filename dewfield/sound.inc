// SPDX-License-Identifier: MIT
// Existing PXA streamed-music path; decoding and buffering stay on the Host.
uint64_t sound_permission, sound_session, music_instance;
bool sound_pending, sound_ready, sound_failed, music_wanted = true;
bool music_paused,effects_wanted=true,effect_packets=true;
int wanted_music=0,playing_music=-1;
uint64_t last_effect_us[6]{};
void request_sound() {
  if (!(music_wanted||effects_wanted) || sound_pending || sound_ready || sound_failed) return;
  constexpr char name[]="audio.playback";
  constexpr uint8_t scope[]={'m','e','d','i','a'};
  uint32_t n;
  sound_pending=true;
  if(!pxa_permission_build(packet,sizeof(packet),PXA_PERMISSION_ACQUIRE,
       SOUND_PERMISSION,name,sizeof(name)-1,scope,sizeof(scope),&n) ||
       pxa_control_packet(packet,n)<0) {sound_pending=false;sound_failed=true;}
}
void clear_sound() {
  if(sound_ready&&effect_packets)for(int track=0;track<6;++track)pxa_audio_stop_effect(sound_session,track);
  if(sound_ready && music_instance && !music_paused) {
    if(pxa_audio_control_music(sound_session,PXA_AUDIO_ASSET_PAUSE,0)>=0) music_paused=true;
  }
}
void service_sound() {
  request_sound();
  if(!sound_ready) return;
  if(music_instance&&playing_music!=wanted_music){if(pxa_audio_control_music(sound_session,PXA_AUDIO_ASSET_STOP,0)>=0)music_instance=0;}
  bool audible=music_wanted && running && !background && !paused && !exit_requested;
  if(!music_wanted && music_instance) {
    if(pxa_audio_control_music(sound_session,PXA_AUDIO_ASSET_STOP,0)>=0) music_instance=0;
    return;
  }
  if(!music_instance && audible && !sound_failed) {
    const char *tracks[]={"assets/dewfield-morning.ogg","assets/dewfield-evening.ogg","assets/dewfield-interior.ogg","assets/dewfield-cave.ogg","assets/dewfield-rain.ogg"};
    int status=pxa_audio_play_music(sound_session,tracks[wanted_music],1,-12*256,&music_instance);
    if(status>=0)playing_music=wanted_music;
    if(status<0 && status!=PXA_STATUS_WOULD_BLOCK) {
      sound_failed=true;pxa_log_write(3,"Dewfield: music not available");
    }
    music_paused=false;
  } else if(music_instance && !audible && !music_paused) clear_sound();
  else if(music_instance && audible && music_paused) {
    if(pxa_audio_control_music(sound_session,PXA_AUDIO_ASSET_RESUME,0)>=0) music_paused=false;
  }
}
int32_t handle_sound(const pxa_event_t &e) {
  if(e.token==SOUND_PERMISSION && e.service==PXA_PERMISSION_SERVICE) {
    pxa_permission_acquire_result_t r{};
    if(!pxa_permission_parse_acquire(&e,SOUND_PERMISSION,&r) || r.status) {
      sound_pending=false;sound_failed=true;return PXA_EVENT_HANDLED;
    }
    sound_permission=r.handle;
    if(pxa_audio_open_media(SOUND_OPEN,r.handle)<0) {sound_pending=false;sound_failed=true;}
    return PXA_EVENT_HANDLED;
  }
  if(e.token==SOUND_OPEN && e.service==PXA_AUDIO_SERVICE) {
    pxa_audio_open_result_t r{};
    if(!pxa_audio_parse_open(&e,SOUND_OPEN,&r) || r.status) {
      sound_pending=false;sound_failed=true;return PXA_EVENT_HANDLED;
    }
    sound_session=r.handle;
    if(pxa_audio_commit_gain(SOUND_GAIN,r.handle,-2*256)<0) {sound_pending=false;sound_failed=true;}
    return PXA_EVENT_HANDLED;
  }
  if(e.token==SOUND_GAIN && e.service==PXA_AUDIO_SERVICE) {
    int32_t status;
    sound_ready=pxa_audio_parse_status(&e,SOUND_GAIN,PXA_AUDIO_COMMIT_GRAPH,&status) && status==0;
    sound_pending=false;sound_failed=!sound_ready;
    return PXA_EVENT_HANDLED;
  }
  pxa_audio_playback_event_t playback{};
  if(pxa_audio_parse_playback(&e,&playback)) {
    if(playback.instance==music_instance) {
      if(playback.state==PXA_AUDIO_PLAYBACK_READY) pxa_log_write(2,"Dewfield: music decoder ready");
      if(playback.state==PXA_AUDIO_PLAYBACK_ERROR) {music_instance=0;sound_failed=true;pxa_log_write(3,"Dewfield: music decoder failed");}
    }
    return PXA_EVENT_HANDLED;
  }
  return PXA_EVENT_UNHANDLED;
}
void add_tone(float, float, float) {}

void play_effect(dewfield_audio::Sfx effect) {
  if(!effects_wanted||!sound_ready||!running||background||paused||exit_requested)return;
  auto index=unsigned(effect);if(index>=unsigned(dewfield_audio::Sfx::Count))return;
  const auto &design=dewfield_audio::sounds[index];auto now=now_us();
  uint64_t cooldown=design.track==1?125000:design.track==5?900000:30000;
  if(now-last_effect_us[design.track]<cooldown)return;last_effect_us[design.track]=now;
  pxa_audio_tone_t tones[4]{};
  for(int i=0;i<design.count;++i){auto &a=design.tones[i];tones[i]={a.frequency,a.ms,int16_t(a.db*256),a.waveform,a.attack,a.release,a.delay};}
  if(effect_packets){auto status=pxa_audio_play_effect(sound_session,tones,design.count,design.track);if(status>=0||status==PXA_STATUS_WOULD_BLOCK)return;effect_packets=false;}
  // Older audio hosts still support bounded enveloped tones; no PCM fallback.
  for(int i=0;i<design.count;++i){auto &a=tones[i];pxa_audio_play_tone(sound_session,a.waveform,a.frequency_hz,a.duration_ms,a.gain_db_q8,a.attack_ms,a.release_ms,a.delay_ms);}
}
