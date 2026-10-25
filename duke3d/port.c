#include "port.h"
#include "pointer_motion.h"
#include "audio_convert.h"
#include "audio.h"
#include "display_frame.h"
#include "extMemory.h"
#include "graphics.h"
#include "maincfg.h"
#include "internalFlash.h"
#include "board.h"
#include "port/keyboard.h"
#include "vflash.h"
#include "keymap.h"
#include <string.h>
static displayFrame_t frame;
static uint8_t scq[256], downq[256];
static unsigned kh, kt;
static uint8_t keys_this_update[256];
static bool menu = true;
static bool audio_ready;
static float mx, my;
static port_pointer_motion_t pointer_motion;
extern int g_iTickRate, g_iTicksPerFrame;
void port_duke_input_stats(void);
void port_game_stats(void) {
  vflash_stats_t io;
  vflash_stats(&io,1);
  port_log("PXA DUKE cache_used=%u cache_free=%d",
           INTERNAL_FLASH_ARENA_SIZE-internalFlashSpaceLeft(),internalFlashSpaceLeft());
  port_log("PXA IO hit=%llu miss=%llu read_us=%llu nvm_reads=%llu nvm_us=%llu cache_bytes=%u nvm_hit=%llu nvm_miss=%llu",
    (unsigned long long)io.hits,(unsigned long long)io.misses,
    (unsigned long long)io.read_us,(unsigned long long)io.nvm_reads,
    (unsigned long long)io.nvm_read_us,io.cache_bytes,
    (unsigned long long)io.nvm_cache_hits,(unsigned long long)io.nvm_cache_misses);
  port_duke_input_stats();
}
uint8_t audioBuffer[AUDIO_BUFFER_LENGTH * 2];

uint32_t espTimerCount(void) { return port_time_us(); }
unsigned int I_GetTimeMicrosecs(void) { return port_time_us(); }
void delay(unsigned ms) { port_sleep(ms); }
void NVIC_SystemReset(void) {
  port_error("Duke Nukem 3D requested a restart.");
}
void displayInit(void) {}
void displaySetBacklight(uint8_t b) { (void)b; }
displayFrame_t *displayAcquireFrame(void) {
  frame.pixels = displayData.displayFrameBuffer[0];
  return &frame;
}
void displaySubmitFrame(displayFrame_t *f) {
  port_palette_rgb565(f->palette, 1);
  port_frame(f->pixels, 320, 240, 320);
}
bool displayBusy(void) { return false; }
void initKeyboard(void) {}
void getKeys(uint16_t *p) { *p = 0; }
void getAnalogInput(int32_t *a, int32_t *b, int32_t *c, int32_t *d) {
  int pad_x, pad_y;
  /* getinput runs once per fixed simulation step, including catch-up steps.
   * Sample held directions here so one step cannot consume a whole render
   * frame's turn and leave the following step with zero input. */
  const uint32_t elapsed_us = 1000000u * (g_iTickRate / g_iTicksPerFrame) /
      g_iTickRate;
  port_sample_controller(elapsed_us, &pad_x, &pad_y);
  const port_pointer_delta_t touch = port_pointer_sample(&pointer_motion, elapsed_us);
  mx += touch.x * 3;
  my += touch.y * 3;
  const int touch_x = (int)mx, touch_y = (int)my;
  *a = *b = 0;
  *c = touch_x + pad_x * 3;
  *d = touch_y + pad_y * 3;
  mx -= touch_x;
  my -= touch_y;
}
void inputSetMenuMode(bool m) {
  if (m != menu) port_game_reset_motion();
  menu = m;
}
bool inputIsMenuMode(void) { return menu; }
int port_game_in_menu(void) { return menu; }
void inputPostKeyEvent(uint8_t sc, bool down) {
  unsigned next = (kh + 1) & 255;
  if (next != kt) {
    scq[kh] = sc;
    downq[kh] = down;
    kh = next;
  }
}
void inputBeginKeyUpdate(void) {
  memset(keys_this_update, 0, sizeof(keys_this_update));
  port_poll_input();
}
bool inputGetKeyEvent(uint8_t *sc, bool *down) {
  if (kh == kt)
    return false;
  /* Apply different keys together, but keep a quick down/up visible to at
   * least one engine poll. Leave the release in order for the next update. */
  if (!downq[kt] && keys_this_update[scq[kt]]) return false;
  *sc = scq[kt];
  *down = downq[kt];
  if (*down) keys_this_update[*sc] = 1;
  kt = (kt + 1) & 255;
  return true;
}
void port_game_key(int key, int down) {
  static uint8_t pressed[256];
  if (!down && key >= 0 && key < 256) {
    if (pressed[key])
      inputPostKeyEvent(pressed[key], false);
    return;
  }
  int sc = port_duke_scancode(key, menu);
  if (key >= 0 && key < 256)
    pressed[key] = sc;
  if (sc)
    inputPostKeyEvent(sc, down);
}
void port_game_motion(int x, int y) {
  if (!menu) port_pointer_push(&pointer_motion, x, y);
}
void port_game_reset_motion(void) {
  port_pointer_reset(&pointer_motion);
  mx = my = 0;
}
void initAudio(void) {
  memset(audioBuffer, 128, sizeof(audioBuffer));
  audio_ready = true;
}
uint32_t audioGetPlayIndex(void) {
  return (port_time_us() * 11025 / 1000000) & (AUDIO_BUFFER_LENGTH - 1);
}
int SNDDMA_GetDMAPos(void) { return audioGetPlayIndex() * 2; }
void port_game_audio(void) {
  if (!audio_ready) return;
  static unsigned played;
  unsigned end = audioGetPlayIndex();
  played=port_audio_u8_stereo_ring(audioBuffer,AUDIO_BUFFER_LENGTH-1,played,end,11025);
}
void __real_sampletimer(void);
void __wrap_sampletimer(void) {
  static uint64_t last_audio;
  __real_sampletimer();
  const uint64_t now = port_time_us();
  // Intro and menu waits call sampletimer without presenting a frame. Keep
  // their audio and input progressing and bound each cooperative callback.
  if (now - last_audio >= 8000) {
    port_game_audio();
    last_audio=now;
  }
  if(port_cooperate_due(now,PORT_COOPERATE_BUDGET_US)) port_sleep(0);
}
int dn3dmain(int argc, char **argv);
void port_game_main(void) {
  initGraphics();
  extMemInit();
  char *argv[] = {"duke3d", NULL};
  dn3dmain(1, argv);
  port_error("Duke Nukem 3D closed.");
}
