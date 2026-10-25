#include "port.h"
#include "pointer_motion.h"
#include "audio_convert.h"
#include "vflash.h"
#include "audio.h"
#include "esp_parallel.h"
#include "graphics.h"
#include "keyboard.h"
#include "quakedef.h"
#include "lightmap_cache.h"
#include "pxa_game_render_spans.h"
#if PXA_QUAKE_HOST_AUDIO_MIX
#include "pxa_audio.h"
#include "host_audio_mix.h"
int Pxa_QuakeMixS8(void *paintbuffer, const void *source,
    int count, int left, int right) {
  static int unavailable, announced;
  const uint64_t session = port_audio_session();
  if (!session || unavailable || count <= 0 || count > PXA_AUDIO_MIX_MAX_FRAMES ||
      left < 0 || left > 256 || right < 0 || right > 256) return 0;
  uint8_t record[PXA_AUDIO_MIX_SPAN_BYTES];
  const pxa_audio_mix_span_t span = {
    .source_offset = (uint32_t)(uintptr_t)source, .source_bytes = (uint32_t)count,
    .step_q16 = 65536, .frames = (uint32_t)count,
    .left_gain_q8 = (uint16_t)left, .right_gain_q8 = (uint16_t)right,
    .encoding = PXA_AUDIO_MIX_S8, .channels = 1};
  if (!pxa_audio_encode_mix_span(record, sizeof(record), &span)) return 0;
  int32_t status = pxa_audio_mix_pcm(session, (uint32_t)(uintptr_t)paintbuffer,
    (uint32_t)count, (uint32_t)(uintptr_t)record, 1);
  if (status == PXA_STATUS_UNSUPPORTED) unavailable = 1;
  if (status) return 0;
  if (!announced) {
    announced = 1;
    port_log("PXA AUDIO Host S8 accumulation active; engine timing and quantization retained");
  }
  return 1;
}
#endif
#if PXA_GAME_RENDER_SPANS || PXA_GAME_RENDER_PROJECTED_SPANS || PXA_GAME_RENDER_FRAME_SPANS || PXA_GAME_RENDER_BLIT
extern uint64_t port_game_render_context;
#endif
static port_pointer_motion_t pointer_motion;
#ifdef PXA_QUAKE_RENDER_PROBE
unsigned port_render_probe_mode;
static unsigned probe_serial, probe_selected, probe_completed;
static uint64_t probe_view;
/* An explicitly private build polls one private byte only after presentation.
 * No partial render or borrowed command packet survives a
 * mode change; ordinary App builds do not contain this control. */
