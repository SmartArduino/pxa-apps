#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define NES_AUDIO_FIFO_SAMPLES 4096u
typedef struct {
    int16_t data[NES_AUDIO_FIFO_SAMPLES];
    unsigned head,count;
} nes_audio_fifo_t;
static inline void nes_audio_fifo_clear(nes_audio_fifo_t *q) { q->head=q->count=0; }
/* Retain the latest 256 ms if the consumer falls irrecoverably behind. */
static inline unsigned nes_audio_fifo_push(nes_audio_fifo_t *q,const int16_t *p,unsigned n) {
    unsigned dropped=0;
    if(n>=NES_AUDIO_FIFO_SAMPLES) {
        dropped=q->count+n-NES_AUDIO_FIFO_SAMPLES;
        p+=n-NES_AUDIO_FIFO_SAMPLES;n=NES_AUDIO_FIFO_SAMPLES;
        q->head=q->count=0;
    } else if(n>NES_AUDIO_FIFO_SAMPLES-q->count) {
        dropped=n-(NES_AUDIO_FIFO_SAMPLES-q->count);
        q->head=(q->head+dropped)%NES_AUDIO_FIFO_SAMPLES;q->count-=dropped;
    }
    unsigned tail=(q->head+q->count)%NES_AUDIO_FIFO_SAMPLES;
    unsigned first=NES_AUDIO_FIFO_SAMPLES-tail;if(first>n)first=n;
    memcpy(q->data+tail,p,first*sizeof(*p));
    if(n>first)memcpy(q->data,p+first,(n-first)*sizeof(*p));
    q->count+=n;return dropped;
}
static inline int16_t *nes_audio_fifo_peek(nes_audio_fifo_t *q,unsigned maximum,unsigned *n) {
    *n=NES_AUDIO_FIFO_SAMPLES-q->head;
    if(*n>q->count)*n=q->count;if(*n>maximum)*n=maximum;
    return q->data+q->head;
}
static inline void nes_audio_fifo_consume(nes_audio_fifo_t *q,unsigned n) {
    if(n>q->count)n=q->count;
    q->head=(q->head+n)%NES_AUDIO_FIFO_SAMPLES;q->count-=n;
}
