#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define GB_AUDIO_FIFO_SAMPLES 4096u
typedef struct {
    int16_t data[GB_AUDIO_FIFO_SAMPLES];
    unsigned head,count;
} gb_audio_fifo_t;
static inline void gb_audio_fifo_clear(gb_audio_fifo_t *q) { q->head=q->count=0; }
/* Retain the latest 256 ms if the consumer falls irrecoverably behind. */
static inline unsigned gb_audio_fifo_push(gb_audio_fifo_t *q,const int16_t *p,unsigned n) {
    unsigned dropped=0;
    if(n>=GB_AUDIO_FIFO_SAMPLES) {
        dropped=q->count+n-GB_AUDIO_FIFO_SAMPLES;
        p+=n-GB_AUDIO_FIFO_SAMPLES;n=GB_AUDIO_FIFO_SAMPLES;
        q->head=q->count=0;
    } else if(n>GB_AUDIO_FIFO_SAMPLES-q->count) {
        dropped=n-(GB_AUDIO_FIFO_SAMPLES-q->count);
        q->head=(q->head+dropped)%GB_AUDIO_FIFO_SAMPLES;q->count-=dropped;
    }
    unsigned tail=(q->head+q->count)%GB_AUDIO_FIFO_SAMPLES;
    unsigned first=GB_AUDIO_FIFO_SAMPLES-tail;if(first>n)first=n;
    memcpy(q->data+tail,p,first*sizeof(*p));
    if(n>first)memcpy(q->data,p+first,(n-first)*sizeof(*p));
    q->count+=n;return dropped;
}
static inline int16_t *gb_audio_fifo_peek(gb_audio_fifo_t *q,unsigned maximum,unsigned *n) {
    *n=GB_AUDIO_FIFO_SAMPLES-q->head;
    if(*n>q->count)*n=q->count;if(*n>maximum)*n=maximum;
    return q->data+q->head;
}
static inline void gb_audio_fifo_consume(gb_audio_fifo_t *q,unsigned n) {
    if(n>q->count)n=q->count;
    q->head=(q->head+n)%GB_AUDIO_FIFO_SAMPLES;q->count-=n;
}
