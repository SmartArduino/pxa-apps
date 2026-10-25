// SPDX-License-Identifier: MIT
#include "../core.hpp"
#include "../../common/pocket-play/runtime.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace test_runtime { extern bool drawing, keys[12]; extern uint32_t clock_ms; extern unsigned saves, logs, music_tones; void image(const char *); }
using garden::State;
using garden::Mode;
using garden::Event;
using pocket::Key;
using pocket::Phase;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); assert(x); } } while(0)
static void advance(State &s,unsigned ms) { while(ms) { unsigned n=ms>100?100:ms; garden::step(s,n); ms-=n; } }
static void live(unsigned ms) { while(ms) { unsigned n=ms>100?100:ms; test_runtime::clock_ms+=n; game::tick(n); ms-=n; } }
static State current() {
    uint8_t b[garden::SAVE_BYTES]; State s; CHECK(game::serialize(b,sizeof(b))==sizeof(b)); CHECK(garden::decode(s,b,sizeof(b))); return s;
}
static void restore(const State &s) { uint8_t b[garden::SAVE_BYTES]; CHECK(garden::encode(s,b,sizeof(b))==sizeof(b)); game::load(b,sizeof(b)); }
static void same(const State &a,const State &b) {
    uint8_t x[garden::SAVE_BYTES],y[garden::SAVE_BYTES]; CHECK(garden::encode(a,x,sizeof(x))==sizeof(x)); CHECK(garden::encode(b,y,sizeof(y))==sizeof(y)); CHECK(!memcmp(x,y,sizeof(x)));
}
static void roundtrip(const State &s) { uint8_t b[garden::SAVE_BYTES]; State t; CHECK(garden::encode(s,b,sizeof(b))==sizeof(b)); CHECK(garden::decode(t,b,sizeof(b))); same(s,t); }
static void rhythm(State &s,bool perfect) {
    garden::start(s,1);
    for(unsigned i=0;i<8;++i) {
        unsigned at=1500+i*1200;
        while(s.game_ms<at) garden::step(s,100);
        Event e=garden::beat(s); CHECK(e==(perfect?Event::BEAT:Event::NONE));
        if(perfect) CHECK(garden::beat(s)==Event::NONE);
        else break;
    }
    while(s.mode==Mode::RHYTHM) garden::step(s,100);
}
static void snapshot(const char *root,const char *name,const State &s) {
    restore(s); test_runtime::drawing=true; game::draw(); test_runtime::drawing=false;
    char path[1024]; snprintf(path,sizeof(path),"%s/%s.ppm",root,name); test_runtime::image(path);
}
static void rehash(uint8_t *p) {
    uint32_t h=2166136261u; for(size_t i=0;i<garden::SAVE_BYTES-4;++i) h=(h^p[i])*16777619u;
    for(unsigned i=0;i<4;++i) p[garden::SAVE_BYTES-4+i]=uint8_t(h>>(i*8));
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    pocket::set_english(getenv("POCKET_TEST_EN")!=nullptr);
    State s; garden::fresh(s);
    CHECK(s.growth==0&&s.stage==0&&s.coins==8&&s.snacks==5); roundtrip(s);
    CHECK(garden::action(s,garden::Action::FEED)==Event::FED); CHECK(s.food==100&&s.snacks==4&&s.growth==7);
    CHECK(garden::action(s,garden::Action::FEED)==Event::WAIT); CHECK(s.growth==7);
    CHECK(garden::action(s,garden::Action::WASH)==Event::WASHED); CHECK(s.growth==14);
    advance(s,5000); CHECK(garden::action(s,garden::Action::WASH)==Event::WASHED); CHECK(s.growth==21);
    // Real actions, with their actual cooldowns, reach every evolution branch.
    State moss,moon,sun;
    for(unsigned i=0;i<18;++i) {
        Event e=garden::action(moss,garden::Action::WASH); CHECK(e==Event::WASHED||e==Event::EVOLVED);
        if(i==6) CHECK(moss.stage==1&&moss.growth==49);
        if(i!=17) advance(moss,5000);
    }
    CHECK(moss.growth==126&&moss.stage==2&&moss.branch==1&&moss.active_ms==85000); roundtrip(moss);
    for(unsigned i=0;i<40;++i) { garden::action(moon,garden::Action::PET); if(i!=39) advance(moon,3000); }
    CHECK(moon.growth==120&&moon.stage==2&&moon.branch==3&&moon.active_ms==117000); roundtrip(moon);
    for(unsigned i=0;i<7;++i) rhythm(sun,true);
    CHECK(sun.growth==126&&sun.stage==2&&sun.branch==2&&sun.rounds==7); CHECK(sun.best_rhythm==8&&sun.coins==78); roundtrip(sun);
    // Catch mechanics, rewards, moving drops and an interrupted game resume.
    State catcher; garden::start(catcher,0); bool resumed=false;
    while(catcher.mode==Mode::CATCH) {
        int best=-1;
        for(unsigned i=0;i<5;++i) if(catcher.drops[i].active&&(best<0||catcher.drops[i].y>catcher.drops[best].y)) best=int(i);
        if(best>=0) catcher.basket_x=catcher.drops[best].x;
        garden::step(catcher,100);
        if(catcher.game_ms==7000) { roundtrip(catcher); resumed=true; }
    }
    CHECK(resumed&&catcher.rounds==1&&catcher.score>8&&catcher.best_catch==catcher.score);
    CHECK(catcher.reward==2+catcher.score/2&&catcher.coins==8+catcher.reward&&catcher.growth==10+catcher.score);
    State rewarded=catcher; advance(catcher,1000); CHECK(catcher.coins==rewarded.coins&&catcher.rounds==1&&catcher.growth==rewarded.growth);
    roundtrip(catcher);
    // Real frame intervals rarely divide a game's duration exactly. A final
    // variable tick must leave an encodable, atomically reloadable reward and
    // evolution, then continue without awarding that reward a second time.
    for(unsigned which=0;which<2;++which) {
        State jitter; jitter.growth=40; jitter.affinity[0]=40;
        garden::start(jitter,uint8_t(which));
        const unsigned duration=which?11200:15000;
        jitter.game_ms=duration-8;
        if(which) { jitter.score=3; jitter.beat_index=8; jitter.attempts=8; }
        else jitter.score=8;
        roundtrip(jitter);
        CHECK(garden::step(jitter,23)==Event::EVOLVED);
        CHECK(jitter.game_ms==duration&&jitter.mode==Mode::RESULT);
        CHECK(jitter.growth==50+jitter.score&&jitter.stage==1&&jitter.branch==0&&jitter.rounds==1);
        CHECK(jitter.reward==(which?5:6)&&jitter.coins==8+jitter.reward);
        roundtrip(jitter);
        uint8_t finished[garden::SAVE_BYTES]; CHECK(garden::encode(jitter,finished,sizeof(finished))==sizeof(finished));
        State reloaded=moon; CHECK(garden::decode(reloaded,finished,sizeof(finished))); same(jitter,reloaded);
        unsigned coins=reloaded.coins,growth=reloaded.growth;
        for(unsigned tick=0;tick<10;++tick) garden::step(reloaded,tick%2?17:23);
        CHECK(reloaded.coins==coins&&reloaded.growth==growth&&reloaded.rounds==1);
        roundtrip(reloaded);
    }
    // A consumed early beat cannot be turned into a hit by repeated presses.
    State early; garden::start(early,1); advance(early,1000); CHECK(garden::beat(early)==Event::MISSED); advance(early,500); CHECK(garden::beat(early)==Event::NONE); CHECK(early.score==0&&early.attempts==1);
    State missed; garden::start(missed,1); advance(missed,11200); CHECK(missed.mode==Mode::RESULT&&missed.score==0&&missed.reward==2&&missed.growth==10);
    // Free harvest, rest, decoration purchase and persistent ownership.
    State cozy; advance(cozy,30000); CHECK(cozy.snacks==6&&cozy.active_ms==30000);
    cozy.energy=90; CHECK(garden::action(cozy,garden::Action::REST)==Event::RESTING); advance(cozy,5000); CHECK(cozy.energy==100&&!cozy.sleeping&&cozy.growth==6);
    CHECK(garden::decorate(cozy,0)==Event::BOUGHT&&cozy.coins==2&&cozy.decor_owned==1&&cozy.decor_on==1);
    CHECK(garden::decorate(cozy,0)==Event::TOGGLED&&cozy.decor_on==0&&cozy.coins==2);
    CHECK(garden::decorate(cozy,3)==Event::POOR&&cozy.coins==2); roundtrip(cozy);
    State neglected=moon; unsigned retained_growth=neglected.growth; advance(neglected,1800000);
    CHECK(neglected.food==0&&neglected.clean==0&&neglected.energy==0&&neglected.growth==retained_growth&&neglected.stage==2&&neglected.branch==3);
    State abandoned; garden::start(abandoned,0); advance(abandoned,5000); garden::home(abandoned);
    CHECK(abandoned.coins==8&&abandoned.growth==0&&abandoned.rounds==0); roundtrip(abandoned);
    // Every changed bit, bad length/version and authenticated invalid fields fail
    // without changing the destination state.
    uint8_t good[garden::SAVE_BYTES],bad[garden::SAVE_BYTES]; CHECK(garden::encode(sun,good,sizeof(good))==sizeof(good));
    State target=moon;
    for(size_t i=0;i<sizeof(good);++i) for(unsigned bit=0;bit<8;++bit) { memcpy(bad,good,sizeof(bad)); bad[i]^=uint8_t(1u<<bit); CHECK(!garden::decode(target,bad,sizeof(bad))); same(target,moon); }
    CHECK(!garden::decode(target,good,sizeof(good)-1)); CHECK(!garden::decode(target,nullptr,0));
    CHECK(garden::encode(sun,bad,sizeof(bad)-1)==0);
    const unsigned offsets[]={4,8,12,13,14,15,16,17,18,19,20,21,23,25,27,34,36,44,48,51,53,56,60,66,68,70,72,73,75,77,78,79,105,110};
    for(unsigned off:offsets) { memcpy(bad,good,sizeof(bad)); bad[off]=255; rehash(bad); CHECK(!garden::decode(target,bad,sizeof(bad))); same(target,moon); }
    // Hooks run against the native runtime, including touch cancel, held movement,
    // load rejection, periodic save and reset returning home without erasing a pet.
    game::init(); CHECK(!strcmp(game::title(),pocket::tr("Pet Garden","宠物花园"))&&!strcmp(game::app_id(),"pxa-pet-garden"));
    State initial=current();
    // An idle tick schedules no audio and adds no save fields.
    const unsigned first_music=test_runtime::music_tones;
    game::tick(0);CHECK(test_runtime::music_tones==first_music);same(initial,current());
    game::pointer(Phase::DOWN,144,120); game::pointer(Phase::CANCEL,144,120); game::pointer(Phase::UP,144,120); same(initial,current());
    game::pointer(Phase::DOWN,144,120); game::pointer(Phase::UP,-1,120); same(initial,current());
    game::pointer(Phase::DOWN,144,120); game::pointer(Phase::UP,144,120); CHECK(current().growth==3);
    game::key(Key::RIGHT); game::key(Key::A); CHECK(current().growth==10);
    game::key(Key::UP); CHECK(current().room==1);
    game::key(Key::X); CHECK(current().mode==Mode::GAMES); game::key(Key::A); CHECK(current().mode==Mode::CATCH);
    test_runtime::keys[unsigned(Key::RIGHT)]=true; live(400); test_runtime::keys[unsigned(Key::RIGHT)]=false; CHECK(current().basket_x>200);
    game::pointer(Phase::DOWN,45,160); game::pointer(Phase::MOVE,70,160); CHECK(current().basket_x==70); game::pointer(Phase::CANCEL,70,160);
    State before_reset=current(); game::reset(); State after_reset=current(); CHECK(after_reset.mode==Mode::HOME&&after_reset.growth==before_reset.growth&&after_reset.coins==before_reset.coins&&after_reset.snacks==before_reset.snacks&&after_reset.room==1);
    CHECK(garden::encode(after_reset,good,sizeof(good))==sizeof(good)); memcpy(bad,good,sizeof(good)); bad[8]^=128; game::load(bad,sizeof(bad)); same(after_reset,current());
    unsigned saves=test_runtime::saves; live(5000); CHECK(test_runtime::saves>saves&&test_runtime::logs>5);
    // Framebuffer evidence uses production draw() and asserts all text is in bounds.
    State home; snapshot(argv[1],"home",home); home.room=1; home.decor_owned=home.decor_on=15; snapshot(argv[1],"garden-decor",home);
    garden::home(moss); snapshot(argv[1],"mossbun",moss); garden::home(moon); snapshot(argv[1],"moonmoth",moon);
    garden::home(sun); snapshot(argv[1],"sunfox",sun);
    home.mode=Mode::GAMES; home.selection=1; snapshot(argv[1],"games",home);
    garden::start(home,0); advance(home,5000); snapshot(argv[1],"catch",home);
    garden::start(home,1); advance(home,1400); snapshot(argv[1],"bloom",home);
    snapshot(argv[1],"result",rewarded);
    home.mode=Mode::DECOR; home.selection=2; snapshot(argv[1],"decor",home);
    home.mode=Mode::HELP; snapshot(argv[1],"help",home);
    test_runtime::drawing=true; game::key(Key::A); game::draw(); test_runtime::drawing=false;
    char extra_path[512];snprintf(extra_path,sizeof(extra_path),"%s/help-growth.ppm",argv[1]);test_runtime::image(extra_path);
    State maximum=sun;maximum.coins=9999;maximum.growth=999;maximum.snacks=20;restore(maximum);live(5000);
    test_runtime::drawing=true;game::draw();test_runtime::drawing=false;
    snprintf(extra_path,sizeof(extra_path),"%s/home-maximum.ppm",argv[1]);test_runtime::image(extra_path);
    // A pending care toast follows the locale and locale changes preserve every game byte.
    game::key(Key::L);State before_locale=current();const bool was_english=pocket::english();
    pocket::set_english(!was_english);test_runtime::drawing=true;game::draw();test_runtime::drawing=false;
    CHECK(strcmp(game::title(),pocket::tr("Pet Garden","宠物花园"))==0);same(before_locale,current());pocket::set_english(was_english);
    printf("PASS %u checks: 3 branches, cooldowns, both minigames, rewards, harvest, rest, decor, reset, touch/held, save corruption, bounded drawing\n",checks);
}
