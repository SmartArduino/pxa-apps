#include "port.h"
#include "audio.h"
#include "display_frame.h"
#include "extMemory.h"
#include "graphics.h"
#include "maincfg.h"
#include "internalFlash.h"
#include "board.h"
#include "port/keyboard.h"
#include "vflash.h"
#include <string.h>
static displayFrame_t frame;
static uint8_t scq[256], downq[256];
static unsigned kh, kt;
static bool menu = true;
static bool audio_ready;
static int mx, my;
void port_game_stats(void) {
  vflash_stats_t io;
  vflash_stats(&io,1);
  port_log("PXA DUKE cache_used=%u cache_free=%d",
           INTERNAL_FLASH_ARENA_SIZE-internalFlashSpaceLeft(),internalFlashSpaceLeft());
  port_log("PXA IO hit=%llu miss=%llu read_us=%llu nvm_reads=%llu nvm_us=%llu cache_bytes=%u",
    (unsigned long long)io.hits,(unsigned long long)io.misses,
    (unsigned long long)io.read_us,(unsigned long long)io.nvm_reads,
    (unsigned long long)io.nvm_read_us,io.cache_bytes);
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
  *a = *b = 0;
  *c = mx;
  *d = my;
  mx = my = 0;
}
void inputSetMenuMode(bool m) { menu = m; }
bool inputIsMenuMode(void) { return menu; }
void inputPostKeyEvent(uint8_t sc, bool down) {
  unsigned next = (kh + 1) & 255;
  if (next != kt) {
    scq[kh] = sc;
    downq[kh] = down;
    kh = next;
  }
}
bool inputGetKeyEvent(uint8_t *sc, bool *down) {
  if (kh == kt)
    return false;
  *sc = scq[kt];
  *down = downq[kt];
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
  int sc = 0;
  switch (key) {
  case 27:
    sc = 1;
    break;
  case 13:
    sc = 28;
    break;
  case 128:
  case 'w':
    sc = 0x48;
    break;
  case 129:
  case 's':
    sc = 0x50;
    break;
  case 130:
    sc = 0x4b;
    break;
  case 131:
    sc = 0x4d;
    break;
  case 'a':
    sc = menu ? 0x4b : 0x10;
    break;
  case 'd':
    sc = menu ? 0x4d : 0x12;
    break;
  case ' ':
    sc = menu ? 28 : 0x2d;
    break;
  case 'e':
    sc = 0x39;
    break;
  case 'q':
    sc = 0x19;
    break;
  case 200:
    sc = menu ? 28 : 0x1d;
    break;
  }
  if (key >= 0 && key < 256)
    pressed[key] = sc;
  if (sc)
    inputPostKeyEvent(sc, down);
}
void port_game_motion(int x, int y) {
  mx += x * 3;
  my += y * 3;
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
  while (played != end) {
    int16_t s[2] = {(audioBuffer[played * 2] - 128) * 256,
                    (audioBuffer[played * 2 + 1] - 128) * 256};
    port_pcm(s, 1, 11025, 2);
    played = (played + 1) & (AUDIO_BUFFER_LENGTH - 1);
  }
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
