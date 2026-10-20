/* Exact unsigned stereo -> signed stereo batching. GPL-3.0-or-later. */
#pragma once
#include "port.h"

/* ring_mask is a power-of-two frame count minus one. port_pcm consumes its
 * input synchronously, so the fixed scratch block needs no heap allocation. */
static inline unsigned port_audio_u8_stereo_ring(const uint8_t *ring,
    unsigned ring_mask, unsigned played, unsigned end, unsigned rate) {
  int16_t samples[64 * 2];
  while(played!=end) {
    unsigned count=0;
    while(count<64 && played!=end) {
      samples[count*2]=(int16_t)(((int)ring[played*2]-128)*256);
      samples[count*2+1]=(int16_t)(((int)ring[played*2+1]-128)*256);
      ++count;
      played=(played+1)&ring_mask;
    }
    /* Preserve both source channels until the negotiated output boundary.
     * The legacy mono port_pcm still produces exactly the previous average. */
    port_pcm(samples,count,rate,2);
  }
  return played;
}

/* Keep stereo input for port_pcm's original signed averaging (C truncates
 * negative odd sums toward zero). Submit contiguous ring spans without copies. */
static inline unsigned port_audio_s16_stereo_ring(const int16_t *ring,
    unsigned ring_mask, unsigned played, unsigned end, unsigned rate) {
  while(played!=end) {
    unsigned count=(end-played)&ring_mask;
    unsigned contiguous=ring_mask+1-played;
    if(count>contiguous)count=contiguous;
    if(count>64)count=64;
    port_pcm(ring+played*2,count,rate,2);
    played=(played+count)&ring_mask;
  }
  return played;
}
