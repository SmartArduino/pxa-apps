#include "audio.h"


#include "game.h"

#define PD_AUDIO_PERMISSION_REQUEST UINT32_C(0x50440001)
#define PD_AUDIO_OPEN_REQUEST UINT32_C(0x50440002)
#define PD_AUDIO_GRAPH_REQUEST UINT32_C(0x50440003)
#define PD_AUDIO_GRAPH_GAIN_DB_Q8 (-2 * 256)
#define PD_MUSIC_GAIN_Q8 (-7 * 256)
#define PD_MUSIC_START_GAIN_Q8 (-40 * 256)
#define PD_MUSIC_FADE_STEP_Q8 (2 * 256)

static const char *const music_files[PD_MUSIC_COUNT + 1] = {
    "assets/music/sewers_1.ogg",
    "assets/music/prison_1.ogg",
    "assets/music/caves_1.ogg",
    "assets/music/city_1.ogg",
    "assets/music/halls_1.ogg",
    "assets/music/theme_1.ogg",
};

typedef struct {
    const char *path;
    int16_t gain_db_q8;
} pd_sound_file_t;

static const pd_sound_file_t sound_files[PD_SOUND_COUNT] = {
    [PD_SOUND_HIT] = {"assets/sfx/hit.pcm", -3 * 256},
    [PD_SOUND_MISS] = {"assets/sfx/miss.pcm", -6 * 256},
    [PD_SOUND_HIT_STRONG] = {"assets/sfx/hit_crush.pcm", -3 * 256},
    [PD_SOUND_DEATH] = {"assets/sfx/death.pcm", -3 * 256},
    [PD_SOUND_DRINK] = {"assets/sfx/drink.pcm", -4 * 256},
    [PD_SOUND_EAT] = {"assets/sfx/eat.pcm", -5 * 256},
    [PD_SOUND_GOLD] = {"assets/sfx/gold.pcm", -5 * 256},
    [PD_SOUND_ITEM] = {"assets/sfx/item.pcm", -6 * 256},
    [PD_SOUND_READ] = {"assets/sfx/read.pcm", -6 * 256},
    [PD_SOUND_DESCEND] = {"assets/sfx/descend.pcm", -3 * 256},
    [PD_SOUND_DOOR] = {"assets/sfx/door_open.pcm", -6 * 256},
    [PD_SOUND_TRAP] = {"assets/sfx/trap.pcm", -3 * 256},
    [PD_SOUND_LEVELUP] = {"assets/sfx/levelup.pcm", -3 * 256},
    [PD_SOUND_SHATTER] = {"assets/sfx/shatter.pcm", -4 * 256},
    [PD_SOUND_UNLOCK] = {"assets/sfx/unlock.pcm", -4 * 256},
    [PD_SOUND_HUNGRY] = {"assets/sfx/health_warn.pcm", -6 * 256},
    [PD_SOUND_STEP] = {"assets/sfx/step.pcm", -8 * 256},
    [PD_SOUND_GRASS] = {"assets/sfx/grass.pcm", -7 * 256},
    [PD_SOUND_TRAMPLE] = {"assets/sfx/trample.pcm", -6 * 256},
    [PD_SOUND_WATER] = {"assets/sfx/water.pcm", -7 * 256},
    [PD_SOUND_MAGIC] = {"assets/sfx/hit_magic.pcm", -5 * 256},
    [PD_SOUND_LIGHTNING] = {"assets/sfx/lightning.pcm", -6 * 256},
    [PD_SOUND_BLAST] = {"assets/sfx/blast.pcm", -5 * 256},
};

/* Eight Guest holdings in the shared Host cache, with one asynchronous load.
 * A playing voice retains its own object, so replacing a holding is safe. */
