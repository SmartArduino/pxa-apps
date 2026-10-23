/* GPL-2.0-or-later. Read original demo audio without rewriting its GRP. */
#ifndef PXA_DUKE_MEDIA_AUDIO_H
#define PXA_DUKE_MEDIA_AUDIO_H
#include <stdbool.h>
#include <stdint.h>

typedef bool (*duke_media_read_fn)(void *context,uint32_t offset,void *output,uint32_t bytes);
#define DUKE_PCM_SEGMENTS 8
typedef struct {uint32_t offset,bytes;} duke_pcm_segment_t;
typedef struct {
    uint32_t rate,frames,count,bits;
    duke_pcm_segment_t segments[DUKE_PCM_SEGMENTS];
} duke_pcm_t;

bool duke_voc_probe(duke_media_read_fn read,void *context,uint32_t bytes,duke_pcm_t *pcm);
bool duke_pcm_read(duke_media_read_fn read,void *context,const duke_pcm_t *pcm,
                   uint32_t frame,void *output,uint32_t bytes);
// Stable chronological merge, using only per-track cursors and the final output.
// No event list, full input copy, filesystem cache or sorting allocation.
bool duke_midi_merge(duke_media_read_fn read,void *context,uint32_t bytes,
                     uint8_t *output,uint32_t capacity,uint32_t *written);
#endif
