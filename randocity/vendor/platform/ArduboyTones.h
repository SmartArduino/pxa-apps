/* SPDX-License-Identifier: MIT.
 * Copyright (c) 2017 Scott Allen. Modified 2026-10-10: silent software
 * Timer3 boundary for the pinned Randocity calls. Full original MIT notice
 * in ../original/ArduboyTones/LICENSE.txt and assets/LICENSE.ArduboyTones.MIT.
 * No GPIO, hardware interrupt, sound output or new Host function.
 */
#pragma once
#include <Arduino.h>
#include "ArduboyTonesPitches.h"
#define TONES_END 0x8000
#define TONES_REPEAT 0x8001
#define TONE_HIGH_VOLUME 0x8000
#define VOLUME_IN_TONE 0
#define VOLUME_ALWAYS_NORMAL 1
#define VOLUME_ALWAYS_HIGH 2
struct RandocityToneState {
  uint32_t last_ms;
  uint16_t sequence[7];
  uint8_t index;
  int32_t toggle_count;
  bool playing,silent,high_volume,force_normal,force_high,in_progmem;
  uint16_t counter,compare;
  bool running,interrupt_enabled;
};
extern RandocityToneState randocity_tone_state;
class ArduboyTones {
public:
  explicit ArduboyTones(bool(*outEn)());
  static void tone(uint16_t,uint16_t=0);
  static void tone(uint16_t,uint16_t,uint16_t,uint16_t);
  static void tone(uint16_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t);
  static void noTone();
  static bool playing();
  static void volumeMode(uint8_t);
  static void update();
private:
  static void nextTone();
};
