/* Cooperative PXA Core 2 runtime for the recrof engine ports. GPL-3.0-or-later.
 */
#include "port.h"
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
#include "pxa_ui.h"
#include "pxa_window.h"
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
__attribute__((import_module("asyncify"), import_name("start_unwind"))) void
unwind(void *);
__attribute__((import_module("asyncify"), import_name("stop_unwind"))) void
stop_unwind(void);
__attribute__((import_module("asyncify"), import_name("start_rewind"))) void
rewind_stack(void *);
__attribute__((import_module("asyncify"), import_name("stop_rewind"))) void
stop_rewind(void);
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
static uint8_t saved_stack[PORT_CONTINUATION_BYTES] __attribute__((aligned(16)));
static struct {
  void *cursor, *end;
} continuation;
static uintptr_t engine_sp;
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
static uint64_t audio_permission, audio_pending;
static int audio_decision_pending;
static uint32_t pad_buttons;
static uint64_t perf_since, perf_draw_us;
static uint64_t perf_yield_at, perf_wait_us;
static uint32_t perf_yields;
static uint32_t perf_frames, perf_skipped;
__attribute__((weak)) void port_game_stats(void) {}

typedef struct {
  int active, x, y, cx, cy, role, key;
  uint8_t id;
} Touch;
static Touch touches[8];
static int touch_keys[256], pad_keys[256];

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

/* These two functions are Asyncify's top/bottom runtime boundaries. Keep them
 * out of the optimizer's inliner. No host-specific continuation imports remain
 * after the Binaryen post-link step. */
NOINLINE void port_yield(void) {
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
}
int port_cooperate_due(uint64_t now,uint32_t budget_us) {
  return now>=last_cooperative_us && now-last_cooperative_us>=budget_us;
}
NOINLINE static void run_game(void) {
  engine_running = 1;
  port_log("Starting %s", PORT_TITLE);
  mkdir("/data", 0700);
  if(!port_external_prepare()) return;
  port_game_main();
}
NOINLINE static void pump(void) {
#ifdef PORT_EXTERNAL_GAME_DATA
  if(audio_decision_pending) return;
#endif
  if (finished || background)
    return;
  if (suspended) {
    if (waiting_token && !reply_ready)
      return;
    if (!waiting_token && port_time_us() < wake_at)
      return;
  }
  uintptr_t callback_stack = port_stack_get();
  if (!started) started = 1;
  if (!engine_running) {
    memset(__stack_low,0xa5,PORT_ENGINE_STACK_BYTES);
    callback_stack_floor = callback_stack;
    if(callback_stack > (uintptr_t)__stack_low+PORT_ENGINE_STACK_BYTES)
      memset(__stack_low+PORT_ENGINE_STACK_BYTES,0xa5,callback_stack-(uintptr_t)__stack_low-PORT_ENGINE_STACK_BYTES);
  }
  port_stack_set(suspended ? engine_sp : (uintptr_t)__stack_low + PORT_ENGINE_STACK_BYTES);
  if (suspended) {
    suspended = 0;
    rewinding = 1;
    rewind_stack(&continuation);
  }
  run_game();
  if (suspended) {
    stop_unwind();
    uint32_t bytes=(uint8_t *)continuation.cursor-saved_stack;
    if(bytes>continuation_peak)continuation_peak=bytes;
  } else
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
static void present(int index) {
  pending_frame = ++frame_id;
  int r = pxa_surface_present_buffer(surface,index,pending_frame);
  if (r == PXA_STATUS_WOULD_BLOCK) pending_buffer = index;
  else if (r != 16) port_log("PXA present failed: %d",r);
}
void port_error(const char *message) {
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
  uint64_t begin = port_time_us();
  if (!perf_since) perf_since = begin;
  int index = acquire();
  if (index >= 0) {
    uint16_t *dest = output(index);
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
    port_log("PXA SCHED yields=%u wait_us=%llu",perf_yields,(unsigned long long)perf_wait_us);
    uint8_t *low=__stack_low,*engine=low,*callback=low+PORT_ENGINE_STACK_BYTES;
    while(engine<low+PORT_ENGINE_STACK_BYTES && *engine==0xa5)++engine;
    while((uintptr_t)callback<callback_stack_floor && *callback==0xa5)++callback;
    port_log("PXA STACK engine=%u callback=%u continuation=%u",
             (unsigned)(low+PORT_ENGINE_STACK_BYTES-engine),(unsigned)(low+PORT_ENGINE_STACK_BYTES+PORT_CALLBACK_STACK_BYTES-callback),continuation_peak);
    port_game_stats();
    perf_since=end; perf_frames=perf_skipped=0; perf_draw_us=0;
    perf_wait_us=0;perf_yields=0;
  }
  port_game_audio();
#ifndef PORT_PACED_BY_MAIN
  port_sleep(1);
#endif
}
void port_frame(const uint8_t *p, int w, int h, int pitch) {
  port_frame_cursor(p,w,h,pitch,NULL);
}
void port_pcm(const int16_t *p, unsigned frames, unsigned rate,
              unsigned channels) {
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
static void release_input(void) {
  memset(touches, 0, sizeof(touches));
  memset(touch_keys, 0, sizeof(touch_keys));
  memset(pad_keys, 0, sizeof(pad_keys));
  pad_buttons = 0;
  update_keys();
}
static void input_pointer(const pxa_ui_pointer_data_t *p) {
  int scale = pxa_surface_fit_scale(surface_w, surface_h, display_w, display_h);
  if (scale < 1)
    scale = 1;
  int x = (p->x - (display_w - surface_w * scale) / 2) * 320 /
          (surface_w * scale),
      y = (p->y - (display_h - surface_h * scale) / 2) * 240 /
          (surface_h * scale);
  if (x < 0)
    x = 0;
  if (x > 319)
    x = 319;
  if (y < 0)
    y = 0;
  if (y > 239)
    y = 239;
  mouse_x = x;
  mouse_y = y;
  Touch *t = NULL;
  for (unsigned i = 0; i < 8; i++)
    if (touches[i].active && touches[i].id == p->pointer_id)
      t = &touches[i];
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
    t->role = t->key ? 3 : x < 160 ? 1 : 2;
#endif
  }
  if (!t)
    return;
  t->cx = x;
  t->cy = y;
  if (p->phase == PXA_POINTER_MOVE && t->role == 2) {
    port_game_motion(x - t->x, y - t->y);
    t->x = x;
    t->y = y;
  }
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
int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t n) {
  pxa_event_t e;
  if (!pxa_parse_event(bytes, n, &e))
    return PXA_EVENT_UNHANDLED;
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
    pump();
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
      pxa_audio_open_media(11, r.handle);
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
    static const int map[12] = {128, 129, 130, 131, 200, ' ',
                                27,  13,  'e', 'q', 'a', 'd'};
    memset(pad_keys, 0, sizeof(pad_keys));
    pad_buttons = c.connected ? c.buttons : 0;
    for (int i = 0; i < 12; i++)
      if (pad_buttons & (1u << i))
        pad_keys[map[i]] = 1;
    update_keys();
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
      pump();
    }
    return PXA_EVENT_HANDLED;
  }
  return PXA_EVENT_UNHANDLED;
}
void pxa_app_stop(uint32_t reason) {
  (void)reason;
  finished = 1;
  release_input();
  /* Core imports are forbidden here. The Host has already revoked Clock,
   * Surface, audio and filesystem handles before entering this callback. */
}
