/* PXA adapter for recrof's Quake II engine. GPL-2.0-or-later. */
#include "port.h"
#include "audio_convert.h"
#include "client/client.h"
#include "client/keys.h"
#include "client/snd_loc.h"
#include "quake2.h"
#include "other/q_arena.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static int dx, dy;
static char engine_error[192];
static int engine_error_code;
static uint64_t frame_total_us;
static unsigned completed_frames;
extern int scr_draw_loading;
void port_quake2_record_error(int code, const char *message) {
  engine_error_code = code;
  snprintf(engine_error, sizeof(engine_error), "%s", message);
  port_log("PXA Q2 ERROR code=%d %s", code, engine_error);
}
void port_quake2_abort(void) {
  if (!engine_error[0])
    port_error("Engine aborted this session. Reopen the game to restart.");
  char message[224];
  snprintf(message, sizeof(message), "Engine error %d:\n%s",
           engine_error_code, engine_error);
  port_error(message);
}
void port_game_stats(void) {
  /* One bounded record shares the four-slot device log queue with the three
   * common records. Keep the actual movement fields ahead of allocator data. */
  if (cls.state == ca_active && !cl.attractloop && cl.refresh_prepped &&
      !cls.disable_screen && !cl.cinematictime && !scr_draw_loading)
    port_log("PXA Q2 INPUT x=%d y=%d z=%d destination=%d up=%d down=%d hp=%d arena_total=%d used=%d peak=%d largest=%d frames=%u frame_us=%llu",
             cl.frame.playerstate.pmove.origin[0],
             cl.frame.playerstate.pmove.origin[1],
             cl.frame.playerstate.pmove.origin[2], cls.key_dest,
             port_key_down(K_UPARROW), port_key_down(K_DOWNARROW),
             cl.frame.playerstate.stats[STAT_HEALTH], Q_Arena_Total(),
             Q_Arena_Used(), Q_Arena_Peak(), Q_Arena_Largest(),
             completed_frames, (unsigned long long)frame_total_us);
  else
    port_log("PXA Q2 STATE connection=%d destination=%d attract=%d server=%d prepped=%d loading=%d arena_total=%d used=%d peak=%d largest=%d",
             cls.state, cls.key_dest, cl.attractloop, Com_ServerState(),
             cl.refresh_prepped, !!cls.disable_screen || !!scr_draw_loading,
             Q_Arena_Total(), Q_Arena_Used(), Q_Arena_Peak(), Q_Arena_Largest());
  completed_frames = 0; frame_total_us = 0;
}
void QG_SetPalette(unsigned char p[768]) { port_palette(p, 0); }
void QG_DrawFrame(void *p) { port_frame(p, 320, 240, 320); }
int QG_Milliseconds(void) { return port_time_us() / 1000; }
int QG_Microseconds(void) { return port_time_us(); }
void QG_Parallel2(void (*fn)(void *, int), void *p) {
  fn(p, 0);
  fn(p, 1);
}
int QG_IsGameTask(void) { return 1; }
void QG_Quit(void) { port_error("Quake II closed."); }
void QG_FatalError(const char *s) { port_error(s); }
int QG_GetKey(int *down, int *key) {
  (void)down;
  (void)key;
  return 0;
}
void QG_GetMouseMove(int *x, int *y) {
  *x = dx;
  *y = dy;
  dx = dy = 0;
}
void port_game_motion(int x, int y) {
  dx += x;
  dy += y;
}
int port_game_in_menu(void) { return cls.key_dest != key_game; }
void port_game_key(int key, int down) {
  static int pressed[256];
  int original = key;
  if (down) {
    if (cls.key_dest != key_game) {
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
      if (key == 'e')
        key = K_ENTER;
    }
    if (original >= 0 && original < 256)
      pressed[original] = key;
  } else if (key >= 0 && key < 256)
    key = pressed[key];
  Quake2_SendKey(key, down);
}
static unsigned char snd[8192];
static unsigned played;
qboolean SNDDMA_Init(void) {
  memset(snd, 0, sizeof(snd));
  dma.samplebits = 16;
  dma.speed = 11025;
  dma.channels = 2;
  dma.samples = sizeof(snd) / 2;
  dma.samplepos = 0;
  dma.submission_chunk = 1;
  dma.buffer = snd;
  return true;
}
int SNDDMA_GetDMAPos(void) {
  return ((port_time_us() * 11025 / 1000000) * 2) & (dma.samples - 1);
}
void SNDDMA_Shutdown(void) {}
void SNDDMA_BeginPainting(void) {}
void SNDDMA_Submit(void) {}
void port_game_audio(void) {
  unsigned end = SNDDMA_GetDMAPos() / 2;
  unsigned count = (end - played) & 2047;
  if (count > 1102)
    played = (end - 1102) & 2047;
  played=port_audio_s16_stereo_ring((const int16_t *)snd,2047,played,end,11025);
}
void port_game_main(void) {
  mkdir("/data/baseq2", 0700);
  FILE *f = fopen("/data/baseq2/pak0.pak", "rb");
  if (!f)
    port_error("Missing baseq2/pak0.pak.\nImport Quake II game data with the "
               "included tools.");
  fclose(f);
  /* basedir already searches SD through the resource bridge. Repeating the
   * same path as cddir duplicates the PAK directory and startup probes. */
  char *argv[] = {"quake2", "+set", "basedir", "/data", "+set", "sw_mode", "0",
                  "+set", "s_initsound", "1", "+menu_main", NULL};
  Quake2_Init((int)(sizeof(argv) / sizeof(argv[0])) - 1, argv);
  uint64_t prev = port_time_us();
  for (;;) {
    uint64_t now = port_time_us();
    int ms = (now - prev) / 1000;
    if (ms < 1) {
      port_sleep(1);
      continue;
    }
    if (ms > 100)
      ms = 100;
    prev = now;
    uint64_t frame_start = port_time_us();
    Quake2_Frame(ms);
    frame_total_us += port_time_us() - frame_start;
    ++completed_frames;
    port_sleep(1);
  }
}

/* Upstream defines this no-op as (void), but its spawn table calls (edict_t*).
 * Wasm checks indirect signatures; preserve the no-op via a typed link wrapper. */
void __real_SP_info_player_intermission(void);
void __wrap_SP_info_player_intermission(void *entity) {
  (void)entity;
  __real_SP_info_player_intermission();
}
