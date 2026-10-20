#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static int16_t captured[2048];
static int16_t captured_left[2048],captured_right[2048];
static unsigned written,calls,expected_channels=2;
void port_pcm(const int16_t *p,unsigned count,unsigned rate,unsigned channels) {
  assert(rate==11025 && channels==expected_channels && count<=64);
  ++calls;for(unsigned i=0;i<count;++i) {
    assert(written<2048);
    captured_left[written]=p[i*2];captured_right[written]=p[i*2+1];
    captured[written++]=((int)p[i*2]+p[i*2+1])/2;
  }
}
#include "audio_convert.h"
int main(void) {
  uint8_t ring[4096];
  /* Exhaust every stereo pair, including exact negative/positive rounding. */
  for(unsigned a=0;a<256;++a) {
    for(unsigned b=0;b<256;++b){ring[b*2]=a;ring[b*2+1]=b;}
    written=calls=0;
    assert(port_audio_u8_stereo_ring(ring,2047,0,256,11025)==256);
    assert(written==256 && calls==4);
    for(unsigned b=0;b<256;++b) {
      assert(captured_left[b]==((int)a-128)*256);
      assert(captured_right[b]==((int)b-128)*256);
      assert(captured[b]==(((int)a-128)*256+((int)b-128)*256)/2);
    }
  }
  for(unsigned i=0;i<sizeof(ring);++i)ring[i]=(uint8_t)(i*71+i/97);
  for(unsigned start=0;start<2048;start+=31)for(unsigned count=0;count<2048;count+=43){
    written=calls=0;unsigned end=(start+count)&2047;
    assert(port_audio_u8_stereo_ring(ring,2047,start,end,11025)==end);
    assert(written==count && calls==(count+63)/64);
    for(unsigned i=0;i<count;++i){unsigned j=(start+i)&2047;
      assert(captured_left[i]==((int)ring[j*2]-128)*256);
      assert(captured_right[i]==((int)ring[j*2+1]-128)*256);
      assert(captured[i]==(((int)ring[j*2]-128)*256+((int)ring[j*2+1]-128)*256)/2);}
  }
  int16_t stereo[4096];
  for(unsigned i=0;i<4096;++i)stereo[i]=(int16_t)(i*173+32761);
  expected_channels=2;
  for(unsigned start=0;start<2048;start+=31)for(unsigned count=0;count<2048;count+=43) {
    written=calls=0;unsigned end=(start+count)&2047;
    assert(port_audio_s16_stereo_ring(stereo,2047,start,end,11025)==end);
    assert(written==count && calls<=((count+63)/64)+1);
    for(unsigned i=0;i<count;++i){unsigned j=(start+i)&2047;
      assert(captured_left[i]==stereo[j*2]);
      assert(captured_right[i]==stereo[j*2+1]);
      assert(captured[i]==((int)stereo[j*2]+stereo[j*2+1])/2);}
  }
  puts("PASS: preserved channels for all 65536 U8 pairs, U8/S16 ring wrap, empty/batched output and bit-exact legacy mono average");
}
