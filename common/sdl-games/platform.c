/* SDL2 platform integration for the native PXA event port. GPL-3.0-or-later.
 * SDL's renderer, pixels, codecs and event semantics remain upstream code. */
#include <SDL.h>
#include <SDL_mixer.h>
#include <sys/stat.h>
#include "port.h"
#include "pxa_ui.h"
#include "pxa_window.h"
#include "pxa_audio.h"
#include "rpc.h"
#include "sdl_platform.h"

extern int pxa_sdl_game_main(int argc, char **argv);
extern int __real_SDL_InitSubSystem(Uint32 flags);
extern void __real_SDL_QuitSubSystem(Uint32 flags);
extern Uint32 __real_SDL_WasInit(Uint32 flags);
extern SDL_Window *__real_SDL_CreateWindow(const char*,int,int,int,int,Uint32);
extern SDL_Renderer *__real_SDL_CreateRenderer(SDL_Window*,int,Uint32);
extern int __real_SDL_UpdateWindowSurface(SDL_Window*);
extern int __real_SDL_UpdateWindowSurfaceRects(SDL_Window*,const SDL_Rect*,int);
extern void __real_SDL_RenderPresent(SDL_Renderer*);
extern int __real_SDL_WaitEventTimeout(SDL_Event*,int);

static SDL_Window *game_window;
static Uint32 virtual_subsystems;
static Uint8 keyboard[SDL_NUM_SCANCODES];
static Uint8 pad[SDL_CONTROLLER_BUTTON_MAX];
static SDL_AudioSpec playback;
static int audio_paused=1,audio_busy;
static uint64_t audio_due,perf_since,perf_draw_us;
static unsigned perf_frames,perf_audio_frames;
static uint64_t frame_due;
static unsigned surface_update_depth;
static int rendering_present;
static int mouse_x,mouse_y,mouse_active;
static unsigned mouse_pointer,mouse_button;
static Uint32 mouse_buttons;
#ifndef PORT_SDL_FPS
#define PORT_SDL_FPS 60
#endif
static const Uint32 virtual_mask = SDL_INIT_TIMER;
static SDL_Joystick *virtual_pad;
static int game_finished;
int port_game_finished(void) { return game_finished; }

int __wrap_SDL_InitSubSystem(Uint32 flags);

