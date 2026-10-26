#include "owner_compat.h"
#include "ArduboyPlaytune.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool enabled(){return true;}
static uint8_t sound[]={0x90,69,0,10,0x80,0xf0};static unsigned checks=0;
static void check(bool v){++checks;if(!v){fprintf(stderr,"audio check %u failed\n",checks);abort();}}
static uint32_t u32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void pack(uint8_t p[72]){ObonoWriter w(p);obono_audio_pack(w);check(w.size()==72);}
int main(){obono_audio_reset();ArduboyPlaytune t(enabled);t.initChannel(5);obono_audio_register(0,sound,sizeof sound);uint8_t b[72],again[72];pack(b);check(b[0]==1&&b[3]==0&&b[4]==1);t.playScore(sound);pack(b);check(t.playing()&&b[11]==1&&u32(b+15)==9);check((b[39]|b[40]<<8)==18180);obono_audio_advance(10);pack(b);check(t.playing()&&u32(b+15)==1&&u32(b+60)==8);obono_audio_advance(11);pack(b);check(!t.playing()&&u32(b+15)==0&&u32(b+60)==9&&b[23]==0&&b[29]==6);pack(again);check(!memcmp(b,again,72));t.playScore(sound);obono_audio_advance(21);check(t.playing());obono_audio_advance(22);check(!t.playing());t.stopScore();pack(b);check(u32(b+64)==2&&u32(b+68)==1&&b[55]==0);printf("{\"passed\":true,\"checks\":%u,\"typed_audio_bytes\":72,\"original_frequency2\":880,\"original_ocr\":18180,\"cycles_per_ms\":16000,\"original_wait_toggles\":9,\"playing_after10ms\":true,\"stopped_after11ms\":true,\"pack_mutation_count\":0,\"validated_channels\":1,\"silent_gpio\":true}\n",checks);}
