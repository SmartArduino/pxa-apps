#include "audio.h"
#include "engine.h"
#include "pxa_audio.h"
#include "pxa_permission.h"
#include <stdio.h>
#include <string.h>

static uint64_t permission,session,last_query,last_pump;
static unsigned rate;
static bool active,pending,flushing,querying;
static int16_t pcm[320];
static uint32_t writes,blocked,nonzero;
void audio_start() {
    uint8_t packet[128];uint32_t n;
    if(pxa_permission_build(packet,sizeof(packet),PXA_PERMISSION_ACQUIRE,200,
        "audio.playback",14,(const uint8_t*)"media",5,&n))pxa_control_packet(packet,n);
}
void audio_flush() {
    pending=false;
    if(session && !flushing) {flushing=true;if(pxa_audio_flush(204,session))flushing=false;}
}
void audio_tick(uint64_t now) {
    if(!active || flushing || now-last_pump<12000u)return;
    last_pump=now;
    // Each Host packet is at most 20 ms. Stop on backpressure and keep exactly
    // the unaccepted packet, so retries never skip or duplicate mixer samples.
    for(unsigned i=0;i<4;++i) {
        if(!pending) {
#if OL_AUDIO_IDLE_SKIP
            if(!ol_audio_active())break;
#endif
            ol_audio(pcm,320,rate);pending=true;
        }
        int r=pxa_audio_write_pcm(session,reinterpret_cast<uint8_t*>(pcm),sizeof(pcm));
        if(r==PXA_STATUS_WOULD_BLOCK) {++blocked;break;}
        if(r!=sizeof(pcm)) {pxa_log_write(3,"OpenLara audio stream stopped after Host error");active=false;break;}
        pending=false;++writes;
        for(auto sample:pcm)if(sample)++nonzero;
    }
    if(!querying && now-last_query>=5000000u) {
        querying=true;last_query=now;
        if(pxa_audio_query(203,session))querying=false;
    }
}
bool audio_event(const pxa_event_t &e) {
    pxa_permission_revoked_t revoked;
    if(pxa_permission_parse_revoked(&e,&revoked) && revoked.name_size==14 &&
       !memcmp(revoked.name,"audio.playback",14)) {audio_stop();return true;}
    if(e.service==PXA_PERMISSION_SERVICE && e.token==200) {
        pxa_permission_acquire_result_t r;
        if(pxa_permission_parse_acquire(&e,200,&r) && !r.status) {
            permission=r.handle;pxa_audio_open_media(201,permission);
        } else pxa_log_write(3,"OpenLara: audio permission unavailable; sound disabled");
        return true;
    }
    if(e.service!=PXA_AUDIO_SERVICE || e.token<201 || e.token>204)return false;
    if(e.token==201) {
        pxa_audio_open_result_t r;
        if(pxa_audio_parse_open(&e,201,&r) && !r.status) {
            session=r.handle;rate=r.sample_rate;
            if(r.channels!=1 || rate!=16000) {audio_stop();pxa_log_write(3,"OpenLara requires 16 kHz mono Host audio");}
            else pxa_audio_commit_gain(202,session,-6*256);
        }
    } else if(e.token==202) {
        int32_t status;
        active=pxa_audio_parse_status(&e,202,PXA_AUDIO_COMMIT_GRAPH,&status) && !status;
        if(active)pxa_log_write(2,"OpenLara audio ready rate=16000 channels=1");
    } else if(e.token==203) {
        querying=false;pxa_audio_state_result_t r;
        if(pxa_audio_parse_state(&e,203,&r) && !r.status) {
            char message[224];snprintf(message,sizeof(message),
                "OpenLara audio submitted=%llu rendered=%llu queued=%u flags=%u writes=%u blocked=%u nonzero=%u",
                (unsigned long long)r.submitted_samples,(unsigned long long)r.accepted_samples,
                r.queued_samples,r.flags,writes,blocked,nonzero);pxa_log_write(2,message);
        }
    } else if(e.token==204) {flushing=false;}
    return true;
}
void audio_stop() {
    active=false;pending=false;flushing=false;querying=false;
    if(session)pxa_close_handle(session);session=0;
    if(permission)pxa_close_handle(permission);permission=0;
}
