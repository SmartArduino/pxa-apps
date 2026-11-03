/* SPDX-License-Identifier: MIT */
#include "jy_sdl.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static jy_sdl_ops host;
static SDL_Surface *video;
static Uint32 initialized;
static char error_text[384],caption[128],icon_caption[128];
static SDL_PixelFormat desktop={.BitsPerPixel=32,.BytesPerPixel=4,.Rloss=0,.Gloss=0,.Bloss=0,.Aloss=8,.Rshift=16,.Gshift=8,.Bshift=0,.Rmask=0xff0000,.Gmask=0xff00,.Bmask=0xff,.alpha=255};
static SDL_VideoInfo video_info;
static SDL_Event events[128],repeat_event;
static unsigned event_count;
static Uint8 event_enabled[SDL_NUMEVENTS],keys[SDLK_LAST];
static SDL_EventFilter event_filter;
static SDLMod modifiers;
static int unicode_mode,repeat_delay,repeat_interval,repeat_active;
static uint64_t repeat_next;
void SDL_SetError(const char *format,...){va_list args;
 va_start(args,format);
 vsnprintf(error_text,sizeof error_text,format?format:"",args);
 va_end(args);
 }
char *SDL_GetError(void){return error_text;
 }
void SDL_ClearError(void){error_text[0]=0;
 }
void SDL_Error(SDL_errorcode code){static const char *messages[]={"Out of memory","Read error","Write error","Seek error","Unsupported operation"};
 SDL_SetError("%s",(unsigned)code<5?messages[code]:"SDL software backend error");
 }
int jy_sdl_fail(const char *text){SDL_SetError("%s",text);
 return -1;
 }
int jy_sdl_bind(const jy_sdl_ops *ops){
 if(!ops||!ops->now_us||!ops->delay_us||!ops->present)return jy_sdl_fail("Software video requires clock, real delay and presentation callbacks");
 if(video)return jy_sdl_fail("Close software video before replacing its host callbacks");
 host=*ops;
 event_count=0;
 event_filter=NULL;
 repeat_active=0;
 modifiers=KMOD_NONE;
 memset(keys,0,sizeof keys);
 memset(event_enabled,1,sizeof event_enabled);
 SDL_ClearError();
 return 0;
}
Uint32 SDL_GetTicks(void){if(!host.now_us){SDL_SetError("No monotonic clock bound");
  return 0;
  }return (Uint32)(host.now_us(host.context)/1000);
 }
void SDL_Delay(Uint32 milliseconds){if(!host.delay_us){SDL_SetError("No cooperative delay callback bound");
  return;
  }host.delay_us(host.context,(uint64_t)milliseconds*1000);
 }
int SDL_InitSubSystem(Uint32 flags){
 if(flags&SDL_INIT_AUDIO)return jy_sdl_fail("No audio device in this PXA software backend");
 if(flags&~(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE))return jy_sdl_fail("Requested SDL subsystem is unavailable");
 if((flags&(SDL_INIT_VIDEO|SDL_INIT_TIMER))&&(!host.now_us||!host.delay_us||!host.present))return jy_sdl_fail("SDL host callbacks are not bound");
 initialized|=flags&(SDL_INIT_VIDEO|SDL_INIT_TIMER);
 return 0;
}
int SDL_Init(Uint32 flags){return SDL_InitSubSystem(flags);
 }
void SDL_QuitSubSystem(Uint32 flags){if((flags&SDL_INIT_VIDEO)&&video){SDL_Surface *old=video;
  video=NULL;
  SDL_FreeSurface(old);
  }initialized&=~flags;
 }
Uint32 SDL_WasInit(Uint32 flags){return flags?initialized&flags:initialized;
 }
void SDL_Quit(void){SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_TIMER);
 event_count=0;
 repeat_active=0;
 memset(keys,0,sizeof keys);
 }
int SDL_VideoInit(const char *driver,Uint32 flags){if(driver&&*driver&&strcmp(driver,"pxa-software"))return jy_sdl_fail("Only the PXA software video driver is supported");
 return SDL_InitSubSystem(flags|SDL_INIT_VIDEO);
 }
void SDL_VideoQuit(void){SDL_QuitSubSystem(SDL_INIT_VIDEO);
 }
char *SDL_VideoDriverName(char *name,int size){if(!name||size<=0)return NULL;
 snprintf(name,(size_t)size,"pxa-software");
 return name;
 }
const SDL_version *SDL_Linked_Version(void){static const SDL_version version={1,2,15};
 return &version;
 }
