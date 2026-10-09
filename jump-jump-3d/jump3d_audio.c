#include "jump3d_audio.h"
#include "jump3d_audio_data.h"

#define J3_AUDIO_PIN(name, index, samples, looping) \
    _Static_assert(J3_CLIP_##name == (index), "clip order drifted: " #name);
J3_AUDIO_BANK(J3_AUDIO_PIN)
#undef J3_AUDIO_PIN

/* The Guest owns commands and cache holdings. Host voices pin their assets,
 * own the sampling clock, loop/fade independently and never read Guest PCM. */
static int16_t gain_db(uint16_t gain) {
    // Authored Q12 levels, expressed as dB Q8 without a Guest math library.
    if (gain >= J3_GAIN_FULL) return 0;
    if (gain >= J3_GAIN_LOUD) return -2*256;
    if (gain >= J3_GAIN_SOFT) return -5*256;
    if (gain >= J3_GAIN_QUIET) return -9*256;
    if (gain >= J3_GAIN_BGM) return -11*256;
    if (gain) return -20*256;
    return -60*256;
}
static int sound_slot(const j3_audio_t *audio, uint8_t clip) {
    for (unsigned i=0;i<J3_AUDIO_SOUND_SLOTS;++i)
        if (audio->sound_handles[i] && audio->sound_ids[i]==clip) return (int)i;
    return -1;
}
static int prepare_sound(j3_audio_t *audio, uint8_t clip) {
    int slot=sound_slot(audio,clip);
    if (slot>=0) return slot;
    if (audio->load_token) return -1;
    unsigned selected=0;
    for (unsigned i=0;i<J3_AUDIO_SOUND_SLOTS;++i) {
        if (!audio->sound_handles[i]) {selected=i;break;}
        if (audio->sound_ages[i]<audio->sound_ages[selected]) selected=i;
    }
    if (audio->sound_handles[selected]) {
        if (pxa_close_handle(audio->sound_handles[selected])) return -1;
        audio->sound_handles[selected]=0;
    }
    uint64_t token=++audio->sequence;
    if (!token || pxa_assets_load_sound(token,j3_audio_bank[clip].path)) return -1;
    audio->load_token=token; audio->load_id=clip; audio->load_slot=(uint8_t)selected;
    return -1;
}
void j3_audio_play(j3_audio_t *audio,uint8_t channel,uint8_t clip,uint16_t gain,uint8_t loop) {
    if (!audio || channel>=J3_CHANNEL_COUNT || clip>=J3_CLIP_COUNT ||
        (channel==J3_CHANNEL_BGM && clip!=J3_CLIP_ICON) ||
        (channel!=J3_CHANNEL_BGM && clip==J3_CLIP_ICON)) return;
    j3_audio_voice_t *v=&audio->voices[channel];
    v->clip=clip; v->gain_q12=gain; v->loop=loop!=0; v->requested=1;
    // The current Host voice continues until the replacement is ready.
    v->started=0; v->ends_us=0; v->stop_pending=0;
}
void j3_audio_stop(j3_audio_t *audio,uint8_t channel) {
    if (!audio || channel>=J3_CHANNEL_COUNT) return;
    audio->voices[channel].requested=0;
    audio->voices[channel].stop_pending=1;
    audio->voices[channel].started=0;
}
void j3_audio_stop_all(j3_audio_t *audio) {
    if (!audio) return;
    for (unsigned i=0;i<J3_CHANNEL_COUNT;++i) j3_audio_stop(audio,(uint8_t)i);
    // Shutdown calls this immediately before Guest exit, so send stops now.
    j3_audio_tick(audio,audio->tick_us);
}
int j3_audio_channel_active(const j3_audio_t *audio,uint8_t channel) {
    if (!audio || channel>=J3_CHANNEL_COUNT) return 0;
    const j3_audio_voice_t *v=&audio->voices[channel];
    return v->requested && (!v->started || v->loop || audio->tick_us<v->ends_us);
}
uint8_t j3_audio_channel_clip(const j3_audio_t *audio,uint8_t channel) {
    return j3_audio_channel_active(audio,channel) ? audio->voices[channel].clip : J3_CLIP_COUNT;
}

void j3_audio_start(j3_audio_t *audio, uint8_t *packet, uint32_t capacity) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    if (audio == NULL || packet == NULL ||
        audio->state != J3_AUDIO_OFF)
        return;
    audio->state = J3_AUDIO_WAIT_PERMISSION;
    uint32_t size = 0;
    if (!pxa_permission_build(packet, capacity, PXA_PERMISSION_ACQUIRE,
                                 J3_AUDIO_PERMISSION_REQUEST, permission_name,
                                 sizeof(permission_name) - 1u, permission_scope,
                                 sizeof(permission_scope) - 1u, &size) ||
        pxa_submit(packet, size) != PXA_STATUS_OK)
        audio->state = J3_AUDIO_UNAVAILABLE;
}

