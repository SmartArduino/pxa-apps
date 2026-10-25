// SPDX-License-Identifier: CC0-1.0
#include "../../common/pocket-play/runtime.hpp"
#include "../core.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
namespace {
struct Buffer {uint32_t before;uint16_t pixels[288*224];uint32_t after;} canvas={0x1745acbd,{},0x837ead39};
bool keys[12],drawing;uint32_t clock_ms;unsigned saves,tones;
}
namespace pocket {
void pixel(int x,int y,uint16_t color){assert(drawing);if(x>=0&&y>=0&&x<288&&y<224)canvas.pixels[y*288+x]=color;}
uint16_t *framebuffer(){assert(drawing);return canvas.pixels;}
void rect(int x,int y,int w,int h,uint16_t color){assert(drawing);for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)pixel(i,j,color);}
void clear(uint16_t color){rect(0,0,288,224,color);}
void outline(int x,int y,int w,int h,uint16_t color){rect(x,y,w,1,color);rect(x,y+h-1,w,1,color);rect(x,y,1,h,color);rect(x+w-1,y,1,h,color);}
void line(int x,int y,int x1,int y1,uint16_t color){const int dx=abs(x1-x),dy=-abs(y1-y),sx=x<x1?1:-1,sy=y<y1?1:-1;int error=dx+dy;for(;;){pixel(x,y,color);if(x==x1&&y==y1)break;const int two=error*2;if(two>=dy){error+=dy;x+=sx;}if(two<=dx){error+=dx;y+=sy;}}}
void text(int x,int y,const char *value,uint16_t color,int scale){assert(drawing);pocket_ui::draw_text(x,y,value,scale,[&](int px,int py){pixel(px,py,color);});}
int text_width(const char *text,int scale){return pocket_ui::text_width(text,scale);}
void redraw(){assert(!drawing);}void save(){assert(!drawing);++saves;}uint32_t millis(){return clock_ms;}uint32_t random(){return 42;}void log(const char *){}
bool held(Key key){return keys[unsigned(key)];}
void tone(float f,float duration,float when){assert(!drawing&&f>0&&duration>0&&duration<4&&when>=0);++tones;}void silence(){}
}
static elements::State snapshot(){uint8_t data[2048];size_t size=game::serialize(data,sizeof(data));assert(size==elements::SAVE_SIZE);elements::State s;assert(elements::decode(s,data,size));return s;}
static void draw(){uint8_t a[2048],b[2048];game::serialize(a,sizeof(a));drawing=true;game::draw();drawing=false;game::serialize(b,sizeof(b));assert(!memcmp(a,b,elements::SAVE_SIZE));assert(canvas.before==0x1745acbd&&canvas.after==0x837ead39);}
static void tick(unsigned n){while(n--){clock_ms+=20;game::tick(20);}}
static void tap(int x,int y){game::pointer(pocket::Phase::DOWN,x,y);game::pointer(pocket::Phase::UP,x,y);}
int main(int argc,char **argv){assert(argc==2);using namespace elements;using pocket::Key;using pocket::Phase;
    game::init();draw();assert(snapshot().ui==HELP&&!snapshot().muted);
    game::key(Key::Y);assert(snapshot().muted);const unsigned muted_tones=tones;tick(100);assert(tones==muted_tones);
    uint8_t saved[2048];const size_t saved_size=game::serialize(saved,sizeof(saved));game::init();game::load(saved,saved_size);assert(snapshot().muted);tick(100);assert(tones==muted_tones);
    tap(210,195);assert(!snapshot().muted);tick(30);assert(tones>muted_tones);draw();pocket::set_english(true);draw();pocket::set_english(false);
    game::key(Key::A);tap(267,171);assert(snapshot().ui==CONFIRM);draw();tap(80,180);assert(snapshot().paused&&count(snapshot(),SAND)==0);
    // Touch palette and size live above the Host bottom navigation region.
    tap(94,199);tap(267,76);tap(267,76);assert(snapshot().material==SAND&&snapshot().brush==0);
    game::pointer(Phase::DOWN,32,41);game::pointer(Phase::MOVE,244,182);game::pointer(Phase::UP,900,-200);State s=snapshot();assert(count(s,SAND)>=71);draw();
    game::key(Key::X);unsigned particles=count(snapshot(),SAND);game::pointer(Phase::DOWN,140,61);tick(80);assert(count(snapshot(),SAND)>particles+5);game::pointer(Phase::CANCEL,500,500);particles=count(snapshot(),SAND);tick(80);assert(count(snapshot(),SAND)==particles);
    game::pointer(Phase::DOWN,130,61);tick(20);game::pointer(Phase::UP,-100,999);particles=count(snapshot(),SAND);tick(30);assert(count(snapshot(),SAND)==particles);
    // Controller held A paints continuously without press repeats.
    keys[unsigned(Key::A)]=true;tick(70);keys[unsigned(Key::A)]=false;tick(1);assert(count(snapshot(),SAND)>particles+5);
    game::key(Key::X);s=snapshot();tick(100);assert(snapshot().steps==s.steps);draw();
    game::reset();assert(snapshot().ui==CONFIRM);game::key(Key::B);assert(!memcmp(s.cells,snapshot().cells,CELLS));
    game::key(Key::SELECT);draw();game::key(Key::DOWN);game::key(Key::A);draw();game::key(Key::LEFT);game::key(Key::A);s=snapshot();assert(s.scene==1&&s.paused);draw();
    // All drawing and controller edges are bounded at four world corners.
    for(int i=0;i<100;++i){game::key(Key::LEFT);game::key(Key::UP);}keys[unsigned(Key::A)]=true;tick(10);draw();for(int i=0;i<100;++i){game::key(Key::RIGHT);game::key(Key::DOWN);}tick(10);draw();keys[unsigned(Key::A)]=false;tick(1);
    const unsigned writes=saves;game::key(Key::X);tick(100);assert(saves==writes+1); // pause toggle is durable; no per-tick saves
    char path[1024];snprintf(path,sizeof(path),"%s/app-results.json",argv[1]);FILE *f=fopen(path,"w");assert(f);fprintf(f,"{\"passed\":true,\"save_bytes\":1780,\"checks\":[\"draw_only_and_state_pure\",\"framebuffer_canaries\",\"footer_safe_area\",\"touch_interpolation\",\"continuous_held_touch\",\"outside_UP_and_CANCEL_release\",\"controller_held_A\",\"pause_freezes_steps\",\"reset_confirmation_cancel\",\"task_confirmation\",\"edge_bounds\",\"no_per_tick_storage\"]}\n");fclose(f);puts("PASS Tiny Elements app input, drawing and storage policy");}
