/* SPDX-License-Identifier: MIT. Randocity original sketch adapter.
 * Copyright (c) 2019 pmwasson (upstream game).
 * Typed little-endian full state, explicit original AVR EEPROM record widths.
 */
#include <Arduboy2.h>
#include <new>
#include "backend.h"
#include "vendor/platform/ArduboyTones.h"
#define private public
#include "vendor/platform/World.h"
#include "vendor/platform/FatFont.h"
#include "vendor/platform/Timer.h"
#undef private
#include "vendor/platform/Records.h"
static void records_load_avr(uint16_t at,Records&r) {
  r.edgeRaceMinutes=EEPROM.read(at++);r.edgeRaceSeconds=EEPROM.read(at++);
  uint32_t seed=0;for(unsigned i=0;i<4;++i)seed|=(uint32_t)EEPROM.read(at++)<<(8*i);r.edgeRaceSeed=seed;
  r.crazyCourierScore=EEPROM.read(at++);r.crazyCourierScore|=(uint16_t)EEPROM.read(at++)<<8;
  seed=0;for(unsigned i=0;i<4;++i)seed|=(uint32_t)EEPROM.read(at++)<<(8*i);r.crazyCourierSeed=seed;
  r.checksum=EEPROM.read(at);
}
static void records_save_avr(uint16_t at,const Records&r) {
  EEPROM.write(at++,r.edgeRaceMinutes);EEPROM.write(at++,r.edgeRaceSeconds);
  for(unsigned i=0;i<4;++i)EEPROM.write(at++,(uint8_t)(r.edgeRaceSeed>>(8*i)));
  EEPROM.write(at++,(uint8_t)r.crazyCourierScore);EEPROM.write(at++,(uint8_t)(r.crazyCourierScore>>8));
  for(unsigned i=0;i<4;++i)EEPROM.write(at++,(uint8_t)(r.crazyCourierSeed>>(8*i)));
  EEPROM.write(at,r.checksum);
}
#include "original-prototypes.h"
#include "vendor/platform/Randocity.ino"
static bool initial_boot;
extern "C" void arduboy_game_boot() {
  if(!initial_boot) {
    // The actual Guest did not apply these owner dynamic initializers.
    // Idempotent cold construction reproduces the declared original values;
    // subsequent NEW calls leave process-static Tones timing untouched.
    sound.~ArduboyTones();new(&sound)ArduboyTones(arduboy.audio.enabled);
    font.~FatFont();new(&font)FatFont{};world=World{};
    gameTimer=Timer{};startTimer=Timer{};targetX=offsetX;targetY=offsetY;
  }
  if(initial_boot) {
    // Wrapper restart reconstructs the sketch's declared cold values;
    // independent library process-static tone state remains intact.
    playerDirection=north;frame=0;playerX=playerY=savePlayerX=savePlayerY=0;playerSpeed=0;
    offsetX=targetMidH;offsetY=targetMidV;targetX=offsetX;targetY=offsetY;
    mode=title;savedMode=title;menuItem=0;seedNew=seedRandom=0;helpPage=0;
    playerScore=0;courierX=courierY=0;newRecord=false;
    world=World{};font.~FatFont();new(&font)FatFont{};records=Records{};gameTimer=Timer{};startTimer=Timer{};
  }
  initial_boot=true;setup();
}
extern "C" void arduboy_game_step(){ArduboyTones::update();loop();}
extern "C" const char*arduboy_game_name(){return "Rando City";}
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t capacity) {
  constexpr size_t count=120;if(!out)return count;if(capacity<count)return 0;
  uint8_t*p=out;auto u8=[&](uint8_t v){*p++=v;};auto u16=[&](uint16_t v){u8((uint8_t)v);u8((uint8_t)(v>>8));};auto u32=[&](uint32_t v){u16((uint16_t)v);u16((uint16_t)(v>>16));};
  u8(playerDirection);u8(frame);u32((uint32_t)playerX);u32((uint32_t)playerY);u32((uint32_t)savePlayerX);u32((uint32_t)savePlayerY);u8((uint8_t)playerSpeed);
  u8(offsetX);u8(offsetY);u8(targetX);u8(targetY);u8(mode);u8(savedMode);u8((uint8_t)menuItem);u32(seedNew);u32(seedRandom);u8(helpPage);u16(playerScore);u32((uint32_t)courierX);u32((uint32_t)courierY);u8(newRecord);
  u8(world.miniLeft);u8(world.miniRight);u8(world.miniTop);u8(world.miniBottom);u32(world.seed);u32(world.lfsr.lfsr);u16((uint16_t)world.lfsrX);u16((uint16_t)world.lfsrY);
  u8((uint8_t)font.cursorX);u8((uint8_t)font.baseX);u8((uint8_t)font.cursorY);
  u8(records.edgeRaceMinutes);u8(records.edgeRaceSeconds);u32(records.edgeRaceSeed);u16(records.crazyCourierScore);u32(records.crazyCourierSeed);u8(records.checksum);
  Timer*timers[2]={&gameTimer,&startTimer};for(Timer*t:timers){u8(t->timerSeconds);u8(t->timerMinutes);u8(t->timerMode);}
  const auto&s=randocity_tone_state;u32(s.last_ms);for(uint16_t v:s.sequence)u16(v);u8(s.index);u32((uint32_t)s.toggle_count);u8(s.playing);u8(s.silent);u8(s.high_volume);u8(s.force_normal);u8(s.force_high);u8(s.in_progmem);u16(s.counter);u16(s.compare);u8(s.running);u8(s.interrupt_enabled);u8(initial_boot);
  return (size_t)(p-out)==count?count:0;
}