static void probe_poll_mode(void) {
  FILE *f = fopen("/data/pxa-render-probe-mode", "rb");
  if (!f) return;
  int value = fgetc(f), end = fgetc(f);
  fclose(f);
  if (end == EOF) {
    if (value >= '0' && value <= '9') port_render_probe_mode = (unsigned)(value - '0');
    else if (value >= 'a' && value <= 'f') port_render_probe_mode = (unsigned)(value - 'a' + 10);
  }
}
#endif
#ifdef PXA_FRAME_PROFILE
#include "render_profile.h"
static uint64_t render_stamp[PXA_QUAKE_RENDER_STAGES],render_total[PXA_QUAKE_RENDER_STAGES];
void Pxa_QuakeRenderProfile(unsigned stage,int begin) {
  if(stage>=PXA_QUAKE_RENDER_STAGES)return;
#ifdef PXA_QUAKE_RENDER_PROBE
  if(stage>=PXA_QUAKE_RENDER_PREP && !probe_selected)return;
#endif
  uint64_t now=port_time_us();
  if(begin)render_stamp[stage]=now;
  else if(now>=render_stamp[stage]) {
    render_total[stage]+=now-render_stamp[stage];
#ifdef PXA_QUAKE_RENDER_PROBE
    if(stage==PXA_QUAKE_RENDER_TOTAL && probe_selected) {
      probe_view+=now-render_stamp[stage];
      ++probe_completed;
    }
#endif
  }
}
static uint64_t profile_stamp, profile_part[3], profile_total[3];
static unsigned profile_frames;
void Pxa_QuakeProfile(unsigned stage) {
#ifdef PXA_QUAKE_RENDER_PROBE
  if(!stage)probe_selected=(++probe_serial & 15u)==0;
#endif
  uint64_t now = port_time_us();
  if (stage) profile_part[stage-1] = now-profile_stamp;
  profile_stamp = now;
  if (stage == 3) {
    for (unsigned i=0; i<3; ++i) profile_total[i] += profile_part[i];
    ++profile_frames;
  }
}
#endif
void port_game_stats(void) {
  Pxa_QuakeLightStats();
  vflash_stats_t io; vflash_stats(&io, 1);
  uint8_t stats[64] = {0};
#if PXA_GAME_RENDER_SPANS || PXA_GAME_RENDER_PROJECTED_SPANS || PXA_GAME_RENDER_FRAME_SPANS || PXA_GAME_RENDER_BLIT
  if (port_game_render_context)
    pxa_io(port_game_render_context, PXA_GAME_RENDER_IO_SPAN_TELEMETRY, stats, sizeof(stats));
#endif
#ifdef PXA_FRAME_PROFILE
  int ready = _g->cls.state == ca_connected && _g->cls.signon == SIGNONS &&
    !_g->cls.demoplayback && _g->cl.worldmodel && key_dest == key_game &&
    _g->cl.stats[STAT_HEALTH] > 0;
  /* Stay within the 256-byte Log ABI and the four-slot device queue.
   * Timings are per completed engine frame; Host counters are window deltas. */
  static uint64_t previous[6];
  uint64_t delta[6];
  for (unsigned i=0; i<6; ++i) {
    uint64_t value = pxa_load_u64(stats + i*8);
    delta[i] = value >= previous[i] ? value-previous[i] : value;
    previous[i] = value;
  }
  unsigned frames = profile_frames ? profile_frames : 1;
#ifdef PXA_QUAKE_RENDER_PROBE
  unsigned sampled = probe_completed ? probe_completed : 1;
  port_log("PXA QUAKE ready=%d hp=%d xyz=%d,%d,%d n=%u mode=%u pn=%u view=%llu sv=%llu prep=%llu grad=%llu rows=%llu load=%llu pack=%llu io=%llu hb=%llu hr=%llu hv=%llu hx=%llu",
    ready, _g->cl.stats[STAT_HEALTH],
    (int)_g->r_refdef.vieworg[0], (int)_g->r_refdef.vieworg[1], (int)_g->r_refdef.vieworg[2],
    profile_frames, port_render_probe_mode, probe_completed,
    (unsigned long long)(render_total[0]/frames), (unsigned long long)(probe_view/sampled),
    (unsigned long long)(render_total[PXA_QUAKE_RENDER_PREP]/sampled),
    (unsigned long long)(render_total[PXA_QUAKE_RENDER_GRADIENTS]/sampled),
    (unsigned long long)(render_total[PXA_QUAKE_RENDER_ROWS]/sampled),
    (unsigned long long)(render_total[PXA_QUAKE_RENDER_LIGHT_LOAD]/sampled),
    (unsigned long long)(render_total[PXA_QUAKE_RENDER_LIGHT_PACK]/sampled),
    (unsigned long long)(io.read_us/frames), (unsigned long long)delta[0],
    (unsigned long long)delta[1], (unsigned long long)(delta[4]/frames),
    (unsigned long long)(delta[5]/frames));
  probe_view=0;probe_completed=0;
  probe_poll_mode();
#else
  port_log("PXA QUAKE ready=%d hp=%d xyz=%d,%d,%d n=%u eng=%llu view=%llu world=%llu scan=%llu surf=%llu ent=%llu snd=%llu io=%llu hb=%llu hr=%llu px=%llu hv=%llu hx=%llu",
    ready, _g->cl.stats[STAT_HEALTH],
    (int)_g->r_refdef.vieworg[0], (int)_g->r_refdef.vieworg[1], (int)_g->r_refdef.vieworg[2],
    profile_frames, (unsigned long long)(profile_total[0]/frames),
    (unsigned long long)(render_total[0]/frames),
    (unsigned long long)(render_total[1]/frames),
    (unsigned long long)(render_total[3]/frames),
    (unsigned long long)(render_total[4]/frames),
    (unsigned long long)(render_total[5]/frames),
    (unsigned long long)(profile_total[2]/frames),
    (unsigned long long)(io.read_us/frames),
    (unsigned long long)delta[0], (unsigned long long)delta[1],
    (unsigned long long)delta[3], (unsigned long long)(delta[4]/frames),
    (unsigned long long)(delta[5]/frames));
#endif
  memset(render_total, 0, sizeof(render_total));
  memset(profile_total, 0, sizeof(profile_total));
  profile_frames = 0;
#else
  port_log("PXA IO hit=%llu miss=%llu read_us=%llu cache_bytes=%u host_batches=%llu host_rejected=%llu native_us=%llu",
    (unsigned long long)io.hits, (unsigned long long)io.misses,
    (unsigned long long)io.read_us, io.cache_bytes,
    (unsigned long long)pxa_load_u64(stats), (unsigned long long)pxa_load_u64(stats + 8),
    (unsigned long long)pxa_load_u64(stats + 40));
#endif
}

