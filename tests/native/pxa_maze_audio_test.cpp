#include "pxa_control_capture.h"
#include "audio.h"
#include "world.h"

#include <assert.h>

static int host_tones_supported = 1;
static uint32_t tone_writes;
static uint32_t pcm_writes;

int32_t pxa_test_control(const uint8_t *data, uint32_t length) {
    (void)data;
    (void)length;
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data,
                  uint32_t length) {
    assert(handle == UINT64_C(0x10000004d));
    assert(data != 0);
    if (operation == PXA_AUDIO_IO_PLAY_TONE) {
        ++tone_writes;
        if (!host_tones_supported) return PXA_STATUS_UNSUPPORTED;
        assert(length == 14);
        assert(data[6] <= PXA_AUDIO_TONE_NOISE);
        return (int32_t)length;
    }
    assert(operation == PXA_AUDIO_IO_WRITE);
    assert(length == AUDIO_FRAME_SAMPLES * sizeof(int16_t));
    ++pcm_writes;
    return (int32_t)length;
}

int main(void) {
    game_audio_t audio = {0};

    audio.state = AUDIO_READY;
    audio.session_handle = UINT64_C(0x10000004d);
    audio_play(&audio, SND_SHOTGUN, 255);
    assert(audio.tone_mode == AUDIO_TONE_MODE_HOST);
    assert(tone_writes == 3);
    assert(pcm_writes == 0);
    audio_tick(&audio, 1000);
    assert(pcm_writes == 0);

    audio = (game_audio_t){0};
    audio.state = AUDIO_READY;
    audio.session_handle = UINT64_C(0x10000004d);
    host_tones_supported = 0;
    tone_writes = 0;
    audio_play(&audio, SND_SHOTGUN, 255);
    assert(audio.tone_mode == AUDIO_TONE_MODE_PCM_FALLBACK);
    assert(tone_writes == 1);
    audio_tick(&audio, 2000);
    assert(pcm_writes != 0);
    return 0;
}
