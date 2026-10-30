// SPDX-License-Identifier: MIT
// Keep Pocket's tested permission, PCM and event handling; add resident recordings.
#include "pxa_assets.h"
#define clear_sound clear_synth_sound
#define handle_sound handle_synth_sound
#define service_sound service_synth_sound
#include "../pocket-play/audio.inc"
#undef clear_sound
#undef handle_sound
#undef service_sound
struct Recording { uint64_t handle; unsigned id; uint32_t age,duration; };
struct RequestedSound { unsigned id; bool pending,loop; uint64_t until; };
Recording recordings[6]{};
RequestedSound requested_sounds[3]{};
uint64_t recording_started[3]{},recording_stop_at[3]{};
uint32_t recording_duration[3]{};
bool recording_loop[3]{};unsigned recording_track_id[3]{~0u,~0u,~0u};
uint32_t recording_sizes[64]{};unsigned recording_query_id,recording_prepared_id=~0u;bool recording_querying;
const char *const *recording_paths;
unsigned recording_count,recording_age,recording_loading_id,recording_loading_slot;
unsigned effects_gain=100,music_gain=0,prewarm_index;
bool recording_loading,music_started;
uint64_t recording_retry;
constexpr uint64_t RECORDING_LOAD=800;
constexpr uint64_t RECORDING_QUERY=801;
constexpr unsigned RECORDING_BYTES_LIMIT=280*1024;
void clear_sound(){
    clear_synth_sound();
    memset(requested_sounds,0,sizeof(requested_sounds));
    memset(recording_stop_at,0,sizeof(recording_stop_at));
    memset(recording_duration,0,sizeof(recording_duration));
    if(sound_handle){
        for(unsigned i=0;i<3;++i)pxa_audio_control_sound(sound_handle,uint8_t(i),3,0);
        pxa_audio_control_music(sound_handle,3,0);
    }
    music_started=false;
}
int16_t recording_gain(unsigned percentage){
    // 0 is handled before admission. Retain headroom when recordings overlap.
    return int16_t((-3-int((100-percentage)*36/100))*256);
}
int32_t handle_sound(const pxa_event_t &e){
    if(e.service==PXA_ASSETS_SERVICE&&e.token==RECORDING_QUERY){
        pxa_asset_result_t result{};recording_querying=false;
        if(!pxa_assets_parse_result(&e,RECORDING_QUERY,PXA_ASSETS_QUERY,&result))return PXA_STATUS_INTERNAL;
        if(!result.status&&recording_query_id<64){recording_sizes[recording_query_id]=result.info.decoded_bytes;recording_prepared_id=recording_query_id;}
        else recording_retry=now_us()+1000000;
        return PXA_EVENT_HANDLED;
    }
    // Prepared recordings require a media session. A PCM-only session rejects
    // PLAY_SOUND and PLAY_MUSIC even when its output format is identical.
    if(e.service==PXA_PERMISSION_SERVICE&&e.token==SOUND_PERMISSION){
        pxa_permission_acquire_result_t result;
        if(!pxa_permission_parse_acquire(&e,SOUND_PERMISSION,&result))return PXA_STATUS_INTERNAL;
        if(!result.status){sound_permission=result.handle;pxa_audio_open_media(SOUND_LEGACY,result.handle);}
        else pxa_log_write(2,"Milky: optional audio denied; continuing silently");
        return PXA_EVENT_HANDLED;
    }
    if(e.service==PXA_ASSETS_SERVICE&&e.token==RECORDING_LOAD){
        pxa_asset_result_t result;
        if(!pxa_assets_parse_result(&e,RECORDING_LOAD,PXA_ASSETS_LOAD,&result))return PXA_STATUS_INTERNAL;
        recording_loading=false;
        if(!result.status&&result.info.kind==PXA_ASSET_AUDIO&&result.info.encoding==PXA_ASSET_ENCODING_PCM_S16LE_16K_MONO){
            recordings[recording_loading_slot]={result.handle,recording_loading_id,++recording_age,result.info.decoded_bytes/2};
            char line[96];snprintf(line,sizeof(line),"Milky: recording ready id=%u bytes=%lu",recording_loading_id,(unsigned long)result.info.decoded_bytes);pxa_log_write(2,line);
        }else{
            if(result.handle)pxa_close_handle(result.handle);
            recording_retry=now_us()+1000000;
            pxa_log_write(3,"Milky: recording preparation deferred");
        }
        return PXA_EVENT_HANDLED;
    }
    return handle_synth_sound(e);
}
void service_sound(){
    service_synth_sound();
    if(!sound_handle||background||paused||exit_requested)return;
    if(music_gain&&!music_started){
        uint64_t instance;
        const int32_t result=pxa_audio_play_music(sound_handle,"assets/bgm.ogg",1,recording_gain(music_gain),&instance);
        if(result>0)music_started=true;
    }
    unsigned wanted=recording_count;
    for(unsigned track=0;track<3;++track){
        if(recording_stop_at[track]&&sound_now()>=recording_stop_at[track]){
            pxa_audio_control_sound(sound_handle,uint8_t(track),3,0);recording_stop_at[track]=0;
            recording_duration[track]=0;
        }
        auto &request=requested_sounds[track];
        if(!request.pending)continue;
        if(!effects_gain||request.id>=recording_count||sound_now()>request.until){request.pending=false;continue;}
        bool ready=false;
        for(auto &recording:recordings)if(recording.handle&&recording.id==request.id){
            const auto result=pxa_audio_play_sound_track(sound_handle,recording.handle,uint8_t(track),request.loop,recording_gain(effects_gain));
            if(result>=0){
                recording.age=++recording_age;request.pending=false;
                recording_started[track]=sound_now();recording_duration[track]=recording.duration;
                recording_track_id[track]=recording.id;recording_loop[track]=request.loop;
                char line[96];snprintf(line,sizeof(line),"Milky: recording play id=%u track=%u",request.id,track);pxa_log_write(2,line);
            }
            ready=true;break;
        }
        if(!ready&&wanted==recording_count)wanted=request.id;
    }
    if(recording_loading)return;
    // Prepare the first four voices while the player is reading the title.
    while(wanted==recording_count&&prewarm_index<recording_count&&prewarm_index<1){
        unsigned id=prewarm_index++;bool found=false;
        for(const auto &r:recordings)found|=r.handle&&r.id==id;
        if(!found)wanted=id;
    }
    if(wanted==recording_count&&recording_prepared_id<recording_count)wanted=recording_prepared_id;
    if(wanted==recording_count||recording_loading||now_us()<recording_retry)return;
    if(wanted>=64)return;
    if(!recording_sizes[wanted]){
        if(!recording_querying){recording_query_id=wanted;recording_querying=pxa_assets_query(RECORDING_QUERY,recording_paths[wanted])==0;}
        return;
    }
    // Images reserve 224 KiB of the standard 512 KiB shared Assets cache.
    // Retire idle recordings before admitting another complete meme voice.
    auto active=[](unsigned id){for(unsigned t=0;t<3;++t)if(recording_track_id[t]==id&&recording_duration[t]&&(recording_loop[t]||sound_now()-recording_started[t]<recording_duration[t]))return true;return false;};
    unsigned used=0;for(const auto &r:recordings)if(r.handle)used+=r.duration*2;
    while(used+recording_sizes[wanted]>RECORDING_BYTES_LIMIT){
        int oldest=-1;for(unsigned i=0;i<6;++i)if(recordings[i].handle&&!active(recordings[i].id)&&(oldest<0||recordings[i].age<recordings[oldest].age))oldest=int(i);
        if(oldest<0)return;
        auto &r=recordings[oldest];if(pxa_close_handle(r.handle))return;used-=r.duration*2;r={};
    }
    unsigned slot=0;
    for(unsigned i=0;i<6;++i){if(!recordings[i].handle){slot=i;break;}if(recordings[i].age<recordings[slot].age)slot=i;}
    if(recordings[slot].handle)pxa_close_handle(recordings[slot].handle);
    recordings[slot]={};recording_loading_id=wanted;recording_loading_slot=slot;
    const auto result=pxa_assets_load_sound(RECORDING_LOAD,recording_paths[wanted]);
    if(wanted==recording_prepared_id)recording_prepared_id=~0u;
    recording_loading=result==0;
    if(result)recording_retry=now_us()+1000000;
}
