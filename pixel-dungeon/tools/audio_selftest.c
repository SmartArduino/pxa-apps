#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "game.h"
#include "pxa_audio.h"

static int asset_writes;
static int pcm_writes;
static int reject_assets;
static int invalid_assets;

int32_t pxa_control(const uint8_t *data, uint32_t length) {
    (void)data;
    (void)length;
    return 0;
}

int32_t pxa_io(uint32_t handle, uint32_t operation, uint8_t *data,
               uint32_t length) {
    (void)handle;
    if (operation == PXA_AUDIO_IO_PLAY_ASSET) {
        ++asset_writes;
        const uint16_t path_size = (uint16_t)(data[0] | data[1] << 8);
        if (path_size + 8u != length || path_size < 12 ||
            memcmp(data + 8, "assets/", 7) != 0 ||
            (memcmp(data + 8 + path_size - 4, ".pcm", 4) != 0 &&
             memcmp(data + 8 + path_size - 4, ".ogg", 4) != 0))
            ++invalid_assets;
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
    audio.music_host = 1;
    audio.music_track = 0xff;
    pd_audio_play(&audio, PD_SOUND_STEP);
    pd_audio_tick(&audio);
    assert(asset_writes == 1 && pcm_writes == 0 && invalid_assets == 0);
    for (int tick = 0; tick < 50; ++tick) pd_audio_tick(&audio);
    assert(pcm_writes == 0);

    reject_assets = 1;
    pd_audio_play(&audio, PD_SOUND_STEP);
    pd_audio_tick(&audio);
    assert(asset_writes == 2 && pcm_writes == 0 && invalid_assets == 0);
    for (int sound = 0; sound < PD_SOUND_COUNT; ++sound) {
        reject_assets = 0;
        pd_audio_play(&audio, (uint8_t)sound);
        pd_audio_tick(&audio);
    }
    assert(asset_writes == 2 + PD_SOUND_COUNT && pcm_writes == 0 &&
           invalid_assets == 0);
    puts("Packaged sound effect dispatch without embedded PCM: pass");
    return 0;
}
