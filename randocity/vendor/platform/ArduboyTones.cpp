/* SPDX-License-Identifier: MIT.
 * Copyright (c) 2017 Scott Allen. Software adaptation of pinned Timer3
 * duration/sequence logic; original MIT notice supplied with full source.
 * Timer clocks at F_CPU/8, retains TCNT3 phase when OCR3A is replaced,
 * uses the original floor(dur*frequency/512), and advances a sequence on
 * the extra interrupt after toggle count reaches zero. Output is silent.
 */
#include "ArduboyTones.h"
#include "backend.h"
RandocityToneState randocity_tone_state{};
static bool(*output_enabled)();
ArduboyTones::ArduboyTones(bool(*outEn)()) {
  output_enabled=outEn;randocity_tone_state.sequence[6]=TONES_END;
}
void ArduboyTones::nextTone() {
  auto &s=randocity_tone_state;
  uint16_t frequency=s.sequence[s.index++];
  if(frequency==TONES_END){s.interrupt_enabled=false;s.running=false;s.playing=false;return;}
  s.playing=true;
  if(frequency==TONES_REPEAT){s.index=0;frequency=s.sequence[s.index++];}
  s.high_volume=((frequency&TONE_HIGH_VOLUME)||s.force_high)&&!s.force_normal;
  frequency&=~TONE_HIGH_VOLUME;
  s.silent=frequency==0;
  if(!frequency)frequency=250;
  s.compare=(uint16_t)(F_CPU/8/frequency/2-1);
  if(!output_enabled())s.silent=true;
  const uint16_t duration=s.sequence[s.index++];
  s.toggle_count=duration?(int32_t)(((int32_t)duration*frequency)>>9):-1;
  s.running=true;s.interrupt_enabled=true;
}
void ArduboyTones::update() {
  auto &s=randocity_tone_state;uint32_t now=arduboy_pxa_millis();
  uint64_t ticks=(uint64_t)(uint32_t)(now-s.last_ms)*2000u;s.last_ms=now;
  while(s.running&&ticks) {
    uint32_t until=s.counter<=s.compare?(uint32_t)s.compare-s.counter+1u:65536u-s.counter+s.compare+1u;
    if(ticks<until){s.counter=(uint16_t)(s.counter+ticks);break;}
    ticks-=until;s.counter=0;
    if(s.interrupt_enabled) {
      if(s.toggle_count!=0){if(s.toggle_count>0)--s.toggle_count;}
      else nextTone();
    }
  }
}
void ArduboyTones::tone(uint16_t f,uint16_t d) {
  update();auto&s=randocity_tone_state;s.interrupt_enabled=false;s.in_progmem=false;s.index=0;
  s.sequence[0]=f;s.sequence[1]=d;s.sequence[2]=TONES_END;nextTone();
}
void ArduboyTones::tone(uint16_t f1,uint16_t d1,uint16_t f2,uint16_t d2) {
  update();auto&s=randocity_tone_state;s.interrupt_enabled=false;s.in_progmem=false;s.index=0;
  s.sequence[0]=f1;s.sequence[1]=d1;s.sequence[2]=f2;s.sequence[3]=d2;s.sequence[4]=TONES_END;nextTone();
}
void ArduboyTones::tone(uint16_t f1,uint16_t d1,uint16_t f2,uint16_t d2,uint16_t f3,uint16_t d3) {
  update();auto&s=randocity_tone_state;s.interrupt_enabled=false;s.in_progmem=false;s.index=0;
  s.sequence[0]=f1;s.sequence[1]=d1;s.sequence[2]=f2;s.sequence[3]=d2;s.sequence[4]=f3;s.sequence[5]=d3;nextTone();
}
void ArduboyTones::noTone(){update();auto&s=randocity_tone_state;s.interrupt_enabled=false;s.running=false;s.playing=false;}
bool ArduboyTones::playing(){update();return randocity_tone_state.playing;}
void ArduboyTones::volumeMode(uint8_t mode){auto&s=randocity_tone_state;s.force_normal=mode==VOLUME_ALWAYS_NORMAL;s.force_high=mode==VOLUME_ALWAYS_HIGH;}
