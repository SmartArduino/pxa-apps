// SPDX-License-Identifier: MIT
// All visible lettering and digits are generated artwork, never system fonts.
#include "art_layout.hpp"
#include "expansion_art.hpp"
#include "text_bounds.hpp"
#include "actor_layout.hpp"
#include "carrot_visual.hpp"
#include "core.hpp"
#include "game_audio.hpp"
#include "surface_output.hpp"
#include "pxa_canvas.h"
#include "pxa_clock.h"
#include "pxa_storage.h"
#include "pxa_window.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#ifdef CARROT_PROFILE
uint64_t launchStarted=0,lastSceneRevision=0;
bool firstFrameLogged=false;
#include <time.h>
#endif
namespace {
using namespace carrot;
Game game;
CarrotVisual carrotFeedback;
struct {
  float width = 480, height = 480, safe_left = 0, safe_top = 0, safe_right = 0,
        safe_bottom = 0;
} screen;
AudioPlayer audio;
SurfaceOutput output;
size_t staticBytes=0;
size_t hudBegin=0,hudEnd=0,hudTopBytes=0;
uint64_t hudRevision=0;
uint8_t previousDraws[49152];
size_t previousSize=0;
bool displayValid=false;
pxa_canvas_handle_t canvasStream = 0;
bool streamRequested = false;
bool fullRedraw = false;
#ifdef CARROT_PROFILE
struct {
  uint64_t epoch = 0, build = 0, submit = 0, simulation = 0, area = 0;
  uint64_t previous=0;
  std::array<uint32_t,256> intervals{};
  unsigned intervalCount=0;
  unsigned frames = 0, skipped = 0, full = 0;
} profiling;
uint64_t monotonicUs() {
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
void profileTick(uint64_t now) {
  if (!profiling.epoch)
    profiling.epoch = now;
  if (now - profiling.epoch < 5000000)
    return;
  char message[400];
  unsigned n = std::max(1u, profiling.frames);
  auto samples=profiling.intervals;
  unsigned count=std::min(256u,profiling.intervalCount);
  std::sort(samples.begin(),samples.begin()+count);
  std::snprintf(
      message, sizeof(message),
      "carrot perf mode=%s fps10=%llu build_us=%llu submit_us=%llu "
      "dirty_pct=%llu full=%u skipped=%u frames=%u sim_us=%llu units=%d p95_us=%u cache_hits=%llu cache_misses=%llu source_misses=%llu cache_evictions=%llu cache_moved=%llu",
      "surface",
      (unsigned long long)(profiling.frames * 10000000ULL /
                           (now - profiling.epoch)),
      (unsigned long long)(profiling.build / n),
      (unsigned long long)(profiling.submit / n),
      (unsigned long long)(profiling.area * 100 /
                           (n * uint64_t(screen.width * screen.height))),
      profiling.full, profiling.skipped, profiling.frames,
      (unsigned long long)(profiling.simulation / n), game.countEnemies(),
      count?samples[(count-1)*95/100]:0,
      (unsigned long long)output.painter.hits,(unsigned long long)output.painter.misses,
      (unsigned long long)output.painter.sourceMisses,(unsigned long long)output.painter.cacheEvictions(),(unsigned long long)output.painter.cacheMovedBytes());
  pxa_log_write(2, message);
#ifdef CARROT_RASTER_TIMING
  output.painter.reportRaster(profiling.frames);
#endif
  output.query();
  profiling = {};
  profiling.epoch = now;
}
#endif
pxa_canvas_frame_t frame;
uint8_t draws[49152], packet[4096], initialized = 0, best[Levels]{},
                                    helpSeen = 0;
uint32_t generation = 0;
uint64_t lastTime = 0, anim = 0;
float scale = 1, W = 480, H = 480, ox = 0, oy = 0, fx = 16, fy = 58, fw = 448,
      fh = 332, accumulator = 0, paintTime = 0, toastTime = 0;
enum View { Home, LevelSelect, Play, Pause, Help, Complete };
View view = Home, helpReturn = Home;
#ifdef CARROT_VISUAL_PROBE
int visualProbePhase=0;
#endif
int selected = -1, toast = 0, levelPage = 0, buildPage = 0,
    toastTower = 4, newlyUnlocked = 0, helpPage = 0, pendingSell=-1;
uint8_t sellPointer=0;
bool sound = true, fast = false, saveReady = false, savePending = false,
     saveDirty = false, saveRetry = false, soundChanged = false;
constexpr uint32_t Ink = 0x765033, Paper = 0xFFF3D1, Green = 0x70AE67,
                   Gold = 0xF6C957;
int px(float x) { return int(ox + x * scale + .5f); }
int py(float y) { return int(oy + y * scale + .5f); }
int sz(float s) { return std::max(1, int(s * scale + .5f)); }
uint16_t sourceId(const char* path){
  struct Cached {uint32_t hash=0;uint16_t id=0xffff;};
  static std::array<Cached,512> cache{};
  uint32_t hash=2166136261u;for(const char* p=path;*p;++p)hash=(hash^unsigned(*p))*16777619u;
  auto& entry=cache[hash&511];
  if(entry.hash==hash&&entry.id!=0xffff&&!std::strcmp(pixels::sources[entry.id].path,path))return entry.id;
  auto found=std::lower_bound(std::begin(pixels::sources),std::end(pixels::sources),path,[](const auto& a,const char* b){return std::strcmp(a.path,b)<0;});
  uint16_t id=found!=std::end(pixels::sources)&&!std::strcmp(found->path,path)?uint16_t(found-std::begin(pixels::sources)):0xffff;
  entry={hash,id};return id;
}
int imageId(uint16_t id,int x,int y,int w,int h,uint8_t alpha,uint8_t fit,bool flip=false){
  if(id==0xffff||!pxa_canvas_primitive(&frame,13,20))return 0;
  frame.data[frame.length-3]=flip?2:0;
  bool ok=pxa_put_u32(&frame,x)&&pxa_put_u32(&frame,y)&&pxa_put_u32(&frame,w)&&pxa_put_u32(&frame,h)&&pxa_put_u8(&frame,alpha)&&pxa_put_u8(&frame,fit)&&pxa_put_u16(&frame,id);
  if(ok)++frame.records;return ok;
}
int imageRecord(const char *path, int x, int y, int w, int h, uint8_t alpha,uint8_t fit) {
  return imageId(sourceId(path),x,y,w,h,alpha,fit);
}
// Private renderer record, never sent to the Host Canvas. An explicit source
// rectangle keeps animation cells independent of fractional screen density.
int regionId(uint16_t id,int x,int y,int w,int h,int sx,int sy,int sw,int sh,uint8_t alpha=255,uint8_t fit=1,bool nearest=false,bool flip=false,bool sharedEdges=false){
  if(id==0xffff||!pxa_canvas_primitive(&frame,14,28))return 0;
  frame.data[frame.length-3]=unsigned(nearest)|(unsigned(flip)<<1)|(unsigned(sharedEdges)<<2);
  bool ok=pxa_put_u32(&frame,x)&&pxa_put_u32(&frame,y)&&pxa_put_u32(&frame,w)&&pxa_put_u32(&frame,h)&&
      pxa_put_u8(&frame,alpha)&&pxa_put_u8(&frame,fit)&&pxa_put_u16(&frame,sx)&&pxa_put_u16(&frame,sy)&&pxa_put_u16(&frame,sw)&&pxa_put_u16(&frame,sh)&&
      pxa_put_u16(&frame,id);
  if(ok)++frame.records;return ok;
}
int regionRecord(const char* path,int x,int y,int w,int h,int sx,int sy,int sw,int sh,uint8_t alpha=255,uint8_t fit=1,bool nearest=false,bool flip=false,bool sharedEdges=false){return regionId(sourceId(path),x,y,w,h,sx,sy,sw,sh,alpha,fit,nearest,flip,sharedEdges);}
void image(const char *name, float x, float y, float w, float h,
           uint8_t alpha = 255, uint8_t fit = 0,bool flip=false) {
  struct Named {char name[32]{};uint16_t source=0xffff;const ArtRegion* region=nullptr;};
  static std::array<Named,512> cache{};
  uint32_t hash=2166136261u;for(const char* p=name;*p;++p)hash=(hash^unsigned(*p))*16777619u;
  auto& a=cache[hash&511];
  if(std::strcmp(a.name,name)){
    a.region=nullptr;
    const auto* r=std::lower_bound(std::begin(artRegions),std::end(artRegions),name,[](const ArtRegion& v,const char* n){return std::strcmp(v.id,n)<0;});
    char path[96];
    if(r!=std::end(artRegions)&&!std::strcmp(r->id,name)){a.region=r;std::snprintf(path,sizeof(path),"assets/ui/%s.png",r->sheet);}
    else std::snprintf(path,sizeof(path),"assets/%s.png",name);
    a.source=sourceId(path);std::snprintf(a.name,sizeof(a.name),"%s",name);
  }
  if(a.region){auto& r=*a.region;regionId(a.source,px(x),py(y),sz(w),sz(h),r.x,r.y,r.w,r.h,alpha,fit,false,flip);}
  else imageId(a.source,px(x),py(y),sz(w),sz(h),alpha,fit,flip);
}
void rect(float x, float y, float w, float h, uint32_t c, float radius = 12,
          uint8_t a = 255, float border = 0, uint32_t edge = Ink) {
  pxa_canvas_rect_rgba(&frame, px(x), py(y), sz(w), sz(h), (c << 8) | a,
                       sz(radius), border ? sz(border) : 0, (edge << 8) | 255);
}
void line(float x, float y, float x2, float y2, uint32_t c, float width = 2,
          uint8_t a = 255) {
  pxa_canvas_line_rgba(&frame, px(x), py(y), px(x2), py(y2), (c << 8) | a,
                       sz(width));
}
void ellipse(float x, float y, float w, float h, uint32_t c, uint8_t a = 255,
             float border = 0, uint32_t edge = Ink) {
  pxa_canvas_ellipse_rgba(&frame, px(x), py(y), sz(w), sz(h), (c << 8) | a,
                          border ? sz(border) : 0, (edge << 8) | 255);
}
void card(float x, float y, float w, float h, uint32_t c = Paper) {
  constexpr uint32_t palette[] = {Green, Gold, 0x81BFC1, 0xDE9A73, Paper};
  int skin = 4, bestDistance = 0x7fffffff;
  for (int i = 0; i < 5; ++i) {
    int r = int(c >> 16) - int(palette[i] >> 16);
    int g = int((c >> 8) & 255) - int((palette[i] >> 8) & 255);
    int b = int(c & 255) - int(palette[i] & 255);
    int d = r * r + g * g + b * b;
    if (d < bestDistance) {
      bestDistance = d;
      skin = i;
    }
  }
  if (skin == 4 && h > 100)
    skin = 5;
  const auto &a = artSkins[skin];
  int x0 = px(x), y0 = py(y), width = sz(w), height = sz(h);
  // Keep every skin at the button artwork's border scale. The larger panel
  // has a wider source corner; squeezing it to 12 units also thins its rim.
  float borderScale = 12.f / artSkins[0].edge;
  int corner =
      std::min({sz(a.edge * borderScale), width / 2, height / 2});
  int xs[] = {0, a.edge, a.w - a.edge, a.w},
      ys[] = {0, a.edge, a.h - a.edge, a.h};
  int dx[] = {x0, x0 + corner, x0 + width - corner, x0 + width};
  int dy[] = {y0, y0 + corner, y0 + height - corner, y0 + height};
  char path[64];
  std::snprintf(path, sizeof(path), "assets/%s.png", a.id);
  // Adjacent pieces share physical edge coordinates. Draw the generated rim
  // once; stretching an entire framed underlay produces a second contour.
  for (int row = 0; row < 3; ++row)
    for (int col = 0; col < 3; ++col) {
      int dw = dx[col + 1] - dx[col], dh = dy[row + 1] - dy[row];
      if (dw <= 0 || dh <= 0)
        continue;
      regionRecord(path,dx[col],dy[row],dw,dh,xs[col],ys[row],xs[col+1]-xs[col],ys[row+1]-ys[row],255,1,false,false,true);
    }
}
const TextBounds* textInk(const char* id){
  auto ink=std::lower_bound(std::begin(textBounds),std::end(textBounds),id,[](const TextBounds& a,const char* b){return std::strcmp(a.id,b)<0;});
  return ink!=std::end(textBounds)&&!std::strcmp(ink->id,id)?ink:nullptr;
}
void textClip(float x,float y,float w,float h){
  pxa_canvas_clip_push(&frame,px(x),py(y),std::max(1,px(x+w)-px(x)),std::max(1,py(y+h)-py(y)));
}
void label(const char *id, float x, float y, float w, float h,
           bool left = false) {
  if(w<=0||h<=0)return;
  textClip(x,y,w,h);
  if(const auto* ink=textInk(id)){
    float s=std::min(w/ink->w,h/ink->h);
    image(id,x+(left?0:(w-ink->w*s)/2)-ink->x*s,y+(h-ink->h*s)/2-ink->y*s,ink->sourceW*s,ink->sourceH*s);
    pxa_canvas_clip_pop(&frame);
    return;
  }
  auto draw = [&](const auto &atlas) {
    for (auto &a : atlas)
      if (!std::strcmp(a.id, id)) {
        float s = std::min(w / a.w, h / a.h);
        image(id, x + (left ? 0 : (w - a.w * s) / 2), y + (h - a.h * s) / 2,
              a.w * s, a.h * s);
        return true;
      }
    return false;
  };
  if (!draw(artLabels))
    draw(expansionLabels);
  pxa_canvas_clip_pop(&frame);
}
// Stretch only the middle of generated bar art; keep the rounded ends intact.
void barStrip(const char *name, float x, float y, float w, float h) {
  float sourceWidth=256,sourceHeight=32;
  for (const auto &region : artRegions)
    if (!std::strcmp(region.id,name)) {
      sourceWidth=region.w;sourceHeight=region.h;break;
    }
  // Reserve the complete painted cap, including its outer rim and padding.
  float sourceCap=std::min(sourceHeight,sourceWidth/3);
  float corner = std::min(h * sourceCap / sourceHeight, w / 2);
  float src[] = {0,sourceCap,sourceWidth-sourceCap,sourceWidth},
        dst[] = {x,x+corner,x+w-corner,x+w};
  for (int i = 0; i < 3; ++i) {
    float width = dst[i+1] - dst[i];
    if (width <= 0)
      continue;
    int clipLeft=px(dst[i]),clipRight=px(dst[i+1])+(i<2);
    if(clipRight<=clipLeft)continue;
    float scaleX = width / (src[i+1] - src[i]);
    if(!pxa_canvas_clip_push(&frame,clipLeft,py(y),clipRight-clipLeft,sz(h)))continue;
    image(name,dst[i]-src[i]*scaleX,y,sourceWidth*scaleX,h,255,1);
    pxa_canvas_clip_pop(&frame);
  }
}
void progressBar(float x, float y, float w, float h, float progress, bool health = false) {
  if (health && w<=48) {
    image("hp_track",x,y,w,h);
    float fill=(w-2)*std::clamp(progress,0.f,1.f);
    if (fill>=1)
      image("hp_fill",x+1,y+1,fill,h-2);
    return;
  }
  barStrip(health ? "hp_track" : "wave_track",x,y,w,h);
  float inset = health ? 1.f : 2.f;
  float fill = (w - inset*2) * std::clamp(progress,0.f,1.f);
  if (fill >= 1)
    barStrip(health ? "hp_fill" : "wave_fill",x+inset,y+inset,fill,h-inset*2);
}
int digitIndex(char c) {
    return c >= '0' && c <= '9' ? c - '0'
           : c == '/'           ? 10
           : c == '+'           ? 11
           : c == 'x'           ? 12
           : c == '-'           ? 13
           : c == '!'           ? 14
                                : 15;
}
const TextBounds& digitInk(int n){
  static auto ink=[](){std::array<const TextBounds*,16> result{};for(int i=0;i<16;++i){char id[8];std::snprintf(id,sizeof(id),"n%d",i);result[i]=textInk(id);}return result;}();
  return *ink[n];
}
struct NumberInk {float left=0,top=0,right=0,bottom=0;};
NumberInk numberInk(const char* str){
  NumberInk result;float advance=0;bool first=true;
  for (const char *p = str; *p; ++p) {
    int n=digitIndex(*p);const auto& ink=digitInk(n);
    if(first){result={advance+ink.x,float(ink.y),advance+ink.x+ink.w,float(ink.y+ink.h)};first=false;}
    else{result.left=std::min(result.left,advance+ink.x);result.top=std::min(result.top,float(ink.y));result.right=std::max(result.right,advance+ink.x+ink.w);result.bottom=std::max(result.bottom,float(ink.y+ink.h));}
    advance+=digitWidths[n];
  }
  return result;
}
float numberWidth(const char* str,float h){auto ink=numberInk(str);return ink.bottom>ink.top?(ink.right-ink.left)*h/(ink.bottom-ink.top):0;}
void numberBox(const char* str,float x,float y,float w,float h,float maxHeight=24,bool left=false){
  auto ink=numberInk(str);float iw=ink.right-ink.left,ih=ink.bottom-ink.top;
  if(iw<=0||ih<=0||w<=0||h<=0)return;
  float s=std::min({w/iw,h/ih,maxHeight/ih});
  textClip(x,y,w,h);
  x+=(left?0:(w-iw*s)/2)-ink.left*s;y+=(h-ih*s)/2-ink.top*s;
  for(const char* p=str;*p;++p){
    int n=digitIndex(*p);
    char id[8];
    std::snprintf(id, sizeof(id), "n%d", n);
    image(id,x,y,digitWidths[n]*s,digitHeights[n]*s);
    x+=digitWidths[n]*s;
  }
  pxa_canvas_clip_pop(&frame);
}
void numBox(int n,float x,float y,float w,float h,float maxHeight=24,bool left=false){
  char b[24];
  std::snprintf(b, sizeof(b), "%d", n);
  numberBox(b,x,y,w,h,maxHeight,left);
}
void number(const char* str,float x,float y,float h,bool center=false){
  float visibleHeight=h*.75f,width=numberWidth(str,visibleHeight);
  numberBox(str,x-(center?width/2:0),y,width,h,visibleHeight);
}
void star(float x, float y, float r, bool on = true) {
  image(on ? "star_on" : "star_off", x - r * 1.2f, y - r * 1.2f, r * 2.4f,
        r * 2.4f);
}
void button(const char *id, float x, float y, float w, float h,
            uint32_t c = Green) {
  card(x, y, w, h, c);
  float textHeight=std::min(30.f,h*.54f);
  label(id,x+12,y+(h-textHeight)/2,w-24,textHeight);
}
void cross(float x, float y) {
  card(x-22,y-22,44,44,0x81BFC1);
  line(x-9,y-9,x+9,y+9,Paper,3);
  line(x+9,y-9,x-9,y+9,Paper,3);
}
struct PickerBounds {
  float x,y,w,h;
  bool contains(float px,float py)const{return px>=x&&px<x+w&&py>=y&&py<y+h;}
};
// These bounds leave the panel's painted rim visible and also define touch hits.
PickerBounds pickerPage(){return {24,H-69,44,44};}
PickerBounds pickerChoice(int index){
  float step=(W-152)/3;
  return {76+index*step,H-70,step-8,46};
}
PickerBounds footerCloseBounds(){return {W-68,H-69,44,44};}
void footerClose(){
  auto b=footerCloseBounds();cross(b.x+b.w/2,b.y+b.h/2);
}
void pauseIcon(float x,float y,float w,float h){
  // Derive both bars from the actual integer button bounds, including rounding.
  int left=px(x),top=py(y),width=sz(w),height=sz(h);
  int bar=sz(5),gap=sz(6),iconHeight=sz(16);
  int iconLeft=left+(width-2*bar-gap)/2,iconTop=top+(height-iconHeight)/2;
  for(int i=0;i<2;++i)
    pxa_canvas_rect_rgba(&frame,iconLeft+i*(bar+gap),iconTop,bar,iconHeight,(Paper<<8)|255,sz(1),0,0);
}
bool inside(float,float,float,float,float,float);
bool closeHit(float x,float y){return footerCloseBounds().contains(x,y);}
bool sellHit(float x,float y){return inside(x,y,W-178,H-83+8,96,52);}
void coinBox(int n,float x,float y,float w,float h,float maxHeight=17){
  char b[24];std::snprintf(b,sizeof(b),"%d",n);
  float height=std::min(h,maxHeight),gap=4;
  float iconWidth=height*textInk("coin")->w/textInk("coin")->h;
  float width=iconWidth+gap+numberWidth(b,height);
  if(width>w){float factor=w/width;height*=factor;iconWidth*=factor;gap*=factor;width=w;}
  x+=(w-width)/2;
  label("coin",x,y,iconWidth,h);
  numberBox(b,x+iconWidth+gap,y,width-iconWidth-gap,h,height);
}
void carrotSprite(Point foot,float height,int hearts,int maximum,bool animate=true,bool shadow=true){
  float jump=animate?carrotFeedback.jump()*height/64:0;
  float shake=animate?carrotFeedback.shake()*height/64:0;
  float stretch=animate&&carrotFeedback.tapped>0?std::sin(carrotFeedback.tapped/.62f*6.2831853f)*.06f:0;
  float w=height*(1+stretch),h=height*(1-stretch);
  if(shadow)ellipse(foot.x-height*.22f,foot.y-height*.035f,height*.44f,height*.085f,0x665C36,65);
  int pose=animate?carrotFeedback.pose(unsigned(anim)):0;
  int stage=CarrotVisual::damageStage(hearts,maximum);
  regionId(sourceId("assets/carrot_states.png"),px(foot.x-w/2+shake),py(foot.y-h*68/72-jump),sz(w),sz(h),pose*72,stage*72,72,72,255,1);
  if(animate&&carrotFeedback.tapped>0){
    float t=1-carrotFeedback.tapped/.62f;
    for(int side:{-1,1})image("star_on",foot.x+side*height*(.28f+t*.12f)-4,foot.y-height*(.55f+t*.25f),8,8,uint8_t((1-t)*220));
  }
}
Point world(Point p) {
  return {fx + p.x * fw / WorldWidth, fy + p.y * fh / WorldHeight};
}
Point fromWorldView(Point p) {
  return {(p.x - fx) * WorldWidth / fw, (p.y - fy) * WorldHeight / fh};
}
float spriteScale() { return std::min(1.2f, fw / WorldWidth); }
// Window environment insets are Surface pixels, while Canvas uses dp.
void applyEnvironment(const pxa_ui_environment_t &e) {
  if (!e.density_q16 || !e.width || !e.height)
    return;
  output.resize(e.width,e.height,e.density_q16);
  screen.width = uint64_t(e.width) * 65536 / e.density_q16;
  screen.height = uint64_t(e.height) * 65536 / e.density_q16;
  screen.safe_top = uint64_t(e.safe_insets[0]) * 65536 / e.density_q16;
  screen.safe_right = uint64_t(e.safe_insets[1]) * 65536 / e.density_q16;
  screen.safe_bottom = uint64_t(e.safe_insets[2]) * 65536 / e.density_q16;
  screen.safe_left = uint64_t(e.safe_insets[3]) * 65536 / e.density_q16;
}
void layout() {
  displayValid=false;
  displayValid=false;
  float sw = screen.width - screen.safe_left - screen.safe_right,
        sh = screen.height - screen.safe_top - screen.safe_bottom;
  scale = std::min(sw / 480, sh / 480);
  W = sw / scale;
  H = sh / scale;
  ox = screen.safe_left;
  oy = screen.safe_top;
  // One world scale for roads, flowerbeds, sprites and pointer hit testing.
  // Leave extra garden around the board instead of stretching its axes.
  float worldScale = std::min((W - 32) / WorldWidth, (H - 148) / WorldHeight);
  fw = WorldWidth * worldScale;
  fh = WorldHeight * worldScale;
  fx = (W - fw) / 2;
  fy = 58 + (H - 148 - fh) / 2;
  game.setColumns(int((W - 16) / (CellSize * worldScale)));
  if (selected >= 0 && !game.visible(selected))
    selected = -1;
}
bool inside(float x, float y, float a, float b, float w, float h) {
  return x >= a && x < a + w && y >= b && y < b + h;
}
void cue(music::Cue kind) {
  if (sound)
    audio.cue(kind);
}
void logState(const char *action) {
  char b[200];
  std::snprintf(b, sizeof(b),
                "carrot %s view=%d level=%d coins=%d wave=%d hp=%d enemies=%d "
                "kills=%d slot=%d focus=%d",
                action, int(view), game.level, game.coins, game.wave,
                game.hearts, game.countEnemies(), game.kills, selected,
                game.focus);
  pxa_log_write(1, b);
}
void save() {
#if defined(CARROT_PERF_PROBE)||defined(CARROT_VISUAL_PROBE)
  return; // A temporary renderer probe must not modify the user's progress.
#endif
  if (!saveReady) {
    saveDirty = true;
    return;
  }
  if (savePending) {
    saveDirty = true;
    return;
  }
  uint8_t bytes[3 + Levels] = {2, uint8_t(sound), helpSeen};
  for (int i = 0; i < Levels; ++i)
    bytes[3 + i] = best[i];
  int r = pxa_storage_request_set(packet, sizeof(packet), 42, "progress", 8,
                                  bytes, sizeof(bytes));
  savePending = r == PXA_STATUS_OK;
  saveRetry = r == PXA_STATUS_WOULD_BLOCK || r == PXA_STATUS_BUSY;
}
int unlocked() {
  int n = 0;
  while (n < Levels - 1 && best[n])
    ++n;
  return n;
}
void notice(int t) {
  toast = t;
  toastTime = 1.6f;
  cue(music::Cue::Alert);
}
void start(int level) {
  game.unlock(unlocked());
  game.reset(level);
  carrotFeedback.reset(game.hearts);
  levelPage = level / 6;
  buildPage = 0;
  newlyUnlocked = 0;
  selected = -1;
  view = Play;
  fast = false;
  accumulator = 0;
  cue(music::Cue::Begin);
  logState("start");
}
void background() {
  bool inLevel = view == Play || view == Pause || view == Complete ||
                 (view == Help && helpReturn == Pause);
  int theme = inLevel ? game.definition().theme : 0;
  char name[32];
  std::snprintf(name, sizeof(name), "world%d", theme);
  image(name, -ox / scale, -oy / scale,
        screen.width / scale, screen.height / scale,255,2);
  staticBytes=frame.length;
}
void home() {
  background();
  float cx = W / 2, t = (H - 480) / 2;
  label("title", cx - 200, t + 25, 400, 76);
  carrotSprite({cx,t+208},110,10,10);
  for (int i = 1; i <= TowerTypes; ++i) {
    float x = cx - 180 + (i - 1) * 61;
    bool open = unlocked() >= towerDefs[i].unlock;
    ellipse(x + 4, t + 249, 42, 8, 0x416D47, 70);
    image(towerDefs[i].icon, x, t + 202 + std::sin(anim * .002f + i) * 2,
          56, 56, open ? 255 : 85);
  }
  button("start", cx - 128, t + 262, 256, 55);
  button("levels", cx - 128, t + 329, 256, 50, 0xE7B861);
  button("help", cx - 128, t + 391, 256, 48, 0x81BFC1);
  int stars = 0;
  for (int i = 0; i < Levels; ++i)
    stars += best[i];
  char progress[24];
  std::snprintf(progress, sizeof(progress), "%d/%d", stars, Levels * 3);
  float progressWidth=numberWidth(progress,18),groupWidth=24+8+progressWidth;
  star(cx-groupWidth/2+12,t+462,10);
  numberBox(progress,cx-groupWidth/2+32,t+449,progressWidth,26,18);
}
void levels() {
  background();
  label("levels", W / 2 - 145, 20, 290, 48);
  float x0 = W / 2 - 215, top = (H - 404) / 2 + 35;
  for (int cell = 0; cell < 6; ++cell) {
    int i = levelPage * 6 + cell;
    if (i >= Levels)
      break;
    float x = x0 + cell % 2 * 226, y = top + cell / 2 * 104;
    bool open = i <= unlocked();
    card(x, y, 204, 91);
    if (!open)
      rect(x + 3, y + 3, 198, 85, 0x71806A, 12, 65);
    char theme[16];
    std::snprintf(theme, sizeof(theme), "theme%d", levelDefs[i].theme);
    label(theme, x + 43, y + 7, 150, 27);
    numBox(i+1,x+7,y+7,28,27,17);
    char mission[24];
    std::snprintf(mission,sizeof(mission),"mission%d",int(levelDefs[i].mission));
    label(mission,x+61,y+36,132,16);
    image(levelDefs[i].boss >= 0 ? "boss" : "carrot",
          x + 10, y + 38, 43, 43, open ? 255 : 100);
    if (open) {
      for (int j = 0; j < 3; ++j)
        star(x + 81 + j * 39, y + 65, 13, j < best[i]);
    } else {
      rect(x + 108, y + 55, 24, 22, Ink, 4);
      pxa_canvas_arc(&frame, px(x + 120), py(y + 55), sz(9), 180, 360,
                     (Ink << 8) | 255, sz(3));
    }
  }
  button("pageprev", 12, H - 65, 86, 44, levelPage ? Green : Paper);
  button("pagenext", W - 98, H - 65, 86, 44,
         levelPage < (Levels - 1) / 6 ? Green : Paper);
  button("map", W / 2 - 110, H - 65, 220, 44, 0x83B5A2);
}
void healthbar(Point p, float hp, float maxhp, float w = 34) {
  progressBar(p.x - w / 2, p.y, w, 7, maxhp > 0 ? hp / maxhp : 0, true);
}
void turret(int type, Point p, float size, float angle, float pulse, int level) {
  static const char* bodies[6][3]={{"body1-1","body1-2","body1-3"},{"body2-1","body2-2","body2-3"},{"body3-1","body3-2","body3-3"},{"body4-1","body4-2","body4-3"},{"body5-1","body5-2","body5-3"},{"body6-1","body6-2","body6-3"}};
  static auto heads=[](){std::array<std::array<uint16_t,8>,6> ids{};for(int t=0;t<6;++t)for(int d=0;d<8;++d){char path[48];std::snprintf(path,sizeof(path),"assets/h%d-%02d.png",t+1,d);ids[t][d]=sourceId(path);}return ids;}();
  image(bodies[type-1][level-1], p.x - CellSize * size / 2, p.y - CellSize * size / 2,
        CellSize * size, CellSize * size);
  constexpr float Tau = 6.2831853f;
  int direction =
      (int(std::floor((angle + 1.5707963f) / Tau * 32 + .5f)) % 32 + 32) % 32;
  int key = pulse > .105f ? 1 : pulse > .05f ? 2 : pulse > 0 ? 3 : 0;
  constexpr float headSize = CellSize + 12;
  int x = px(p.x - headSize * size / 2), y = py(p.y - headSize * size / 2),
      extent = sz(headSize * size);
  regionId(heads[type-1][direction/4],x,y,extent,extent,(direction%4)*43,key*43,43,43,255,1,true);
}
void field() {
  char scene[16];
  std::snprintf(scene, sizeof(scene), "scene%d", game.definition().map);
  float ss = spriteScale();
  const size_t roadCommand = frame.length;
  image(scene, fx - CellSize * fw / WorldWidth, fy,
        fw * (WorldWidth + 2 * CellSize) / WorldWidth, fh);
  frame.data[roadCommand + 1] = 1;
  Point entry = world(game.map().path[0]);
  image("spawn", entry.x - 15 * ss, entry.y - 49 * ss, 30 * ss, 42 * ss);
  staticBytes=frame.length;
  // Keep only the selected cell visible; the full placement grid is logical.
  if (selected >= 0 && view==Play) {
    Point p = world(game.slotPoint(selected));
    auto &t = game.towers[selected];
    if (t.type) {
      float r = game.range(selected);
      ellipse(p.x - r * fw / WorldWidth, p.y - r * fh / WorldHeight,
              r * 2 * fw / WorldWidth, r * 2 * fh / WorldHeight, 0xEFFFF0, 45,
              1, 0xEEFFE0);
    }
    rect(p.x - CellSize * fw / WorldWidth / 2,
         p.y - CellSize * fh / WorldHeight / 2, CellSize * fw / WorldWidth,
         CellSize * fh / WorldHeight, Gold, 3, 65, 2, 0xFFF7BC);
  }
  for (int i = 0; i < Slots; ++i) {
    auto &t = game.towers[i];
    if (t.type || t.obstacle) {
      Point p=world(game.slotPoint(i));
      if (t.type)
        turret(t.type, p, ss, t.facing, t.pulse,t.level);
      else {
        float size = 46 * ss;
        char prop[16];
        std::snprintf(prop, sizeof(prop), "prop%d", game.definition().theme * 2 + (t.obstacle == 2));
        image(t.obstacle == 3 ? "chest" : prop,
              p.x - size / 2, p.y - size * .7f, size, size);
      }
      if (t.type)
        for (int j = 0; j < t.level; ++j)
          star(p.x + (j - (t.level - 1) / 2.f) * 12, p.y + 19 * ss, 3.5f);
      if (t.obstacle && t.hp < t.maxhp)
        healthbar({p.x, p.y - 44 * ss}, t.hp, t.maxhp);
      if (game.focus == i) {
        ellipse(p.x - 12, p.y - 13, 24, 24, 0, 0, 2, 0xEB795E);
        line(p.x - 17, p.y - 1, p.x + 17, p.y - 1, 0xEB795E, 2);
      }
    }
  }
  Point goal = world(game.goal());
  ellipse(goal.x - CellSize * ss / 2, goal.y - 11 * ss, CellSize * ss, 24 * ss,
          0xF4D598, 255, 2, 0xB39559);
  carrotSprite({goal.x,goal.y+10*ss},64*ss,game.hearts,game.maxHearts,view==Play);
  int order[MaxEnemies], n = 0;
  for (int i = 0; i < MaxEnemies; ++i)
    if (game.enemies[i].alive) {
      int j = n;
      while (j && game.enemies[order[j - 1]].p.y > game.enemies[i].p.y) {
        order[j] = order[j - 1];
        --j;
      }
      order[j] = i;
      ++n;
    }
  for (int k = 0; k < n; ++k) {
    auto &e = game.enemies[order[k]];
    Point p = world(e.p);
    float size = (enemyDefs[e.type].boss ? 78
                  : e.type == 2 ? 50
                                : 42) *
                 ss,
          bob = std::sin(anim * .013f + e.id) * 1.25f*ss;
    char id[12];
    bool flip=e.facesRight!=enemyDefs[e.type].facesRight;
    int pose=int(anim/140+e.id)%4;
    const auto& foot=actorFeet[e.type][pose];
    bool flying=e.type==4||e.type==7||e.type==10;
    float ground=p.y+8*ss,footX=flip?1-foot.x:foot.x;
    float left=p.x-footX*size,top=ground-foot.y*size-(flying?9*ss:0)+bob;
    float shadowWidth=size*std::clamp(foot.width,.38f,.68f)*(flying?.85f:1);
    ellipse(p.x-shadowWidth/2,ground-3*ss,shadowWidth,6*ss,0x526C6C,flying?45:65);
    if(e.type==0)std::snprintf(id,sizeof(id),"walk%d",pose);
    if (e.type < 4)
      image(e.type == 0 ? id : enemyDefs[e.type].sprite,
            left,top,size,size,
            e.hit > 0 ? 190 : 255,0,flip);
    else {
      int x=px(left),y=py(top),extent=sz(size);
      static auto actors=[](){std::array<uint16_t,EnemyTypes> ids{};for(int i=4;i<EnemyTypes;++i){char path[64];std::snprintf(path,sizeof(path),"assets/%s.png",enemyDefs[i].sprite);ids[i]=sourceId(path);}return ids;}();
      int sourceExtent=enemyDefs[e.type].boss?96:72;
      regionId(actors[e.type],x,y,extent,extent,pose*sourceExtent,0,sourceExtent,sourceExtent,e.hit>0?190:255,1,false,flip);
    }
    if (e.slow > 0 || game.freeze > 0)
      ellipse(p.x - size * .4f, p.y - 7, size * .8f, 16, 0xB6EEFF, 90, 1,
              0xE8FAFF);
    if (e.shield > 0)
      ellipse(p.x - size * .55f, p.y - size * .85f, size * 1.1f, size,
              0xB8DFFF, 45, 2, 0xE3F6FF);
    if (e.burn > 0)
      ellipse(p.x - size * .3f, p.y - size * .3f, size * .6f, size * .5f,
              0xFF8C43, 90, 1, 0xFFD36E);
    if (e.hp < e.maxhp || enemyDefs[e.type].boss)
      healthbar({p.x, p.y - size * .8f}, e.hp, e.maxhp, enemyDefs[e.type].boss ? 62 : 32);
  }
  for (auto &s : game.shots)
    if (s.active) {
      Point p = world(s.p);
      float arc =
          s.type == 3 || s.type == 6 ? -std::sin(s.age / s.duration * 3.14159f) * 25 : 0;
      float radius = 3.5f + s.level * 1.25f;
      if (s.level >= 2) {
        float t=std::max(0.f,s.age/s.duration-.08f);
        Point trail=world({s.start.x+(s.end.x-s.start.x)*t,
                           s.start.y+(s.end.y-s.start.y)*t});
        line(trail.x,trail.y+arc,p.x,p.y+arc,towerDefs[s.type].color,s.level+1,180);
      }
      ellipse(p.x - radius, p.y - radius + arc, radius*2, radius*2,
              towerDefs[s.type].color,
              255, 1, 0xFFFFFF);
      ellipse(p.x - 2, p.y - 4 + arc, 3, 3, 0xFFFFFF, 200);
    }
  for (auto &e : game.effects)
    if (e.time > 0) {
      Point p = world(e.p);
      float t = 1 - e.time / e.total;
      if (e.kind == 6) {
        Point end=world(e.end);
        line(p.x,p.y,end.x,end.y,0xC798F4,4,170);
        line(p.x,p.y,end.x,end.y,0xF1F8FF,1.5f,230);
      } else if (e.kind == 3) {
        char b[24];
        std::snprintf(b, sizeof(b), "+%d", e.value);
        number(b, p.x - 12, p.y - 25 - t * 24, 19);
      } else {
        uint32_t c = e.kind == 2 ? 0xBCF3FF : e.kind == 4 ? 0xFA8D69 : 0xFFF0A0;
        float r = 5 + t * (e.kind == 0 ? 38 : 21);
        ellipse(p.x - r, p.y - r, r * 2, r * 2, c, uint8_t((1 - t) * 70), 2, c);
        for (int i = 0; i < 5; ++i) {
          float a = i * 1.2566f;
          ellipse(p.x + std::cos(a) * r - 2, p.y + std::sin(a) * r - 2, 4, 4, c,
                  uint8_t((1 - t) * 255));
        }
      }
    }
  if (game.freeze > 0)
    rect(fx, fy, fw, fh, 0xA8E8FF, 0, 40);
}
void hud() {
  card(12, 9, 104, 39);
  image("coin",18,14,29,29);
  numBox(game.coins,47,14,62,29,20);
  card(126, 9, 101, 39);
  carrotSprite({146,44},34,game.hearts,game.maxHearts,false,false);
  numBox(game.hearts,167,14,52,29,20);
  card(237, 9, 111, 39);
  line(248, 19, 248, 39, Ink, 3);
  line(249, 19, 261, 24, 0xED9570, 6);
  char b[24];
  std::snprintf(b, sizeof(b), "%d/%d", game.wave, game.totalWaves);
  numberBox(b,269,15,71,27,18);
  card(W - 121, 9, 49, 39, fast ? Gold : Paper);
  numberBox(fast?"x2":"x1",W-115,15,37,27,17);
  card(W - 62, 9, 49, 39, 0x80B6A0);
  pauseIcon(W-62,9,49,39);
  float p = float(game.wave) / game.totalWaves;
  if (game.active)
    p = (game.wave - 1 + game.waveProgress) / game.totalWaves;
  if (game.result == Result::Win)
    p = 1;
  progressBar(18, 51, W - 36, 12, p);
}
void footer() {
  float y = H - 83;
  card(12, y, W - 24, 72, 0xF7ECC6);
  if (selected >= 0) {
    auto &t = game.towers[selected];
    if (!t.type && !t.obstacle) {
      auto page=pickerPage();
      card(page.x,page.y,page.w,page.h,0x89B8A7);
      line(page.x+15,page.y+8,page.x+26,page.y+18,Paper,3);
      line(page.x+26,page.y+18,page.x+15,page.y+28,Paper,3);
      numberBox(buildPage?"2/2":"1/2",page.x+5,page.y+29,34,11,10);
      for (int i = 0; i < 3; ++i) {
        int kind = buildPage * 3 + i + 1;
        const auto &tower = towerDefs[kind];
        bool open = kind <= game.availableTowers;
        auto b=pickerChoice(i);
        rect(b.x,b.y,b.w,b.h,
             open && game.coins >= tower.cost ? 0xE4E7B9 : 0xD1CDB2,8,255,1,0xC7B888);
        float contentWidth=std::min(124.f,b.w-10),contentX=b.x+(b.w-contentWidth)/2;
        image(tower.icon,contentX,b.y+5,36,36,open?255:80);
        float textX=contentX+44,textWidth=contentWidth-44;
        label(tower.label,textX,b.y+4,textWidth,17);
        if (open)
          coinBox(tower.cost,textX,b.y+25,textWidth,16,13);
        else {
          float hintWidth=std::min(56.f,textWidth-18);
          float hintX=textX+(textWidth-hintWidth-18)/2;
          label("unlock",hintX,b.y+26,hintWidth,14);
          numBox(tower.unlock,hintX+hintWidth+2,b.y+25,16,16,12);
        }
      }
      footerClose();
    } else if (t.type) {
      image(towerDefs[t.type].icon,
            23, y + 7, 52, 52);
      label(towerDefs[t.type].label,
            84,y+9,80,21);
      if(W>650){char hint[24];std::snprintf(hint,sizeof(hint),"tower_hint%d",t.type);label(hint,225,y+25,W-550,20);}
      for (int i = 0; i < t.level; ++i)
        star(96+i*24,y+48,7);
      card(W-304,y+8,114,52,
           t.level == 3                               ? 0xC5C9A5
           : game.coins >= game.upgradeCost(selected) ? Green
                                                      : 0xB9B89D);
      label(t.level==3?"max":"upgrade",W-292,t.level==3?y+21:y+15,90,t.level==3?26:20);
      if (t.level < 3)
        coinBox(game.upgradeCost(selected),W-292,y+38,90,16,12);
      button("sell",W-178,y+8,96,52,pendingSell==selected?Gold:0xDE9A73);
      footerClose();
    } else {
      image(t.obstacle == 1   ? "bush"
            : t.obstacle == 2 ? "rocks"
                              : "chest",
            25, y + 5, 64, 64);
      healthbar({140, y + 24}, t.hp, t.maxhp, 80);
      coinBox(t.obstacle==3?90:t.obstacle==2?35:25,100,y+37,80,24,17);
      button("clear",W-270,y+12,188,48,0xE5B259);
      footerClose();
    }
  } else {
    card(22, y + 9, 87, 53, game.skillCooldown > 0 ? 0xA2B0AC : 0x8DC8D5);
    label("freeze", 29, y + 15, 73, game.skillCooldown > 0 ? 23 : 39);
    if (game.skillCooldown > 0)
      numBox(int(game.skillCooldown)+1,34,y+41,62,18,14);
    if (!game.wave)
      label("prepare", 122, y + 22, W - 307, 28);
    else if (!game.active)
      numBox(int(game.intermission)+1,122,y+15,W-307,44,25);
    else
      for (int i = 0; i < 3; ++i)
        image(i == 0   ? "pea"
              : i == 1 ? "ice"
                       : "sun",
              124 + i * 44, y + 17, 44, 44);
    button("wave", W - 178, y + 9, 151, 53, game.active ? 0xB9C79E : Green);
  }
}
void cachedHud(){
  // UI commands are independent of the animated battlefield. Rebuild only
  // when a visible value, selection, unlock or layout actually changes.
  static std::vector<uint8_t> commands;
  static std::array<int,23> previous{};
  static unsigned records=0;
  float progress=game.active?(game.wave-1+game.waveProgress)/game.totalWaves:float(game.wave)/game.totalWaves;
  const auto* t=selected>=0?&game.towers[selected]:nullptr;
  std::array<int,23> state{game.coins,game.hearts,game.wave,game.totalWaves,
    fast,selected,buildPage,game.availableTowers,int(std::ceil(game.skillCooldown)),int(game.intermission)+1,
    int(progress*(W-36)*scale*4),t?t->type:0,t?t->level:0,t?t->obstacle:0,
    t&&t->maxhp>0?int(t->hp/t->maxhp*320):0,game.active,int(W*100),int(H*100),int(scale*10000),int(ox*100),int(oy*100),int(game.result),pendingSell};
  hudBegin=frame.length;
  if(!commands.empty()&&previous==state){pxa_put_bytes(&frame,commands.data(),commands.size());frame.records+=records;hudEnd=frame.length;return;}
  size_t at=frame.length;unsigned count=frame.records;hud();hudTopBytes=frame.length-at;footer();
  commands.assign(frame.data+at,frame.data+frame.length);records=frame.records-count;previous=state;
  hudEnd=frame.length;++hudRevision;
}
void gameplay() {
  background();
  field();
  cachedHud();
  if (!game.wave) {
    char mission[24];
    std::snprintf(mission, sizeof(mission), "mission%d", int(game.definition().mission));
    float bannerWidth=(game.definition().mission==Mission::Treasure||game.definition().mission==Mission::Timed)?310:220;
    card(W/2-bannerWidth/2,fy+3,bannerWidth,32);
    label(game.definition().mission == Mission::Treasure ? "objective_treasure"
          : game.definition().mission == Mission::Timed ? "objective_time" : mission,
          W/2-bannerWidth/2+10,fy+8,bannerWidth-20,22);
  } else if (game.definition().mission == Mission::Treasure ||
             game.definition().mission == Mission::Timed) {
    card(W - 105, fy + 4, 87, 30);
    if (game.definition().mission == Mission::Treasure) {
      image("chest", W - 101, fy + 7, 25, 25);
      numBox(game.treasureLeft,W-71,fy+8,45,22,17);
    } else
      numBox(std::max(0,game.definition().timeLimit-int(game.elapsed)),W-97,fy+8,71,22,17);
  }
  for (const auto &enemy : game.enemies)
    if (enemy.alive && enemyDefs[enemy.type].boss) {
      char name[24];
      std::snprintf(name,sizeof(name),"boss_name%d",game.definition().theme);
      card(W / 2 - 116, fy + 10, 232, 40);
      label(name,W/2-100,fy+14,200,20);
      progressBar(W/2-97,fy+37,194,7,enemy.hp/enemy.maxhp,true);
      break;
    }
  if (game.banner > 0 && game.wave) {
    char b[24];
    std::snprintf(b, sizeof(b), "%d/%d", game.wave, game.totalWaves);
    card(W / 2 - 68, fy + 6, 136, 39, 0xDAAF59);
    numberBox(b,W/2-56,fy+12,112,27,20);
  }
  if (toastTime > 0) {
    card(W / 2 - 111, fy + fh / 2 - 22, 222, 46, 0xDDB36C);
    if (toast == 4) {
      image(towerDefs[toastTower].icon, W / 2 - 102, fy + fh / 2 - 20, 40, 40);
      label("unlock", W / 2 - 58, fy + fh / 2 - 12, 113, 25);
      numBox(towerDefs[toastTower].unlock,W/2+61,fy+fh/2-12,38,25,22);
    } else
      label(toast == 1   ? "poor"
          : toast == 2 ? "hint_build"
                       : "clear",
          W / 2 - 103, fy + fh / 2 - 14, 206, 30);
  }
}
void shade() {
  rect(-ox / scale, -oy / scale, screen.width / scale, screen.height / scale,
       0x244C3F, 0, 140);
}
void paused() {
  background();field();
  shade();
  float x = W / 2 - 157, y = H / 2 - 166;
  card(x, y, 314, 332);
  label("pause", x + 56, y + 15, 202, 45);
  button("resume", x + 28, y + 77, 258, 48);
  button("help", x + 28, y + 137, 258, 43, 0x90BCBF);
  button("retry", x + 28, y + 192, 258, 43, 0xE4B15E);
  button("map", x + 28, y + 247, 258, 43, 0x94AF87);
}
void help() {
  background();
  shade();
  float x = W / 2 - 224, y = H / 2 - 228;
  card(x, y, 448, 456);
  label(helpPage ? "collection" : "help", x + 88, y + 14, 272, 42);
  const char *ids[] = {"hint_build", "hint_coins", "hint_upgrade",
                       "hint_clear", "hint_combo", "hint_goal"};
  for (int i = 0; i < 6; ++i) {
    if (helpPage) {
      const auto &tower = towerDefs[i+1];
      bool open = unlocked() >= tower.unlock;
      image(tower.icon,x+16,y+70+i*46,40,40,open?255:100);
      label(tower.label,x+67,y+70+i*46,123,22,true);
      coinBox(tower.cost,x+200,y+70+i*46,76,24,14);
      if (!open) {
        label("unlock",x+290,y+72+i*46,80,17);
        numBox(tower.unlock,x+375,y+70+i*46,36,24,14);
      }
      char hint[24];
      std::snprintf(hint,sizeof(hint),"tower_hint%d",i+1);
      label(hint,x+67,y+94+i*46,352,17,true);
      continue;
    }
    image(i == 0   ? "pea"
          : i == 1 ? "coin"
          : i == 2 ? "sun"
          : i == 3 ? "chest"
          : i == 4 ? "ice"
                   : "carrot",
          x + 16, y + 70 + i * 46, 40, 40);
    label(ids[i],x+67,y+75+i*46,355,30,true);
  }
  button("resume", x + 20, y + 373, 200, 48);
  button(helpPage ? "help" : "collection", x + 230, y + 373, 95, 48, 0xE7B861);
  button("sound", x + 335, y + 373, 90, 48, sound ? Green : 0xA5AAA0);
  if (!sound)
    line(x + 350, y + 407, x + 410, y + 385, 0xF6976D, 3);
}
void completed() {
  background();field();
  shade();
  float x = W / 2 - 168, y = H / 2 - 172;
  card(x, y, 336, 344);
  bool win = game.result == Result::Win;
  label(win ? "win" : "lose", x + 35, y + 17, 266, 48);
  carrotSprite({x+168,y+181},104,game.hearts,game.maxHearts,false);
  if (win && newlyUnlocked) {
    image(towerDefs[newlyUnlocked].icon, x + 27, y + 95, 65, 65);
    label(towerDefs[newlyUnlocked].label, x + 15, y + 163, 92, 22);
  }
  if (!win && game.definition().mission == Mission::Treasure && game.treasureLeft)
    label("treasure_left", x + 40, y + 191, 256, 25);
  if (win)
    for (int i = 0; i < 3; ++i)
      star(x + 90 + i * 78, y + 193, 25, i < game.stars());
  button(win && game.level < Levels - 1 ? "next" : "retry", x + 30, y + 231,
         276, 43);
  button("map", x + 30, y + 285, 276, 40, 0xA7BD90);
}
bool render() {
  if(output.painter.loadingBackground()){output.painter.pump();return true;}
#ifdef CARROT_PROFILE
  uint64_t began = monotonicUs();
#endif
  static View previousView = Home;
  if (view != previousView) {
    displayValid=false;
    previousView = view;
  }
  pxa_canvas_begin(&frame, draws, sizeof(draws));
  hudBegin=hudEnd=0;
  switch (view) {
  case Home:
    home();
    break;
  case LevelSelect:
    levels();
    break;
  case Play:
    gameplay();
    break;
  case Pause:
    paused();
    break;
  case Help:
    help();
    break;
  case Complete:
    completed();
    break;
  }
  if (frame.failed) {
    pxa_log_write(3, "carrot display list overflow");
    return false;
  }
  if (displayValid&&previousSize==frame.length&&!std::memcmp(previousDraws,frame.data,frame.length)) {
#ifdef CARROT_PROFILE
    ++profiling.skipped;
#endif
    return true;
  }
#ifdef CARROT_PROFILE
  uint64_t submitted = monotonicUs();
  profiling.build += submitted - began;
#endif
  const bool whole = true;
  // Keep an empty transparent Canvas solely for the existing touch routing.
  if(!initialized) {
    uint8_t emptyBytes[4];pxa_canvas_frame_t empty;
    pxa_canvas_begin(&empty,emptyBytes,sizeof(emptyBytes));
    if(!pxa_canvas_present(2,&empty,&generation,&initialized,packet,sizeof(packet),packet,sizeof(packet)))return false;
    pxa_ui_transaction_t transaction{};
    if(!pxa_ui_transaction_begin(&transaction,++generation,PXA_UI_TRANSACTION_PATCH,packet,sizeof(packet))||
       !pxa_ui_set_rgba(&transaction,1,PXA_UI_PROPERTY_BACKGROUND,0)||
       !pxa_ui_transaction_commit(&transaction))return false;
  }
  const bool ok=output.paint(frame.data,frame.length,staticBytes,hudBegin,hudEnd,hudRevision,
    int((int64_t(py(67))*output.painter.density+32768)>>16),
    int((int64_t(py(H-86))*output.painter.density+32768)>>16),hudTopBytes);
  if (ok){std::memcpy(previousDraws,frame.data,frame.length);previousSize=frame.length;displayValid=true;}
#ifdef CARROT_PROFILE
  if(ok&&!firstFrameLogged){char message[160];std::snprintf(message,sizeof(message),"carrot first complete frame component_start_us=%llu",(unsigned long long)(monotonicUs()-launchStarted));pxa_log_write(2,message);firstFrameLogged=true;}
  if(ok&&lastSceneRevision!=output.painter.backdropRevision){char message[160];std::snprintf(message,sizeof(message),"carrot complete scene render_us=%llu revision=%llu",(unsigned long long)(monotonicUs()-began),(unsigned long long)output.painter.backdropRevision);pxa_log_write(2,message);lastSceneRevision=output.painter.backdropRevision;}
  profiling.submit += monotonicUs() - submitted;
  profiling.frames+=ok;
  profiling.skipped+=!ok;
  if(ok){auto now=monotonicUs();if(profiling.previous)profiling.intervals[(profiling.intervalCount++)%256]=uint32_t(now-profiling.previous);profiling.previous=now;}
  profiling.full += whole;
  if (whole)
    profiling.area += uint64_t(screen.width * screen.height);
#endif
  return true; // A busy Surface buffer is retried on the next tick.
}
void tap(float x, float y) {
#ifdef CARROT_VISUAL_PROBE
  if(y<51&&x<W/2){visualProbePhase=std::min(8,visualProbePhase+1);return;}
#endif
  float cx = W / 2, t = (H - 480) / 2;
  cue(music::Cue::Tap);
  if (view == Home) {
    if(inside(x,y,cx-55,t+98,110,110)){carrotFeedback.tap();return;}
    if (inside(x, y, cx - 128, t + 262, 256, 55)) {
      start(unlocked());
      if (!helpSeen) {
        helpReturn = Play;
        view = Help;
      }
    } else if (inside(x, y, cx - 128, t + 329, 256, 50)) {
      levelPage = unlocked() / 6;
      view = LevelSelect;
    }
    else if (inside(x, y, cx - 128, t + 391, 256, 48)) {
      helpReturn = Home;
      helpPage = 0;
      view = Help;
    }
  } else if (view == LevelSelect) {
    if (y >= H - 68) {
      if (x < 100)
        levelPage = std::max(0, levelPage - 1);
      else if (x > W - 100)
        levelPage = std::min((Levels - 1) / 6, levelPage + 1);
      else
        view = Home;
    }
    else {
      float x0 = W / 2 - 215, ty = (H - 404) / 2 + 35;
      for (int cell = 0; cell < 6; ++cell) {
        int i = levelPage * 6 + cell;
        if (i < Levels && inside(x, y, x0 + cell % 2 * 226, ty + cell / 2 * 104, 204, 91) &&
            i <= unlocked())
          start(i);
      }
    }
  } else if (view == Help) {
    float a = W / 2 - 224, b = H / 2 - 228;
    if (inside(x, y, a + 20, b + 373, 200, 48)) {
      helpSeen = 1;
      save();
      view = helpReturn;
    } else if (inside(x, y, a + 230, b + 373, 95, 48))
      helpPage = 1-helpPage;
    else if (inside(x, y, a + 335, b + 373, 90, 48)) {
      sound = !sound;
      audio.setEnabled(sound);
      soundChanged = true;
      save();
    }
  } else if (view == Pause) {
    float a = W / 2 - 157, b = H / 2 - 166;
    if (inside(x, y, a + 28, b + 77, 258, 48))
      view = Play;
    else if (inside(x, y, a + 28, b + 137, 258, 43)) {
      helpReturn = Pause;
      helpPage = 0;
      view = Help;
    } else if (inside(x, y, a + 28, b + 192, 258, 43))
      start(game.level);
    else if (inside(x, y, a + 28, b + 247, 258, 43)) {
      levelPage = game.level / 6;
      view = LevelSelect;
    }
  } else if (view == Complete) {
    float a = W / 2 - 168, b = H / 2 - 172;
    if (inside(x, y, a + 30, b + 231, 276, 43))
      start(game.result == Result::Win && game.level < Levels - 1
                ? game.level + 1
                : game.level);
    else if (inside(x, y, a + 30, b + 285, 276, 40))
      view = LevelSelect;
  } else if (view == Play) {
    if (y < 51) {
      if (x >= W - 64)
        view = Pause;
      else if (x >= W - 123)
        fast = !fast;
      return;
    }
    float by = H - 83;
    if (y >= by) {
      if (selected >= 0) {
        auto &tw = game.towers[selected];
        if (closeHit(x,y)) {
          selected = -1;
          return;
        }
        if (!tw.type && !tw.obstacle) {
          if (pickerPage().contains(x,y)) {
            buildPage = 1 - buildPage;
            return;
          }
          int type=0;
          for(int i=0;i<3;++i)if(pickerChoice(i).contains(x,y))type=i+1+buildPage*3;
          if (type >= 1 && type <= TowerTypes) {
            if (type > game.availableTowers) {
              toastTower = type;
              notice(4);
              return;
            }
            if (game.build(selected, type)) {
              cue(music::Cue::Build);
              logState("build");
            } else
              notice(1);
          }
        } else if (tw.type) {
          if (inside(x,y,W-304,by+8,114,52)) {
            if (game.upgrade(selected)) {
              cue(music::Cue::Upgrade);
              logState("upgrade");
            } else if (tw.level < 3)
              notice(1);
          } else if (sellHit(x,y))pendingSell=selected;
        } else if (inside(x,y,W-270,by+12,188,48)) {
          game.focus = selected;
          selected = -1;
          logState("clear");
        }
      } else if (x < 112) {
        if (game.ice())
          cue(music::Cue::Freeze);
      } else if (x > W - 179) {
        bool built = false;
        for (auto &tw : game.towers)
          built |= tw.type != 0;
        if (!built)
          notice(2);
        else if (game.nextWave()) {
          cue(music::Cue::Begin);
          logState("wave");
        }
      }
      return;
    }
    auto goal=world(game.goal());float ss=spriteScale();
    if(inside(x,y,goal.x-32*ss,goal.y-51*ss,64*ss,64*ss)){carrotFeedback.tap();logState("carrot-pet");return;}
    selected = game.pick(fromWorldView({x, y}));
  }
  logState("tap");
}
void back() {
  pendingSell=-1;
  if (view == Home) {
    pxa_window_request_back();
    return;
  }
  if (view == Play)
    view = Pause;
  else if (view == Pause)
    view = Play;
  else if (view == Help) {
    view = helpReturn;
    helpSeen = 1;
    save();
  } else if (view == LevelSelect)
    view = Home;
  else
    view = LevelSelect;
}
#ifdef CARROT_PERF_PROBE
#include "tests/device_perf.inc"
#endif
#ifdef CARROT_VISUAL_PROBE
#include "tests/visual_probe.inc"
#endif
} // namespace
extern "C" int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
#ifdef CARROT_PROFILE
  launchStarted=monotonicUs();
#endif
  pxa_ui_environment_t env;
  if (pxa_ui_parse_start_environment(config, length, &env))
    applyEnvironment(env);
  layout();
  pxa_window_fullscreen();
  pxa_window_keep_screen_on(1);
  audio.start();
  pxa_storage_request_get(41, "progress", 8);
  if (pxa_clock_set_period(20) != PXA_STATUS_OK)
    return PXA_STATUS_INTERNAL;
  return render() ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}