static int sound_slot(pd_audio_t *audio,uint8_t id) {
    for(unsigned i=0;i<PD_AUDIO_SOUND_SLOTS;++i)
        if(audio->sound_handles[i] && audio->sound_ids[i]==id) return (int)i;
    return -1;
}
static int prepare_sound(pd_audio_t *audio,uint8_t id) {
    if(sound_slot(audio,id)>=0) return 1;
    if(audio->sound_load_token) return 0;
    unsigned slot=0;
    for(unsigned i=0;i<PD_AUDIO_SOUND_SLOTS;++i) {
        if(!audio->sound_handles[i]) {slot=i;break;}
        if(audio->sound_ages[i]<audio->sound_ages[slot]) slot=i;
    }
    if(audio->sound_handles[slot]) {
        if(pxa_close_handle(audio->sound_handles[slot])) return -1;
        audio->sound_handles[slot]=0;
    }
    if(audio->sound_sequence==UINT64_MAX) return -1;
    uint64_t token=++audio->sound_sequence;
    if(pxa_assets_load_sound(token,sound_files[id].path)) return -1;
    audio->sound_load_token=token; audio->sound_load_id=id; audio->sound_load_slot=(uint8_t)slot;
    return 0;
}
static void pop_sound(pd_audio_t *audio) {
    for(unsigned i=1;i<audio->pending_count;++i) audio->pending[i-1]=audio->pending[i];
    if(audio->pending_count) --audio->pending_count;
}

void pd_audio_init(pd_audio_t *audio) {
    static const char permission_name[] = "audio.playback";
    static const uint8_t permission_scope[] = "media";
    uint8_t packet[96];
    uint32_t packet_size = 0;
    for (uint32_t index = 0; index < sizeof(*audio); ++index)
        ((uint8_t *)audio)[index] = 0;
    audio->sound_sequence = UINT64_C(0x5044534600000000);
    audio->music_track = 0xFF;
    audio->music_pending = 0xFF;
    audio->state = PD_AUDIO_WAIT_PERMISSION;
    if (!pxa_permission_build(
            packet, sizeof(packet), PXA_PERMISSION_ACQUIRE,
            PD_AUDIO_PERMISSION_REQUEST, permission_name,
            sizeof(permission_name) - 1u, permission_scope,
            sizeof(permission_scope) - 1u, &packet_size) ||
        pxa_submit(packet, packet_size) != PXA_STATUS_OK) {
        audio->state = PD_AUDIO_UNAVAILABLE;
    }
}

static void open_session(pd_audio_t *audio) {
    if (audio->permission_handle == 0) {
        audio->state = PD_AUDIO_UNAVAILABLE;
        return;
    }
    if (pxa_audio_open_media(PD_AUDIO_OPEN_REQUEST,
                                audio->permission_handle) != PXA_STATUS_OK) {
        audio->state = PD_AUDIO_UNAVAILABLE;
    }
}

static void commit_graph(pd_audio_t *audio) {
    const pxa_audio_eq_band_t band = {120, 0, 128};
    const pxa_audio_graph_t graph = {
        PD_AUDIO_GRAPH_GAIN_DB_Q8, &band, 1};
    if (audio->session_handle == 0) return;
    if (pxa_audio_commit_graph(PD_AUDIO_GRAPH_REQUEST,
                                  audio->session_handle,
                                  &graph) != PXA_STATUS_OK)
        audio->state = PD_AUDIO_UNAVAILABLE;
}

static int start_music_asset(pd_audio_t *audio, uint8_t track) {
    if (track > PD_MUSIC_TITLE) return 0;
    uint64_t instance;
    int32_t status=pxa_audio_play_music(audio->session_handle,
        music_files[track],1,PD_MUSIC_START_GAIN_Q8,&instance);
    if(status<0) {
        if(status!=PXA_STATUS_WOULD_BLOCK)
            (void)pxa_log_write(2,"pd: packaged music command rejected");
        audio->music_pending=status==PXA_STATUS_WOULD_BLOCK ? track : 0xff;
        audio->music_fading_out=0;
        return 0;
    }
    audio->music_track = track;
    audio->music_asset_gain_db_q8 = PD_MUSIC_START_GAIN_Q8;
    audio->music_host=1; audio->music_ready=0; audio->music_instance=instance;
    audio->music_pending=0xff; audio->music_fading_out=0;
    return 1;
}