int j3_audio_handle_event(j3_audio_t *audio, const pxa_event_t *event,
                          uint8_t *packet, uint32_t capacity) {
    (void)packet;
    (void)capacity;
    if (audio == NULL || event == NULL || packet == NULL) return 0;
    if (event->service == PXA_ASSETS_SERVICE && event->opcode == PXA_ASSETS_LOAD &&
        audio->load_token && event->token == audio->load_token) {
        pxa_asset_result_t result;
        if (!pxa_assets_parse_result(event,event->token,PXA_ASSETS_LOAD,&result)) return 1;
        audio->load_token=0;
        if (!result.status && audio->state == J3_AUDIO_READY) {
            audio->sound_handles[audio->load_slot]=result.handle;
            audio->sound_ids[audio->load_slot]=audio->load_id;
            audio->sound_ages[audio->load_slot]=++audio->sound_age;
        } else {
            if (!result.status) (void)pxa_close_handle(result.handle);
            for (unsigned i=0;i<J3_CHANNEL_BGM;++i)
                if (audio->voices[i].clip==audio->load_id && !audio->voices[i].started)
                    audio->voices[i].requested=0;
            (void)pxa_log_write(2,"jump-jump-3d: Host sound preparation failed");
        }
        return 1;
    }
    if (event->service == PXA_AUDIO_SERVICE && event->opcode == PXA_AUDIO_PLAYBACK_EVENT) {
        pxa_audio_playback_event_t result;
        if (!pxa_audio_parse_playback(event,&result) || result.session != audio->session_handle ||
            result.instance != audio->music_instance) return 0;
        if (result.state == PXA_AUDIO_PLAYBACK_READY) audio->music_ready=1;
        else {
            audio->music_ready=0; audio->music_instance=0;
            audio->voices[J3_CHANNEL_BGM].started=0;
            audio->voices[J3_CHANNEL_BGM].requested=0;
        }
        return 1;
    }
    if (event->service == PXA_PERMISSION_SERVICE &&
        event->opcode == PXA_PERMISSION_ACQUIRE &&
        event->token == J3_AUDIO_PERMISSION_REQUEST) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(event,
                                             J3_AUDIO_PERMISSION_REQUEST,
                                             &result) ||
            result.status != PXA_STATUS_OK) {
            audio->state = J3_AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->permission_handle = result.handle;
        audio->state = J3_AUDIO_WAIT_OPEN;
        if (pxa_audio_open_media(J3_AUDIO_OPEN_REQUEST,
                                    audio->permission_handle) != PXA_STATUS_OK)
            audio->state = J3_AUDIO_UNAVAILABLE;
        return 1;
    }
    if (event->service == PXA_AUDIO_SERVICE &&
        event->opcode == PXA_AUDIO_OPEN_SESSION &&
        event->token == J3_AUDIO_OPEN_REQUEST) {
        pxa_audio_open_result_t result;
        if (!pxa_audio_parse_open(event, J3_AUDIO_OPEN_REQUEST, &result) ||
            result.status != PXA_STATUS_OK ||
            result.sample_rate != J3_AUDIO_SAMPLE_RATE || result.channels != 1 ||
            result.frame_ms != 20) {
            audio->state = J3_AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->session_handle = result.handle;
        audio->state = J3_AUDIO_WAIT_GRAPH;
        {
            const pxa_audio_eq_band_t band = {1500, 256, 256};
            const pxa_audio_graph_t graph = {-256, &band, 1};
            if (pxa_audio_commit_graph(J3_AUDIO_GRAPH_REQUEST,
                                           audio->session_handle,
                                           &graph) != PXA_STATUS_OK)
                audio->state = J3_AUDIO_UNAVAILABLE;
        }
        return 1;
    }
    if (event->service == PXA_AUDIO_SERVICE &&
        event->opcode == PXA_AUDIO_COMMIT_GRAPH &&
        event->token == J3_AUDIO_GRAPH_REQUEST) {
        int32_t status;
        if (!pxa_audio_parse_status(event, J3_AUDIO_GRAPH_REQUEST,
                                       PXA_AUDIO_COMMIT_GRAPH, &status) ||
            status != PXA_STATUS_OK) {
            audio->state = J3_AUDIO_UNAVAILABLE;
            return 1;
        }
        audio->state = J3_AUDIO_READY;
        (void)pxa_log_write(2, "jump-jump-3d audio ready");
        return 1;
    }
    return 0;
}

