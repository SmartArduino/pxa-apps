// SPDX-License-Identifier: CC0-1.0
// Native adapter: mirrors the shared runtime's CC0 5x7 font and clipped canvas.
#include "../../common/pocket-play/runtime.hpp"
#include "../core.hpp"
#include "../motion.hpp"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <initializer_list>
#include <utility>
namespace {
uint16_t canvas[480*480];bool keys[12];bool portrait_test=false;uint32_t clock_ms;unsigned saves,tones,music_tones,menu_requests;int focus_test,screen_test;
constexpr char names[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-:./!?+()<>#=%";
constexpr uint8_t glyph[][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
{14,17,17,15,1,1,14},{0,0,0,31,0,0,0},{0,4,4,0,4,4,0},
{0,0,0,0,0,12,12},{1,2,2,4,8,8,16},{4,4,4,4,4,0,4},
{14,17,1,2,4,0,4},{0,4,4,31,4,4,0},{2,4,8,8,8,4,2},
{8,4,2,2,2,4,8},{2,4,8,16,8,4,2},{8,4,2,1,2,4,8},
{10,10,31,10,31,10,10},{0,0,31,0,31,0,0},{25,25,2,4,8,19,19}};
}
namespace pocket {
void pixel(int x,int y,uint16_t c) {if(x>=0&&y>=0&&x<WIDTH&&y<HEIGHT)canvas[y*WIDTH+x]=c;}
uint16_t *framebuffer(){return canvas;}
void rect(int x,int y,int w,int h,uint16_t c){for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)pixel(i,j,c);}
void clear(uint16_t c){rect(0,0,WIDTH,HEIGHT,c);}
void outline(int x,int y,int w,int h,uint16_t c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
void line(int x,int y,int x1,int y1,uint16_t c){int dx=abs(x1-x),sx=x<x1?1:-1,dy=-abs(y1-y),sy=y<y1?1:-1,e=dx+dy;for(;;){pixel(x,y,c);if(x==x1&&y==y1)break;int e2=e*2;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}}
int text_width(const char *s,int scale) { return pocket_ui::text_width(s,scale); }
void text(int x,int y,const char *s,uint16_t c,int scale) {
    if(!s)return;
    const char *scan=s;bool cjk=false;while(*scan)if(pocket_ui::next_utf8(scan)>=0x80)cjk=true;
    const int w=text_width(s,scale);
    if(x<0||y<0||x+w>WIDTH||y+(cjk?10:7)*scale>HEIGHT) {
        fprintf(stderr,"text out of bounds: %s at %d,%d width=%d scale=%d\n",s,x,y,w,scale);assert(false);
    }
    assert(!(y<20&&x+w>WIDTH-24));
    pocket_ui::draw_text(x,y,s,scale,[=](int px,int py){pixel(px,py,c);});
}
void redraw(){}void save(){++saves;}uint32_t random(){return 7;}uint32_t millis(){return clock_ms;}void log(const char *s){int f,sc;if(sscanf(s,"Marble: focus=%d screen=%d",&f,&sc)==2){focus_test=f;screen_test=sc;}}
bool portrait_display(){return portrait_test;}void open_menu(){++menu_requests;}
bool held(Key k){return keys[unsigned(k)];}void tone(float f,float d,float when){assert(f>0&&f<8000&&d>0&&d<=4);++tones;if(when>float(millis())/1000.f+.035f)++music_tones;}void silence(){}
}
static void tick(unsigned n){while(n--){clock_ms+=20;game::tick(20);}}
static marble::State snapshot(){uint8_t data[256];size_t n=game::serialize(data,sizeof(data));marble::State s;assert(marble::load(s,data,n>=88?88:n));return s;}
static void apply(const marble::State &s){uint8_t data[256];size_t n=marble::serialize(s,data,sizeof(data));marble::State valid;assert(marble::load(valid,data,n));game::load(data,n);}
static void image(const char *root,const char *name){game::draw();char path[1024];snprintf(path,sizeof(path),"%s/%s.ppm",root,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",pocket::WIDTH,pocket::HEIGHT);for(int i=0;i<pocket::WIDTH*pocket::HEIGHT;++i) { uint16_t p=canvas[i];{unsigned char rgb[3]={uint8_t((p>>11)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};fwrite(rgb,1,3,f);}}fclose(f);}
static void motion_hooks(){
    using pocket::Key;using pocket::Phase;using namespace marble_motion;
    game::init();game::key(Key::A);device.status=Status::Streaming;device.request(true);device.now=1000000;
    const int32_t flat[]={0,0,9806650};
    for(unsigned i=0;i<20;++i){device.now+=50000;device.tilt.acceleration(flat,device.now,device.now);}
    assert(device.ready());auto before=snapshot();game::tick(20);assert(snapshot().run.x==before.run.x&&snapshot().run.y==before.run.y);
    const int32_t right[]={0,-4000000,8950000};
    for(unsigned i=0;i<10;++i){device.now+=50000;device.tilt.acceleration(right,device.now,device.now);game::tick(20);}
    assert(snapshot().run.x>before.run.x+.02f);before=snapshot();device.now+=1000000;game::tick(100);assert(snapshot().run.elapsed_ms==before.run.elapsed_ms&&snapshot().run.x==before.run.x);
    keys[unsigned(Key::RIGHT)]=true;tick(10);keys[unsigned(Key::RIGHT)]=false;assert(snapshot().run.elapsed_ms>before.run.elapsed_ms);
    game::key(Key::Y);before=snapshot();tick(20);assert(snapshot().run.elapsed_ms==before.run.elapsed_ms);
    game::pointer(Phase::DOWN,120,80);game::pointer(Phase::UP,120,80);assert(device.tilt.calibrating&&!device.tilt.calibrated);
    for(unsigned i=0;i<20;++i){device.now+=50000;device.tilt.acceleration(flat,device.now,device.now);}assert(device.ready());
    game::pointer(Phase::DOWN,100,60);game::pointer(Phase::CANCEL,100,60);game::pointer(Phase::UP,100,60);assert(device.options.enabled);
    auto tap=[](int y){game::pointer(Phase::DOWN,120,y);game::pointer(Phase::UP,120,y);};
    tap(60);tap(104);tap(126);tap(148);assert(!device.options.enabled&&device.options.sensitivity==2&&device.options.rotation==1&&device.options.mirror);
    for(bool english:{false,true}){pocket::set_english(english);game::draw();}pocket::set_english(false);
    uint8_t saved[256],after[256],corrupt[256];size_t n=game::serialize(saved,sizeof(saved));assert(n==104&&game::serialize(after,103)==0);
    game::init();game::load(saved,n);assert(!device.options.enabled&&device.options.sensitivity==2&&device.options.rotation==1&&device.options.mirror);
    for(unsigned i=0;i<n;++i){memcpy(corrupt,saved,n);corrupt[i]^=128;game::load(corrupt,n);assert(game::serialize(after,sizeof(after))==n&&!memcmp(saved,after,n));}
    uint8_t legacy[88];assert(marble::serialize(snapshot(),legacy,sizeof(legacy))==88);game::load(legacy,88);assert(device.options.enabled&&device.options.sensitivity==1&&device.options.rotation==0&&!device.options.mirror);
    puts("PASS motion app hooks: real tilt movement, stale freeze/manual takeover, settings, cancel, bilingual bounds, options persistence/corruption and v1 migration");
}
static void choose_key(int action){
    for(int i=0;i<16&&focus_test!=action;++i)game::key(pocket::Key::DOWN);
    assert(focus_test==action);game::key(pocket::Key::A);game::draw();
}
static void three_key_hooks(const char *root){
    using namespace marble_motion;using pocket::Key;
    for(bool en:{false,true}){
        pocket::set_english(en);portrait_test=true;game::init();game::draw();
        assert(focus_test==0&&screen_test==0);choose_key(6);assert(screen_test==3);
        choose_key(25);assert(device.options.navigation==1);choose_key(25);assert(device.options.navigation==0);
        choose_key(26);assert(screen_test==0);choose_key(0);assert(screen_test==1&&focus_test==7);
        device.status=Status::Streaming;device.now=1000000;
        const int32_t flat[]={0,0,9806650};
        for(unsigned i=0;i<20;++i){device.now+=50000;device.tilt.acceleration(flat,device.now,device.now);}
        assert(device.ready());auto before=snapshot();
        keys[unsigned(Key::DOWN)]=true;for(unsigned i=0;i<5;++i){game::key(Key::DOWN);tick(2);}keys[unsigned(Key::DOWN)]=false;
        keys[unsigned(Key::UP)]=true;for(unsigned i=0;i<5;++i){game::key(Key::UP);tick(2);}keys[unsigned(Key::UP)]=false;
        assert(snapshot().run.x==before.run.x&&snapshot().run.y==before.run.y);
        choose_key(7);image(root,en?"three-brake-en":"three-brake-zh");choose_key(7);
        choose_key(3);assert(screen_test==2);before=snapshot();
        for(int id=11;id<=17;++id){choose_key(id);assert(screen_test==2&&snapshot().run.level==before.run.level);}
        choose_key(1);assert(screen_test==1);choose_key(4);assert(snapshot().run.elapsed_ms==0);
        choose_key(6);choose_key(21);assert(device.tilt.calibrating);choose_key(22);choose_key(23);choose_key(24);choose_key(25);
        assert(device.options.navigation==1&&device.options.sensitivity==2&&device.options.rotation==1&&device.options.mirror);
        image(root,en?"three-settings-en":"three-settings-zh");choose_key(26);choose_key(8);assert(menu_requests);
        uint8_t data[104],old[104];assert(game::serialize(data,sizeof(data))==104&&data[92]==2&&data[97]==1);
        game::init();game::load(data,104);assert(device.options.navigation==1&&focus_test==7);choose_key(3);choose_key(2);assert(screen_test==0);choose_key(0);
        // The 1.5.0 extension migrates transactionally with default Auto navigation.
        memcpy(old,data,104);old[92]=1;old[97]=0;uint32_t hash=2166136261u;
        for(unsigned i=0;i<100;++i)hash=(hash^old[i])*16777619u;
        for(unsigned i=0;i<4;++i)old[100+i]=uint8_t(hash>>(8*i));
        game::load(old,104);assert(device.options.navigation==0&&device.options.rotation==1&&device.options.mirror);
        assert(game::serialize(data,sizeof(data))==104&&data[92]==2&&data[97]==0);
        // Saved Gamepad mode cannot strand a portrait Gen4 user.
        device.options.navigation=2;choose_key(3);assert(screen_test==2);choose_key(1);assert(screen_test==1);
        // Win/next/islands controls are reachable with the same three keys.
        auto won=snapshot();won.run.x=5.5f;won.run.y=1.5f;won.run.vx=won.run.vy=0;won.run.coins=7;apply(won);
        device.options.enabled=false;tick(1);assert(snapshot().run.phase==marble::Phase::WON);
        image(root,en?"three-win-en":"three-win-zh");choose_key(6);choose_key(26);assert(focus_test==5);choose_key(3);assert(screen_test==2);choose_key(1);assert(focus_test==5);choose_key(5);assert(snapshot().run.level==1);
    }
    portrait_test=false;
    puts("PASS Gen4 three keys: no accidental steering, brake, all eight cards, locked level, retry, settings, back/help/menu, win/next, bilingual bounds, v1.5 migration and portrait safety");
}
static void responsive_hooks(const char *root){
    for(auto size:{std::pair<int,int>{288,341},{288,384},{336,224},{288,288},{288,234},{288,432}}){
        pocket::WIDTH=size.first;pocket::HEIGHT=size.second;portrait_test=pocket::HEIGHT>=pocket::WIDTH;
        for(bool en:{false,true}){
            pocket::set_english(en);game::init();game::draw();game::key(pocket::Key::A);game::draw();
            if(portrait_test){choose_key(6);choose_key(26);choose_key(3);}
            else {game::key(pocket::Key::Y);game::draw();game::key(pocket::Key::B);game::key(pocket::Key::B);}
            game::draw();game::pointer(pocket::Phase::DOWN,12,48);game::pointer(pocket::Phase::UP,12,48);assert(snapshot().run.level==0);
            game::draw();char name[80];snprintf(name,sizeof(name),"fit-%dx%d-%s",pocket::WIDTH,pocket::HEIGHT,en?"en":"zh");image(root,name);
        }
    }
    pocket::WIDTH=288;pocket::HEIGHT=224;portrait_test=false;
    puts("PASS responsive drawing and touch: six aspects, both languages, measured font bounds and card hit testing");
}
int main(int argc,char **argv){
    assert(argc==2);pocket::set_english(std::getenv("POCKET_TEST_EN")!=nullptr);game::init();image(argv[1],"app-tutorial");
    // The idle tutorial is silent while all game/save bytes remain frozen.
    uint8_t audio_before[256],audio_after[256];size_t audio_size=game::serialize(audio_before,sizeof(audio_before));
    const unsigned intro_music=music_tones;tick(100);assert(music_tones==intro_music);
    assert(game::serialize(audio_after,sizeof(audio_after))==audio_size);assert(!memcmp(audio_before,audio_after,audio_size));
    game::key(pocket::Key::A);marble::State before=snapshot();
    keys[unsigned(pocket::Key::UP)]=true;tick(20);keys[unsigned(pocket::Key::UP)]=false;
    marble::State after=snapshot();assert(after.run.y<before.run.y-.2f);assert(after.run.x==before.run.x);image(argv[1],"app-race");
    // Controller held state moves with no repeated press edges; brake slows that motion.
    keys[unsigned(pocket::Key::A)]=true;tick(10);keys[unsigned(pocket::Key::A)]=false;assert(fabsf(snapshot().run.vy)<fabsf(after.run.vy)*.15f);
    // Visible control centers stay above the host's bottom navigation strip.
    before=snapshot();game::pointer(pocket::Phase::DOWN,253,170);tick(10);game::pointer(pocket::Phase::UP,253,170);assert(fabsf(snapshot().run.vy)<fabsf(before.run.vy)*.15f);
    game::pointer(pocket::Phase::DOWN,43,203);game::pointer(pocket::Phase::UP,43,203);before=snapshot();tick(5);assert(snapshot().run.elapsed_ms==before.run.elapsed_ms);
    game::pointer(pocket::Phase::DOWN,45,203);game::pointer(pocket::Phase::UP,45,203);tick(5);assert(snapshot().run.elapsed_ms>before.run.elapsed_ms);
    game::pointer(pocket::Phase::DOWN,125,203);game::pointer(pocket::Phase::UP,125,203);assert(snapshot().run.elapsed_ms==0);
    // Menu presses only activate on matching UP; CANCEL and outside UP preserve the run.
    game::reset();before=snapshot();game::pointer(pocket::Phase::DOWN,43,203);game::pointer(pocket::Phase::CANCEL,43,203);tick(5);assert(snapshot().run.elapsed_ms>before.run.elapsed_ms);
    game::key(pocket::Key::B);before=snapshot();game::pointer(pocket::Phase::DOWN,160,60);game::pointer(pocket::Phase::CANCEL,160,60);tick(5);assert(snapshot().run.elapsed_ms==before.run.elapsed_ms&&snapshot().run.level==before.run.level);
    game::pointer(pocket::Phase::DOWN,22,60);game::pointer(pocket::Phase::UP,500,400);tick(5);assert(snapshot().run.elapsed_ms==before.run.elapsed_ms);
    game::key(pocket::Key::B);
    game::reset();before=snapshot();game::pointer(pocket::Phase::DOWN,120,120);game::pointer(pocket::Phase::MOVE,150,120);tick(20);after=snapshot();assert(after.run.x>before.run.x+.2f);
    game::pointer(pocket::Phase::CANCEL,150,120);float old_v=snapshot().run.vx;tick(15);assert(snapshot().run.vx<old_v*.55f);
    // An UP well outside the viewport still ends the captured analog gesture.
    game::reset();game::pointer(pocket::Phase::DOWN,100,120);game::pointer(pocket::Phase::MOVE,130,120);tick(10);game::pointer(pocket::Phase::UP,500,-200);old_v=snapshot().run.vx;tick(15);assert(snapshot().run.vx<old_v*.55f);
    game::reset();game::key(pocket::Key::B);image(argv[1],"app-islands");before=snapshot();tick(100);assert(snapshot().run.elapsed_ms==before.run.elapsed_ms);
    game::key(pocket::Key::RIGHT);game::key(pocket::Key::A);assert(snapshot().run.level==0);image(argv[1],"app-locked");game::key(pocket::Key::B);
    // Runtime/native integration can restore a legitimate just-before-goal save.
    marble::State won=snapshot();won.run.x=5.5f;won.run.y=1.5f;won.run.vx=won.run.vy=0;won.run.coins=7;apply(won);tick(1);won=snapshot();assert(won.run.phase==marble::Phase::WON);assert(won.progress.unlocked==2);assert(tones>=3);image(argv[1],"app-win");
    game::key(pocket::Key::A);assert(snapshot().run.level==1);game::key(pocket::Key::B);game::pointer(pocket::Phase::DOWN,22,60);game::pointer(pocket::Phase::UP,22,60);assert(snapshot().run.level==0);
    before=snapshot();uint8_t data[256];size_t n=game::serialize(data,sizeof(data));data[20]^=64;game::load(data,n);after=snapshot();assert(after.run.x==before.run.x&&after.run.level==before.run.level&&after.progress.unlocked==before.progress.unlocked);
    marble::State all=snapshot();all.progress.unlocked=8;
    for(unsigned i=0;i<8;++i){all.progress.medals[i]=1;all.progress.best_ms[i]=3600000;}
    for(unsigned level=0;level<8;++level) {
        assert(marble::start(all,level));all.run.elapsed_ms=3600000;apply(all);
        char name[32];snprintf(name,sizeof(name),"app-level-%u-max-time",level+1);image(argv[1],name);
    }
    game::key(pocket::Key::B);image(argv[1],"app-all-islands");
    game::pointer(pocket::Phase::DOWN,214,163);game::pointer(pocket::Phase::UP,214,163);assert(snapshot().run.level==7);apply(all);
    uint8_t locale_before[256],locale_after[256];size_t locale_size=game::serialize(locale_before,sizeof(locale_before));
    const bool was_english=pocket::english();pocket::set_english(!was_english);game::draw();
    assert(game::serialize(locale_after,sizeof(locale_after))==locale_size);assert(!memcmp(locale_before,locale_after,locale_size));pocket::set_english(was_english);
    all.run.phase=marble::Phase::FALLING;all.run.fall_ms=100;apply(all);image(argv[1],"app-falling");
    all.run.phase=marble::Phase::WON;all.run.fall_ms=0;all.run.vx=all.run.vy=0;all.run.coins=uint16_t((1u<<marble::coin_count(7))-1);
    for(int y=0;y<marble::SIZE;++y)for(int x=0;x<marble::SIZE;++x)if(marble::cell(7,x,y)=='G'){all.run.x=x+.5f;all.run.y=y+.5f;}
    apply(all);image(argv[1],"app-complete");
    motion_hooks();
    three_key_hooks(argv[1]);
    responsive_hooks(argv[1]);
    assert(saves>5);printf("PASS: game hooks, held arrows, analog drag, CANCEL/outside UP, brake, selector freeze, unlock, save reload, all-level/max-time layout and locale-save invariance\n");
}