extern "C" int32_t pxa_app_on_event(const uint8_t *data, uint32_t length) {
  pxa_event_t e;
  if (!pxa_canvas_parse_event(data, length, &e))
    return PXA_EVENT_UNHANDLED;
  uint8_t lifecycle = 0;
  if (pxa_lifecycle_parse(&e, &lifecycle)) {
    lastTime = 0;
    accumulator = 0;
    audio.setForeground(lifecycle == PXA_SYSTEM_LIFECYCLE_FOREGROUND);
    if (lifecycle == PXA_SYSTEM_LIFECYCLE_BACKGROUND && view == Play)
      view = Pause;
    render();
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_UI_SERVICE && e.opcode == PXA_UI_ENVIRONMENT_CHANGED &&
      e.token == 0) {
    pxa_ui_environment_t env;
    if (pxa_ui_parse_environment_records(e.payload, e.payload_size, &env))
      applyEnvironment(env);
    layout();
    render();
    return PXA_EVENT_HANDLED;
  }
  if (pxa_window_is_back_requested(&e)) {
    if (view == Home)
      return PXA_EVENT_UNHANDLED;
    back();
    render();
    return PXA_EVENT_HANDLED;
  }
  if (audio.handle(e))
    return PXA_EVENT_HANDLED;
  if(output.event(e)){
    if(e.service==PXA_SURFACE_SERVICE&&e.opcode==PXA_SURFACE_QUERY_STATE)return PXA_EVENT_HANDLED;
    if(!output.painter.loadingBackground()){displayValid=false;displayValid=false;render();}
    return PXA_EVENT_HANDLED;
  }
  uint32_t streamRequest = 0;
  int32_t streamStatus = 0;
  pxa_canvas_handle_t streamHandle = 0;
  if (pxa_canvas_parse_stream_ready(&e, &streamRequest, &streamHandle,
                                    &streamStatus) &&
      streamRequest == 61) {
    if (streamStatus == PXA_STATUS_OK)
      canvasStream = streamHandle;
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_STORAGE_SERVICE) {
    if (e.opcode == PXA_STORAGE_GET && e.token == 41) {
      pxa_storage_get_result_t r;
      if (pxa_storage_parse_get(&e, 41, &r)) {
        bool oldSave = r.status == PXA_STATUS_OK && r.value.size == 9 &&
                       r.value.data[0] == 1;
        bool campaignSave = r.status == PXA_STATUS_OK &&
                            r.value.size == 3 + Levels && r.value.data[0] == 2;
        if (oldSave || campaignSave) {
          if (!soundChanged)
            sound = r.value.data[1] != 0;
          audio.setEnabled(sound);
          helpSeen |= r.value.data[2] != 0;
          for (int i = 0; i < (oldSave ? 6 : Levels); ++i)
            best[i] =
                std::max(best[i], std::min(uint8_t(3), r.value.data[3 + i]));
          saveReady = true;
          levelPage = unlocked() / 6;
          unsigned legacy=0,stars=0;
          for(int i=0;i<Levels;++i){stars+=best[i];if(i<6)legacy|=unsigned(best[i])<<(i*2);}
          char message[160];std::snprintf(message,sizeof(message),"carrot progress loaded schema=%d completed=%d stars=%u legacy_stars=%u",oldSave?1:2,unlocked(),stars,legacy);
          pxa_log_write(2,message);
          if(oldSave)save();
        } else if (r.status == PXA_STATUS_NOT_FOUND)
          saveReady = true;
        else
          pxa_log_write(2,
                        "carrot progress unreadable; existing save preserved");
        if (saveReady && saveDirty) {
          saveDirty = false;
          save();
        }
        render();
      }
    } else if (e.opcode == PXA_STORAGE_SET && e.token == 42) {
      int32_t status;
      if (pxa_storage_parse_status(&e, 42, PXA_STORAGE_SET, &status)) {
        savePending = false;
        if (saveDirty) {
          saveDirty = false;
          save();
        } else if (status == PXA_STATUS_BUSY ||
                   status == PXA_STATUS_WOULD_BLOCK)
          saveRetry = true;
        else if (status == PXA_STATUS_OK)
          pxa_log_write(1, "carrot progress saved");
        else
          pxa_log_write(2, "carrot progress save failed");
      }
    }
    return PXA_EVENT_HANDLED;
  }
  if (e.service == PXA_CLOCK_SERVICE && e.opcode == PXA_CLOCK_TICK) {
    uint64_t now = 0;
    if (!pxa_clock_parse_tick(&e, &now))
      return PXA_EVENT_UNHANDLED;
#ifdef CARROT_PERF_PROBE
    updatePerfProbe(now);
#endif
#ifdef CARROT_VISUAL_PROBE
    updateVisualProbe(now);
#endif
    float dt = lastTime && now > lastTime
                   ? std::min(.1f, float(now - lastTime) / 1000000.f)
                   : .02f;
    lastTime = now;
    if(view==Play||view==Home){anim+=uint64_t(dt*1000);carrotFeedback.tick(dt,view==Play?game.hearts:carrotFeedback.previousHearts);}
    audio.tick(now);
    if (saveRetry) {
      saveRetry = false;
      save();
    }
    toastTime = std::max(0.f, toastTime - dt);
    if (view == Play) {
      int oldKills = game.kills;
      int previousHearts=game.hearts;
      float previousPulse[Slots];
      int previousObstacles = 0;
      for (int s = 0; s < Slots; ++s) {
        previousPulse[s] = game.towers[s].pulse;
        previousObstacles += game.towers[s].obstacle != 0;
      }
#ifdef CARROT_TEST_SPEED
      float simulationDt = dt * CARROT_TEST_SPEED;
#else
      float simulationDt = dt;
#endif
      accumulator = std::min(.2f, accumulator + simulationDt * (fast ? 2 : 1));
      int steps = 0;
#ifdef CARROT_PROFILE
      uint64_t simulationBegan = monotonicUs();
#endif
      while (accumulator >= .02f && steps++ < 10) {
#if (!defined(CARROT_PERF_PROBE)||defined(CARROT_LIVE_PROBE))&&!defined(CARROT_VISUAL_PROBE)
        game.step(.02f);
#endif
        accumulator -= .02f;
      }
      if(game.hearts<previousHearts)carrotFeedback.tick(0,game.hearts);
#ifdef CARROT_PROFILE
      profiling.simulation += monotonicUs() - simulationBegan;
#endif
      int obstacles = 0, fired = 0;
      for (int s = 0; s < Slots; ++s) {
        const auto &tower = game.towers[s];
        obstacles += tower.obstacle != 0;
        if (tower.type && tower.pulse > previousPulse[s])
          fired = tower.type;
      }
      if (obstacles < previousObstacles)
        cue(music::Cue::Clear);
      else if (game.kills > oldKills)
        cue(music::Cue::Coin);
      else if (fired)
        cue(fired == 2   ? music::Cue::Ice
            : fired == 3 ? music::Cue::Firework
                         : music::Cue::Fire);
      if (game.result != Result::None) {
        view = Complete;
        selected = -1;
        if (game.result == Result::Win) {
          int before = unlocked();
          best[game.level] = std::max(best[game.level], uint8_t(game.stars()));
          int after = unlocked();
          for (int kind = 4; kind <= TowerTypes; ++kind)
            if (before < towerDefs[kind].unlock && after >= towerDefs[kind].unlock)
              newlyUnlocked = kind;
          save();
          cue(music::Cue::Win);
        } else
          cue(music::Cue::Lose);
        logState(game.result == Result::Win ? "win" : "loss");
      }
    }
    paintTime += dt;
    if (paintTime >= (view == Play ? .04f : view == Home ? .12f : .3f)) {
      paintTime = std::fmod(paintTime, view == Play   ? .04f
                                       : view == Home ? .12f
                                                      : .3f);
      render();
    }
#ifdef CARROT_PROFILE
    profileTick(now);
#endif
    return PXA_EVENT_HANDLED;
  }
  pxa_ui_pointer_data_t p;
  if (pxa_canvas_parse_pointer(&e, 2, &p)) {
    if(output.painter.loadingBackground()){pendingSell=-1;return PXA_EVENT_HANDLED;}
    float x=(p.x-ox)/scale,y=(p.y-oy)/scale;
    if (p.phase == PXA_POINTER_DOWN) {
      pendingSell=-1;sellPointer=p.pointer_id;tap(x,y);
      render();
    }else if(p.phase==PXA_POINTER_UP&&p.pointer_id==sellPointer){
      int candidate=pendingSell;pendingSell=-1;
      if(candidate>=0&&view==Play&&selected==candidate&&sellHit(x,y)&&game.sell(candidate)){
        cue(music::Cue::Sell);logState("sell");selected=-1;
      }
      if(candidate>=0)render();
    }else if(p.pointer_id==sellPointer&&(p.phase==PXA_POINTER_CANCEL||(p.phase==PXA_POINTER_MOVE&&pendingSell>=0&&!sellHit(x,y)))){
      pendingSell=-1;render();
    }
    return PXA_EVENT_HANDLED;
  }
  return PXA_EVENT_UNHANDLED;
}
extern "C" int32_t pxa_app_stop() {
  pxa_clock_set_period(0);
  audio.stop();
  output.stop();
  if (canvasStream)
    pxa_close_handle(canvasStream);
  return PXA_STATUS_OK;
}
