#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define EMU_AUDIO_FIFO_SAMPLES 4096u
typedef struct {
    int16_t data[EMU_AUDIO_FIFO_SAMPLES];
    unsigned head,count;
} emu_audio_fifo_t;
static inline void emu_audio_fifo_clear(emu_audio_fifo_t *q) { q->head=q->count=0; }
/* Retain the latest 256 ms if the consumer falls irrecoverably behind. */
static inline unsigned emu_audio_fifo_push(emu_audio_fifo_t *q,const int16_t *p,unsigned n) {
    unsigned dropped=0;
    if(n>=EMU_AUDIO_FIFO_SAMPLES) {
        dropped=q->count+n-EMU_AUDIO_FIFO_SAMPLES;
        p+=n-EMU_AUDIO_FIFO_SAMPLES;n=EMU_AUDIO_FIFO_SAMPLES;
        q->head=q->count=0;
    } else if(n>EMU_AUDIO_FIFO_SAMPLES-q->count) {
        dropped=n-(EMU_AUDIO_FIFO_SAMPLES-q->count);
        q->head=(q->head+dropped)%EMU_AUDIO_FIFO_SAMPLES;q->count-=dropped;
    }
    unsigned tail=(q->head+q->count)%EMU_AUDIO_FIFO_SAMPLES;
    unsigned first=EMU_AUDIO_FIFO_SAMPLES-tail;if(first>n)first=n;
    memcpy(q->data+tail,p,first*sizeof(*p));
    if(n>first)memcpy(q->data,p+first,(n-first)*sizeof(*p));
    q->count+=n;return dropped;
}
static inline int16_t *emu_audio_fifo_peek(emu_audio_fifo_t *q,unsigned maximum,unsigned *n) {
    *n=EMU_AUDIO_FIFO_SAMPLES-q->head;
    if(*n>q->count)*n=q->count;if(*n>maximum)*n=maximum;
    return q->data+q->head;
}
static inline void emu_audio_fifo_consume(emu_audio_fifo_t *q,unsigned n) {
    if(n>q->count)n=q->count;
    q->head=(q->head+n)%EMU_AUDIO_FIFO_SAMPLES;q->count-=n;
}
