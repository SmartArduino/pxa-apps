#include "port.h"
#include "audio_convert.h"
#include "vflash.h"
#include "audio.h"
#include "esp_parallel.h"
#include "graphics.h"
#include "keyboard.h"
#include "quakedef.h"
static int mdx, mdy;
#ifdef PXA_FRAME_PROFILE
static uint64_t profile_stamp, profile_part[3], profile_total[3];
static unsigned profile_frames;
void Pxa_QuakeProfile(unsigned stage) {
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
  vflash_stats_t io;vflash_stats(&io,1);
  port_log("PXA IO hit=%llu miss=%llu read_us=%llu cache_bytes=%u",
    (unsigned long long)io.hits,(unsigned long long)io.misses,
    (unsigned long long)io.read_us,io.cache_bytes);
#ifdef PXA_FRAME_PROFILE
  port_log("PXA QUAKE complete=%u engine_us=%llu render_us=%llu sound_us=%llu",
    profile_frames, (unsigned long long)profile_total[0],
    (unsigned long long)profile_total[1], (unsigned long long)profile_total[2]);
  memset(profile_total, 0, sizeof(profile_total));
  profile_frames = 0;
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
void port_game_key(int key, int down) {
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
  mdx += x;
  mdy += y;
}
void Sys_SendKeyEvents(void) {
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
    _g->cl.viewangles[YAW] -= mdx * 0.3f;
    _g->cl.viewangles[PITCH] += mdy * 0.3f;
    if (mdy)
      V_StopPitchDrift();
    if (_g->cl.viewangles[PITCH] > 80)
      _g->cl.viewangles[PITCH] = 80;
    if (_g->cl.viewangles[PITCH] < -70)
      _g->cl.viewangles[PITCH] = -70;
  }
  mdx = mdy = 0;
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
