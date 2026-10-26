/* SPDX-License-Identifier: MIT. Session-only tone timing adapter; output is silent. */
#ifndef PXA_CLASSIC_ARDUBOY_TONES_H
#define PXA_CLASSIC_ARDUBOY_TONES_H
#include "Arduino.h"
#include "backend.h"
#include "ArduboyTonesPitches.h"
#define TONES_END 0x8000
#define TONES_REPEAT 0x8001
#define TONES_VOLUME_NORMAL 0
#define TONES_VOLUME_HIGH 1
#define TONES_VOLUME_LOW 2
class ArduboyTones {
 bool (*enabled_)(void);
 uint32_t until_ = 0;
 public:
 explicit ArduboyTones(bool (*enabled)(void)): enabled_(enabled) {}
 void tone(uint16_t frequency, uint16_t duration) {
  if (!enabled_ || enabled_()) {
   arduboy_pxa_state.tone1 = frequency;
   until_ = arduboy_pxa_millis() + duration;
  }
 }
 void tones(const uint16_t *sequence) {
  uint32_t duration = 0;
  for (unsigned i=0; i<256; i+=2) {
   if (sequence[i] == TONES_END || sequence[i] == TONES_REPEAT) break;
   duration += sequence[i+1];
  }
  until_ = arduboy_pxa_millis() + duration;
 }
 void tonesInRAM(const uint16_t *sequence) { tones(sequence); }
 void noTone() { until_=0; arduboy_pxa_state.tone1=0; }
 bool playing() const { return (int32_t)(until_-arduboy_pxa_millis()) > 0; }
 void volumeMode(uint8_t) {}
};
#endif
