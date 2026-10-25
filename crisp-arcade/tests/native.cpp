// SPDX-License-Identifier: MIT
#include "../../common/pocket-play/runtime.hpp"
#include "../backend.hpp"
#include "../core_bridge.h"
#include "../vendor/src/lib/cglp.h"
#include "../vendor/src/lib/machineDependent.h"
#include "../vendor/src/lib/sound.h"
#include <cfloat>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
int test_cast_state(); float test_cast_power(); float test_cast_x();
int test_grappling_attached(); float test_grappling_y(); void test_grappling_offer_anchor();
float test_reflector_power(); float test_reflector_direction();
float test_color_shot();
}
namespace stub {
std::array<uint16_t,pocket::WIDTH*pocket::HEIGHT> pixels{};
bool drawing=false, buttons[12]{};
uint32_t now=1000, rng=0x12345678;
unsigned save_calls=0, tones=0;
float tone_frequency=0, tone_duration=0, tone_when=0;
std::vector<std::string> logs;
void draw() { drawing=true; game::draw(); drawing=false; }
void step(unsigned count=1,uint32_t ms=17) { for(unsigned i=0;i<count;++i) { now+=ms; game::tick(ms); assert(!isPlayingBgm); } }
void press() { buttons[int(pocket::Key::A)]=true; game::key(pocket::Key::A); }
void release() { buttons[int(pocket::Key::A)]=false; }
void fresh() { std::fill(buttons,buttons+12,false); game::init(); }
void launch(int i) { game::pointer(pocket::Phase::DOWN,20+(i%2)*140,50+(i/2)*75); game::pointer(pocket::Phase::UP,20,50); }
void ppm(const char *root,const char *name) {
    if(!root) return;
    const std::string path=std::string(root)+"/"+name+".ppm";
    FILE *f=std::fopen(path.c_str(),"wb"); assert(f);
    std::fprintf(f,"P6\n288 224\n255\n");
    for(uint16_t p:pixels) { const unsigned char rgb[]={uint8_t((p>>11)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)}; assert(std::fwrite(rgb,1,3,f)==3); }
    std::fclose(f);
}
}
namespace pocket {
void pixel(int x,int y,uint16_t color) { assert(stub::drawing); if(x>=0&&x<WIDTH&&y>=0&&y<HEIGHT) stub::pixels[y*WIDTH+x]=color; }
uint16_t *framebuffer() { assert(stub::drawing); return stub::pixels.data(); }
void clear(uint16_t color) { assert(stub::drawing); stub::pixels.fill(color); }
void rect(int x,int y,int w,int h,uint16_t color) { assert(stub::drawing); for(int py=std::max(y,0);py<std::min(y+h,HEIGHT);++py) for(int px=std::max(x,0);px<std::min(x+w,WIDTH);++px) pixel(px,py,color); }
void outline(int x,int y,int w,int h,uint16_t c) { rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c); }
void line(int x0,int y0,int x1,int y1,uint16_t c) { int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy; for(;;) { pixel(x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;} } }
int text_width(const char *s,int scale) { return pocket_ui::text_width(s,scale); }
void text(int x,int y,const char *s,uint16_t c,int scale) {
    assert(stub::drawing);if(!s)return;
    const char *scan=s;bool cjk=false;while(*scan)if(pocket_ui::next_utf8(scan)>=0x80)cjk=true;
    const int w=text_width(s,scale);
    if(x<0||y<0||x+w>WIDTH||y+(cjk?10:7)*scale>HEIGHT) {
        std::fprintf(stderr,"text out of bounds: %s at %d,%d width=%d scale=%d\n",s,x,y,w,scale);assert(false);
    }
    assert(!(y<20&&x+w>264));
    pocket_ui::draw_text(x,y,s,scale,[=](int px,int py){pixel(px,py,c);});
}
void redraw() {}
void save() { ++stub::save_calls; }
uint32_t random() { stub::rng^=stub::rng<<13;stub::rng^=stub::rng>>17;stub::rng^=stub::rng<<5;return stub::rng; }
uint32_t millis() { return stub::now; }
void log(const char *s) { stub::logs.emplace_back(s); }
bool held(Key k) { return stub::buttons[int(k)]; }
void tone(float f,float d,float w) { assert(std::isfinite(f)&&std::isfinite(d)&&std::isfinite(w)); stub::tone_frequency=f;stub::tone_duration=d;stub::tone_when=w;if(f>0&&d>0)++stub::tones; }
void silence() {}
}
static uint32_t read32(const uint8_t *p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
static void write32(uint8_t *p,uint32_t v) { for(int i=0;i<4;++i)p[i]=uint8_t(v>>(i*8)); }
static uint32_t hash(const uint8_t *p,size_t n) { uint32_t h=2166136261u;for(size_t i=0;i<n;++i)h=(h^p[i])*16777619u;return h; }
int main(int argc,char **argv) {
    pocket::set_english(std::getenv("POCKET_TEST_EN")!=nullptr);
    const char *art=argc>1?argv[1]:nullptr;
    md_initView(100,150);md_clearView(0,0,0);
    md_drawRect(-2.5f,-1.5f,5.9f,4.9f,255,255,255);
    const uint16_t *source=crisp::view_pixels();
    assert(source[0]==0xffff&&source[1]==0xffff&&source[100]==0xffff&&source[101]==0xffff&&source[2]==0);
    unsigned char grid[6][6][3]{};grid[0][0][0]=255;grid[5][5][1]=255;
    md_drawCharacter(grid,0,0,0);assert(source[0]==pocket::rgb(255,0,0));assert(source[1]==0xffff);assert(source[505]==pocket::rgb(0,255,0));
    md_drawCharacter(grid,-5,-5,0);assert(source[0]==pocket::rgb(0,255,0));
    md_drawRect(FLT_MAX,-FLT_MAX,FLT_MAX,FLT_MAX,1,2,3);
    md_drawRect(NAN,0,10,10,1,2,3);md_drawCharacter(grid,FLT_MAX,0,0);md_drawCharacter(grid,0,NAN,0);
    md_initView(150,150);md_initView(-1,10);assert(crisp::view_width()==100&&crisp::view_height()==150);
    md_drawRect(-FLT_MAX,-FLT_MAX,FLT_MAX,FLT_MAX,1,2,3);
    assert(source[0]==pocket::rgb(0,255,0));
    md_clearScreen(97,97,97);assert(source[0]==pocket::rgb(97,97,97)&&source[14999]==source[0]);
    assert(std::fabs(md_getAudioTime()-float(stub::now)/1000)<0.001f);
    md_playTone(440,.25f,7.5f);assert(stub::tone_frequency==440&&stub::tone_duration==.25f&&stub::tone_when==7.5f);
    std::puts("PASS backend clipping, RGB565, zero-grid transparency, huge/nonfinite geometry, view bounds and audio clock");
    stub::fresh(); stub::draw(); stub::ppm(art,"launcher");
    assert(gameCount==5);
    const int before_menu=ticks;stub::step(500);assert(ticks==before_menu);
    const char *names[]={"cast-n","grappling-h","reflector","color-roll"};
    for(int i=0;i<4;++i) {
        stub::launch(i);assert(crisp_core_state()==1);assert(ticks==1);
        assert(crisp::view_width()==(i==0?150:100));assert(crisp::view_height()==(i==1?150:100));
        stub::step(30);const int draw_tick=ticks;stub::draw();assert(ticks==draw_tick);stub::ppm(art,names[i]);
        const auto *v=crisp::view_pixels();assert(std::count(v,v+crisp::view_width()*crisp::view_height(),v[0])<crisp::view_width()*crisp::view_height());
        game::key(pocket::Key::B);
    }
    std::puts("PASS launches and real simulation for all four games; launcher frozen");

    stub::launch(0);assert(test_cast_state()==0);
    stub::press();stub::step(12);assert(test_cast_state()==1);const float power=test_cast_power();assert(power>1.5f);
    stub::step(5);assert(test_cast_state()==1);assert(test_cast_power()>power);
    stub::release();stub::step();assert(test_cast_state()==2);const float castx=test_cast_x();stub::step(7);assert(test_cast_x()>castx);
    stub::press();stub::step();assert(test_cast_state()==3);stub::release();
    game::key(pocket::Key::B);
    // Touch holds remain captured across MOVE outside the play field, and both
    // UP outside the canvas and CANCEL must deliver the real fishing release.
    for(auto ending:{pocket::Phase::UP,pocket::Phase::CANCEL}) {
        stub::launch(0);game::pointer(pocket::Phase::DOWN,160,120);stub::step(10);assert(test_cast_state()==1);
        game::pointer(pocket::Phase::MOVE,-20,-20);stub::step(3);assert(test_cast_state()==1);
        game::pointer(ending,-20,-20);stub::step();assert(test_cast_state()==2);game::key(pocket::Key::B);
    }
    stub::launch(0);game::pointer(pocket::Phase::DOWN,150,100);game::pointer(pocket::Phase::UP,150,100);
    stub::step();assert(test_cast_state()==1);stub::step();assert(test_cast_state()==2);game::key(pocket::Key::B);
    std::puts("PASS CAST N sustained A/touch, power ramp, release cast, tap pull, quick tap and cancel");

    stub::launch(1);assert(test_grappling_attached());stub::press();stub::step();assert(!test_grappling_attached());
    stub::release();stub::step();assert(input.isJustReleased);test_grappling_offer_anchor();stub::press();stub::step();assert(test_grappling_attached());stub::step(3);assert(test_grappling_attached());stub::release();game::key(pocket::Key::B);
    stub::launch(2);const float dir=test_reflector_direction();stub::press();stub::step(20);assert(test_reflector_direction()==-dir);assert(test_reflector_power()>0.6f);
    const float powered=test_reflector_power();stub::release();stub::step(8);assert(test_reflector_power()<powered);game::key(pocket::Key::B);
    stub::launch(3);assert(test_color_shot()==-999);stub::press();stub::step();assert(test_color_shot()>0);stub::step(120);assert(test_color_shot()==-999);assert(score>=1);stub::release();stub::step();game::key(pocket::Key::B);
    std::puts("PASS GRAPPLING H hook release/grab/hold, REFLECTOR hold power/turn, COLOR ROLL shoot and actual scoring");

    for(int i=0;i<4;++i) {
        stub::launch(i);
        unsigned frames=0;
        while(crisp_core_state()==1 && frames<25000) { stub::step();++frames; }
        assert(crisp_core_state()==2);
        stub::draw();stub::ppm(art,(std::string(names[i])+"-over").c_str());
        uint8_t locale_before[48],locale_after[48];game::serialize(locale_before,48);
        const bool was_english=pocket::english();pocket::set_english(!was_english);stub::draw();
        game::serialize(locale_after,48);assert(!std::memcmp(locale_before,locale_after,48));pocket::set_english(was_english);
        std::printf("PASS natural game-over %s after %u native ticks\n",names[i],frames);
        game::key(pocket::Key::B);
    }

    stub::launch(0);const int before=ticks;stub::step(100,10);assert(ticks-before==60);const int catchup=ticks;stub::step(1,1000000);assert(ticks-catchup==6);
    game::pointer(pocket::Phase::DOWN,280,10);stub::step();assert(ticks>catchup);game::pointer(pocket::Phase::UP,280,10);
    game::pointer(pocket::Phase::DOWN,20,12);const int home_tick=ticks;stub::step();assert(ticks==home_tick);game::pointer(pocket::Phase::UP,20,12);
    // Launcher selection A must not be treated as CAST N's power press.
    stub::fresh();stub::press();stub::step(10);assert(test_cast_state()==0);
    stub::release();stub::step();stub::press();stub::step();assert(test_cast_state()==1);stub::release();game::key(pocket::Key::B);
    std::puts("PASS bounded catch-up and launch-button release gating");

    stub::fresh();uint8_t saved[48],check[48],small[47];
    assert(!game::serialize(small,sizeof(small)));assert(!game::serialize(nullptr,48));
    for(int i=0;i<4;++i) {
        stub::launch(i);score=float((i+1)*123);stub::step();gameOver();stub::step();assert(crisp_core_state()==2);
        const int over=ticks;stub::step(150);assert(crisp_core_state()==2);assert(ticks>over);
        game::key(pocket::Key::B);
    }
    assert(game::serialize(saved,sizeof(saved))==48);
    for(int i=0;i<4;++i)assert(read32(saved+8+i*4)==uint32_t((i+1)*123));
    stub::fresh();game::load(saved,sizeof(saved));game::serialize(check,sizeof(check));assert(!std::memcmp(saved,check,48));
    for(size_t i=0;i<48;++i) { uint8_t bad[48];std::memcpy(bad,saved,48);bad[i]^=0x80;game::load(bad,48);game::serialize(check,48);assert(!std::memcmp(saved,check,48)); }
    for(size_t len=0;len<48;++len)game::load(saved,len);
    game::load(nullptr,48);game::load(saved,49);
    uint8_t bad[48];std::memcpy(bad,saved,48);write32(bad+8,100000000);write32(bad+44,hash(bad,44));game::load(bad,48);game::serialize(check,48);assert(!std::memcmp(saved,check,48));
    std::memcpy(bad,saved,48);bad[5]=4;write32(bad+44,hash(bad,44));game::load(bad,48);game::serialize(check,48);assert(!std::memcmp(saved,check,48));
    std::memcpy(bad,saved,48);write32(bad+24,100000000);write32(bad+44,hash(bad,44));game::load(bad,48);game::serialize(check,48);assert(!std::memcmp(saved,check,48));
    assert(stub::save_calls>0 && stub::tones>0);
    for(const char *needle:{"launch","game-over","score","save-restored","save-rejected"}) {
        assert(std::any_of(stub::logs.begin(),stub::logs.end(),[&](const std::string &s){return s.find(needle)!=std::string::npos;}));
    }
    stub::fresh();stub::launch(0);score=1e30f;game::key(pocket::Key::B);game::serialize(check,48);assert(read32(check+8)==99999999);
    stub::launch(0);score=NAN;game::key(pocket::Key::B);game::serialize(check,48);assert(read32(check+8)==99999999);
    std::puts("PASS per-game high scores, score saturation/NaN, game-over stability, save round-trip, every-byte corruption, malformed lengths/ranges and logs");
    std::printf("PASS all native tests; %u save requests, %u optional tones\n",stub::save_calls,stub::tones);
}
