#ifndef PXA_QUAKE_HOST_AUDIO_MIX_H
#define PXA_QUAKE_HOST_AUDIO_MIX_H
/* Nonzero means the complete span was accumulated. Failure writes nothing and
 * the vendored scalar loop remains the exact fallback. Borrow ends on return. */
int Pxa_QuakeMixS8(void *stereo_s32, const void *source_s8,
    int frames, int left_gain, int right_gain);
#endif