SDL_Surface *SDL_GetVideoSurface(void){return video;
 }
const SDL_VideoInfo *SDL_GetVideoInfo(void){memset(&video_info,0,sizeof video_info);
 video_info.wm_available=1;
 video_info.blit_sw=1;
 video_info.blit_sw_CC=1;
 video_info.blit_sw_A=1;
 video_info.blit_fill=1;
 video_info.vfmt=video?video->format:&desktop;
 video_info.current_w=video?video->w:640;
 video_info.current_h=video?video->h:480;
 return &video_info;
 }
int SDL_VideoModeOK(int w,int h,int bpp,Uint32 flags){if(w<=0||h<=0||w>4096||h>4096||(flags&SDL_OPENGL))return 0;
 return bpp==0||bpp==32?32:bpp==8||bpp==16||bpp==24?bpp:0;
 }
SDL_Rect **SDL_ListModes(SDL_PixelFormat *format,Uint32 flags){(void)format;
 return flags&SDL_OPENGL?NULL:(SDL_Rect **)(intptr_t)-1;
 }
SDL_Surface *SDL_SetVideoMode(int w,int h,int bpp,Uint32 flags){
 if(!(initialized&SDL_INIT_VIDEO)){jy_sdl_fail("Video subsystem is not initialized");
  return NULL;
  }int depth=SDL_VideoModeOK(w,h,bpp,flags);
 if(!depth){jy_sdl_fail("Unsupported software video mode");
  return NULL;
  }
 SDL_Surface *next=SDL_CreateRGBSurface(0,w,h,depth,0,0,0,0);
 if(!next)return NULL;
 if(video)SDL_FreeSurface(video);
 video=next;
 video->flags|=flags&SDL_FULLSCREEN;
 return video;
}
void jy_sdl_surface_freed(SDL_Surface *surface){if(video==surface)video=NULL;
 }
static int present(SDL_Surface *surface,const SDL_Rect *rects,int count){if(!surface||!host.present)return jy_sdl_fail("No software frame presentation callback");
 int result=host.present(host.context,surface,rects,count);
 return result<0?jy_sdl_fail("Host rejected software frame presentation"):0;
 }
void SDL_UpdateRects(SDL_Surface *surface,int count,SDL_Rect *rects){if(count<0||(!rects&&count)){SDL_SetError("Invalid dirty-rectangle list");
  return;
  }if(count)present(surface,rects,count);
 }
void SDL_UpdateRect(SDL_Surface *surface,Sint32 x,Sint32 y,Uint32 w,Uint32 h){
 if(!surface){SDL_SetError("Null screen surface");
  return;
  }if(x<0||y<0||x>surface->w||y>surface->h){SDL_SetError("Invalid update rectangle");
  return;
  }if(!w)w=(Uint32)(surface->w-x);
 if(!h)h=(Uint32)(surface->h-y);
 if(w>(Uint32)(surface->w-x))w=(Uint32)(surface->w-x);
 if(h>(Uint32)(surface->h-y))h=(Uint32)(surface->h-y);
 SDL_Rect rectangle={(Sint16)x,(Sint16)y,(Uint16)w,(Uint16)h};
 if(w&&h)present(surface,&rectangle,1);
}
int SDL_Flip(SDL_Surface *surface){if(!surface)return jy_sdl_fail("Null screen surface");
 SDL_Rect all={0,0,(Uint16)surface->w,(Uint16)surface->h};
 return present(surface,&all,1);
 }
