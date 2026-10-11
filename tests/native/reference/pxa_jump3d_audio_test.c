#include "pxa_control_capture.h"
/* Host commands stay independent of the Guest sampling/render clock. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "jump3d_audio.h"
#include "jump3d_audio_data.h"

static uint32_t operation, bytes, play_calls, stop_calls;
static uint8_t command[528];
static uint64_t load_token;
static int blocked;
int32_t pxa_test_control(const uint8_t *p,uint32_t n) {
    (void)n;
    load_token=pxa_load_u64(p+PXA_WIRE_REQUEST_TOKEN_OFFSET);
    return 0;
}
int32_t pxa_io(uint64_t handle,uint32_t op,uint8_t *data,uint32_t n) {
    assert(handle>>32);
    assert(op!=PXA_AUDIO_IO_WRITE); // Guest never supplies PCM again.
    operation=op;bytes=n;memcpy(command,data,n);
    if (blocked && op==PXA_AUDIO_IO_PLAY_SOUND) return PXA_STATUS_WOULD_BLOCK;
    if (op==PXA_AUDIO_IO_PLAY_SOUND) ++play_calls;
    if (op==PXA_AUDIO_IO_CONTROL_SOUND) ++stop_calls;
    if (op==PXA_AUDIO_IO_PLAY_MUSIC) pxa_store_u64(data,99);
    return (int32_t)n;
}
static void arm(j3_audio_t *a) {
    memset(a,0,sizeof(*a)); a->state=J3_AUDIO_READY;
    a->session_handle=UINT64_C(0x100000007);a->preload_index=4;
    a->sound_handles[0]=UINT64_C(0x200000001);a->sound_ids[0]=J3_CLIP_SCALE_LOOP;
    a->sound_handles[1]=UINT64_C(0x200000002);a->sound_ids[1]=J3_CLIP_SUCCESS;
}
int main(void) {
    j3_audio_t a;arm(&a);
    assert(J3_AUDIO_CLIP_RATE_HZ==16000 && J3_AUDIO_CLIP_COUNT==19);
    j3_audio_play(&a,J3_CHANNEL_SUSTAIN,J3_CLIP_SCALE_LOOP,J3_GAIN_SOFT,1);
    blocked=1;j3_audio_tick(&a,1000000);assert(!a.voices[J3_CHANNEL_SUSTAIN].started);
    blocked=0;j3_audio_tick(&a,1020000);
    assert(operation==PXA_AUDIO_IO_PLAY_SOUND && bytes==16);
    assert(command[10]==J3_CHANNEL_SUSTAIN && command[11]==1);
    assert(a.voices[J3_CHANNEL_SUSTAIN].started && play_calls==1);
    j3_audio_tick(&a,3000000);assert(play_calls==1); // no Guest loop restart/PCM pump
    j3_audio_stop(&a,J3_CHANNEL_SUSTAIN);j3_audio_tick(&a,3020000);
    assert(operation==PXA_AUDIO_IO_CONTROL_SOUND && command[0]==J3_CHANNEL_SUSTAIN);
    assert(command[1]==PXA_AUDIO_ASSET_STOP && stop_calls==1);
    j3_audio_play(&a,J3_CHANNEL_LAND,J3_CLIP_SUCCESS,J3_GAIN_FULL,0);
    j3_audio_tick(&a,4000000);assert(j3_audio_channel_active(&a,J3_CHANNEL_LAND));
    j3_audio_tick(&a,5000000);assert(!j3_audio_channel_active(&a,J3_CHANNEL_LAND));
    // Stopping a pending asynchronous load must not start it when it completes.
    j3_audio_play(&a,J3_CHANNEL_COMBO,J3_CLIP_COMBO1,J3_GAIN_LOUD,0);
    j3_audio_tick(&a,6000000);assert(a.load_token && load_token==a.load_token);
    j3_audio_stop(&a,J3_CHANNEL_COMBO);
    unsigned plays=play_calls;
    uint8_t result[32]={0};pxa_store_u64(result+4,UINT64_C(0x300000003));
    result[12]=PXA_ASSET_AUDIO;result[13]=PXA_ASSET_ENCODING_PCM_S16LE_16K_MONO;
    pxa_event_t event={0};event.service=PXA_ASSETS_SERVICE;event.opcode=PXA_ASSETS_LOAD;
    event.token=a.load_token;event.payload=result;event.payload_size=32;
    uint8_t packet[128];
    assert(j3_audio_handle_event(&a,&event,packet,sizeof(packet)));
    j3_audio_tick(&a,6020000);assert(play_calls==plays && !a.load_token);
    j3_audio_play(&a,J3_CHANNEL_COMBO,J3_CLIP_COMBO1,J3_GAIN_LOUD,0);
    j3_audio_tick(&a,6040000);assert(play_calls==plays+1);
    // Music ducking uses a music-only command, preserving sound-track gains.
    j3_audio_play(&a,J3_CHANNEL_BGM,J3_CLIP_ICON,J3_GAIN_BGM,1);
    j3_audio_tick(&a,7000000);assert(a.music_instance==99);
    a.music_ready=1;
    j3_audio_play(&a,J3_CHANNEL_LAND,J3_CLIP_SUCCESS,J3_GAIN_FULL,0);
    j3_audio_tick(&a,7020000);assert(operation==PXA_AUDIO_IO_CONTROL_MUSIC);
    puts("Jump Host audio: async cancellation, backpressure, loops, independent stop and music ducking passed");
    return 0;
}
