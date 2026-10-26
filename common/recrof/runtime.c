/* Cooperative PXA Core 2 runtime for the recrof engine ports. GPL-3.0-or-later.
 */
#include "port.h"
#include "controller.h"
#ifdef PORT_RTS
#include "loading.h"
#endif
#include "pxa.h"
#include "rpc.h"
#define PXA_UI_COMMAND_FAILED(op, status)                                      \
  port_log("PXA UI operation %u failed: %d", (unsigned)(op), (int)(status))
#include "font8x8.h"
#include "pxa_audio.h"
#include "pxa_clock.h"
#include "pxa_permission.h"
#include "external.h"
#include "pxa_surface.h"
#ifndef PORT_NATIVE_PCM
#define PORT_NATIVE_PCM 0
#endif
#ifndef PORT_NATIVE_MAIN_LOOP
#define PORT_NATIVE_MAIN_LOOP 0
#endif
#if PORT_NATIVE_PCM && !PORT_NATIVE_MAIN_LOOP
#error "Native PCM requires the native event port"
#endif
#if PORT_NATIVE_MAIN_LOOP
#include "pxa_event_port.h"
#endif
#ifndef PXA_GAME_RENDER_BLIT
#define PXA_GAME_RENDER_BLIT 0
#endif
#ifdef PORT_SOFTWARE_FRAME_BLIT
#undef PXA_GAME_RENDER_BLIT
#define PXA_GAME_RENDER_BLIT 0
#endif
#if PXA_GAME_RENDER_BLIT
#include "pxa_game_render_compute.h"
uint64_t port_game_render_context;
#endif
#include "pxa_ui.h"
#include "pxa_window.h"
#include "../pxa_canvas_pixels.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define NOINLINE __attribute__((noinline))
#ifndef PORT_TICK_US
#define PORT_TICK_US (PORT_TICK_MS*1000u)
#endif
_Static_assert(PORT_TICK_US>0 && PORT_TICK_US<=60000000u,"invalid preferred Clock period");
_Static_assert(PORT_TICK_MS>=16 && PORT_TICK_MS<=1000,"Clock period must be 16..1000 ms");
#if !PORT_NATIVE_MAIN_LOOP
__attribute__((import_module("asyncify"), import_name("start_unwind"))) void
unwind(void *);
__attribute__((import_module("asyncify"), import_name("stop_unwind"))) void
stop_unwind(void);
__attribute__((import_module("asyncify"), import_name("start_rewind"))) void
rewind_stack(void *);
__attribute__((import_module("asyncify"), import_name("stop_rewind"))) void
stop_rewind(void);
#endif
extern uintptr_t port_stack_get(void);
extern void port_stack_set(uintptr_t);
/* Both C stacks live inside the linker-declared auxiliary stack, so WAMR
 * interpreter and AOT bounds checks agree. The upper region serves callbacks. */
extern unsigned char __stack_low[];
#ifndef PORT_ENGINE_STACK_BYTES
#define PORT_ENGINE_STACK_BYTES 262144u
#endif
#ifndef PORT_CALLBACK_STACK_BYTES
#define PORT_CALLBACK_STACK_BYTES 262144u
#endif
_Static_assert(PORT_ENGINE_STACK_BYTES>=16384 && PORT_ENGINE_STACK_BYTES%16==0,
               "invalid engine stack size");
_Static_assert(PORT_CALLBACK_STACK_BYTES>=16384 && PORT_CALLBACK_STACK_BYTES%16==0,
               "invalid callback stack size");
#ifndef PORT_CONTINUATION_BYTES
#define PORT_CONTINUATION_BYTES 262144u
#endif
_Static_assert(PORT_CONTINUATION_BYTES>=16384 && PORT_CONTINUATION_BYTES%16==0,
               "invalid continuation buffer size");
#if !PORT_NATIVE_MAIN_LOOP
static uint8_t saved_stack[PORT_CONTINUATION_BYTES] __attribute__((aligned(16)));
static struct {
  void *cursor, *end;
} continuation;
static uintptr_t engine_sp;
#else
static uint64_t event_port;
static uint8_t event_bytes[PXA_MAX_CONTROL_BYTES];
static int32_t handle_event(const uint8_t *bytes, uint32_t n);
#endif
static uint32_t continuation_peak;
static uintptr_t callback_stack_floor;
static int suspended, rewinding, finished, background, started, engine_running;
static uint64_t wake_at, surface, audio, frame_id;
static uint64_t last_cooperative_us;
static uint16_t palette[256];
static uint16_t surface_buffers[2][320 * 240]
    __attribute__((aligned(PXA_SURFACE_BUFFER_ALIGNMENT)));
static uint16_t surface_w = 320, surface_h = 240;
static uint8_t pending_buffer = 255;
static uint64_t pending_frame;
static int display_w = 320, display_h = 240, mouse_x = 160, mouse_y = 120;
static uint32_t display_density = 65536;
static uint32_t shape_insets[4],bar_insets[4];
static uint64_t window_revision;
static int keys[256];
static uint8_t packet[512];
static uint64_t token_cursor = 100;
static uint64_t clock_pending;
static uint32_t clock_requested;
static int clock_timing;
/* Keep the validated legacy timer running until negotiation completes. */
static int clock_apply(void) {
  if(clock_timing) {
    clock_requested=background?0:PORT_TICK_US;
    clock_pending=++token_cursor;
    int status=pxa_clock_configure(clock_pending,clock_requested);
    if(!status)return 0;
    clock_timing=0;clock_pending=0;
    port_log("PXA Clock configure unavailable: %d; legacy fallback",status);
  }
  return pxa_clock_set_period(background?0:PORT_TICK_MS);
}
static uint64_t waiting_token;
static pxa_event_t reply;
static uint8_t reply_data[PXA_MAX_CONTROL_BYTES];
static int reply_ready;
static unsigned audio_rate = 16000, audio_channels = 1;
#if PORT_NATIVE_PCM
static int16_t audio_packet[960 * 2];
static unsigned audio_fill, audio_epoch;
static int audio_failed;
#ifdef PORT_PCM_FALLBACK
static int audio_fallback;
static uint64_t fallback_phase;
static unsigned fallback_fill, fallback_source_rate;
static int16_t fallback_samples[960];
#endif
#endif
static uint64_t audio_permission, audio_pending;
static int audio_decision_pending;
static port_controller_t controller;
#ifdef PORT_RTS
#define CONTROLLER_RTS 1
#else
#define CONTROLLER_RTS 0
#endif
static uint64_t perf_since, perf_draw_us;
static uint64_t perf_yield_at, perf_wait_us;
static uint64_t perf_audio_wait_us;
static uint32_t perf_audio_blocks;
static uint32_t perf_yields;
static uint32_t perf_frames, perf_skipped;
#ifdef PORT_RTS
static const char *loading_stage = "Opening game files";
static uint64_t loading_began, loading_painted;
static int loading_active;
static void paint_loading(uint64_t now);
#endif
__attribute__((weak)) void port_game_stats(void) {}
__attribute__((weak)) void port_game_reset_motion(void) {}

