/* Exact unsigned stereo -> signed mono batching. GPL-3.0-or-later. */
#pragma once
#include "port.h"

/* ring_mask is a power-of-two frame count minus one. port_pcm consumes its
 * input synchronously, so the fixed scratch block needs no heap allocation. */
static inline unsigned port_audio_u8_stereo_ring(const uint8_t *ring,
    unsigned ring_mask, unsigned played, unsigned end, unsigned rate) {
  int16_t samples[64];
  while(played!=end) {
    unsigned count=0;
    while(count<64 && played!=end) {
      int sum=(int)ring[played*2]+ring[played*2+1]-256;
      samples[count++]=(int16_t)(sum*128);
      played=(played+1)&ring_mask;
    }
    port_pcm(samples,count,rate,1);
  }
  return played;
}
