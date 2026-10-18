// PXA platform adapter for XProger/OpenLara. BSD-2-Clause.
#include "pxa.h"
#include "pxa_assets.h"
#include "pxa_clock.h"
#include "pxa_fs_roots.h"
#include "pxa_surface.h"
#include "pxa_ui.h"
#include "pxa_window.h"
#include "engine.h"
#include "storage.h"
#include "audio.h"
#include "music-stream.h"
#include "levels.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static uint8_t packet[512];
static uint8_t level_index[16];
alignas(PXA_SURFACE_BUFFER_ALIGNMENT) static uint16_t buffers[2][320*240];
static uint64_t surface, media_root, file, frame_id;
static uint32_t loaded, total, pad_keys, touch_keys;
static uint32_t display_w=320, display_h=240;
static int level_id=0, load_state, pending_buffer=-1, load_kind;
static uint8_t *load_dest;
static uint32_t load_capacity;
static const char *load_name;
static bool root_ready,boot_started,menu,help,settings,levels;
static unsigned menu_selection;
static uint32_t controller_last,controller_suppressed;
static uint64_t message_until;
static const char *notification="";
static const char *last_store_message="";
static uint32_t ui_transaction=2;
static void status(const char *text) {
    pxa_ui_transaction_t tx{};
    if(pxa_ui_transaction_begin(&tx,ui_transaction++,PXA_UI_TRANSACTION_PATCH,packet,sizeof(packet))) {
        pxa_ui_set_text(&tx,2,text,strlen(text));pxa_ui_transaction_commit(&tx);
    }
}
static bool ready, background;
static uint64_t previous_us, simulation_units, stats_since, render_us, convert_us;
static uint32_t frames, updates, skipped;
static uint64_t time_us() {
    timespec t{}; clock_gettime(CLOCK_MONOTONIC, &t);
    return uint64_t(t.tv_sec)*1000000u + t.tv_nsec/1000u;
}
static void error(const char *why) {
    pxa_log_write(4, why); status(why);ready=false; load_state=-1;
    if (file) pxa_close_handle(file);
    file=0;
}
static void asset_read() {
    char path[64]; snprintf(path,sizeof(path),"assets/game/%s",load_name);
    unsigned count=load_capacity-loaded;
    if(count>PXA_ASSETS_READ_MAX_BYTES)count=PXA_ASSETS_READ_MAX_BYTES;
    if (pxa_assets_read(20,path,loaded,count)) error("OpenLara asset request failed");
}
static void open_data(const char *name,uint8_t *dest,uint32_t capacity) {
    ready=false;load_state=1;loaded=total=0;load_dest=dest;load_capacity=capacity;load_name=name;
    char message[96];snprintf(message,sizeof(message),"Loading %s...",name);status(message);
    if(media_root) {
        if(pxa_fs_request_open_at(12,media_root,name,strlen(name),PXA_FS_OPEN_READ))error("OpenLara SD open request failed");
    } else {load_state=3;asset_read();}
}
static void open_level(int id) {
    if(id<0 || unsigned(id)>=OL_LEVEL_COUNT) {error("Unknown OpenLara level");return;}
    level_id=id;load_kind=2;menu=help=settings=levels=false;pad_keys=touch_keys=0;audio_flush();
    open_data(ol_level_file(id),ol_level_data,OL_LEVEL_CAPACITY);
}
static void start_engine() {
    if(const char *why=ol_validate_level(loaded)) {error(why);return;}
    ol_set_time(time_us()); ol_start(level_id);
    status("");ready=true; load_state=0; previous_us=stats_since=time_us();
    simulation_units=0; render_us=convert_us=0; frames=updates=skipped=0;
    char message[160]; snprintf(message,sizeof(message),"OpenLara ready level=%d bytes=%u linear_bytes=%u",level_id,loaded,
        (unsigned)__builtin_wasm_memory_size(0)*65536u); pxa_log_write(2,message);
}
static void data_done() {
    if(load_kind==0) {
        if(loaded!=240*160) {error("Invalid TITLE.SCR: expected 240x160 indexed image");return;}
#if OL_CLASSIC
        load_kind=4;open_data("title.pal",ol_title_palette_data,sizeof(ol_title_palette_data));
    } else if(load_kind==4) {
        if(loaded!=512) {error("Invalid TITLE.PAL: expected RGB565 palette");return;}
#endif
        load_kind=1;open_data("tracks.ad4",ol_tracks_data,OL_TRACKS_CAPACITY);
    } else if(load_kind==1) {
        if(loaded!=2048) {error("Invalid TRACKS.AD4 directory");return;}
        for(unsigned i=0;i<64;++i) {
            uint32_t offset=pxa_load_u32(ol_tracks_data+i*8),size=pxa_load_u32(ol_tracks_data+i*8+4)&0x7fffffffu;
            if(size && (offset<2048 || offset>ol_tracks_size || size>ol_tracks_size-offset)) {error("Invalid music bank range");return;}
        }
        music_stream_config(media_root,ol_tracks_size);
#if OL_CLASSIC
        load_kind=3;open_data("levels.idx",level_index,sizeof(level_index));
#else
        open_level(0);
#endif
    } else if(load_kind==3) {
        if(loaded!=16 || memcmp(level_index,"OLCI",4) || pxa_load_u32(level_index+4)!=1 ||
           pxa_load_u32(level_index+12)!=OL_LEVEL_COUNT || !(pxa_load_u32(level_index+8)&1u)) {
            error("Invalid OpenLara level catalog; prepare game data again");return;
        }
        ol_set_levels_mask(pxa_load_u32(level_index+8));open_level(0);
    } else start_engine();
}
static void notify(const char *text,uint64_t now) {notification=text;message_until=now+3000000u;}
static void choose_menu(unsigned selection) {
    if(levels) {
        if(selection<OL_PLAYABLE_COUNT) {
            if(ol_level_available(OL_PLAYABLE_IDS[selection]))open_level(OL_PLAYABLE_IDS[selection]);
            else notify("Level data not installed",time_us());
        }
        return;
    }
    if(settings) {
        if(selection==5) {settings=false;menu_selection=0;}
        else {menu_selection=selection;notify(ol_toggle_setting(selection)?"Saving settings...":"Settings busy",time_us());}
        return;
    }
    menu=false;pad_keys=touch_keys=0;previous_us=time_us();simulation_units=0;
    switch(selection) {
        case 0:break;
        case 1:notify(ol_quick_save()?"Saving game...":"Cannot save here",previous_us);break;
        case 2:notify(ol_quick_load()?"Loading saved game...":"No saved game",previous_us);break;
        case 3:{int id=ol_first_level();if(id>=0)open_level(id);else notify("Install full game data",previous_us);break;}
        case 4:if(ol_level_available(3))open_level(3);else notify("Level data not installed",previous_us);break;
        case 5:if(ol_level_available(1))open_level(1);else notify("Level data not installed",previous_us);break;
        case 6:open_level(0);break;
        case 7:menu=settings=true;menu_selection=0;break;
        case 8:help=true;break;
        case 9:
            menu=levels=true;menu_selection=0;
            for(unsigned i=0;i<OL_PLAYABLE_COUNT;++i)if(OL_PLAYABLE_IDS[i]==unsigned(level_id))menu_selection=i;
            break;
    }
}
static void convert(uint16_t *dest) {
    const uint8_t *src=ol_pixels();
#if OL_CLASSIC
    memcpy(dest,src,320u*240u*sizeof(uint16_t));
#elif OL_OPTIMIZED
    // The engine owns an aligned, even-size INDEX8 framebuffer. Four input
    // pixels share one bounds-checked load and two RGB565 stores.
    const auto *in=reinterpret_cast<const uint32_t*>(src);
    auto *out=reinterpret_cast<uint32_t*>(dest);
    for (unsigned i=0;i<320u*240u/4u;++i) {
        uint32_t p=in[i];
        out[i*2]=uint32_t(ol_palette[p&255u]) | (uint32_t(ol_palette[(p>>8)&255u])<<16);
        out[i*2+1]=uint32_t(ol_palette[(p>>16)&255u]) | (uint32_t(ol_palette[p>>24])<<16);
    }
#else
    for (unsigned i=0;i<320u*240u;++i) dest[i]=ol_palette[src[i]];
#endif
}
static void tick(uint64_t now) {
    store_tick();
    if(!boot_started && root_ready && store_ready()) {
        boot_started=true;load_kind=0;open_data("title.scr",ol_title_data,sizeof(ol_title_data));
    }
    if (background || !surface || load_state<0) return;
    music_stream_tick();
    if (load_state==2) {
        uint32_t count=total-loaded; if (count>8192) count=8192;
        int32_t n=pxa_fs_read(file,load_dest+loaded,count);
        if (n<=0 || uint32_t(n)>count) { error("OpenLara SD read failed or truncated"); return; }
        loaded+=uint32_t(n);
        if (loaded==total) { pxa_close_handle(file); file=0; data_done(); }
        return; // Each SD read returns to Host: no long blocking startup callback.
    }
    if (!ready) return;
    audio_tick(time_us());
    const char *m=store_message();
    if(m!=last_store_message) {last_store_message=m;notify(m,time_us());}
#if OL_OPTIMIZED
    // A release can also mean that a pending frame was replaced. Bound frame
    // production even on a fast Host; release events must not form a busy loop.
    if(time_us()-previous_us<16000u)return;
#endif
    int next=ol_next_level();
    if (next>=0) {open_level(next);return;}
    if (pending_buffer>=0) {
        int r=pxa_surface_present_buffer(surface,pending_buffer,frame_id);
        if (r==PXA_STATUS_WOULD_BLOCK) {++skipped;return;}
        if (r!=16) {error("OpenLara present retry failed");return;}
        pending_buffer=-1;
    }
    uint8_t index;
    int r=pxa_surface_acquire_buffer(surface,&index);
    if (r==PXA_STATUS_WOULD_BLOCK) {++skipped;return;}
    if (r!=4 || index>=2) {error("OpenLara buffer acquisition failed");return;}
    uint64_t begin=time_us();
    uint64_t dt=begin-previous_us; previous_us=begin;
    if (dt>250000) dt=250000;
    simulation_units+=dt*30u;
    unsigned steps=unsigned(simulation_units/1000000u); simulation_units%=1000000u;
    ol_set_time(begin);
    if(!menu && !help && !store_busy()) {ol_update(steps,pad_keys|touch_keys);updates+=steps;}
    else simulation_units=0;
    ol_render();
    if(levels)ol_draw_levels(menu_selection);
    else if(menu || help) ol_draw_menu(menu_selection,help,settings);
    if(begin<message_until)ol_draw_message(notification);
    uint64_t rendered=time_us(); convert(buffers[index]); uint64_t converted=time_us();
    render_us+=rendered-begin; convert_us+=converted-rendered;
    r=pxa_surface_present_buffer(surface,index,++frame_id);
    if (r==PXA_STATUS_WOULD_BLOCK) pending_buffer=index;
    else if (r!=16) {error("OpenLara present failed");return;}
    ++frames;
    if (converted-stats_since>=5000000u) {
        int32_t pos[4]; ol_player_position(pos);
        char message[256];
        snprintf(message,sizeof(message),"OpenLara frames=%u elapsed_us=%llu render_us=%llu convert_us=%llu updates=%u skipped=%u pos=%d,%d,%d yaw=%d",
            frames,(unsigned long long)(converted-stats_since),(unsigned long long)render_us,
            (unsigned long long)convert_us,updates,skipped,pos[0],pos[1],pos[2],pos[3]);
        pxa_log_write(2,message); ol_set_fps(frames*1000000u/(converted-stats_since));
        frames=updates=skipped=0; render_us=convert_us=0; stats_since=converted;
    }
    audio_tick(time_us());
    (void)now;
}
static void pointer(const pxa_ui_pointer_data_t &p) {
    // A fixed D-pad and action buttons also allow PXADB pointer validation.
    int scale=pxa_surface_fit_scale(320,240,display_w,display_h);
    if(scale<1)scale=1;
    int x=(int(p.x)-(int(display_w)-320*scale)/2)/scale;
    int y=(int(p.y)-(int(display_h)-240*scale)/2)/scale;
    if (p.phase==PXA_POINTER_UP || p.phase==PXA_POINTER_CANCEL) {touch_keys=0;return;}
    if(help) {if(p.phase==PXA_POINTER_DOWN)help=false;touch_keys=0;return;}
    if(menu) {
        if(p.phase==PXA_POINTER_DOWN) {
            if(levels && y>=38 && y<198)choose_menu((menu_selection/8)*8+unsigned(y-38)/20);
            else if(levels && y>=210)menu_selection=(menu_selection+8)%OL_PLAYABLE_COUNT;
            else if(!levels && y>=(settings?40:35) && y<(settings?190:215))choose_menu(unsigned(y-(settings?40:35))/(settings?25:18));
        }
        touch_keys=0;return;
    }
    if(p.phase==PXA_POINTER_DOWN && x>=140 && x<=220 && y>200) {menu=true;settings=false;menu_selection=0;touch_keys=0;return;}
    if (x<140) {
        touch_keys=0;
        if (x<40) touch_keys|=1u<<3; else if (x>95) touch_keys|=1u<<1;
        if (y<150) touch_keys|=1u; else if (y>195) touch_keys|=1u<<2;
    } else if (y<50) touch_keys=1u<<15;
    else if (x>260 && y>175) touch_keys=1u<<4; // action
    else if (x>220 && y>120) touch_keys=1u<<7; // jump
    else touch_keys=1u<<8; // draw/holster
}
extern "C" int32_t pxa_app_start(const uint8_t *config,uint32_t length) {
    pxa_ui_environment_t env;
    if (pxa_ui_parse_start_environment(config,length,&env)) {display_w=env.width;display_h=env.height;}
    pxa_window_fullscreen();
    pxa_ui_transaction_t tx{};
    if (!pxa_ui_transaction_begin(&tx,1,PXA_UI_TRANSACTION_REPLACE_SURFACE,packet,sizeof(packet)) ||
        !pxa_ui_create(&tx,1,0,0,PXA_UI_NODE_ROOT) ||
        !pxa_ui_set_rgba(&tx,1,PXA_UI_PROPERTY_BACKGROUND,0) ||
        !pxa_ui_create(&tx,2,1,0,PXA_UI_NODE_TEXT) ||
        !pxa_ui_set_rgba(&tx,2,PXA_UI_PROPERTY_FOREGROUND,0xffffffff) ||
        !pxa_ui_set_text(&tx,2,"Starting OpenLara...",20) ||
        !pxa_ui_set_event_mask(&tx,1,PXA_UI_EVENT_MASK_POINTER|PXA_UI_EVENT_MASK_CONTROLLER_STATE) ||
        !pxa_ui_transaction_commit(&tx)) return PXA_STATUS_INTERNAL;
    pxa_surface_desc_t d{}; d.width=320;d.height=240;d.format=PXA_SURFACE_RGB565;d.buffer_count=2;
    d.flags=PXA_SURFACE_PREFER_DIRECT_SCANOUT|PXA_SURFACE_GUEST_MAPPED;
    uint32_t n;
    if (!pxa_surface_build_create(packet,sizeof(packet),1,d,&n)) return PXA_STATUS_INTERNAL;
    return pxa_control_packet(packet,n);
}
extern "C" int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t length) {
    pxa_event_t e; if (!pxa_parse_event(bytes,length,&e)) return PXA_EVENT_UNHANDLED;
    uint8_t life;
    if (pxa_lifecycle_parse(&e,&life)) {
        background=life==PXA_SYSTEM_LIFECYCLE_BACKGROUND;
        pad_keys=touch_keys=controller_last=controller_suppressed=0;previous_us=time_us();
        audio_flush();pxa_clock_set_period(background?0:16);return PXA_EVENT_HANDLED;
    }
    if(store_event(e) || audio_event(e) || music_stream_event(e))return PXA_EVENT_HANDLED;
    if (e.service==PXA_SURFACE_SERVICE && e.opcode==PXA_SURFACE_CREATE && e.token==1) {
        pxa_surface_create_result_t result;
        if (!pxa_surface_parse_create(&e,1,&result) || result.status) return PXA_STATUS_INTERNAL;
        surface=result.handle;
        if (pxa_surface_register_buffers(surface,buffers,sizeof(buffers[0]),2)!=sizeof(buffers)) return PXA_STATUS_INTERNAL;
        uint32_t n;
        if (!pxa_surface_build_configure_layer(packet,sizeof(packet),2,surface,0,0,320,240,0,1,&n)) return PXA_STATUS_INTERNAL;
        pxa_control_packet(packet,n); pxa_clock_set_period(16);
        store_start();audio_start();
        if (pxa_fs_request_app_media(10))root_ready=true;
        return PXA_EVENT_HANDLED;
    }
    if (e.service==PXA_FS_SERVICE && e.token==10) {
        pxa_fs_root_result_t result;
        if (pxa_fs_parse_root(&e,10,&result) && !result.status) media_root=result.handle;
        root_ready=true;return PXA_EVENT_HANDLED;
    }
    if (e.service==PXA_FS_SERVICE && e.token==12) {
        pxa_fs_at_result_t result;
        if (!pxa_fs_parse_open_at(&e,12,&result) || result.status) {
            load_state=3;asset_read();return PXA_EVENT_HANDLED;
        }
        if (result.kind!=PXA_FS_KIND_REGULAR || !result.size ||
            (load_kind==1 ? (result.size<2048 || result.size>UINT32_MAX) : result.size>load_capacity)) {
            pxa_close_handle(result.handle);error("OpenLara: invalid SD level size");return PXA_EVENT_HANDLED;
        }
        if(load_kind==1)ol_tracks_size=uint32_t(result.size);
        if(load_kind==2) {
            if(!ol_allocate_level(uint32_t(result.size))) {pxa_close_handle(result.handle);error("OpenLara: insufficient level memory");return PXA_EVENT_HANDLED;}
            load_dest=ol_level_data;
        }
        file=result.handle;total=load_kind==1?2048:uint32_t(result.size);load_state=2;
        return PXA_EVENT_HANDLED;
    }
    if (e.service==PXA_ASSETS_SERVICE && e.token==20) {
        pxa_asset_read_result_t result;
        if (!pxa_assets_parse_read(&e,20,&result) || result.status ||
            (load_kind==1?result.total_bytes<2048:result.total_bytes>load_capacity) ||
            !result.total_bytes || !result.bytes || result.bytes>load_capacity-loaded || result.offset!=loaded) {
            error("OpenLara: missing level. Install GBA PKD files in the app's fixed SD media directory.");return PXA_EVENT_HANDLED;
        }
        if(load_kind==1)ol_tracks_size=result.total_bytes;
        if(load_kind==2 && !loaded) {
            if(!ol_allocate_level(result.total_bytes)) {error("OpenLara: insufficient level memory");return PXA_EVENT_HANDLED;}
            load_dest=ol_level_data;
        }
        memcpy(load_dest+loaded,result.data,result.bytes);loaded+=result.bytes;total=load_kind==1?2048:result.total_bytes;
        if (loaded==total) data_done(); else asset_read();
        return PXA_EVENT_HANDLED;
    }
    pxa_ui_pointer_data_t p;
    if (pxa_ui_parse_pointer(&e,&p)) {pointer(p);return PXA_EVENT_HANDLED;}
    pxa_ui_controller_data_t c;
    if (pxa_ui_parse_controller(&e,&c)) {
        uint32_t buttons=c.connected?c.buttons:0;
        controller_suppressed&=buttons;
        uint32_t hits=buttons&~controller_last;controller_last=buttons;
        if(load_state<0 && (hits&PXA_CONTROLLER_SELECT)) {open_level(0);return PXA_EVENT_HANDLED;}
        if(hits&PXA_CONTROLLER_SELECT) {menu=!menu;help=settings=levels=false;menu_selection=0;pad_keys=touch_keys=0;controller_suppressed|=PXA_CONTROLLER_SELECT;}
        if(menu || help) {
            controller_suppressed|=buttons&(PXA_CONTROLLER_A|PXA_CONTROLLER_B|PXA_CONTROLLER_SELECT);
            if(hits&PXA_CONTROLLER_B) {
                if(levels) {levels=false;menu_selection=9;}
                else menu=help=settings=false;
                pad_keys=touch_keys=0;
            }
            else if(menu) {
                unsigned count=levels?OL_PLAYABLE_COUNT:(settings?6:10);
                if(hits&PXA_CONTROLLER_UP)menu_selection=(menu_selection+count-1)%count;
                if(hits&PXA_CONTROLLER_DOWN)menu_selection=(menu_selection+1)%count;
                if(levels && (hits&(PXA_CONTROLLER_LEFT|PXA_CONTROLLER_RIGHT)))menu_selection=(menu_selection+8)%count;
                if(hits&PXA_CONTROLLER_A)choose_menu(menu_selection);
            }
            return PXA_EVENT_HANDLED;
        }
        const uint8_t map[]={0,2,3,1,4,7,15,14,8,5,10,11};
        // Select belongs to the pause menu; don't also pass it to the
        // engine's free-camera key. Menu confirmation stays consumed until up.
        buttons&=~controller_suppressed;
        pad_keys=0; if (c.connected) for(unsigned i=0;i<12;++i) if(i!=7 && (buttons&(1u<<i)))pad_keys|=1u<<map[i];
        return PXA_EVENT_HANDLED;
    }
    if (e.service==PXA_WINDOW_SERVICE && e.opcode==PXA_WINDOW_BACK_REQUESTED) {
        // A back request is an impulse; do not leave a controller key held.
        if(load_state<0)open_level(0);
        else if(ready) {menu=!menu;help=settings=levels=false;menu_selection=0;pad_keys=touch_keys=0;}
        return PXA_EVENT_HANDLED;
    }
    if(e.service==PXA_UI_SERVICE && e.opcode==PXA_UI_ENVIRONMENT_CHANGED) {
        pxa_ui_environment_t env;
        if(pxa_ui_parse_environment_event(&e,&env)) {display_w=env.width;display_h=env.height;}
        pad_keys=touch_keys=controller_last=controller_suppressed=0;return PXA_EVENT_HANDLED;
    }
#if OL_OPTIMIZED
    pxa_surface_released_t released;
    if (pxa_surface_parse_released(&e,&released)) {
        // Reliable release makes the buffer acquirable. Fill it immediately;
        // the clock remains the fallback and the simulation stays at 30 Hz.
        if (released.handle==surface && ready) tick(time_us());
        return PXA_EVENT_HANDLED;
    }
#endif
    uint64_t now; if (pxa_clock_parse_tick(&e,&now)) {tick(now);return PXA_EVENT_HANDLED;}
    return PXA_EVENT_UNHANDLED;
}
extern "C" void pxa_app_stop(uint32_t) {
    pxa_clock_set_period(0);audio_stop();music_stream_stop();store_stop();ol_free();
    if(file)pxa_close_handle(file);if(media_root)pxa_close_handle(media_root);
    if(surface)pxa_close_handle(surface);
}