typedef struct {
  int active, x, y, cx, cy, role, key;
  uint8_t id;
} Touch;
static Touch touches[8];
static int touch_keys[256];
static uint8_t pad_keys[256];

uint64_t port_time_us(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  static uint64_t epoch;
  uint64_t now = (uint64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
  if (!epoch)
    epoch = now;
  return now - epoch;
}
const char *port_data_path(void) { return "/data"; }
void port_log(const char *fmt, ...) {
  char text[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(text, sizeof(text), fmt, ap);
  va_end(ap);
  if (*text)
    pxa_log_write(2, text);
}
int port_mouse_x(void) { return mouse_x; }
int port_mouse_y(void) { return mouse_y; }
int port_key_down(int k) { return k >= 0 && k < 256 ? keys[k] : 0; }

void port_poll_input(void) {
#if PORT_NATIVE_MAIN_LOOP
  /* Long rendering/file operations may have queued input since the last
   * yield. Drain without waiting so that this update uses the latest state.
   * Bound the work even if the host continuously produces events. */
  for (unsigned pending = 0; pending < 64; ++pending) {
    int count = pxa_event_port_wait(event_port, event_bytes, sizeof(event_bytes), 0);
    if (count == PXA_STATUS_WOULD_BLOCK || count == PXA_STATUS_TIMED_OUT) break;
    if (count <= 0 || handle_event(event_bytes, (uint32_t)count) < 0) {
      port_log("PXA input poll failed: %d", count);
      __builtin_trap();
    }
    if (background) {
      port_yield();
      break;
    }
  }
#endif
}

/* These two functions are Asyncify's top/bottom runtime boundaries. Keep them
 * out of the optimizer's inliner. No host-specific continuation imports remain
 * after the Binaryen post-link step. */
NOINLINE void port_yield(void) {
#if PORT_NATIVE_MAIN_LOOP
  const uint64_t began = port_time_us();
  ++perf_yields;
  for (;;) {
    uint64_t now = port_time_us(), timeout = 0;
    if (background || (waiting_token && !reply_ready)) timeout = 1000000;
    else if (now < wake_at) timeout = wake_at - now;
    int count = pxa_event_port_wait(event_port, event_bytes, sizeof(event_bytes), timeout);
    if (count > 0) {
      if (handle_event(event_bytes, (uint32_t)count) < 0) {
        port_log("PXA main-loop event failed");
        __builtin_trap();
      }
    } else if (count != PXA_STATUS_WOULD_BLOCK && count != PXA_STATUS_TIMED_OUT) {
      port_log("PXA main-loop wait failed: %d", count);
      __builtin_trap();
    }
    now = port_time_us();
    if (!background &&
        (waiting_token ? reply_ready : now >= wake_at && (count <= 0 || now - began >= 2000)))
      break;
  }
  wake_at = 0;
  last_cooperative_us = port_time_us();
  perf_wait_us += last_cooperative_us - began;
#else
  if (rewinding) {
    rewinding = 0;
    stop_rewind();
    last_cooperative_us=port_time_us();
    perf_wait_us += last_cooperative_us-perf_yield_at;
    return;
  }
  last_cooperative_us=port_time_us();
  perf_yield_at=last_cooperative_us;
  ++perf_yields;
  continuation.cursor = saved_stack;
  continuation.end = saved_stack + sizeof(saved_stack);
  /* Rewind restores Wasm locals, but it skips the C stack prologues.
   * Resume with the suspended SP: resetting it to the stack top would let
   * the first new callee overwrite its live parent objects. */
  engine_sp = port_stack_get();
  suspended = 1;
  unwind(&continuation);
#endif
}
int port_cooperate_due(uint64_t now,uint32_t budget_us) {
  return now>=last_cooperative_us && now-last_cooperative_us>=budget_us;
}
NOINLINE static void run_game(void) {
  engine_running = 1;
  port_log("Starting %s", PORT_TITLE);
  port_loading_stage("Opening game files");
  mkdir("/data", 0700);
  if(!port_external_prepare()) return;
  port_game_main();
}
NOINLINE static void pump(void) {
#if PORT_NATIVE_MAIN_LOOP
  if (!event_port || engine_running) return;
#endif
#ifdef PORT_EXTERNAL_GAME_DATA
  if(audio_decision_pending) return;
#endif
  if (finished || background)
    return;
#if !PORT_NATIVE_MAIN_LOOP
  if (suspended) {
    if (waiting_token && !reply_ready)
      return;
    if (!waiting_token && port_time_us() < wake_at)
      return;
  }
#endif
  uintptr_t callback_stack = port_stack_get();
  if (!started) started = 1;
  if (!engine_running) {
    memset(__stack_low,0xa5,PORT_ENGINE_STACK_BYTES);
    callback_stack_floor = callback_stack;
    if(callback_stack > (uintptr_t)__stack_low+PORT_ENGINE_STACK_BYTES)
      memset(__stack_low+PORT_ENGINE_STACK_BYTES,0xa5,callback_stack-(uintptr_t)__stack_low-PORT_ENGINE_STACK_BYTES);
  }
#if PORT_NATIVE_MAIN_LOOP
  port_stack_set((uintptr_t)__stack_low + PORT_ENGINE_STACK_BYTES);
#else
  port_stack_set(suspended ? engine_sp : (uintptr_t)__stack_low + PORT_ENGINE_STACK_BYTES);
  if (suspended) {
    suspended = 0;
    rewinding = 1;
    rewind_stack(&continuation);
  }
#endif
  run_game();
#if !PORT_NATIVE_MAIN_LOOP
  if (suspended) {
    stop_unwind();
    uint32_t bytes=(uint8_t *)continuation.cursor-saved_stack;
    if(bytes>continuation_peak)continuation_peak=bytes;
  } else
#endif
    finished = 1;
  port_stack_set(callback_stack);
}
void port_sleep(unsigned ms) {
  wake_at = port_time_us() + (uint64_t)ms * 1000;
  port_yield();
}
const pxa_event_t *port_rpc(uint16_t service, uint16_t opcode,
                            const void *payload, uint32_t n) {
  /* C++ global file objects resolve optional paths during reactor
   * initialization, before an event pump exists. Treat those probes as
   * absent; real opens and resource discovery run inside run_game(). */
  if (!engine_running)
    return NULL;
  uint64_t tok = ++token_cursor;
  waiting_token = tok;
  reply_ready = 0;
  int status = pxa_control_call(service, opcode, tok, payload, n);
  if (status != 0) {
    waiting_token = 0;
    return NULL;
  }
  port_yield();
  waiting_token = 0;
  reply_ready = 0;
  return &reply;
}
const pxa_event_t *port_rpc_packet(const uint8_t *p, uint32_t n) {
  pxa_wire_view_t v;
  if (!pxa_wire_decode(p, n, PXA_MAX_CONTROL_BYTES, &v))
    return NULL;
  return port_rpc(v.service, v.opcode, v.payload, v.payload_size);
}
void port_palette(const uint8_t *rgb, int sixbit) {
  for (int i = 0; i < 256; i++) {
    unsigned r = rgb[3 * i], g = rgb[3 * i + 1], b = rgb[3 * i + 2];
    if (sixbit) {
      r = r * 255 / 63;
      g = g * 255 / 63;
      b = b * 255 / 63;
    }
    palette[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  }
}
void port_palette_rgb565(const uint16_t *rgb, int swapped) {
  for (int i = 0; i < 256; ++i)
    palette[i] = swapped ? (uint16_t)((rgb[i] >> 8) | (rgb[i] << 8)) : rgb[i];
}
/* Never write a buffer until Surface has handed ownership back to the guest.
 * If a previous present is blocked, retry it before acquiring another buffer. */
static int acquire(void) {
  if (!surface) return -1;
  if (pending_buffer != 255) {
    int r = pxa_surface_present_buffer(surface, pending_buffer, pending_frame);
    if (r == PXA_STATUS_WOULD_BLOCK) return -1;
    if (r != 16) {port_log("PXA present failed: %d",r); return -1;}
    pending_buffer = 255;
  }
  uint8_t index;
  int r = pxa_surface_acquire_buffer(surface, &index);
  if (r == PXA_STATUS_WOULD_BLOCK) return -1;
  if (r != 4 || index >= 2) {port_log("PXA acquire failed: %d",r); return -1;}
  return index;
}
static uint16_t *output(int index) {
  return (uint16_t *)surface_buffers + (size_t)index * surface_w * surface_h;
}
static int reserved_compute_buffer = -1;
void *port_frame_scratch(size_t *bytes) {
  if (!bytes) return NULL;
  *bytes = 0;
  if (reserved_compute_buffer < 0) reserved_compute_buffer = acquire();
  if (reserved_compute_buffer < 0) return NULL;
  *bytes = (size_t)surface_w * surface_h * 2;
  return output(reserved_compute_buffer);
}
static void present(int index) {
  pending_frame = ++frame_id;
  int r = pxa_surface_present_buffer(surface,index,pending_frame);
  if (r == PXA_STATUS_WOULD_BLOCK) pending_buffer = index;
  else if (r != 16) port_log("PXA present failed: %d",r);
}
#ifdef PORT_RTS
static void paint_loading(uint64_t now) {
  if(!loading_active || background || !surface || reserved_compute_buffer>=0 ||
      (loading_painted && now-loading_painted<250000))return;
  int index=acquire();
  if(index<0)return;
  port_draw_loading(output(index),surface_w,surface_h,PORT_TITLE,loading_stage,
      now>=loading_began?now-loading_began:0);
  present(index);
  loading_painted=now?now:1;
}
#endif
void port_loading_stage(const char *stage) {
#ifdef PORT_RTS
  if(!stage)return;
  port_filesystem_loading_cache(1);
  if(!loading_began) {loading_began=port_time_us();if(!loading_began)loading_began=1;}
  loading_stage=stage;loading_active=1;loading_painted=0;
  port_log("PXA LOAD stage=%s",stage);
  paint_loading(port_time_us());
#else
  (void)stage;
#endif
}
void port_loading_restart(const char *stage) {
#ifdef PORT_RTS
  loading_began=0;
#endif
  port_loading_stage(stage);
}
void port_error(const char *message) {
#ifdef PORT_RTS
  loading_active=0;
  port_filesystem_loading_cache(0);
#endif
  port_log("%s",message);
  for (;;) {
    int index = acquire();
    if (index >= 0) {
      uint16_t *dest = output(index);
      memset(dest,0,(size_t)surface_w*surface_h*2);
      port_draw_text(dest,surface_w,surface_h,8,12,PORT_TITLE);
      port_draw_text(dest,surface_w,surface_h,8,38,message);
      present(index);
    }
    port_sleep(1000);
  }
}
void port_frame_cursor(const uint8_t *p, int w, int h, int pitch,
                        const port_cursor_t *cursor) {
  if (!p || w <= 0 || h <= 0 || pitch < w) return;
#ifdef PORT_RTS
  if(loading_active) {
    loading_active=0;
    port_filesystem_loading_cache(0);
  }
#endif
  uint64_t begin = port_time_us();
  if (!perf_since) perf_since = begin;
  int index = reserved_compute_buffer;
  reserved_compute_buffer = -1;
  if (index < 0) index = acquire();
  if (index >= 0) {
    uint16_t *dest = output(index);
#if PXA_GAME_RENDER_BLIT
    int32_t blit_result = PXA_STATUS_UNSUPPORTED;
    if (port_game_render_context &&
#ifdef PXA_QUAKE_RENDER_PROBE
        (port_render_probe_mode & PORT_RENDER_PROBE_NATIVE_BLIT) &&
#endif
        w <= 65535 && h <= 65535 &&
        (uint64_t)(h-1)*(unsigned)pitch + (unsigned)w <= UINT32_MAX &&
        (!cursor || !cursor->pixels || cursor->width <= 0 || cursor->height <= 0 ||
         (cursor->width <= 65535 && cursor->height <= 65535))) {
      pxa_game_render_blit_wire_t frame = {0};
      frame.source_offset = (uint32_t)(uintptr_t)p;
      frame.source_bytes = (uint32_t)((uint64_t)(h-1)*(unsigned)pitch + (unsigned)w);
      frame.source_width = w; frame.source_height = h; frame.source_pitch = pitch;
      frame.palette_offset = (uint32_t)(uintptr_t)palette; frame.palette_bytes = sizeof(palette);
      frame.target_offset = (uint32_t)(uintptr_t)dest;
      frame.target_bytes = surface_w*surface_h*2;
      frame.target_width = surface_w; frame.target_height = surface_h; frame.target_pitch = surface_w*2;
      if (cursor && cursor->pixels && cursor->width > 0 && cursor->height > 0 &&
          cursor->width <= 65535 && cursor->height <= 65535) {
        frame.cursor_offset = (uint32_t)(uintptr_t)cursor->pixels;
        frame.cursor_width = cursor->width; frame.cursor_height = cursor->height;
        frame.cursor_pitch = cursor->width; frame.cursor_bytes = (uint32_t)cursor->width*(uint32_t)cursor->height;
        frame.cursor_x = cursor->x; frame.cursor_y = cursor->y;
      }
      blit_result = pxa_game_render_blit_indexed_frame(port_game_render_context, &frame);
    }
    if (blit_result < 0)
#endif
    port_blit(dest,surface_w,surface_h,p,w,h,pitch,palette,cursor);
#ifdef PORT_RTS
    port_draw_controls(dest,surface_w,surface_h,1);
#else
    port_draw_controls(dest,surface_w,surface_h,0);
#endif
    present(index);
    ++perf_frames;
  } else ++perf_skipped;
  uint64_t end = port_time_us();
  perf_draw_us += end - begin;
  if (end - perf_since >= 5000000) {
    port_log("PXA GAME frames=%u elapsed_us=%llu draw_us=%llu skipped=%u linear_bytes=%u",
             perf_frames,(unsigned long long)(end-perf_since),
             (unsigned long long)perf_draw_us,perf_skipped,
             (unsigned)__builtin_wasm_memory_size(0)*65536u);
    port_log("PXA SCHED yields=%u wait_us=%llu audio_wait_us=%llu audio_blocks=%u",
      perf_yields,(unsigned long long)perf_wait_us,
      (unsigned long long)perf_audio_wait_us,perf_audio_blocks);
    uint8_t *low=__stack_low,*engine=low,*callback=low+PORT_ENGINE_STACK_BYTES;
    while(engine<low+PORT_ENGINE_STACK_BYTES && *engine==0xa5)++engine;
    while((uintptr_t)callback<callback_stack_floor && *callback==0xa5)++callback;
    port_log("PXA STACK engine=%u callback=%u continuation=%u",
             (unsigned)(low+PORT_ENGINE_STACK_BYTES-engine),(unsigned)(low+PORT_ENGINE_STACK_BYTES+PORT_CALLBACK_STACK_BYTES-callback),continuation_peak);
    port_game_stats();
    perf_since=end; perf_frames=perf_skipped=0; perf_draw_us=0;
    perf_wait_us=0;perf_yields=0;
    perf_audio_wait_us=0;perf_audio_blocks=0;
  }
  port_game_audio();
#ifndef PORT_PACED_BY_MAIN
  port_sleep(1);
#endif
}
void port_frame(const uint8_t *p, int w, int h, int pitch) {
  port_frame_cursor(p,w,h,pitch,NULL);
}
void port_frame_rgb565(const uint16_t *pixels, int w, int h, int pitch) {
  if (!pixels || w <= 0 || h <= 0 || pitch < w * 2) return;
  int index = acquire();
  if (index < 0) return;
  uint16_t *dest = output(index);
  if (w == surface_w && h == surface_h && pitch == w * 2) {
    port_copy_read(dest, pixels, (size_t)w * h * 2);
  } else {
    /* Small displays retain the same source map across frames. Avoid one
     * software integer division per output pixel on the AOT targets. */
    static uint32_t xmap[320],ymap[240];
    static int old_w,old_h,old_sw,old_sh;
    const int cached=surface_w<=320 && surface_h<=240;
    if(cached && (old_w!=w || old_h!=h || old_sw!=surface_w || old_sh!=surface_h)) {
      for(int x=0;x<surface_w;++x)xmap[x]=(uint64_t)x*w/surface_w;
      for(int y=0;y<surface_h;++y)ymap[y]=(uint64_t)y*h/surface_h;
      old_w=w;old_h=h;old_sw=surface_w;old_sh=surface_h;
    }
    for (int y = 0; y < surface_h; ++y) {
      const uint16_t *row = (const uint16_t *)((const uint8_t *)pixels +
          (size_t)(cached?ymap[y]:(uint64_t)y*h/surface_h) * pitch);
      for (int x = 0; x < surface_w; ++x)
        dest[y * surface_w + x] = row[cached?xmap[x]:(uint64_t)x*w/surface_w];
    }
  }
  port_draw_controls(dest, surface_w, surface_h, 0);
  present(index);
}
#if PORT_NATIVE_PCM
#ifdef PORT_PCM_FALLBACK
static int open_fallback_pcm(void) {
  uint8_t request[96];uint32_t bytes;
  if(!pxa_audio_build_open(request,sizeof(request),1,audio_permission,&bytes))return 0;
  const pxa_event_t *event=port_rpc_packet(request,bytes);
  pxa_audio_open_result_t opened={0};
  if(!event || !pxa_audio_parse_open(event,event->token,&opened) || opened.status || opened.channels!=1)return 0;
  const pxa_audio_graph_t graph={.gain_db_q8=-1536};
  if(!pxa_audio_build_graph(request,sizeof(request),1,opened.handle,&graph,&bytes)){pxa_close_handle(opened.handle);return 0;}
  event=port_rpc_packet(request,bytes);
  int32_t status;
  if(!event || !pxa_audio_parse_status(event,event->token,PXA_AUDIO_COMMIT_GRAPH,&status) || status) {
    pxa_close_handle(opened.handle);return 0;
  }
  audio=opened.handle;audio_rate=opened.sample_rate;audio_channels=opened.channels;
  audio_fill=fallback_fill=0;fallback_phase=0;audio_fallback=1;
  port_log("PXA PCM fallback=%uHz/1ch S16LE",audio_rate);
  return 1;
}
#endif
static int open_native_pcm(unsigned rate, unsigned channels) {
  uint8_t request[16] = {0};
  pxa_store_u64(request, audio_permission); pxa_store_u32(request+8, rate);
  request[12] = (uint8_t)channels;
  const pxa_event_t *event = port_rpc(PXA_AUDIO_SERVICE, PXA_AUDIO_OPEN_PCM, request, sizeof(request));
  pxa_audio_pcm_result_t opened = {0};
  if (!event || !pxa_audio_parse_pcm(event, event->token, &opened) || opened.source.status ||
      opened.source.sample_rate != rate || opened.source.channels != channels) {
    if (opened.source.handle >> 32) pxa_close_handle(opened.source.handle);
    port_log("PXA original PCM unavailable: rate=%u channels=%u status=%d", rate, channels,
      event ? opened.source.status : PXA_STATUS_IO_ERROR);
    return 0;
  }
  uint8_t command[64]; uint32_t bytes = 0;
  const pxa_audio_graph_t graph = {.gain_db_q8 = -1536};
  const uint64_t session = opened.source.handle;
  int32_t status = PXA_STATUS_INTERNAL;
  if (pxa_audio_build_graph(command, sizeof(command), 1, session, &graph, &bytes)) {
    event = port_rpc_packet(command, bytes);
    if (!event || !pxa_audio_parse_status(event, event->token, PXA_AUDIO_COMMIT_GRAPH, &status))
      status = PXA_STATUS_INTERNAL;
  }
  if (status) { pxa_close_handle(session); return 0; }
  audio = session; audio_rate = rate; audio_channels = channels; audio_fill = 0;
  port_log("PXA PCM source=%uHz/%uch S16LE sink=%uHz/%uch queue=%u source-frames",
    rate, channels, opened.sink_rate, opened.sink_channels, opened.queue_frames);
  return 1;
}
#endif
uint64_t port_audio_session(void) { return audio; }
void port_pcm(const int16_t *p, unsigned frames, unsigned rate,
              unsigned channels) {
#if PORT_NATIVE_PCM
  if (!p || !frames || rate < 8000 || rate > 48000 || channels < 1 || channels > 2 ||
      !audio_permission || audio_failed) return;
#ifdef PORT_PCM_FALLBACK
  if(!audio && !open_native_pcm(rate,channels) && !open_fallback_pcm()) {audio_failed=1;return;}
  if(audio_fallback && (rate!=audio_rate || channels!=audio_channels)) {
    if(fallback_source_rate!=rate) {fallback_phase=0;fallback_source_rate=rate;}
    const unsigned epoch=audio_epoch;
    for(unsigned i=0;i<frames;++i) {
      int sample=p[i*channels];
      if(channels==2)sample=(sample+p[i*2+1])/2;
      fallback_phase+=audio_rate;
      while(fallback_phase>=rate) {
        fallback_phase-=rate;fallback_samples[fallback_fill++]=(int16_t)sample;
        if(fallback_fill==audio_rate/50) {
          unsigned count=fallback_fill;fallback_fill=0;
          port_pcm(fallback_samples,count,audio_rate,1);
          if(epoch!=audio_epoch)return;
        }
      }
    }
    return;
  }
#endif
  if (audio && (audio_rate != rate || audio_channels != channels)) {
    pxa_close_handle(audio); audio = 0; audio_fill = 0;
  }
  if (!audio && !open_native_pcm(rate, channels)) { audio_failed = 1; return; }
  const unsigned packet_frames = rate / 50, epoch = audio_epoch;
  while (frames) {
    const unsigned count = frames < packet_frames-audio_fill ? frames : packet_frames-audio_fill;
    memcpy(audio_packet + audio_fill*channels, p, count*channels*sizeof(*p));
    audio_fill += count; p += count*channels; frames -= count;
    if (audio_fill != packet_frames) continue;
    const uint32_t bytes = packet_frames*channels*2;
    for (;;) {
      int32_t status = pxa_audio_write_pcm(audio, (uint8_t*)audio_packet, bytes);
      if (status == (int32_t)bytes) break;
      if (status != PXA_STATUS_WOULD_BLOCK) {
        port_log("PXA PCM write failed: %d", status);
        pxa_close_handle(audio); audio = 0; audio_failed = 1; audio_fill = 0; return;
      }
      const uint64_t wait_begin = port_time_us();
      port_sleep(1);
      perf_audio_wait_us += port_time_us() - wait_begin;
      ++perf_audio_blocks;
      if (epoch != audio_epoch) return; /* Lifecycle flush discarded the old packet. */
    }
    audio_fill = 0;
  }
#else
  static int16_t out[320];
  static unsigned fill;
  static uint64_t phase;
  if (!audio || !rate || !channels)
    return;
  for (unsigned i = 0; i < frames; i++) {
    int sample = p[i * channels];
    if (channels == 2)
      sample = (sample + p[i * 2 + 1]) / 2;
    phase += audio_rate;
    while (phase >= rate) {
      phase -= rate;
      out[fill++] = (int16_t)sample;
      if (fill == 320) {
        pxa_audio_write_pcm(audio, (uint8_t *)out, sizeof(out));
        fill = 0;
      }
    }
  }
#endif
}
static void setkey(int key, int down) {
  if (key < 0 || key >= 256 || keys[key] == down)
    return;
  keys[key] = down;
  port_game_key(key, down);
}
static void update_keys(void) {
  for (int k = 0; k < 256; k++)
    setkey(k, touch_keys[k] || pad_keys[k]);
}
static void release_touch(void) {
#ifdef PORT_SDL_CONTROLLER
  for (unsigned i=0; i<8; ++i) if (touches[i].active && touches[i].role == 4)
    port_game_pointer(touches[i].cx,touches[i].cy,PXA_POINTER_CANCEL,0,touches[i].id);
#endif
  memset(touches, 0, sizeof(touches));
  memset(touch_keys, 0, sizeof(touch_keys));
  port_game_reset_motion();
}
static void release_input(void) {
  release_touch();
  memset(pad_keys, 0, sizeof(pad_keys));
  memset(&controller, 0, sizeof(controller));
#ifdef PORT_RTS
  port_game_scroll(0, 0);
#endif
  update_keys();
}
void port_sample_controller(uint32_t elapsed_us, int *dx, int *dy) {
  *dx = *dy = 0;
  if (background) return;
  if (port_controller_menu(&controller, port_game_in_menu(), CONTROLLER_RTS,
                           pad_keys)) update_keys();
  const port_controller_motion_t move = port_controller_advance(&controller,
      elapsed_us, CONTROLLER_RTS);
  *dx = move.x;
  *dy = move.y;
}
#ifndef PORT_ENGINE_CONTROLLER
static void controller_tick(void) {
  if (background || !controller.buttons) return;
  if (port_controller_menu(&controller, port_game_in_menu(), CONTROLLER_RTS,
                           pad_keys)) update_keys();
  port_controller_motion_t move = port_controller_step(&controller,
      port_time_us(), CONTROLLER_RTS);
  if (!move.x && !move.y) return;
#ifdef PORT_RTS
  if (move.scroll) {
    port_game_scroll(move.x, move.y);
    return;
  }
  port_game_move_cursor(move.x, move.y, &mouse_x, &mouse_y);
#else
  port_game_motion(move.x, move.y);
#endif
}
#endif
static void input_pointer(const pxa_ui_pointer_data_t *p) {
  if (background) return;
  int scale=pxa_surface_fit_scale(surface_w,surface_h,display_w,display_h);
  if(scale<1)scale=1;
  int x,y;
  int inside=pxa_game_pointer_coordinates(p->x,p->y,display_density,
      display_w,display_h,surface_w,surface_h,(unsigned)scale,&x,&y);
#ifdef PORT_SDL_CONTROLLER
  x = x * 320 / surface_w;
  y = y * 240 / surface_h;
#endif
  if(p->phase==PXA_POINTER_DOWN && !inside)return;
  if (p->phase == PXA_POINTER_DOWN && !port_controls_visible()) {
    port_controls_set_visible(1);
    port_log("PXA CONTROLS source=touch visible=1");
    /* Consume this whole gesture: there is no Touch slot for its move/up.
     * Revealing FIRE/OK must not activate the previously invisible button. */
    return;
  }
  Touch *t = NULL;
  for (unsigned i = 0; i < 8; i++)
    if (touches[i].active && touches[i].id == p->pointer_id)
      t = &touches[i];
  if (!t && p->phase != PXA_POINTER_DOWN) return;
  mouse_x = x;
  mouse_y = y;
  if (p->phase == PXA_POINTER_DOWN) {
    if (!t)
      for (unsigned i = 0; i < 8; i++)
        if (!touches[i].active) {
          t = &touches[i];
          break;
        }
    if (!t)
      return;
    *t = (Touch){
        .active = 1, .id = p->pointer_id, .x = x, .y = y, .cx = x, .cy = y};
#ifdef PORT_RTS
    t->role = 3;
    t->key = port_control_key_at(x,y,1);
    if(!t->key)t->key = (p->buttons & 2) ? 202 : 200;
#else
    t->key = port_control_key_at(x,y,0);
#ifdef PORT_SDL_CONTROLLER
    t->role = t->key ? 3 : 4;
#else
    t->role = t->key ? 3 : x < 160 ? 1 : 2;
#endif
#endif
  }
  if (!t)
    return;
  t->cx = x;
  t->cy = y;
#ifdef PORT_SDL_CONTROLLER
  if (t->role == 3 && p->phase == PXA_POINTER_MOVE)
    t->key = port_control_key_at(x,y,0);
  if (t->role == 4)
    port_game_pointer(x,y,p->phase,p->buttons,p->pointer_id);
#endif
  if ((p->phase == PXA_POINTER_MOVE || p->phase == PXA_POINTER_UP) && t->role == 2) {
    port_game_motion(x - t->x, y - t->y);
    t->x = x;
    t->y = y;
  }
  if (p->phase == PXA_POINTER_DOWN)
    port_log("PXA TOUCH raw=%d,%d logical=%d,%d role=%d key=%d",
        p->x,p->y,x,y,t->role,t->key);
  if (p->phase == PXA_POINTER_UP || p->phase == PXA_POINTER_CANCEL)
    t->active = 0;
  memset(touch_keys, 0, sizeof(touch_keys));
  for (unsigned i = 0; i < 8; i++)
    if (touches[i].active) {
      Touch *v = &touches[i];
      if (v->role == 3)
        touch_keys[v->key] = 1;
      if (v->role == 1) {
        int dx = v->cx - v->x, dy = v->cy - v->y;
        if (dx > 12)
          touch_keys['d'] = 1;
        if (dx < -12)
          touch_keys['a'] = 1;
        if (dy > 12)
          touch_keys['s'] = 1;
        if (dy < -12)
          touch_keys['w'] = 1;
      }
    }
  update_keys();
}
static void update_control_layout(void) {
  int scale=pxa_surface_fit_scale(surface_w,surface_h,display_w,display_h);
  if(scale<1)scale=1;
  int width=surface_w*scale,height=surface_h*scale;
  int x=(display_w-width)/2,y=(display_h-height)/2;
  /* Intersect the device safe area with the letterboxed game surface. Round
   * inward so the first control pixel also accepts pointer input. */
  int insets[4];
  for(unsigned i=0;i<4;++i)insets[i]=(int)(shape_insets[i]>bar_insets[i]?shape_insets[i]:bar_insets[i]);
  int top=insets[0]-y;
  int right=insets[1]-(display_w-x-width);
  int bottom=insets[2]-(display_h-y-height);
  int left=insets[3]-x;
  port_controls_safe_area(top>0?(top*240+height-1)/height:0,
      right>0?(right*320+width-1)/width:0,
      bottom>0?(bottom*240+height-1)/height:0,
      left>0?(left*320+width-1)/width:0);
}
static void controls_environment(const pxa_ui_environment_t *env) {
  display_density=env->density_q16?env->density_q16:65536;
  memcpy(shape_insets,env->safe_insets,sizeof(shape_insets));
  update_control_layout();
}
static void controls_window(const pxa_window_snapshot_t *snapshot) {
  if(snapshot->status || snapshot->revision<=window_revision)return;
  window_revision=snapshot->revision;
  shape_insets[0]=snapshot->safe_insets.top;
  shape_insets[1]=snapshot->safe_insets.right;
  shape_insets[2]=snapshot->safe_insets.bottom;
  shape_insets[3]=snapshot->safe_insets.left;
  bar_insets[0]=snapshot->system_bar_insets.top;
  bar_insets[1]=snapshot->system_bar_insets.right;
  bar_insets[2]=snapshot->system_bar_insets.bottom;
  bar_insets[3]=snapshot->system_bar_insets.left;
  update_control_layout();
  release_input();
}
int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
#if PORT_NATIVE_MAIN_LOOP
  int port_status = pxa_event_port_open(31);
  if (port_status) return port_status;
#endif
  pxa_ui_environment_t env={0};
  if (pxa_ui_parse_start_environment(config, length, &env)) {
    display_w = env.width;
    display_h = env.height;
  }
  pxa_window_fullscreen();
  /* Shape insets from UI do not include transient system/gesture bars. */
  int window_status=pxa_window_request_snapshot(20);
  if(window_status)port_log("PXA Window snapshot unavailable: %d",window_status);
  uint32_t n;
  pxa_ui_transaction_t tx = {0};
  if (!pxa_ui_transaction_begin(&tx, 1, PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                packet, sizeof(packet)) ||
      !pxa_ui_create(&tx, 1, 0, 0, PXA_UI_NODE_ROOT) ||
      !pxa_ui_set_rgba(&tx, 1, PXA_UI_PROPERTY_BACKGROUND, 0) ||
      !pxa_ui_set_event_mask(&tx, 1,
                             PXA_UI_EVENT_MASK_POINTER |
                                 PXA_UI_EVENT_MASK_CONTROLLER_STATE) ||
      !pxa_ui_transaction_commit(&tx))
    return PXA_STATUS_INTERNAL;
  if (display_w < 320 || display_h < 240) {
    if (display_w * 3 <= display_h * 4) {
      surface_w = display_w;
      surface_h = display_w * 3 / 4;
    } else {
      surface_h = display_h;
      surface_w = display_h * 4 / 3;
    }
  }
  controls_environment(&env);
#if PXA_GAME_RENDER_BLIT
  int raster_status = pxa_game_render_create_raster(30, 256);
  if (raster_status) return raster_status;
#endif
  pxa_surface_desc_t d = {.width = surface_w,
                          .height = surface_h,
                          .format = PXA_SURFACE_RGB565,
                          .buffer_count = 2,
                          .flags = PXA_SURFACE_PREFER_DIRECT_SCANOUT |
                                   PXA_SURFACE_GUEST_MAPPED};
  if (!pxa_surface_build_create(packet, sizeof(packet), 1, d, &n))
    return PXA_STATUS_INTERNAL;
  pxa_control_packet(packet, n);
  pxa_permission_build(packet, sizeof(packet), PXA_PERMISSION_ACQUIRE, 10,
                       "audio.playback", 14, (const uint8_t *)"media", 5, &n);
  audio_decision_pending=pxa_control_packet(packet, n)==0;
  return 0;
}
static int32_t handle_event(const uint8_t *bytes, uint32_t n) {
  pxa_event_t e;
  if (!pxa_parse_event(bytes, n, &e))
    return PXA_EVENT_UNHANDLED;
#ifdef PORT_SDL_CONTROLLER
  if(port_game_finished()) {
    /* SDL may already have destroyed its virtual controller and windows. */
    return e.service==PXA_WINDOW_SERVICE && e.opcode==PXA_WINDOW_BACK_REQUESTED
        ? PXA_EVENT_UNHANDLED : PXA_EVENT_HANDLED;
  }
#endif
#if PORT_NATIVE_MAIN_LOOP
  if (e.service == PXA_CORE_SERVICE && e.opcode == PXA_CORE_OPEN_EVENT_PORT && e.token == 31) {
    pxa_event_port_result_t result;
    if (!pxa_event_port_parse_open(&e, 31, &result)) return PXA_STATUS_INTERNAL;
    if (result.status) return result.status;
    event_port = result.handle;
    port_log("PXA native main-loop ready; continuation copies disabled");
    return PXA_EVENT_HANDLED;
  }
#endif
#if PXA_GAME_RENDER_BLIT
  if (e.service == PXA_GAME_RENDER_SERVICE &&
      e.opcode == PXA_GAME_RENDER_CREATE_RASTER_CONTEXT && e.token == 30) {
    pxa_game_render_raster_result_t result;
    if (!pxa_game_render_parse_raster(&e, 30, &result)) return PXA_STATUS_INTERNAL;
    if (result.status) return result.status;
    port_game_render_context = result.handle;
    port_log("PXA GameRender compute ready max=%u packet_capacity=%u",
        result.max_commands, PXA_GAME_RENDER_BLIT_BYTES);
    return PXA_EVENT_HANDLED;
  }
#endif
  pxa_window_snapshot_t window;
  if(pxa_window_parse_metrics_changed(&e,&window) ||
     pxa_window_parse_snapshot(&e,20,&window)) {
    controls_window(&window);
    return PXA_EVENT_HANDLED;
  }
  if(e.service==PXA_CLOCK_SERVICE && e.opcode==PXA_CLOCK_LIMITS && e.token==14) {
    pxa_clock_limits_result_t limits;
    if(pxa_clock_parse_limits(&e,14,&limits) && !limits.status &&
       pxa_clock_period_supported(&limits.limits,PORT_TICK_US)) {
      clock_timing=1;
      int status=clock_apply();
      if(status)port_log("PXA Clock negotiation failed: %d",status);
    } else port_log("PXA Clock preferred=%u us unavailable; legacy=%u us",PORT_TICK_US,PORT_TICK_MS*1000u);
    return PXA_EVENT_HANDLED;
  }
  if(e.service==PXA_CLOCK_SERVICE && e.opcode==PXA_CLOCK_CONFIGURE) {
    if(e.token==clock_pending) {
      pxa_clock_configure_result_t result;
      clock_pending=0;
      if(!pxa_clock_parse_configure(&e,e.token,clock_requested,&result) || result.status) {
        clock_timing=0;
        int status=clock_apply();
        port_log("PXA Clock rejected preferred period; legacy fallback status=%d",status);
      } else port_log("PXA Clock accepted period_us=%u",result.period_us);
    }
    return PXA_EVENT_HANDLED;
  }
  uint8_t life;
  if (pxa_lifecycle_parse(&e, &life)) {
    background = life == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
#if PORT_NATIVE_PCM
    audio_fill = 0; ++audio_epoch;
#ifdef PORT_PCM_FALLBACK
    fallback_fill=0;fallback_phase=0;
#endif
#endif
    release_input();
    if (audio)
      pxa_audio_flush(13, audio);
    int clock_status=clock_apply();
    if(clock_status) port_log("PXA Clock lifecycle failed: %d",clock_status);
    return PXA_EVENT_HANDLED;
  }
  if (e.token == waiting_token && waiting_token) {
    if (e.payload_size > sizeof(reply_data))
      return PXA_STATUS_INTERNAL;
    memcpy(reply_data, e.payload, e.payload_size);
    reply = e;
    reply.payload = reply_data;
    reply_ready = 1;
#if !PORT_NATIVE_MAIN_LOOP
    pump();
#endif
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_SURFACE_SERVICE && e.opcode == PXA_SURFACE_CREATE &&
      e.token == 1) {
    pxa_surface_create_result_t r;
    if (!pxa_surface_parse_create(&e, 1, &r)) {
      port_log("PXA surface reply is invalid");
      return PXA_STATUS_INTERNAL;
    }
    if (r.status) {
      port_log("PXA surface create failed: %d", r.status);
      return r.status;
    }
    surface = r.handle;
    uint32_t size;
    int registered = pxa_surface_register_buffers(
        surface, surface_buffers, (uint32_t)surface_w * surface_h * 2, 2);
    if (registered != (int)surface_w * surface_h * 4) {
      port_log("PXA surface register failed: %d", registered);
      return PXA_STATUS_INTERNAL;
    }
    if (pxa_surface_build_configure_layer(packet, sizeof(packet), 2, surface, 0,
                                          0, surface_w, surface_h, 0, 1, &size))
      pxa_control_packet(packet, size);
    int clock_status=clock_apply();
    if(clock_status) {port_log("PXA Clock start failed: %d",clock_status);return clock_status;}
    if(PORT_TICK_US!=PORT_TICK_MS*1000u) {
      int status=pxa_clock_limits(14);
      if(status)port_log("PXA Clock limits unavailable: %d; legacy=%u us",status,PORT_TICK_MS*1000u);
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_PERMISSION_SERVICE && e.token == 10) {
    audio_decision_pending=0;
    pxa_permission_acquire_result_t r;
    if (pxa_permission_parse_acquire(&e, 10, &r) && !r.status) {
      audio_permission = r.handle;
#if !PORT_NATIVE_PCM
      pxa_audio_open_media(11, r.handle);
#endif
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_AUDIO_SERVICE && e.token == 11) {
    pxa_audio_open_result_t r;
    if (pxa_audio_parse_open(&e, 11, &r) && !r.status && r.channels == 1 &&
        r.sample_rate == 16000) {
      audio_pending = r.handle;
      audio_rate = r.sample_rate;
      audio_channels = r.channels;
      pxa_audio_graph_t graph = {.gain_db_q8 = -1536};
      pxa_audio_commit_graph(12, audio_pending, &graph);
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_AUDIO_SERVICE && e.token == 12) {
    int32_t status;
    if (pxa_audio_parse_status(&e, 12, PXA_AUDIO_COMMIT_GRAPH, &status) &&
        !status)
      audio = audio_pending;
    return PXA_EVENT_HANDLED;
  }
  pxa_ui_pointer_data_t p;
  if (pxa_ui_parse_pointer(&e, &p)) {
    input_pointer(&p);
    return PXA_EVENT_HANDLED;
  }
  pxa_ui_controller_data_t c;
  if (pxa_ui_parse_controller(&e, &c)) {
    if (!background && c.controller == 0) {
      const uint32_t buttons = c.connected ? c.buttons & PXA_CONTROLLER_BUTTON_MASK : 0;
      /* Full-state refreshes and releases are not a new input method.
       * After touching the display, a held pad key must not hide it again. */
      const uint32_t pressed = buttons & ~controller.buttons;
      const int released_touch = pressed && port_controls_set_visible(0);
      if (released_touch) {
        release_touch();
#ifdef PORT_RTS
        /* An overlay touch leaves mouse_x/y over RMB/OK. Preserve the engine
         * cursor before those rectangles stop intercepting the pointer. */
        port_game_move_cursor(0, 0, &mouse_x, &mouse_y);
#endif
        port_log("PXA CONTROLS source=controller visible=0");
      } else if (!c.connected && port_controls_set_visible(1)) {
        port_log("PXA CONTROLS source=disconnect visible=1");
      }
      const int changed = port_controller_state(&controller, c.controller,
          c.connected, buttons, port_game_in_menu(), CONTROLLER_RTS, pad_keys);
      if (released_touch || changed) update_keys();
      if (changed) {
        port_log("PXA PAD controller=0 connected=%u buttons=%u", c.connected,
                 (unsigned)buttons);
      }
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_WINDOW_SERVICE &&
      e.opcode == PXA_WINDOW_BACK_REQUESTED) {
    port_game_key(27, 1);
    port_game_key(27, 0);
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_UI_SERVICE && e.opcode == PXA_UI_ENVIRONMENT_CHANGED) {
    pxa_ui_environment_t env;
    if (pxa_ui_parse_environment_event(&e, &env)) {
      display_w = env.width;
      display_h = env.height;
      controls_environment(&env);
      release_input();
    }
    return PXA_EVENT_HANDLED;
  }
  uint64_t now;
  if (pxa_clock_parse_tick(&e, &now)) {
#ifdef PORT_RTS
    if(engine_running)paint_loading(port_time_us());
#endif
#ifndef PORT_ENGINE_CONTROLLER
    controller_tick();
#endif
#ifdef PORT_CNC_MEDIA_HANDLES
    extern void port_filesystem_tick(uint64_t);
    port_filesystem_tick(now);
#endif
#ifdef PORT_LOADING_TRACE
    static uint64_t loading_log;
    if(engine_running && !perf_since && now-loading_log>=5000000) {
      extern void port_filesystem_loading_stats(void);
      loading_log=now;
      port_log("PXA LOAD pending=%llu reply=%u suspended=%u finished=%u background=%u",
        (unsigned long long)waiting_token,reply_ready,suspended,finished,background);
      port_filesystem_loading_stats();
    }
#endif
    if (surface) {
      started = 1;
      /* The Host only posts Clock ticks after foregrounding this UI. Resource
       * and permission replies can arrive earlier; they must not start the
       * retained game call while launch is still in the background. In the
       * native loop pump() immediately returns when the engine is running. */
      pump();
    }
    return PXA_EVENT_HANDLED;
  }
  return PXA_EVENT_UNHANDLED;
}
int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t n) {
  return handle_event(bytes, n);
}
void pxa_app_stop(uint32_t reason) {
  (void)reason;
  finished = 1;
#ifdef PORT_SDL_CONTROLLER
  if(!port_game_finished())release_input();
#else
  release_input();
#endif
  /* Core imports are forbidden here. The Host has already revoked Clock,
   * Surface, audio and filesystem handles before entering this callback. */
}
