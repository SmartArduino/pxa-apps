#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "pxa_audio.h"

static int asset_writes;
static int pcm_writes;
static int reject_assets;

int32_t pxa_control(const uint8_t *data, uint32_t length) {
    (void)data;
    (void)length;
    return 0;
}

int32_t pxa_io(uint32_t handle, uint32_t operation, uint8_t *data,
               uint32_t length) {
    (void)handle;
    (void)data;
    if (operation == PXA_AUDIO_IO_PLAY_ASSET) {
        ++asset_writes;
        return reject_assets ? PXA_STATUS_UNSUPPORTED : (int32_t)length;
    }
    if (operation == PXA_IO_WRITE) ++pcm_writes;
    return (int32_t)length;
}

int main(void) {
    pd_audio_t audio;
    memset(&audio, 0, sizeof(audio));
    audio.state = PD_AUDIO_READY;
    audio.session_handle = 1;
    audio.sample_rate = 16000;
    audio.frame_samples = 320;
    audio.music_host = 1;
    audio.music_track = 0xff;
    audio.pending[0] = PD_SFX_STEP;
    audio.pending_count = 1;
    pd_audio_tick(&audio);
    assert(asset_writes == 1 && pcm_writes == 0);
    for (int tick = 0; tick < 50; ++tick) pd_audio_tick(&audio);
    assert(pcm_writes == 0);

    reject_assets = 1;
    audio.pending[0] = PD_SFX_STEP;
    audio.pending_count = 1;
    pd_audio_tick(&audio);
    assert(asset_writes == 2 && pcm_writes == 1);
    assert(audio.voices[0].position == (320u << 16));
    for (int tick = 0; tick < 10; ++tick) pd_audio_tick(&audio);
    assert(!audio.voices[0].active);
    assert(pcm_writes == 10);
    puts("Host SFX dispatch and Guest PCM fallback: pass");
    return 0;
}