static struct {
  int k, d;
} keyq[128];
static unsigned kh, kt;
displayData_t displayData;
int8_t audioBuffer[AUDIO_BUFFER_LENGTH * 2];
void parallelInit(void) {}
void parallelBegin(void) {}
void parallelEnd(void) {}
bool parallelPost(void (*fn)(void *), void *p) {
  (void)fn;
  (void)p;
  return false;
}
void parallelFinish(void) {}
unsigned int I_GetTimeMicrosecs(void) { return port_time_us(); }
float Sys_FloatTime(void) { return port_time_us() / 1000000.0; }
void NVIC_SystemReset(void) { port_error("Quake requested a restart."); }
void Sys_Error(char *fmt, ...) {
  char s[256];
  va_list a;
  va_start(a, fmt);
  vsnprintf(s, sizeof(s), fmt, a);
  va_end(a);
  port_error(s);
}
void initGraphics(void) {
  displayData.displayDmaLineBuffersSent = NUMBER_OF_DMA_LINES + 1;
}
void startDisplayRefresh(uint8_t b, int all) {
  (void)b;
  static uint8_t p[320 * 200];
  for (int y = 0; y < (all ? 200 : DRAW_BUFFER_HEIGHT); y++)
    memcpy(p + 320 * y,
           y < DRAW_BUFFER_HEIGHT
               ? displayData.displayFrameBuffer[0] + 320 * y
               : (uint8_t *)aux_buffer + 320 * (y - DRAW_BUFFER_HEIGHT),
           320);
  port_palette_rgb565(displayData.pPalette, 1);
  displayData.displayDmaLineBuffersSent = NUMBER_OF_DMA_LINES + 1;
  port_frame(p, 320, 200, 320);
}
void setDisplayPen(int c, int b) {
  (void)c;
  (void)b;
}
void displayPrintf(int x, int y, const char *fmt, ...) {
  (void)x;
  (void)y;
  char s[256];
  va_list a;
  va_start(a, fmt);
  vsnprintf(s, sizeof(s), fmt, a);
  va_end(a);
  port_log("%s", s);
}
void displayPrintln(bool update, const char *fmt, ...) {
  (void)update;
  char s[256];
  va_list a;
  va_start(a, fmt);
  vsnprintf(s, sizeof(s), fmt, a);
  va_end(a);
  port_log("%s", s);
}
void initKeyboard(void) {}
void getKeys(uint16_t *k) { *k = 0; }
void getAnalogInput(int32_t *a, int32_t *b, int32_t *c, int32_t *d) {
  *a = *b = *c = *d = 0;
}
int port_game_in_menu(void) { return key_dest != key_game; }
void port_game_key(int key, int down) {
  if (down && key == 27) port_game_reset_motion();
  static int pressed[256];
  int original = key;
  if (down) {
    if (key_dest != key_game) {
      if (key == 'w')
        key = K_UPARROW;
      if (key == 's')
        key = K_DOWNARROW;
      if (key == 'a')
        key = K_LEFTARROW;
      if (key == 'd')
        key = K_RIGHTARROW;
      if (key == 200)
        key = K_ENTER;
    } else {
      if (key == 'w')
        key = K_UPARROW;
      if (key == 's')
        key = K_DOWNARROW;
      if (key == 'a' || key == K_LEFTARROW)
        key = ',';
      if (key == 'd' || key == K_RIGHTARROW)
        key = '.';
      if (key == 'q')
        key = '/';
      /* These are the handheld's fixed controls. Media default.cfg and an
       * existing saved config may omit mouse attack or comma/period strafing.
       * Bind the actual engine key before delivering its press, so releases
       * use the same +command even if the menu changed in between. */
      const char *binding = NULL;
      switch (key) {
      case K_UPARROW: binding = "+forward"; break;
      case K_DOWNARROW: binding = "+back"; break;
      case ',': binding = "+moveleft"; break;
      case '.': binding = "+moveright"; break;
      case K_MOUSE1: binding = "+attack"; break;
      case K_SPACE: binding = "+jump"; break;
      case '/': binding = "impulse 10"; break;
      case 'e': binding = "+use"; break;
      }
      if (binding) Key_SetBinding(key, (char *)binding);
    }
    if (original >= 0 && original < 256)
      pressed[original] = key;
  } else if (key >= 0 && key < 256)
    key = pressed[key];
  unsigned next = (kh + 1) % 128;
  if (next != kt) {
    keyq[kh].k = key;
    keyq[kh].d = down;
    kh = next;
  }
}
void port_game_motion(int x, int y) {
  if (key_dest == key_game) port_pointer_push(&pointer_motion, x, y);
}
void port_game_reset_motion(void) {
  port_pointer_reset(&pointer_motion);
}
void Sys_SendKeyEvents(void) {
  port_poll_input();
  while (kt != kh) {
    Key_Event(keyq[kt].k, keyq[kt].d);
    kt = (kt + 1) % 128;
  }
}
void IN_Init(void) {}
void IN_Shutdown(void) {}
void IN_Commands(void) {}
void IN_Move(usercmd_t *cmd) {
  (void)cmd;
  if (key_dest == key_game) {
    int pad_x, pad_y;
    port_sample_controller((uint32_t)(host_frametime * 1000000), &pad_x, &pad_y);
    const port_pointer_delta_t touch = port_pointer_sample(&pointer_motion,
        (uint32_t)(host_frametime * 1000000));
    const float mdx = touch.x + pad_x, mdy = touch.y + pad_y;
    _g->cl.viewangles[YAW] -= mdx * 0.3f;
    _g->cl.viewangles[PITCH] += mdy * 0.3f;
    if (mdy)
      V_StopPitchDrift();
    if (_g->cl.viewangles[PITCH] > 80)
      _g->cl.viewangles[PITCH] = 80;
    if (_g->cl.viewangles[PITCH] < -70)
      _g->cl.viewangles[PITCH] = -70;
  } else port_game_reset_motion();
}
void initAudio(void) { memset(audioBuffer, 128, sizeof(audioBuffer)); }
uint32_t audioGetPlayIndex(void) {
  return (port_time_us() * 11025 / 1000000) & (AUDIO_BUFFER_LENGTH - 1);
}
void port_game_audio(void) {
  static unsigned played;
  unsigned end = audioGetPlayIndex();
  played=port_audio_u8_stereo_ring((const uint8_t *)audioBuffer,
      AUDIO_BUFFER_LENGTH-1,played,end,11025);
}
void port_game_main(void) {
#ifdef PXA_QUAKE_RENDER_PROBE
  probe_poll_mode();
#endif
  initGraphics();
  extMemInit();
  Z_Init();
  Cvar_Init();
  memset(_g, 0, sizeof(global_data_t));
  Host_Init(NULL);
  double prev = Sys_FloatTime();
  for (;;) {
    double now = Sys_FloatTime();
    float dt = now - prev;
    prev = now;
    if (dt > 0.1f)
      dt = 0.1f;
    Host_Frame(dt);
    port_sleep(1);
  }
}
