// SPDX-License-Identifier: MIT
#pragma once
#define PXA_POCKET_RESPONSIVE 1
#include "../pocket-game/runtime.hpp"
#include "layout.hpp"
namespace milky {
struct NativeImage {const char *path=nullptr;uint16_t width=0,height=0,x=0,y=0,w=0,h=0;};
struct Sprite { uint16_t width, height; const uint32_t *pixels; NativeImage large{},small{},medium{},tiny{}; };
enum class Icon : uint8_t { None,Back,Next,Pause,Lock };
void icon(Icon,Rect,uint16_t color);
void image(const Sprite &, int x, int y, int width=0, int height=0);
void rounded(int x,int y,int w,int h,uint16_t color,int radius=5);
void center(int x,int y,int w,const char *value,uint16_t color,int scale=1);
void button(int x,int y,int w,int h,const char *value,bool selected=false);
bool inside(int x,int y,int left,int top,int width,int height);
bool held(pocket::Key key);
void sounds(const char *const *paths,unsigned count);
void play(unsigned id,unsigned track=0,bool loop=false);
void stop(unsigned track=0);
// End a looping recording after its current iteration, retaining the laugh tail.
void finish(unsigned track=0);
void volume(unsigned effects,unsigned music);
unsigned effects_volume();
unsigned music_volume();
void audio_test();
void panel(Rect,uint16_t color=0,bool shadow=true);
void label(Rect,const char *,uint16_t color,int scale=1,unsigned align=0);
void portrait(const Sprite &,Rect);
void thumbnail(const Sprite &,Rect);
void header(const char *title,bool back=false);
void metric(Rect,const char *caption,uint32_t value,bool accent=false);
void action(Rect,const char *caption,bool primary=false,Icon symbol=Icon::None);
void settings_draw();
// Returns 1 for a changed setting, 2 for Back, zero otherwise.
int settings_touch(int x,int y);
void menu_draw(bool paused,int selected,bool saving);
void configure_screen(int width,int height,uint32_t density,const uint32_t *insets,unsigned shape,const uint32_t *corners);
constexpr uint16_t CREAM=pocket::rgb(251,248,233), BROWN=pocket::rgb(73,63,40);
constexpr uint16_t HONEY=pocket::rgb(245,197,77), PAPER=pocket::rgb(255,254,247);
constexpr uint16_t SAGE=pocket::rgb(74,113,84), SOFT=pocket::rgb(238,225,187);
constexpr uint16_t SUBTLE=pocket::rgb(129,132,111), BORDER=pocket::rgb(226,218,193);
// Explicit little-endian save utilities, independent of compiler layouts.
inline void put32(uint8_t *p,uint32_t v){for(int i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
inline uint32_t get32(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline uint32_t hash(const uint8_t *p,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;++i)h=(h^p[i])*16777619u;return h;}
}