void pd_audio_handle_event(pd_audio_t *audio, const pxa_event_t *event,
                           uint8_t *packet, uint32_t packet_capacity) {
    (void)packet;
    (void)packet_capacity;
    if(event->service==PXA_AUDIO_SERVICE && event->opcode==PXA_AUDIO_PLAYBACK_EVENT) {
        pxa_audio_playback_event_t playback;
        if(!pxa_audio_parse_playback(event,&playback) ||
           playback.session!=audio->session_handle || playback.instance!=audio->music_instance) return;
        if(playback.state==PXA_AUDIO_PLAYBACK_READY) {
            audio->music_ready=1;
            (void)pxa_log_write(2,"pd: packaged music ready");
        } else {
            audio->music_host=audio->music_ready=audio->music_fading_out=0;
            audio->music_instance=0; audio->music_track=0xff;
            if(playback.state==PXA_AUDIO_PLAYBACK_ERROR)
                (void)pxa_log_write(2,"pd: packaged music playback failed");
        }
        return;
    }
    if(event->service==PXA_ASSETS_SERVICE && event->opcode==PXA_ASSETS_LOAD &&
       audio->sound_load_token && event->token==audio->sound_load_token) {
        pxa_asset_result_t result;
        if(!pxa_assets_parse_result(event,event->token,PXA_ASSETS_LOAD,&result)) return;
        audio->sound_load_token=0;
        if(!result.status && audio->state==PD_AUDIO_READY) {
            unsigned slot=audio->sound_load_slot;
            audio->sound_handles[slot]=result.handle; audio->sound_ids[slot]=audio->sound_load_id;
            audio->sound_ages[slot]=++audio->sound_age;
        } else {
            if(!result.status) (void)pxa_close_handle(result.handle);
            if(audio->pending_count && audio->pending[0]==audio->sound_load_id) pop_sound(audio);
        }
        return;
    }
    if (event->service == PXA_PERMISSION_SERVICE &&
        event->opcode == PXA_PERMISSION_ACQUIRE &&
        event->token == PD_AUDIO_PERMISSION_REQUEST) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(event,
                                             PD_AUDIO_PERMISSION_REQUEST,
                                             &result)) return;
        if (result.status != PXA_STATUS_OK) {
            audio->state = PD_AUDIO_UNAVAILABLE;
            (void)pxa_log_write(2, "pd: audio permission denied");
            return;
        }
        audio->permission_handle = result.handle;
        audio->state = PD_AUDIO_WAIT_SESSION;
        open_session(audio);
        return;
    }
    if (event->service == PXA_AUDIO_SERVICE &&
        event->opcode == PXA_AUDIO_OPEN_SESSION &&
        event->token == PD_AUDIO_OPEN_REQUEST) {
        pxa_audio_open_result_t opened;
        if (!pxa_audio_parse_open(event, PD_AUDIO_OPEN_REQUEST,
                                      &opened)) return;
        if (opened.status != PXA_STATUS_OK) {
            audio->state = PD_AUDIO_UNAVAILABLE;
            (void)pxa_log_write(2, "pd: audio session unavailable");
            return;
        }
        audio->session_handle = opened.handle;
        audio->state = PD_AUDIO_WAIT_GRAPH;
        commit_graph(audio);
        return;
    }
    if (event->service == PXA_AUDIO_SERVICE &&
        event->opcode == PXA_AUDIO_COMMIT_GRAPH &&
        event->token == PD_AUDIO_GRAPH_REQUEST) {
        int32_t status = 0;
        if (pxa_audio_parse_status(event, PD_AUDIO_GRAPH_REQUEST,
                                      PXA_AUDIO_COMMIT_GRAPH, &status)) {
            if (status != PXA_STATUS_OK) {
                audio->state = PD_AUDIO_UNAVAILABLE;
                return;
            }
            audio->state = PD_AUDIO_READY;
            (void)prepare_sound(audio,PD_SOUND_STEP);
            if (audio->music_track != 0xFF)
                start_music_asset(audio, audio->music_track);
            (void)pxa_log_write(2, "pd: audio ready");
        }
    }
}

void pd_audio_play(pd_audio_t *audio, uint8_t sfx) {
    if (sfx >= PD_SOUND_COUNT) return;
    if (audio->pending_count < PD_AUDIO_PENDING_SOUNDS)
        audio->pending[audio->pending_count++] = sfx;
}

