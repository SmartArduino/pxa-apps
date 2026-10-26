#include "backend.h"
#include "owner_compat.h"
#include "vendor/platform/common.h"
#include "vendor/platform/data.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks=0;
static void need(bool v){++checks;if(!v)abort();}
static void get(uint8_t*b){uint8_t all[772];need(arduboy_game_pack(all,sizeof(all))==772);for(unsigned i=0;i<64;i++)b[i]=all[708+i];need(b[47]==0);}
int main(){const byte*scores[]={soundStart,soundOver,soundScatter,soundSprout,soundWither,soundDamage,soundWarn,soundCloud};size_t sizes[]={sizeof(soundStart),sizeof(soundOver),sizeof(soundScatter),sizeof(soundSprout),sizeof(soundWither),sizeof(soundDamage),sizeof(soundWarn),sizeof(soundCloud)};unsigned durations[8];uint8_t b[64];arduboy_pxa_boot(123);
for(unsigned s=0;s<8;s++){arduboy_pxa_new();obono_audio_register(8+s,scores[s],sizes[s]);ab.playScore(scores[s],s);get(b);need(b[0]==1&&b[16]==1&&b[20]==8+s);uint8_t before[64];get(before);ab.playTone(440,10,255);get(b);for(unsigned i=0;i<64;i++)need(before[i]==b[i]);unsigned ms=0;while(b[16]&&ms<4000){arduboy_pxa_advance(1);++ms;get(b);}need(ms>0&&ms<4000&&b[16]==0);durations[s]=ms;need(b[23]==8+s&&b[25]==0);}
arduboy_pxa_new();ab.playTone(587,20);get(b);need(b[17]);for(unsigned i=0;i<24;i++)arduboy_pxa_advance(1);get(b);need(!b[17]);
arduboy_pxa_new();obono_audio_register(8,soundStart,sizeof(soundStart));ab.playScore(soundStart,7);ab.playTone(440,10,0);get(b);need(!b[16]&&b[17]);
printf("{\"passed\":true,\"checks\":%u,\"original_scores\":8,\"bounded_priority_comparisons\":512,\"score_stop_ms\":[",checks);for(unsigned i=0;i<8;i++)printf("%s%u",i?",":"",durations[i]);printf("],\"scope\":\"native original public audio APIs and1msclock; silent; no AVRhardware proof\"}\n");}