void port_game_main(void) {
  char *argv[] = {"game",NULL};
  int result = pxa_sdl_game_main(1,argv);
  port_log("PXA SDL game returned status=%d",result);
  if(result){char message[256];snprintf(message,sizeof(message),"Game exited with status %d: %s",result,SDL_GetError());port_error(message);}
  /* Returning releases the retained native main call. The queued back event
   * can then return UNHANDLED through the exported callback to the Host. */
  game_finished=1;audio_paused=1;
  pxa_window_request_back();
}
int port_game_in_menu(void) { return 0; }
void port_game_motion(int x,int y) { (void)x; (void)y; }
void port_game_pointer(int x,int y,unsigned phase,unsigned buttons,unsigned id) {
  if (!game_window) return;
  if(phase==PXA_POINTER_DOWN) {
    if(mouse_active)return;
    mouse_active=1;mouse_pointer=id;mouse_button=(buttons&2)?SDL_BUTTON_RIGHT:SDL_BUTTON_LEFT;
  } else if(!mouse_active || mouse_pointer!=id)return;
  int w,h;SDL_GetWindowSize(game_window,&w,&h);x=x*w/320;y=y*h/240;
  SDL_Event event={0};
  if (phase==PXA_POINTER_MOVE || phase==PXA_POINTER_DOWN) {
    event.type=SDL_MOUSEMOTION; event.motion.windowID=SDL_GetWindowID(game_window);
    event.motion.x=x; event.motion.y=y;
    event.motion.xrel=x-mouse_x;event.motion.yrel=y-mouse_y;
    SDL_PushEvent(&event);
  }
  mouse_x=x;mouse_y=y;
  if (phase!=PXA_POINTER_MOVE) {
    memset(&event,0,sizeof(event));
    event.type=phase==PXA_POINTER_DOWN?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
    event.button.windowID=SDL_GetWindowID(game_window);
    event.button.button=mouse_button;
    event.button.state=phase==PXA_POINTER_DOWN?SDL_PRESSED:SDL_RELEASED;
    event.button.x=x; event.button.y=y;
    if(phase==PXA_POINTER_DOWN)mouse_buttons|=SDL_BUTTON(mouse_button);
    else {mouse_buttons&=~SDL_BUTTON(mouse_button);mouse_active=0;}
    SDL_PushEvent(&event);
  }
}
Uint32 __wrap_SDL_GetMouseState(int *x,int *y) { if(x)*x=mouse_x;if(y)*y=mouse_y;return mouse_buttons; }
void port_game_reset_motion(void) {
  if(!mouse_active)return;
  SDL_Event event={0};event.type=SDL_MOUSEBUTTONUP;event.button.button=mouse_button;
  event.button.windowID=game_window?SDL_GetWindowID(game_window):0;
  event.button.x=mouse_x;event.button.y=mouse_y;SDL_PushEvent(&event);
  mouse_buttons=0;mouse_active=0;
}
void port_game_key(int key,int down) {
  static const Uint8 buttons[12] = {
    SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_BACK,SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
  };
  SDL_Event event={0};
  if (key>=224 && key<236) {
    int button=buttons[key-224]; pad[button]=!!down;
    if (virtual_pad) {
      SDL_JoystickSetVirtualButton(virtual_pad,button,!!down);
#ifdef PORT_SDL_JOYSTICK_AXES
      SDL_JoystickSetVirtualAxis(virtual_pad,0,(pad[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]-pad[SDL_CONTROLLER_BUTTON_DPAD_LEFT])*32767);
      SDL_JoystickSetVirtualAxis(virtual_pad,1,(pad[SDL_CONTROLLER_BUTTON_DPAD_DOWN]-pad[SDL_CONTROLLER_BUTTON_DPAD_UP])*32767);
#endif
      SDL_JoystickUpdate();
    }
    return;
  } else {
    SDL_Keycode symbol=key==128?SDLK_UP:key==129?SDLK_DOWN:key==130?SDLK_LEFT:key==131?SDLK_RIGHT:key;
    SDL_Scancode scan=SDL_GetScancodeFromKey(symbol);
    if (scan>=0 && scan<SDL_NUM_SCANCODES) keyboard[scan]=!!down;
    event.type=down?SDL_KEYDOWN:SDL_KEYUP;
    event.key.state=!!down; event.key.keysym.scancode=scan; event.key.keysym.sym=symbol;
  }
  event.common.timestamp=(Uint32)(port_time_us()/1000);
  SDL_PushEvent(&event);
}