static int enqueue(const SDL_Event *event,int filter){
 if(!event||event->type>=SDL_NUMEVENTS)return jy_sdl_fail("Invalid SDL1 event");
 if(!event_enabled[event->type])return 0;
 if(filter&&event_filter&&!event_filter(event))return 0;
 if(event_count==128)return jy_sdl_fail("SDL1 event queue is full");
 events[event_count++]=*event;
 return 0;
}
int jy_sdl_push_event(const SDL_Event *event){
 if(!event)return jy_sdl_fail("Null host event");
 if(event->type==SDL_KEYDOWN||event->type==SDL_KEYUP){int sym=event->key.keysym.sym;
  if(sym>=0&&sym<SDLK_LAST)keys[sym]=event->type==SDL_KEYDOWN;
  modifiers=event->key.keysym.mod;
  if(event->type==SDL_KEYDOWN){repeat_event=*event;
   repeat_active=repeat_delay>0;
   repeat_next=(uint64_t)SDL_GetTicks()+(unsigned)repeat_delay;
   }
  else if(repeat_active&&event->key.keysym.sym==repeat_event.key.keysym.sym)repeat_active=0;
 }return enqueue(event,1);
}
void SDL_PumpEvents(void){
 if(host.poll_event){for(unsigned n=0;n<64;++n){SDL_Event event;
   int status=host.poll_event(host.context,&event);
   if(status<=0){if(status<0)SDL_SetError("Host event poll failed");
    break;
    }if(jy_sdl_push_event(&event)<0)break;
   }}
 if(repeat_active&&repeat_interval>0){uint64_t now=SDL_GetTicks();
  unsigned count=0;
  while(now>=repeat_next&&count++<16){enqueue(&repeat_event,1);
   repeat_next+=(unsigned)repeat_interval;
   }if(now>=repeat_next)repeat_next=now+(unsigned)repeat_interval;
  }
}
int SDL_PollEvent(SDL_Event *event){SDL_PumpEvents();
 if(!event_count)return 0;
 if(event){*event=events[0];
  memmove(events,events+1,(--event_count)*sizeof *events);
  }return 1;
 }
int SDL_WaitEvent(SDL_Event *event){if(!host.delay_us)return jy_sdl_fail("WaitEvent requires a real delay callback");
 for(;;){if(SDL_PollEvent(event))return 1;
  SDL_Delay(1);
  }}
int SDL_PushEvent(SDL_Event *event){return enqueue(event,0);
 }
void SDL_SetEventFilter(SDL_EventFilter filter){event_filter=filter;
 }
SDL_EventFilter SDL_GetEventFilter(void){return event_filter;
 }
Uint8 SDL_EventState(Uint8 type,int state){if(type>=SDL_NUMEVENTS)return SDL_IGNORE;
 Uint8 previous=event_enabled[type];
 if(state==SDL_IGNORE||state==SDL_ENABLE)event_enabled[type]=(Uint8)state;
 return previous;
 }
int SDL_PeepEvents(SDL_Event *output,int count,SDL_eventaction action,Uint32 mask){
 if(count<0||(!output&&count))return jy_sdl_fail("Invalid event output buffer");
 int found=0;
 if(action==SDL_ADDEVENT){for(int n=0;n<count;++n)if(enqueue(&output[n],0)<0)return found;
  else ++found;
  return found;
  }
 for(unsigned n=0;n<event_count&&found<count;){if(mask&(UINT32_C(1)<<events[n].type)){output[found++]=events[n];
   if(action==SDL_GETEVENT){memmove(events+n,events+n+1,(--event_count-n)*sizeof *events);
    continue;
    }}++n;
  }return found;
}
Uint8 *SDL_GetKeyState(int *count){if(count)*count=SDLK_LAST;
 return keys;
 }
SDLMod SDL_GetModState(void){return modifiers;
 }
void SDL_SetModState(SDLMod mod){modifiers=mod;
 }
int SDL_EnableKeyRepeat(int delay,int interval){if(delay<0||interval<0||(delay&&interval==0))return jy_sdl_fail("Invalid key repeat timing");
 repeat_delay=delay;
 repeat_interval=interval;
 repeat_active=0;
 return 0;
 }
void SDL_GetKeyRepeat(int *delay,int *interval){if(delay)*delay=repeat_delay;
 if(interval)*interval=repeat_interval;
 }
int SDL_EnableUNICODE(int enabled){int previous=unicode_mode;
 if(enabled>=0)unicode_mode=!!enabled;
 return previous;
 }
char *SDL_GetKeyName(SDLKey key){static char one[2];
 switch(key){case SDLK_UP:return "up";
  case SDLK_DOWN:return "down";
  case SDLK_LEFT:return "left";
  case SDLK_RIGHT:return "right";
  case SDLK_ESCAPE:return "escape";
  case SDLK_RETURN:return "return";
  case SDLK_SPACE:return "space";
  default:if(key>=32&&key<127){one[0]=(char)key;
   one[1]=0;
   return one;
   }return "unknown key";
  }}
void SDL_WM_SetCaption(const char *title,const char *icon){snprintf(caption,sizeof caption,"%s",title?title:"");
 snprintf(icon_caption,sizeof icon_caption,"%s",icon?icon:"");
 }
void SDL_WM_GetCaption(char **title,char **icon){if(title)*title=caption;
 if(icon)*icon=icon_caption;
 }
int SDL_WM_ToggleFullScreen(SDL_Surface *surface){if(!surface)return 0;
 surface->flags^=SDL_FULLSCREEN;
 return 1;
 }