void j3_audio_tick(j3_audio_t *audio,uint64_t now) {
    static const uint8_t preload[]={J3_CLIP_SCALE_INTRO,J3_CLIP_SCALE_LOOP,J3_CLIP_SUCCESS,J3_CLIP_POP};
    if (!audio || audio->state!=J3_AUDIO_READY) return;
    audio->tick_us=now;
    for (unsigned i=0;i<J3_CHANNEL_COUNT;++i) {
        j3_audio_voice_t *v=&audio->voices[i];
        if (v->stop_pending) {
            int32_t result=i==J3_CHANNEL_BGM ?
                pxa_audio_control_music(audio->session_handle,PXA_AUDIO_ASSET_STOP,0) :
                pxa_audio_control_sound(audio->session_handle,(uint8_t)i,PXA_AUDIO_ASSET_STOP,0);
            if (result>=0) {
                v->stop_pending=0;
                if (i==J3_CHANNEL_BGM) {audio->music_instance=0;audio->music_ready=0;}
            }
            continue;
        }
        if (!v->requested) continue;
        if (v->started) {
            if (!v->loop && now>=v->ends_us) {v->requested=0;v->started=0;}
            continue;
        }
        int32_t result;
        if (i==J3_CHANNEL_BGM) {
            uint64_t instance;
            result=pxa_audio_play_music(audio->session_handle,j3_audio_bank[v->clip].path,
                v->loop,gain_db(v->gain_q12),&instance);
            if (result>=0) {
                audio->music_instance=instance;audio->music_ready=0;
                audio->music_gain_q12=v->gain_q12;
            }
        } else {
            int slot=prepare_sound(audio,v->clip);
            if (slot<0) continue;
            result=pxa_audio_play_sound_track(audio->session_handle,audio->sound_handles[slot],
                (uint8_t)i,v->loop,gain_db(v->gain_q12));
            if (result>=0) audio->sound_ages[slot]=++audio->sound_age;
        }
        if (result>=0) {
            v->started=1;
            v->ends_us=now+(uint64_t)j3_audio_bank[v->clip].samples*1000000/J3_AUDIO_SAMPLE_RATE;
        } else if (result!=PXA_STATUS_WOULD_BLOCK) {
            v->requested=0;
            (void)pxa_log_write(2,"jump-jump-3d: Host playback command rejected");
        }
    }
    j3_audio_voice_t *bgm=&audio->voices[J3_CHANNEL_BGM];
    if (bgm->started && audio->music_ready) {
        uint16_t gain=J3_GAIN_BGM;
        for (unsigned i=J3_CHANNEL_LAND;i<=J3_CHANNEL_BONUS;++i)
            if (j3_audio_channel_active(audio,(uint8_t)i)) gain=J3_GAIN_BGM_DUCKED;
        if (gain!=audio->music_gain_q12 && pxa_audio_control_music(audio->session_handle,
            PXA_AUDIO_ASSET_SET_GAIN,gain_db(gain))>=0) audio->music_gain_q12=gain;
    }
    // Prepare common effects during idle ticks, using the same bounded cache.
    if (!audio->load_token && audio->preload_index<sizeof(preload)) {
        if (prepare_sound(audio,preload[audio->preload_index])>=0) ++audio->preload_index;
    }
}
