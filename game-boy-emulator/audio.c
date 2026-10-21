/* Streaming gnuboy PCM conversion. GPL-2.0-or-later. */
#include "audio.h"
#include <string.h>
void gb_audio_configure(gb_audio_t *a,unsigned rate,unsigned channels) {
    a->rate=rate?rate:16000;a->channels=channels==2?2:1;
    a->step=(uint32_t)(((uint64_t)GB_AUDIO_STAGE_RATE<<16)/a->rate);
    if(!a->step)a->step=1;
}
void gb_audio_reset(gb_audio_t *a){memset(a,0,sizeof(*a));gb_audio_configure(a,16000,1);}
void gb_audio_start(gb_audio_t *a,unsigned rate,unsigned channels) {
    memset(a,0,sizeof(*a));gb_audio_configure(a,rate,channels);
    a->fade_total=a->fade_remaining=a->rate/200u;
}
unsigned gb_audio_convert(gb_audio_t *a,const int16_t *src,size_t frames,int16_t *dst,unsigned capacity) {
    unsigned produced=0;
    for(size_t i=0;i<frames;++i){
        const int16_t *sample=src+i*2;
        if(!a->have_previous){a->previous[0]=sample[0];a->previous[1]=sample[1];a->have_previous=1;continue;}
        while(a->phase<65536u){
            int32_t value[2];
            for(unsigned ch=0;ch<2;++ch) {
                value[ch]=((int32_t)a->previous[ch]*(int32_t)(65536u-a->phase)+(int32_t)sample[ch]*(int32_t)a->phase)>>16;
                if(a->fade_remaining)value[ch]=value[ch]*(int32_t)(a->fade_total-a->fade_remaining)/(int32_t)a->fade_total;
            }
            if(a->fade_remaining)--a->fade_remaining;
            if(produced<capacity){
                if(a->channels==1)dst[produced]=(int16_t)((value[0]+value[1])/2);
                else {dst[produced*2]=(int16_t)value[0];dst[produced*2+1]=(int16_t)value[1];}
                ++produced;
            }
            a->phase+=a->step;
        }
        a->phase-=65536u;a->previous[0]=sample[0];a->previous[1]=sample[1];
    }
    return produced;
}
