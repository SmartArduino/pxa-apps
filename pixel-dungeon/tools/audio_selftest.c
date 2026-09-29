#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "game.h"

static uint64_t pending_token, next_handle=UINT64_C(0x1234567800000000);
static unsigned plays, loads, closes, cancels, rejected;
static uint64_t next_music=UINT64_C(0x567890ab00000000);
static unsigned music_rejected;
int32_t pxa_submit(const uint8_t *data,uint32_t n) {
    pxa_event_t event; assert(pxa_parse_event(data,n,&event));
    if(event.service==PXA_ASSETS_SERVICE) {
        assert(event.opcode==PXA_ASSETS_LOAD && event.payload[2]==PXA_ASSET_AUDIO);
        assert(!pending_token);
        if(rejected) return PXA_STATUS_WOULD_BLOCK;
        pending_token=event.token; ++loads;
    } else if(event.service==PXA_CORE_SERVICE) {
        if(event.opcode==PXA_CORE_CLOSE_HANDLE) ++closes;
        else if(event.opcode==PXA_CORE_CANCEL_REQUEST) ++cancels;
    }
    return 0;
}
int32_t pxa_io(uint64_t session,uint32_t op,uint8_t *data,uint32_t n) {
    assert(session);
    if(op==0x103) { assert(n==12 && pxa_load_u64(data)>UINT32_MAX); ++plays; }
    if(op==PXA_AUDIO_IO_PLAY_MUSIC) {
        assert(n>16 && !pxa_load_u64(data));
        if(music_rejected) return PXA_STATUS_WOULD_BLOCK;
        pxa_store_u64(data,++next_music);
    }
    return (int32_t)n;
}
static void complete(pd_audio_t *audio) {
    assert(pending_token); uint8_t result[32]={0};
    pxa_store_u64(result+4,++next_handle); result[12]=PXA_ASSET_AUDIO;
    pxa_store_u32(result+20,160); pxa_store_u32(result+24,160);
    pxa_event_t event={PXA_ASSETS_SERVICE,PXA_ASSETS_LOAD,pending_token,result,sizeof(result)};
    pending_token=0; pd_audio_handle_event(audio,&event,NULL,0);
}
static void music_event(pd_audio_t *audio,uint64_t session,uint64_t instance,uint8_t state) {
    uint8_t payload[24]={0};
    pxa_store_u64(payload,session); pxa_store_u64(payload+8,instance); payload[16]=state;
    if(state==PXA_AUDIO_PLAYBACK_ERROR) pxa_store_u32(payload+20,(uint32_t)PXA_STATUS_IO_ERROR);
    pxa_event_t event={PXA_AUDIO_SERVICE,PXA_AUDIO_PLAYBACK_EVENT,0,payload,sizeof(payload)};
    pd_audio_handle_event(audio,&event,NULL,0);
}
int main(void) {
    pd_audio_t audio={0}; audio.state=PD_AUDIO_READY; audio.session_handle=UINT64_C(0x100000001);
    audio.sound_sequence=UINT64_C(0x5044534600000000);
    audio.music_track=0xff;
    audio.music_pending=0xff;
    pd_audio_set_music(&audio,PD_MUSIC_TITLE);
    uint64_t first=audio.music_instance;
    assert(audio.music_host && first>UINT32_MAX && !audio.music_ready);
    int16_t initial_gain=audio.music_asset_gain_db_q8;
    pd_audio_tick(&audio); assert(audio.music_asset_gain_db_q8==initial_gain);
    music_event(&audio,audio.session_handle+1,first,PXA_AUDIO_PLAYBACK_READY);
    assert(!audio.music_ready);
    music_event(&audio,audio.session_handle,first,PXA_AUDIO_PLAYBACK_READY);
    pd_audio_tick(&audio); assert(audio.music_ready && audio.music_asset_gain_db_q8>initial_gain);
    music_rejected=1; pd_audio_set_music(&audio,PD_MUSIC_SEWERS); pd_audio_tick(&audio);
    assert(audio.music_host && audio.music_instance==first && audio.music_pending==PD_MUSIC_SEWERS);
    music_rejected=0; pd_audio_tick(&audio);
    uint64_t second=audio.music_instance;
    assert(second!=first && audio.music_host && !audio.music_ready && audio.music_pending==0xff);
    music_event(&audio,audio.session_handle,first,PXA_AUDIO_PLAYBACK_REPLACED);
    assert(audio.music_host && audio.music_instance==second);
    music_event(&audio,audio.session_handle,second,PXA_AUDIO_PLAYBACK_ERROR);
    assert(!audio.music_host && !audio.music_instance);
    pd_audio_set_music(&audio,PD_MUSIC_SEWERS);
    assert(audio.music_host && audio.music_instance!=second);
    pd_audio_set_music(&audio,0xff);
    assert(!audio.music_host && !audio.music_instance);
    for(unsigned i=0;i<PD_SOUND_COUNT;++i) {
        pd_audio_play(&audio,(uint8_t)i); pd_audio_tick(&audio);
        assert(pending_token && audio.pending_count==1 && plays==i);
        complete(&audio); pd_audio_tick(&audio); assert(plays==i+1 && !audio.pending_count);
        unsigned held=0; for(unsigned j=0;j<PD_AUDIO_SOUND_SLOTS;++j) held+=audio.sound_handles[j]!=0;
        assert(held<=PD_AUDIO_SOUND_SLOTS);
    }
    unsigned before=loads;
    pd_audio_play(&audio,PD_SOUND_COUNT-1); pd_audio_tick(&audio); assert(loads==before && !pending_token);
    rejected=1; pd_audio_play(&audio,PD_SOUND_HIT); pd_audio_tick(&audio);
    assert(!pending_token && !audio.pending_count); rejected=0;
    pd_audio_play(&audio,PD_SOUND_HIT); pd_audio_tick(&audio); assert(pending_token);
    pd_audio_stop(&audio); assert(cancels==1 && !audio.pending_count);
    before=closes; complete(&audio); assert(closes==before+1 && !audio.sound_load_token);
    for(unsigned i=0;i<PD_AUDIO_SOUND_SLOTS;++i) assert(!audio.sound_handles[i]);
    puts("Pixel Dungeon audio: bounded asynchronous preparation, hot handles, eviction, immediate rejection and late cancellation passed");
}
