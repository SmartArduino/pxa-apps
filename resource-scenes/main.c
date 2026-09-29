#include "pxa.h"
#include "pxa_assets.h"
#include "pxa_audio.h"
#include "pxa_permission.h"
#include "pxa_asset_scene.h"
#include "pxa_clock.h"
#include "pxa_game_render.h"
#include "pxa_raster.h"

/* No texture pixels, palettes or decoder buffers live in Guest memory. */
static uint64_t context, palette, next_token = 16, frame;
static pxa_asset_scene_t loader;
static pxa_game_render_binding_t loaded[4];
static pxa_asset_scene_item_t items[4];
static char paths[4][sizeof("assets/t00.pxr")];
static uint8_t draw[256], ready, failed, ticks, finished;
static unsigned scene, retries;
static uint64_t prefetch_token;
static uint64_t audio_session, audio_permission, sound;
static uint8_t audio_ready;
static uint64_t music_instance;
static uint8_t music_ready;
/* High bits deliberately exercise WAMR's full-width token mapping. Only the
 * current event's borrowed bytes are inspected; there is no map-sized array. */
static uint64_t read_token = UINT64_C(0x1234567800000000);
static uint32_t map_offset;
static uint8_t map_done;
static int read_map(void) {
    return pxa_assets_read(++read_token,"assets/map.bin",map_offset,PXA_ASSETS_READ_MAX_BYTES);
}