void pd_audio_set_music(pd_audio_t *audio, uint8_t track) {
    if (track > PD_MUSIC_TITLE) {
        if (audio->music_host)
            (void)pxa_audio_control_asset(audio->session_handle,
                                              PXA_AUDIO_ASSET_STOP, 0);
        audio->music_host = 0;
        audio->music_ready=0; audio->music_instance=0;
        audio->music_track = 0xFF;
        audio->music_pending = 0xFF;
        return;
    }
    if (audio->music_track == track && (audio->music_host || audio->state!=PD_AUDIO_READY)) {
        audio->music_pending = 0xFF;
        audio->music_fading_out = 0;
        return;
    }
    if (audio->music_pending == track) return;
    if (audio->music_host) {
        audio->music_pending = track;
        audio->music_fading_out = 1;
        return;
    }
    if (audio->state == PD_AUDIO_READY && audio->session_handle != 0) {
        start_music_asset(audio, track);
        return;
    }
    audio->music_track = track;
}

void pd_audio_stop(pd_audio_t *audio) {
    if(audio->sound_load_token) (void)pxa_cancel(audio->sound_load_token);
    for(unsigned i=0;i<PD_AUDIO_SOUND_SLOTS;++i) if(audio->sound_handles[i]) {
        if(!pxa_close_handle(audio->sound_handles[i])) audio->sound_handles[i]=0;
    }
    if (audio->music_host)
        (void)pxa_audio_control_asset(audio->session_handle,
                                          PXA_AUDIO_ASSET_STOP, 0);
    audio->music_host = 0;
    audio->music_ready=0; audio->music_instance=0; audio->music_pending=0xff;
    audio->music_track = 0xFF;
    audio->pending_count = 0;
    audio->state = PD_AUDIO_UNAVAILABLE;
    if (audio->session_handle != 0) {
        (void)pxa_close_handle(audio->session_handle);
        audio->session_handle = 0;
    }
    if (audio->permission_handle != 0) {
        (void)pxa_close_handle(audio->permission_handle);
        audio->permission_handle = 0;
    }
}

int pd_audio_active(const pd_audio_t *audio) {
    return audio->state == PD_AUDIO_READY;
}

void pd_audio_tick(pd_audio_t *audio) {
    if (audio->state != PD_AUDIO_READY || audio->session_handle == 0) return;
    if(audio->music_pending!=0xff && !audio->music_fading_out)
        (void)start_music_asset(audio,audio->music_pending);
    if (audio->music_host && audio->music_ready) {
        int16_t gain = audio->music_asset_gain_db_q8;
        if (audio->music_fading_out) {
            gain -= PD_MUSIC_FADE_STEP_Q8;
            if (gain <= PD_MUSIC_START_GAIN_Q8) {
                (void)start_music_asset(audio,audio->music_pending);
                gain=audio->music_asset_gain_db_q8;
            }
        } else if (gain < PD_MUSIC_GAIN_Q8) {
            gain += PD_MUSIC_FADE_STEP_Q8;
            if (gain > PD_MUSIC_GAIN_Q8) gain = PD_MUSIC_GAIN_Q8;
        }
        if (audio->music_host && gain != audio->music_asset_gain_db_q8) {
            (void)pxa_audio_control_asset(audio->session_handle,
                                              PXA_AUDIO_ASSET_SET_GAIN, gain);
            audio->music_asset_gain_db_q8 = gain;
        }
    }
    while (audio->pending_count > 0) {
        const uint8_t sound = audio->pending[0];
        const pd_sound_file_t *asset = &sound_files[sound];
        int prepared=prepare_sound(audio,sound);
        if(!prepared) break; /* Existing event loop resumes this queue. */
        pop_sound(audio);
        int slot=sound_slot(audio,sound);
        if(prepared>0 && slot>=0 && pxa_audio_play_sound(audio->session_handle,
            audio->sound_handles[slot],asset->gain_db_q8)>0) {
            audio->sound_ages[slot]=++audio->sound_age;
            if (audio->sfx_host_reported != 1) {
                audio->sfx_host_reported = 1;
                (void)pxa_log_write(2, "pd: sound effects playing in host mixer");
            }
        } else if (audio->sfx_host_reported != 2) {
            audio->sfx_host_reported = 2;
            (void)pxa_log_write(2, "pd: packaged sound effect unavailable");
        }
    }
}
