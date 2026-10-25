// Anarch PXA frontend. SPDX-License-Identifier: CC0-1.0
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "pxa.h"
#include "pxa_audio.h"
#include "pxa_clock.h"
#include "pxa_event_port.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include "pxa_surface.h"
#include "pxa_system.h"
#include "pxa_ui.h"
#include "pxa_window.h"
#include "audio.h"

#define SFG_SCREEN_RESOLUTION_X 160
#define SFG_SCREEN_RESOLUTION_Y 120
#define SFG_RESOLUTION_SCALEDOWN 1
#define SFG_FPS 30
#define SFG_CAN_EXIT 1
#define SFG_LOG(text) pxa_log_write(2, "Anarch: " text);
#include "third_party/anarch/game.h"
#include "view.hpp"

namespace {
constexpr unsigned WIDTH = SFG_SCREEN_RESOLUTION_X, HEIGHT = SFG_SCREEN_RESOLUTION_Y;
constexpr unsigned MAX_PIXELS = anarch_view::max_width * anarch_view::max_height;
constexpr char SAVE_KEY[] = "anarch.save.v1", PREFS_KEY[] = "anarch.preferences.v1";
enum : uint64_t { PORT = 1, CREATE, CONFIGURE, LOAD, SAVE, PERMISSION, AUDIO_OPEN, AUDIO_GRAPH, AUDIO_FLUSH, AUDIO_LEGACY, PREFS_LOAD, PREFS_SAVE };
// Registration packs frames at the current frame_bytes, which can change on resize.
alignas(64) uint16_t frames[2 * MAX_PIXELS];
uint8_t image[WIDTH * HEIGHT], packet[512], event_bytes[4096];
uint8_t saved[SFG_SAVE_SIZE], sent_save[SFG_SAVE_SIZE];
uint64_t event_port, surface, audio, pending_audio, audio_permission, frame_id;
uint64_t epoch, paused_us, pause_at, audio_next_us, save_retry_us;
uint32_t buttons, touch_buttons, touch_id;
bool touch_active;
uint32_t display_w = 320, display_h = 240, safe_insets[4]{};
anarch_view::Viewport viewport;
uint32_t surface_w = 320, surface_h = 240, surface_bytes = 320*240*2;
uint32_t surface_display_w, surface_display_h;
bool surface_pending;
bool prefs_ready, prefs_dirty, prefs_pending, help_open;
uint8_t sent_prefs[4];
uint64_t prefs_retry_us;
bool running, finished, background = true, foreground_tick, surface_ready, mapped = true;
bool load_ready, save_valid, save_dirty, save_pending, dirty, pcm_pending, touch_visible;
bool audio_decided, copied_staged;
int16_t pcm[320], audio_previous;
unsigned pcm_samples = 160;
uint64_t perf_at, perf_render_us, perf_frames;

uint64_t now_us() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
uint64_t game_us() {
    return epoch ? (background ? pause_at : now_us()) - epoch - paused_us : 0;
}
void release_input() { buttons = touch_buttons = 0; touch_active = false; }
int32_t handle_event(const uint8_t *bytes, uint32_t length);

bool wait_event(uint64_t timeout) {
    int32_t n = pxa_event_port_wait(event_port, event_bytes, sizeof(event_bytes), timeout);
    if (n > 0) {
        if (handle_event(event_bytes, uint32_t(n)) < 0) __builtin_trap();
        return true;
    } else if (n != PXA_STATUS_WOULD_BLOCK && n != PXA_STATUS_TIMED_OUT) {
        pxa_log_write(3, "Anarch: event port failed");
        __builtin_trap();
    }
    return false;
}
void send_save() {
    if (!save_dirty || save_pending || now_us() < save_retry_us) return;
    memcpy(sent_save, saved, sizeof(saved));
    int32_t status = pxa_storage_request_set(packet, sizeof(packet), SAVE,
        SAVE_KEY, sizeof(SAVE_KEY)-1, sent_save, sizeof(sent_save));
    if (!status) { save_pending = true; save_dirty = false; }
    else {
        save_retry_us = now_us() + 1000000;
        pxa_log_write(3, "Anarch: could not submit save; retrying");
    }
}
void send_preferences() {
    if (!prefs_dirty || prefs_pending || now_us() < prefs_retry_us) return;
    sent_prefs[0]='A'; sent_prefs[1]='L'; sent_prefs[2]=1; sent_prefs[3]=pocket::english()?1:0;
    if (!pxa_storage_request_set(packet,sizeof(packet),PREFS_SAVE,PREFS_KEY,sizeof(PREFS_KEY)-1,sent_prefs,sizeof(sent_prefs))) {
        prefs_pending=true; prefs_dirty=false;
    } else prefs_retry_us=now_us()+1000000;
}
void change_language() {
    pocket::set_english(!pocket::english()); prefs_dirty=true; dirty=true;
    pxa_log_write(2,pocket::english()?"Anarch: language en":"Anarch: language zh");
    send_preferences();
}
void frontend_controls(uint32_t pressed) {
    if (SFG_game.state != SFG_GAME_STATE_MENU) return;
    if (pressed & PXA_CONTROLLER_X) change_language();
    if (help_open && (pressed & (PXA_CONTROLLER_A|PXA_CONTROLLER_B|PXA_CONTROLLER_Y|PXA_CONTROLLER_START))) {
        help_open=false; release_input(); SFG_game.frameTime=SFG_getTimeMs(); dirty=true;
    } else if (pressed & PXA_CONTROLLER_Y) { help_open=true; release_input(); dirty=true; }
}
void service_audio() {
    if (!audio || background) return;
    uint64_t now = game_us();
    // A delayed frame must not enqueue seconds of obsolete audio.
    if (audio_next_us + 100000 < now) audio_next_us = now;
    for (unsigned block = 0; block < 3 && audio_next_us < now + 40000; ++block) {
        if (!pcm_pending) {
            anarch_audio_render(pcm, 160);
            // Older PXA backends expose only the 16 kHz media graph. Keep the
            // original 8 kHz generator and interpolate each sample twice.
            if (pcm_samples == 320) {
                int16_t last = pcm[159];
                for (int i = 159; i >= 0; --i) {
                    int16_t current = pcm[i], previous = i ? pcm[i-1] : audio_previous;
                    pcm[i*2+1] = current; pcm[i*2] = (int(current)+previous)/2;
                }
                audio_previous = last;
            }
            pcm_pending = true;
        }
        unsigned bytes = pcm_samples * sizeof(int16_t);
        int32_t n = pxa_audio_write_pcm(audio, reinterpret_cast<uint8_t *>(pcm), bytes);
        if (n == PXA_STATUS_WOULD_BLOCK) break;
        if (n != int(bytes)) {
            pxa_log_write(3, "Anarch: audio write failed; continuing silently");
            audio = 0; pcm_pending = false; break;
        }
        pcm_pending = false;
        audio_next_us += 20000;
    }
}
void present() {
    if (!dirty || !surface_ready || background) return;
    uint8_t index = 0;
    if (mapped) {
        int n = pxa_surface_acquire_buffer(surface, &index);
        if (n == PXA_STATUS_WOULD_BLOCK) return;
        if (n != 4 || index > 1) __builtin_trap();
    }
    anarch_view::pixels=image;
    anarch_view::overlays();
    if (help_open) anarch_view::help();
    uint16_t *dst = frames + index*(surface_bytes/2);
    memset(dst,0,surface_bytes);
    for (int y=0;y<viewport.h;++y) for (int x=0;x<viewport.w;++x) {
        int gx=x*int(WIDTH)/viewport.w, gy=y*int(HEIGHT)/viewport.h;
        uint16_t color=paletteRGB565[image[gy*WIDTH+gx]];
        // Draw the same logical hit rectangles after rendering, so raw game pixels
        // are never changed by transient controller hints.
        if (touch_visible && !help_open && SFG_game.state!=SFG_GAME_STATE_MENU) {
            auto edge=[&](int bx,int by,int bw,int bh) {
                return gx>=bx && gx<bx+bw && gy>=by && gy<by+bh &&
                    (gx==bx || gx==bx+bw-1 || gy==by || gy==by+bh-1);
            };
            if (edge(0,36,48,48)||edge(16,36,16,48)||edge(0,52,48,16)) color=0xffff;
            if (edge(128,40,32,32)) color=0xf800;
            if (edge(128,76,32,20)) color=0x07e0;
            if (edge(128,0,32,16)) color=0xffe0;
            if (edge(96,96,32,24)) color=0x07ff;
        }
        dst[(viewport.y+y)*surface_w+viewport.x+x]=color;
    }
    if (mapped) {
        int32_t result = pxa_surface_present_buffer(surface, index, ++frame_id);
        if (result == PXA_STATUS_WOULD_BLOCK) return;
        if (result != 16) __builtin_trap();
    } else {
        // Stream writes only stage pixels; QUEUE_FRAME publishes that stage.
        // Preserve it if the command stream temporarily applies backpressure.
        if (!copied_staged) {
            int32_t result = pxa_surface_write_frame(surface, reinterpret_cast<uint8_t *>(dst), surface_bytes);
            if (result == PXA_STATUS_WOULD_BLOCK) return;
            if (result != int(surface_bytes)) __builtin_trap();
            copied_staged = true;
        }
        uint32_t written;
        if (!pxa_surface_build_queue_frame(packet, sizeof(packet), surface, frame_id+1, nullptr, 0, &written))
            __builtin_trap();
        int32_t result = pxa_control_packet(packet, written);
        if (result == PXA_STATUS_WOULD_BLOCK) return;
        if (result) __builtin_trap();
        ++frame_id; copied_staged = false;
    }
    dirty = false; ++perf_frames;
}
int request_surface() {
    viewport.fit(display_w,display_h,safe_insets);
    surface_w=viewport.canvas_w; surface_h=viewport.canvas_h; surface_bytes=surface_w*surface_h*2;
    surface_display_w=display_w; surface_display_h=display_h;
    pxa_surface_desc_t desc{uint16_t(surface_w), uint16_t(surface_h), PXA_SURFACE_RGB565, 2,
        uint8_t(PXA_SURFACE_PREFER_DIRECT_SCANOUT | (mapped ? PXA_SURFACE_GUEST_MAPPED : 0))};
    uint32_t n;
    int32_t status=pxa_surface_build_create(packet, sizeof(packet), CREATE, desc, &n)
        ? pxa_control_packet(packet, n) : PXA_STATUS_INTERNAL;
    if (!status) surface_pending=true;
    return status;
}
int resize_surface() {
    if (surface_pending) return 0;
    anarch_view::Viewport next; next.fit(display_w,display_h,safe_insets);
    if (surface && (surface_w!=unsigned(next.canvas_w) || surface_h!=unsigned(next.canvas_h) || surface_display_w!=display_w || surface_display_h!=display_h)) {
        int32_t status=pxa_close_handle(surface);
        if (status) return status;
        surface=0; surface_ready=false; copied_staged=false; frame_id=0;
    }
    if (!surface) return request_surface();
    viewport=next;
    uint32_t n;
    surface_ready=false; surface_pending=true;
    if (!pxa_surface_build_configure_layer(packet,sizeof(packet),CONFIGURE,surface,0,0,
        uint16_t(surface_w),uint16_t(surface_h),0,1,&n)) return PXA_STATUS_INTERNAL;
    return pxa_control_packet(packet,n);
}
void update_environment(const pxa_ui_environment_t &env) {
    display_w=env.width?env.width:320; display_h=env.height?env.height:240;
    memcpy(safe_insets,env.safe_insets,sizeof(safe_insets));
}
void log_viewport() {
    char log[160]; snprintf(log,sizeof(log),"Anarch: viewport canvas=160x120 origin=%d,%d size=%dx%d display=%ux%u",
        viewport.host_x+viewport.x*viewport.host_scale,viewport.host_y+viewport.y*viewport.host_scale,
        viewport.w*viewport.host_scale,viewport.h*viewport.host_scale,display_w,display_h);
    pxa_log_write(2,log);
}
void pointer(const pxa_ui_pointer_data_t &p) {
    if (background || !surface_ready) return;
    // Host pointer IDs identify streams, not zero-based touch slots. Capture
    // the first contact until UP/CANCEL; another contact must not steal it.
    if (p.phase == PXA_POINTER_DOWN && !touch_active) {
        touch_id = p.pointer_id; touch_active = true;
    }
    if (!touch_active || p.pointer_id != touch_id) return;
    touch_visible = true; dirty = true; touch_buttons = 0;
    if (p.phase == PXA_POINTER_UP || p.phase == PXA_POINTER_CANCEL) {
        touch_active = false; return;
    }
    int x,y;
    if (!viewport.map(p.x,p.y,x,y)) return;
    if (help_open) {
        if (p.phase==PXA_POINTER_DOWN) { help_open=false; SFG_game.frameTime=SFG_getTimeMs(); release_input(); }
        return;
    }
    if (SFG_game.state==SFG_GAME_STATE_MENU) {
        if (y<14 && p.phase==PXA_POINTER_DOWN) {
            if (x>=110) change_language();
            else if (x>=58) { help_open=true; release_input(); }
        } else if (y>=17 && y<102) {
            unsigned row=unsigned(y-17)/12;
            if (SFG_getMenuItem(row)!=SFG_MENU_ITEM_NONE) {
                SFG_game.selectedMenuItem=row;
                touch_buttons=PXA_CONTROLLER_A;
            }
        }
        return;
    }
    if (x < 48 && y >= 36 && y < 84) {
        if (x < 16) touch_buttons |= PXA_CONTROLLER_LEFT;
        if (x >= 32) touch_buttons |= PXA_CONTROLLER_RIGHT;
        if (y < 52) touch_buttons |= PXA_CONTROLLER_UP;
        if (y >= 68) touch_buttons |= PXA_CONTROLLER_DOWN;
    } else if (x >= 128) {
        if (y < 16) touch_buttons = PXA_CONTROLLER_START;
        else if (y >= 40 && y < 72) touch_buttons = PXA_CONTROLLER_A;
        else if (y >= 76 && y < 96) touch_buttons = PXA_CONTROLLER_X;
    } else if (x >= 96 && y >= 96) touch_buttons = PXA_CONTROLLER_B;
}
int32_t handle_event(const uint8_t *bytes, uint32_t length) {
    pxa_event_t e;
    if (!pxa_parse_event(bytes, length, &e)) return PXA_STATUS_INTERNAL;
    pxa_event_port_result_t port_result;
    if (pxa_event_port_parse_open(&e, PORT, &port_result)) {
        if (port_result.status) return port_result.status;
        event_port = port_result.handle;
        return PXA_EVENT_HANDLED;
    }
    uint8_t life;
    if (pxa_lifecycle_parse(&e, &life)) {
        bool next = life == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
        uint64_t now = now_us();
        if (running && next != background) {
            if (next) pause_at = now;
            else paused_us += now - pause_at;
        }
        background = next;
        if (next) foreground_tick = false;
        release_input();
        anarch_audio_set_paused(next);
        pcm_pending = false;
        if (audio) pxa_audio_flush(AUDIO_FLUSH, audio);
        if (!next) { audio_next_us = game_us(); dirty = true; }
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_SURFACE_SERVICE && e.token == CREATE) {
        pxa_surface_create_result_t result;
        if (!pxa_surface_parse_create(&e, CREATE, &result)) return PXA_STATUS_INTERNAL;
        if (result.status == PXA_STATUS_UNSUPPORTED && mapped) { mapped = false; return request_surface(); }
        if (result.status || result.stride_bytes != surface_w*2 || result.frame_bytes != surface_bytes || result.buffer_count != 2)
            return PXA_STATUS_INTERNAL;
        surface = result.handle;
        if (mapped && pxa_surface_register_buffers(surface, frames, surface_bytes, 2) != int32_t(surface_bytes*2))
            return PXA_STATUS_INTERNAL;
        uint32_t n;
        if (!pxa_surface_build_configure_layer(packet, sizeof(packet), CONFIGURE, surface, 0, 0,
            uint16_t(surface_w), uint16_t(surface_h), 0, 1, &n))
            return PXA_STATUS_INTERNAL;
        return pxa_control_packet(packet, n);
    }
    if (e.service == PXA_SURFACE_SERVICE && e.token == CONFIGURE) {
        int32_t status;
        if (!pxa_surface_parse_status(&e, CONFIGURE, PXA_SURFACE_CONFIGURE_LAYER, &status) || status)
            return PXA_STATUS_INTERNAL;
        surface_pending=false;
        anarch_view::Viewport next; next.fit(display_w,display_h,safe_insets);
        if (surface_w!=unsigned(next.canvas_w) || surface_h!=unsigned(next.canvas_h) || surface_display_w!=display_w || surface_display_h!=display_h) return resize_surface();
        viewport=next; surface_ready=true; dirty=true; log_viewport();
        return PXA_EVENT_HANDLED;
    }
    if (e.service==PXA_STORAGE_SERVICE && e.token==PREFS_LOAD) {
        pxa_storage_get_result_t r;
        if (!pxa_storage_parse_get(&e,PREFS_LOAD,&r)) return PXA_STATUS_INTERNAL;
        if (!r.status && anarch_view::valid_preferences(r.value.data,r.value.size)) pocket::set_english(r.value.data[3]==1);
        prefs_ready=true;
        pxa_log_write(2,pocket::english()?"Anarch: language en":"Anarch: language zh");
        return PXA_EVENT_HANDLED;
    }
    if (e.service==PXA_STORAGE_SERVICE && e.token==PREFS_SAVE) {
        int32_t status;
        if (!pxa_storage_parse_status(&e,PREFS_SAVE,PXA_STORAGE_SET,&status)) return PXA_STATUS_INTERNAL;
        prefs_pending=false;
        if (status) { prefs_dirty=true; prefs_retry_us=now_us()+1000000; }
        send_preferences();
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_STORAGE_SERVICE && e.token == LOAD) {
        pxa_storage_get_result_t r;
        if (!pxa_storage_parse_get(&e, LOAD, &r)) return PXA_STATUS_INTERNAL;
        if (!r.status && r.value.size == sizeof(saved)) {
            memcpy(saved, r.value.data, sizeof(saved)); save_valid = true;
            char log[80]; snprintf(log, sizeof(log), "Anarch: save loaded settings=%u progress=%u", saved[1], saved[0]);
            pxa_log_write(2, log);
        }
        else if (r.status != PXA_STATUS_NOT_FOUND) pxa_log_write(2, "Anarch: no valid save; using defaults");
        load_ready = true;
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_STORAGE_SERVICE && e.token == SAVE) {
        int32_t status;
        if (!pxa_storage_parse_status(&e, SAVE, PXA_STORAGE_SET, &status)) return PXA_STATUS_INTERNAL;
        save_pending = false;
        if (status) {
            save_dirty = true; save_retry_us = now_us() + 1000000;
            pxa_log_write(3, "Anarch: save failed; retrying");
        } else pxa_log_write(2, "Anarch: progress/settings saved");
        send_save();
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_PERMISSION_SERVICE && e.token == PERMISSION) {
        pxa_permission_acquire_result_t r;
        if (!pxa_permission_parse_acquire(&e, PERMISSION, &r)) return PXA_STATUS_INTERNAL;
        audio_decided = true;
        if (!r.status) { audio_permission = r.handle; pxa_audio_open_pcm(AUDIO_OPEN, r.handle, ANARCH_AUDIO_RATE, 1); }
        else pxa_log_write(2, "Anarch: audio permission unavailable; continuing silently");
        return PXA_EVENT_HANDLED;
    }
    pxa_permission_revoked_t revoked;
    if (pxa_permission_parse_revoked(&e, &revoked)) {
        if (revoked.name_size == 14 && !memcmp(revoked.name, "audio.playback", 14)) {
            audio = pending_audio = audio_permission = 0; pcm_pending = false;
        }
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_AUDIO_SERVICE && e.token == AUDIO_OPEN) {
        pxa_audio_pcm_result_t r;
        if (!pxa_audio_parse_pcm(&e, AUDIO_OPEN, &r)) return PXA_STATUS_INTERNAL;
        if (!r.source.status && r.source.sample_rate == ANARCH_AUDIO_RATE && r.source.channels == 1) {
            pending_audio = r.source.handle;
            pxa_audio_commit_gain(AUDIO_GRAPH, pending_audio, 0);
        } else {
            pxa_log_write(2, "Anarch: native PCM unavailable; opening legacy 16 kHz audio");
            pxa_audio_open_media(AUDIO_LEGACY, audio_permission);
        }
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_AUDIO_SERVICE && e.token == AUDIO_LEGACY) {
        pxa_audio_open_result_t r;
        if (!pxa_audio_parse_open(&e, AUDIO_LEGACY, &r)) return PXA_STATUS_INTERNAL;
        if (!r.status && r.sample_rate == 16000 && r.channels == 1 && r.frame_ms == 20) {
            pcm_samples = 320; pending_audio = r.handle;
            pxa_audio_commit_gain(AUDIO_GRAPH, pending_audio, 0);
        } else pxa_log_write(2, "Anarch: audio unavailable; continuing silently");
        return PXA_EVENT_HANDLED;
    }
    if (e.service == PXA_AUDIO_SERVICE && e.token == AUDIO_GRAPH) {
        int32_t status;
        if (!pxa_audio_parse_status(&e, AUDIO_GRAPH, PXA_AUDIO_COMMIT_GRAPH, &status)) return PXA_STATUS_INTERNAL;
        if (!status && audio_permission) { audio = pending_audio; audio_next_us = game_us(); pxa_log_write(2, "Anarch: original audio ready (8000 Hz mono)"); }
        return PXA_EVENT_HANDLED;
    }
    pxa_ui_controller_data_t c;
    if (pxa_ui_parse_controller(&e, &c)) {
        if (c.controller == 0 && !background) {
            uint32_t pressed=c.connected?(c.buttons&~buttons):0;
            if (pressed) { touch_visible = false; touch_buttons = 0; touch_active=false; dirty = true; }
            buttons = c.connected ? c.buttons : 0;
            frontend_controls(pressed);
        }
        return PXA_EVENT_HANDLED;
    }
    pxa_ui_pointer_data_t p;
    if (pxa_ui_parse_pointer(&e, &p)) { pointer(p); return PXA_EVENT_HANDLED; }
    if (pxa_window_is_back_requested(&e)) {
        // The Host closes on an unhandled BACK. Let the queued exit BACK
        // reach it after run_game has flushed storage and returned.
        if (!running || finished) return PXA_EVENT_UNHANDLED;
        if (help_open) { help_open=false; release_input(); dirty=true; SFG_game.frameTime=SFG_getTimeMs(); }
        else if (SFG_game.state != SFG_GAME_STATE_MENU) {
            SFG_setGameState(SFG_GAME_STATE_MENU); release_input(); dirty = true;
        } else { SFG_game.continues=0; release_input(); }
        return PXA_EVENT_HANDLED;
    }
    pxa_ui_environment_t env;
    if (pxa_ui_parse_environment_event(&e, &env)) {
        update_environment(env); release_input(); dirty = true;
        int32_t status=resize_surface();
        return status?status:PXA_EVENT_HANDLED;
    }
    uint64_t timestamp;
    if (pxa_clock_parse_tick(&e, &timestamp)) {
        foreground_tick = true;
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}
void run_game() {
    running = true; epoch = now_us(); paused_us = 0;
    anarch_audio_reset();
    SFG_init();
    perf_at = now_us();
    pxa_log_write(2, "Anarch: ready 160x120 RGB565, 30 Hz logic, native main-loop");
    // Clock is only needed to receive the first foreground checkpoint.
    pxa_clock_set_period(0);
    while (!finished) {
        // Audio and frame notifications may arrive faster than rendering.
        // Drain a bounded batch so they cannot delay input behind old events.
        for (unsigned i = 0; i < 32 && wait_event(0); ++i) {}
        if (background) { wait_event(1000000); continue; }
        service_audio();
        uint64_t before = now_us();
        bool continues = true;
        if (help_open) { SFG_game.frameTime=SFG_getTimeMs(); wait_event(10000); }
        else continues = SFG_mainLoopBody();
        perf_render_us += now_us() - before;
        present(); send_save(); send_preferences();
        if (!continues) {
            uint64_t deadline = now_us() + 2000000;
            while ((save_dirty || save_pending || prefs_dirty || prefs_pending) && now_us() < deadline) {
                send_save(); send_preferences(); wait_event(10000);
            }
            finished = true; pxa_window_request_back();
        }
        if (now_us() - perf_at >= 10000000) {
            char log[192];
            snprintf(log, sizeof(log), "Anarch: presented=%llu elapsed_ms=%llu loop_us=%llu state=%u level=%u position=%ld,%ld",
                (unsigned long long)perf_frames, (unsigned long long)((now_us()-perf_at)/1000),
                (unsigned long long)perf_render_us, SFG_game.state, SFG_currentLevel.levelNumber,
                (long)SFG_player.camera.position.x, (long)SFG_player.camera.position.y);
            pxa_log_write(2, log); perf_at = now_us(); perf_frames = perf_render_us = 0;
        }
    }
}
} // namespace

static inline void SFG_setPixel(uint16_t x, uint16_t y, uint8_t color) {
    image[y * WIDTH + x] = color; dirty = true;
}
int8_t SFG_keyPressed(uint8_t key) {
    static const uint32_t mapping[SFG_KEY_COUNT] = {
        PXA_CONTROLLER_UP, PXA_CONTROLLER_RIGHT, PXA_CONTROLLER_DOWN, PXA_CONTROLLER_LEFT,
        PXA_CONTROLLER_A, PXA_CONTROLLER_B, PXA_CONTROLLER_SELECT, PXA_CONTROLLER_X,
        PXA_CONTROLLER_L, PXA_CONTROLLER_R, 0, 0, PXA_CONTROLLER_Y, 0, PXA_CONTROLLER_START, 0
    };
    return !help_open && surface_ready && key < SFG_KEY_COUNT && ((buttons | touch_buttons) & mapping[key]) != 0;
}
void SFG_getMouseOffset(int16_t *x, int16_t *y) { *x = *y = 0; }
uint32_t SFG_getTimeMs() { return uint32_t(game_us()/1000); }
void SFG_sleepMs(uint16_t ms) {
    uint64_t deadline = now_us() + uint64_t(ms) * 1000;
    while (!background) {
        service_audio();
        uint64_t now = now_us();
        if (now >= deadline) break;
        wait_event(deadline - now < 5000 ? deadline - now : 5000);
    }
}
void SFG_playSound(uint8_t sound, uint8_t volume) { anarch_audio_play_sound(sound, volume); }
void SFG_setMusic(uint8_t command) { anarch_audio_set_music(command); }
void SFG_processEvent(uint8_t event, uint8_t data) {
    if (event == SFG_EVENT_LEVEL_STARTS) {
        char log[64]; snprintf(log, sizeof(log), "Anarch: level %u started", data); pxa_log_write(2, log);
    }
}
void SFG_save(uint8_t data[SFG_SAVE_SIZE]) {
    memcpy(saved, data, sizeof(saved)); save_valid = save_dirty = true; send_save();
}
uint8_t SFG_load(uint8_t data[SFG_SAVE_SIZE]) {
    if (save_valid) memcpy(data, saved, sizeof(saved));
    return 1;
}
extern "C" int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    pxa_ui_environment_t env{};
    if (pxa_ui_parse_start_environment(config, length, &env)) update_environment(env);
    pxa_ui_transaction_t tx{};
    if (!pxa_ui_transaction_begin(&tx, 1, PXA_UI_TRANSACTION_REPLACE_SURFACE, packet, sizeof(packet)) ||
        !pxa_ui_create(&tx, 1, 0, 0, PXA_UI_NODE_ROOT) ||
        !pxa_ui_set_rgba(&tx, 1, PXA_UI_PROPERTY_BACKGROUND, 0) ||
        !pxa_ui_set_event_mask(&tx, 1, PXA_UI_EVENT_MASK_CONTROLLER_STATE | PXA_UI_EVENT_MASK_POINTER) ||
        !pxa_ui_transaction_commit(&tx)) return PXA_STATUS_INTERNAL;
    int32_t status = pxa_window_fullscreen();
    if (status) return status;
    if ((status=pxa_event_port_open(PORT))) return status;
    if ((status=request_surface())) return status;
    if ((status=pxa_storage_request_get(LOAD, SAVE_KEY, sizeof(SAVE_KEY)-1))) return status;
    if ((status=pxa_storage_request_get(PREFS_LOAD, PREFS_KEY, sizeof(PREFS_KEY)-1))) prefs_ready=true;
    uint32_t n;
    if (!pxa_permission_build(packet, sizeof(packet), PXA_PERMISSION_ACQUIRE, PERMISSION,
        "audio.playback", 14, reinterpret_cast<const uint8_t *>("media"), 5, &n)) return PXA_STATUS_INTERNAL;
    if (pxa_control_packet(packet, n)) audio_decided = true;
    return pxa_clock_set_period(20);
}
extern "C" int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t length) {
    int32_t status = handle_event(bytes, length);
    if (status < 0) return status;
    if (!running && !finished && !background && event_port && surface_ready && load_ready && prefs_ready && audio_decided && foreground_tick) run_game();
    return status;
}
extern "C" void pxa_app_stop(uint32_t reason) {
    (void)reason; finished = true; release_input();
    // All Host handles have already been revoked; imports are forbidden here.
}