static void log_error(const char *message) { (void)pxa_log_write(4, message); }
static int draw_frame(int visible) {
    pxa_raster_draw_list_t list;
    pxa_raster_draw_list_begin(&list, draw, sizeof(draw), ++frame);
    pxa_raster_clear(&list, 0);
    if (visible)
        for (unsigned i = 0; i < 4; ++i)
            if (!pxa_raster_sprite(&list, (uint8_t)i, 0, 0, (int16_t)((i % 2) * 120),
                (int16_t)((i / 2) * 120), 120, 120, 0, 0, 256, 256, 0)) return 0;
    return pxa_raster_submit(context, &list) >= 0;
}
static int unbind_scene(void) {
    pxa_game_render_binding_t b[4];
    for (unsigned i = 0; i < 4; ++i) b[i] = (pxa_game_render_binding_t){0, 1, (uint8_t)i};
    return pxa_game_render_bind_assets(context, b, 4) >= 0;
}
static void load_scene(void) {
    ready = 0; failed = 0; ticks = 0;
    if (!unbind_scene() || !draw_frame(0)) { log_error("resource-scenes: unbind/loading frame failed"); finished = 1; return; }
    if (pxa_asset_scene_release(&loader) || next_token > UINT64_MAX - 4) {
        failed = 1; return;
    }
    for (unsigned i = 0; i < 4; ++i) {
        unsigned index = (scene % 5) * 4 + i;
        const char path[] = "assets/t00.pxr";
        for (unsigned j = 0; j < sizeof(path); ++j) paths[i][j] = path[j];
        paths[i][8] = (char)('0' + index / 10); paths[i][9] = (char)('0' + index % 10);
        items[i] = (pxa_asset_scene_item_t){paths[i], PXA_ASSET_TEXTURE, (uint8_t)i};
    }
    uint64_t first = next_token + 1; next_token += 4;
    if (pxa_asset_scene_begin(&loader, items, loaded, 4, first, 4)) failed = 1;
}
static void publish_scene(void) {
    if (pxa_asset_scene_bind(&loader, context) < 0 || !draw_frame(1)) {
        failed = 1; log_error("resource-scenes: scene bind/draw failed");
    } else {
        ready = 1; retries = 0;
    }
    /* Bindings and accepted frames now own their references. */
    if (pxa_asset_scene_release(&loader)) { ready = 0; failed = 1; }
    if (ready && scene < 99) {
        unsigned index = ((scene + 1) % 5) * 4;
        char path[] = "assets/t00.pxr";
        path[8] = (char)('0' + index / 10); path[9] = (char)('0' + index % 10);
        if (prefetch_token) (void)pxa_cancel(prefetch_token);
        prefetch_token = ++next_token;
        if (pxa_assets_prefetch_texture(prefetch_token, path)) prefetch_token = 0;
    }
}
int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    uint8_t packet[64]; uint32_t size;
    pxa_game_render_options_t options = {0};
    (void)config; (void)length;
    options.width = options.height = 240; options.buffer_count = 2;
    options.scratch_mode = PXA_GAME_RENDER_SCRATCH_NONE; options.max_draw_bytes = sizeof(draw);
    if (!pxa_game_render_build_create(packet, sizeof(packet), 1, &options, &size)) return -1;
    return pxa_submit(packet, size);
}
int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t length) {
    pxa_event_t event; uint64_t now;
    if (!pxa_parse_event(bytes, length, &event)) return PXA_EVENT_UNHANDLED;
    if (event.service==PXA_AUDIO_SERVICE && event.opcode==PXA_AUDIO_PLAYBACK_EVENT) {
        pxa_audio_playback_event_t playback;
        if (!pxa_audio_parse_playback(&event,&playback) || playback.session!=audio_session || playback.instance!=music_instance) return -1;
        if (playback.state==PXA_AUDIO_PLAYBACK_READY) {
            music_ready=1;
            (void)pxa_log_write(2,"resource-scenes: music READY for accepted instance");
        } else if (playback.state==PXA_AUDIO_PLAYBACK_STOPPED && finished) {
            (void)pxa_log_write(2,"resource-scenes: music STOPPED for matching instance");
            (void)pxa_close_handle(audio_session); audio_session=0;
            (void)pxa_close_handle(audio_permission); audio_permission=0;
            (void)unbind_scene();
            (void)pxa_close_handle(context); context=0;
        } else return -1;
        return PXA_EVENT_HANDLED;
    }
    if (event.service == PXA_PERMISSION_SERVICE && event.token == 4) {
        pxa_permission_acquire_result_t permission;
        if (!pxa_permission_parse_acquire(&event,4,&permission) || permission.status) return -1;
        audio_permission = permission.handle;
        return !pxa_audio_open_media(5,audio_permission) ? PXA_EVENT_HANDLED : -1;
    }
    if (event.service == PXA_AUDIO_SERVICE && event.token == 5) {
        pxa_audio_open_result_t opened;
        if (!pxa_audio_parse_open(&event,5,&opened) || opened.status) return -1;
        audio_session = opened.handle;
        return !pxa_audio_commit_gain(6,audio_session,0) ? PXA_EVENT_HANDLED : -1;
    }
    if (event.service == PXA_AUDIO_SERVICE && event.token == 6) {
        int32_t status;
        if (!pxa_audio_parse_status(&event,6,PXA_AUDIO_COMMIT_GRAPH,&status) || status) return -1;
        audio_ready = 1;
        if (pxa_audio_play_music(audio_session,"assets/tone.ogg",1,-24*256,&music_instance)!=31) return -1;
        return PXA_EVENT_HANDLED;
    }
    if (event.service == PXA_GAME_RENDER_SERVICE && event.token == 1) {
        pxa_game_render_create_result_t r;
        if (!pxa_game_render_parse_create(&event, 1, &r) || r.status) return -1;
        context = r.handle;
        return pxa_assets_status(3, "assets/p.pxr") == 0 ? PXA_EVENT_HANDLED : -1;
    }
    if (event.service == PXA_ASSETS_SERVICE) {
        pxa_asset_result_t r;
        if (event.opcode == PXA_ASSETS_READ) {
            pxa_asset_read_result_t chunk;
            if (!pxa_assets_parse_read(&event,read_token,&chunk) || chunk.status ||
                chunk.offset != map_offset || chunk.total_bytes != 8193) return -1;
            for (uint32_t i=0;i<chunk.bytes;++i)
                if (chunk.data[i] != (map_offset+i)%251) return -1;
            if (!chunk.bytes) {
                map_done = 1;
                (void)pxa_log_write(2,"resource-scenes: installed map 8193 bytes and EOF checked with full-width tokens");
                // Map data is required before entering a scene. Finish its
                // temporary read before pinning four textures, so low budgets
                // cannot deadlock waiting for memory held by that same scene.
                load_scene();
            } else {
                map_offset += chunk.bytes;
                if (read_map()) return -1;
            }
            return PXA_EVENT_HANDLED;
        }
        if (event.token == 7 && event.opcode == PXA_ASSETS_LOAD) {
            if (!pxa_assets_parse_result(&event,7,PXA_ASSETS_LOAD,&r) || r.status || r.info.kind != PXA_ASSET_AUDIO) return -1;
            sound = r.handle; return PXA_EVENT_HANDLED;
        }
        if (event.opcode == PXA_ASSETS_STATUS && event.token == 3) {
            pxa_asset_status_t snapshot;
            if (!pxa_assets_parse_status(&event, 3, &snapshot) || snapshot.status) return -1;
            return pxa_assets_load_palette(2, "assets/p.pxr") == 0 ? PXA_EVENT_HANDLED : -1;
        }
        if (event.opcode == PXA_ASSETS_PREFETCH) {
            if (!pxa_assets_parse_result(&event, event.token, PXA_ASSETS_PREFETCH, &r)) return -1;
            if (event.token == prefetch_token) prefetch_token = 0;
            // Optional warming can fail under pressure. The next scene LOAD
            // remains authoritative and retries after old bindings retire.
            return PXA_EVENT_HANDLED;
        }
        if (pxa_asset_scene_on_event(&loader, &event)) {
            if (loader.state == PXA_SCENE_READY) publish_scene();
            else if (loader.state == PXA_SCENE_FAILED) failed = 1;
            return PXA_EVENT_HANDLED;
        }
        if (!pxa_assets_parse_result(&event, event.token, PXA_ASSETS_LOAD, &r)) return -1;
        if (event.token == 2) {
            if (r.status) return -1;
            palette = r.handle;
            pxa_game_render_binding_t b = {palette, 2, 0};
            if (pxa_game_render_bind_assets(context, &b, 1) < 0) return -1;
            (void)pxa_close_handle(palette); palette = 0;
            (void)pxa_clock_set_period(16);
            if (!draw_frame(0)) return -1;
            if (read_map() || pxa_assets_load_sound(7,"assets/click.pcm")) return -1;
            uint8_t request[96]; uint32_t request_bytes;
            if (!pxa_permission_build(request,sizeof(request),PXA_PERMISSION_ACQUIRE,4,
                "audio.playback",14,(const uint8_t *)"media",5,&request_bytes) ||
                pxa_submit(request,request_bytes)) return -1;
        } else {
            if (r.status == 0 && r.handle) (void)pxa_close_handle(r.handle);
        }
        return PXA_EVENT_HANDLED;
    }
    if (pxa_clock_parse_tick(&event, &now) && context && !finished) {
        if (pxa_asset_scene_pending(&loader)) return PXA_EVENT_HANDLED;
        if (failed) {
            (void)pxa_asset_scene_release(&loader);
            if (++ticks >= 4) {
                if (++retries > 8) {
                    finished = 1;
                    log_error("resource-scenes: resource budget/load failure after bounded retries");
                    (void)pxa_clock_set_period(0);
                    (void)unbind_scene();
                    (void)pxa_close_handle(context); context = 0;
                } else load_scene();
            }
            return PXA_EVENT_HANDLED;
        }
        if (ready && map_done && audio_ready && music_ready && sound && !ticks &&
            pxa_audio_play_sound(audio_session,sound,-12*256) != 12) return -1;
        if (ready && map_done && audio_ready && music_ready && sound && ++ticks >= 4) {
            if (++scene == 100) {
                finished = 1;
                (void)pxa_clock_set_period(0);
                (void)pxa_log_write(2, "resource-scenes: 100 scenes completed; 20 file textures; four visible; no Guest pixel arrays");
                if (pxa_audio_play_sound(audio_session,sound,-12*256) != 12) return -1;
                (void)pxa_log_write(2,"resource-scenes: 100 prepared sound triggers; closing handle during final playback");
                (void)pxa_close_handle(sound); sound = 0; /* Voice still owns a reference. */
                if (pxa_audio_control_asset(audio_session,PXA_AUDIO_ASSET_STOP,0)!=4) return -1;
            } else load_scene();
        }
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(uint32_t reason) {
    (void)reason;
    /* Imports are forbidden in stop. Core revokes this component's accepted
     * requests, handles and context before calling us, including late loads.
     * During normal event callbacks release/drain the group explicitly. */
    pxa_zero(&loader, sizeof(loader));
    pxa_zero(loaded, sizeof(loaded));
    prefetch_token = 0;
    context = palette = 0;
}