int __wrap_SDL_Init(Uint32 flags) { return __wrap_SDL_InitSubSystem(flags); }
int __wrap_SDL_InitSubSystem(Uint32 flags) {
  virtual_subsystems |= flags & virtual_mask;
  SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");
  SDL_SetHint(SDL_HINT_AUDIODRIVER,"dummy");
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION,"0");
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
  int result=__real_SDL_InitSubSystem(flags & ~virtual_mask);
  if (result<0) port_log("PXA SDL init flags=%u error=%s",flags,SDL_GetError());
  if (!result && (flags & (SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER)) && !virtual_pad) {
    SDL_VirtualJoystickDesc desc={0};
    desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes=6;desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;desc.nhats=1;
    desc.name="PXA Controller";
    int index=SDL_JoystickAttachVirtualEx(&desc);
    if (index<0 || !(virtual_pad=SDL_JoystickOpen(index))) port_error(SDL_GetError());
    for (int i=0;i<SDL_CONTROLLER_BUTTON_MAX;++i) SDL_JoystickSetVirtualButton(virtual_pad,i,pad[i]);
    SDL_JoystickUpdate();
  }
  return result;
}
void __wrap_SDL_QuitSubSystem(Uint32 flags) {
  virtual_subsystems &= ~flags;
  if ((flags&SDL_INIT_JOYSTICK) && virtual_pad) {
    SDL_JoystickClose(virtual_pad);virtual_pad=NULL;
  }
  __real_SDL_QuitSubSystem(flags & ~virtual_mask);
}
Uint32 __wrap_SDL_WasInit(Uint32 flags) {
  Uint32 initialized=__real_SDL_WasInit(0)|virtual_subsystems;
  return flags?initialized&flags:initialized;
}
SDL_Window *__wrap_SDL_CreateWindow(const char *title,int x,int y,int w,int h,Uint32 flags) {
  (void)w; (void)h;
  flags &= ~(SDL_WINDOW_OPENGL|SDL_WINDOW_VULKAN|SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_RESIZABLE);
  game_window=__real_SDL_CreateWindow(title,x,y,320,240,flags);
  if (!game_window) port_error(SDL_GetError());
  port_log("PXA SDL video=%s",SDL_GetCurrentVideoDriver());
  return game_window;
}
SDL_Renderer *__wrap_SDL_CreateRenderer(SDL_Window *window,int index,Uint32 flags) {
  (void)index;
  flags &= ~(SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  SDL_Renderer *r=__real_SDL_CreateRenderer(window,-1,flags|SDL_RENDERER_SOFTWARE);
  if (!r) port_error(SDL_GetError());
  return r;
}
void port_sdl_present_rgb565(const uint16_t *pixels,int width,int height,int pitch) {
  uint64_t began=port_time_us();
  port_frame_rgb565(pixels,width,height,pitch);
  perf_draw_us+=port_time_us()-began; ++perf_frames;
}
static void submit_window(SDL_Window *window) {
  SDL_Surface *s=SDL_GetWindowSurface(window);
  if (!s) return;
  if (s->format->format!=SDL_PIXELFORMAT_RGB565) port_error("SDL framebuffer must be RGB565");
  port_sdl_present_rgb565(s->pixels,s->w,s->h,s->pitch);
}
int __wrap_SDL_UpdateWindowSurface(SDL_Window *window) {
  ++surface_update_depth;
  int result=__real_SDL_UpdateWindowSurface(window);
  --surface_update_depth;
  if (!result && !surface_update_depth && !rendering_present) submit_window(window);
  return result;
}
int __wrap_SDL_UpdateWindowSurfaceRects(SDL_Window *window,const SDL_Rect *rects,int count) {
  ++surface_update_depth;
  int result=__real_SDL_UpdateWindowSurfaceRects(window,rects,count);
  --surface_update_depth;
  if (!result && !surface_update_depth && !rendering_present) submit_window(window);
  return result;
}
void __wrap_SDL_RenderPresent(SDL_Renderer *renderer) {
  rendering_present=1;
  __real_SDL_RenderPresent(renderer);
  rendering_present=0;
  if (game_window) submit_window(game_window);
  port_sdl_pace();
}
void port_sdl_pace(void) {
  port_game_audio(); port_poll_input();
#if PORT_SDL_FPS > 0
  /* SDL's desktop VSync flag cannot pace a PXA software window. Preserve
   * frame-count based games' original timing without busy spinning. */
  const uint64_t interval=1000000/PORT_SDL_FPS;
  uint64_t now=port_time_us();
  if (!frame_due)frame_due=now;
  else if(now>frame_due+interval*4)frame_due=now-interval;
  frame_due+=interval;
  while ((now=port_time_us())<frame_due) {
    port_game_audio();port_sleep((frame_due-now)>5000?5:1);
  }
#endif
}
Uint32 __wrap_SDL_GetTicks(void) { return (Uint32)(port_time_us()/1000); }
Uint64 __wrap_SDL_GetTicks64(void) { return port_time_us()/1000; }
Uint64 __wrap_SDL_GetPerformanceCounter(void) { return port_time_us(); }
Uint64 __wrap_SDL_GetPerformanceFrequency(void) { return 1000000; }
void __wrap_SDL_Delay(Uint32 ms) {
  uint64_t end=port_time_us()+(uint64_t)ms*1000;
  do { port_game_audio(); port_sleep(ms>5?5:ms); } while(port_time_us()<end);
}
int __wrap_SDL_PollEvent(SDL_Event *event) {
  port_poll_input(); port_game_audio();
  /* SDL_PollEvent delegates to SDL_WaitEventTimeout internally. Calling it
   * here would re-enter our wait wrapper and recurse indefinitely. */
  int result=__real_SDL_WaitEventTimeout(event,0);
  if (!result) port_sleep(1);
  uint64_t now=port_time_us();
  if (!perf_since) perf_since=now;
  if (now-perf_since>=5000000) {
    port_log("PXA SDL frames=%u elapsed_us=%llu submit_us=%llu audio_frames=%u linear_bytes=%u",
        perf_frames,(unsigned long long)(now-perf_since),(unsigned long long)perf_draw_us,
        perf_audio_frames,(unsigned)__builtin_wasm_memory_size(0)*65536u);
    uint8_t query[64];uint32_t bytes;
    if(port_audio_session() && pxa_audio_build_session_request(query,sizeof(query),PXA_AUDIO_QUERY_STATE,1,port_audio_session(),&bytes)) {
      const pxa_event_t *event=port_rpc_packet(query,bytes);pxa_audio_state_result_t state;
      if(event && pxa_audio_parse_state(event,event->token,&state) && !state.status)
        port_log("PXA SDL PCM submitted=%llu accepted=%llu queued=%u",
          (unsigned long long)state.submitted_samples,(unsigned long long)state.accepted_samples,state.queued_samples);
    }
    perf_frames=perf_audio_frames=0; perf_draw_us=0; perf_since=now;
  }
  return result;
}
int __wrap_SDL_WaitEventTimeout(SDL_Event *event,int timeout) {
  uint64_t end=port_time_us()+(uint64_t)(timeout<0?0:timeout)*1000;
  do { if(__wrap_SDL_PollEvent(event))return 1; } while(timeout<0 || port_time_us()<end);
  return 0;
}
int __wrap_SDL_WaitEvent(SDL_Event *event) { return __wrap_SDL_WaitEventTimeout(event,-1); }
const Uint8 *__wrap_SDL_GetKeyboardState(int *count) { if(count)*count=SDL_NUM_SCANCODES; return keyboard; }
int __wrap_SDL_SetWindowFullscreen(SDL_Window *w,Uint32 flags) { (void)w;(void)flags;return 0; }
int __wrap_SDL_ShowSimpleMessageBox(Uint32 flags,const char *title,const char *message,SDL_Window *w) {
  (void)flags;(void)w;port_log("%s: %s",title,message);return 0;
}
char *__wrap_SDL_GetBasePath(void) { return SDL_strdup("/data/"); }
char *__wrap_SDL_GetPrefPath(const char *org,const char *app) {
  (void)org;(void)app;mkdir("/data/user",0700);return SDL_strdup("/data/user/");
}
SDL_AudioDeviceID __wrap_SDL_OpenAudioDevice(const char *name,int capture,
    const SDL_AudioSpec *desired,SDL_AudioSpec *obtained,int changes) {
  (void)name;
  if (capture || !desired || !desired->callback || playback.callback) {
    SDL_SetError("PXA supports one callback playback device"); return 0;
  }
  playback=*desired;
  if(changes&SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) playback.freq=22050;
  if(changes&SDL_AUDIO_ALLOW_FORMAT_CHANGE) playback.format=AUDIO_S16SYS;
  if (playback.format!=AUDIO_S16SYS || playback.channels<1 || playback.channels>2 ||
      playback.freq<8000 || playback.freq>48000) {
    memset(&playback,0,sizeof(playback));SDL_SetError("PXA audio requires S16 mono/stereo, 8–48 kHz");return 0;
  }
  playback.samples=1024; playback.silence=0; playback.size=playback.samples*playback.channels*2;
  if(obtained)*obtained=playback;
  audio_paused=1;audio_due=port_time_us();return 1;
}
void __wrap_SDL_CloseAudioDevice(SDL_AudioDeviceID id) { (void)id;memset(&playback,0,sizeof(playback));audio_paused=1; }
void __wrap_SDL_PauseAudioDevice(SDL_AudioDeviceID id,int pause) { (void)id;audio_paused=pause;audio_due=port_time_us(); }
void __wrap_SDL_LockAudioDevice(SDL_AudioDeviceID id) { (void)id; }
void __wrap_SDL_UnlockAudioDevice(SDL_AudioDeviceID id) { (void)id; }
int __wrap_SDL_OpenAudio(SDL_AudioSpec *wanted,SDL_AudioSpec *obtained) {
  return __wrap_SDL_OpenAudioDevice(NULL,0,wanted,obtained,0)?0:-1;
}
void __wrap_SDL_CloseAudio(void) { __wrap_SDL_CloseAudioDevice(1); }
void __wrap_SDL_PauseAudio(int paused) { __wrap_SDL_PauseAudioDevice(1,paused); }
void __wrap_SDL_LockAudio(void) {}
void __wrap_SDL_UnlockAudio(void) {}
void port_game_audio(void) {
  if(audio_paused || !playback.callback || audio_busy)return;
  audio_busy=1;
  uint64_t now=port_time_us();
  if(now>audio_due+80000)audio_due=now;
  static int16_t block[960*2];
  /* Ten millisecond pulls keep latency bounded; music decoders stream from
   * the asset instead of retaining the entire song in linear memory. */
  unsigned frames=playback.freq/100;
  for(int i=0;i<3 && now>=audio_due;++i) {
    memset(block,0,frames*playback.channels*2);
    playback.callback(playback.userdata,(Uint8*)block,frames*playback.channels*2);
    audio_due+=(uint64_t)frames*1000000/playback.freq;
    port_pcm(block,frames,playback.freq,playback.channels);
    perf_audio_frames+=frames;now=port_time_us();
  }
  audio_busy=0;
}
