#include "pxa_control_capture.h"
#include "audio.h"
#include "world.h"

#include <assert.h>

static int host_tones_supported = 1;
static uint32_t tone_writes;
static int host_status = 0;

int32_t pxa_test_control(const uint8_t *data, uint32_t length) {
    (void)data;
    (void)length;
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data,
                  uint32_t length) {
    if (operation == 0x107) assert(handle == UINT64_C(0x10000004d));
    assert(data != 0);
    if (operation == 0x107) {
        ++tone_writes;
        if (!host_tones_supported) return PXA_STATUS_UNSUPPORTED;
        if (host_status) return host_status;
        assert(length == 4 + 14 * data[0]);
        assert(data[1] < 4);
        for (unsigned i = 0; i < data[0]; ++i)
            assert(data[4 + 14 * i + 6] <= PXA_AUDIO_TONE_NOISE);
        return (int32_t)length;
    }
    assert(false && "unexpected audio operation");
    return length;
}

int main(void) {
    game_audio_t audio = {0};

    audio.state = AUDIO_READY;
    audio.session_handle = UINT64_C(0x10000004d);
    audio_play(&audio, SND_SHOTGUN, 255);
    assert(audio.state == AUDIO_READY && tone_writes == 1);
    host_status = PXA_STATUS_WOULD_BLOCK;
    audio_play(&audio, SND_SHOTGUN, 255);
    assert(audio.state == AUDIO_READY && tone_writes == 2);
    host_status = 0;
    host_tones_supported = 0;
    audio_play(&audio, SND_SHOTGUN, 255);
    assert(audio.state == AUDIO_UNAVAILABLE && tone_writes == 3);
    audio_play(&audio, SND_SHOTGUN, 255);
    assert(tone_writes == 3);
    return 0;
}
