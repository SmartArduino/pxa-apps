// SPDX-License-Identifier: CC0-1.0
#include "../core.hpp"
#include "../../common/pocket-play/runtime.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <initializer_list>
namespace test_runtime {extern bool drawing;extern unsigned saves,tones,silences;extern uint64_t monotonic,wall;extern uint32_t foreground;}
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"check failed line %u: %s\n",__LINE__,#x);assert(x);}}while(0)
void draw(){test_runtime::drawing=true;game::draw();test_runtime::drawing=false;}
void advance(uint32_t ms,bool foreground=true){test_runtime::monotonic+=ms;test_runtime::wall+=ms/1000;if(foreground){test_runtime::foreground+=ms;game::tick(ms);}}
grove::State snapshot(){uint8_t raw[128];CHECK(game::serialize(raw,sizeof raw)==128);grove::State s;CHECK(grove::decode(s,raw,128));return s;}
void hash(uint8_t *p){uint32_t h=2166136261u;for(unsigned i=0;i<124;++i)h=(h^p[i])*16777619u;for(unsigned i=0;i<4;++i)p[124+i]=uint8_t(h>>(8*i));}
void tap(int x,int y){game::pointer(pocket::Phase::DOWN,x,y);game::pointer(pocket::Phase::UP,x,y);}
int main(){
    using namespace grove;
    State s;CHECK(rate(s,0)==1000&&rate(s,1)==600&&rate(s,2)==280);
    produce(s,1000);CHECK(s.pending[0]==1&&s.pending[1]==0&&s.fraction[1]==600000);
    produce(s,1000);CHECK(s.pending[1]==1&&s.fraction[1]==200000);
    State before=s;CHECK(buy(s,0));CHECK(s.level[0]==2&&s.resources[0]==4&&s.resources[1]==4);
    CHECK(!buy(s,3));CHECK(!buy(s,4));CHECK(s.level[3]==0);
    for(unsigned i=0;i<3;++i)s.resources[i]=LIMIT;
    for(unsigned i=0;i<4;++i)while(buy(s,i)){}
    CHECK(s.level[0]==5&&s.level[1]==4&&s.level[2]==4&&s.level[3]==3);
    for(unsigned i=0;i<4;++i){State t=s;CHECK(!buy(s,i));CHECK(memcmp(t.level,s.level,4)==0);}
    produce(s,UINT64_MAX);for(unsigned i=0;i<3;++i)CHECK(s.pending[i]==capacity(s));
    collect(s);for(unsigned i=0;i<3;++i)CHECK(s.resources[i]<=LIMIT&&s.pending[i]==0);
    CHECK(!claim(s,7));
    // An ordinary idle route, using only real production and actual costs.
    fresh(s);unsigned seconds=0,first_two_seconds=0;
    for(;seconds<600&&s.goals!=255;++seconds){produce(s,1000);if(seconds%3==0)collect(s);for(unsigned g=0;g<8;++g)claim(s,g);
        if(!first_two_seconds&&(s.goals&3)==3) first_two_seconds=seconds+1;
        unsigned g=0;while(g<8&&(s.goals&(1u<<g)))++g;
        if(g==1) buy(s,0);
        if(g==2) buy(s,1);
        if(g==4) buy(s,2);
        if(g==5) buy(s,3);
        if(g==6){if(s.level[0]<4)buy(s,0);else buy(s,1);}if(g==7){if(s.level[2]<3)buy(s,2);else if(s.level[3]<2)buy(s,3);}
    }
    CHECK(s.goals==255);CHECK(seconds>=180&&seconds<=480);CHECK(first_two_seconds>0&&first_two_seconds<=30);printf("festival reachable seconds=%u first_two_seconds=%u\n",seconds,first_two_seconds);
    // Every visible goal reaches 100% exactly when its real rule is met.
    for(unsigned level=0;level<=5;++level)for(unsigned collected:{0u,9u,10u,59u,60u,1799u,1800u,UINT32_MAX}){
        State progress;for(auto &v:progress.level)v=uint8_t(level);progress.collected=collected;
        for(unsigned goal=0;goal<8;++goal){CHECK(goal_progress(progress,goal)<=1000);CHECK((goal_progress(progress,goal)==1000)==achieved(progress,goal));}
        CHECK(goal_progress(progress,8)==0);
    }
    // Fixed field serialization; every single-bit mutation is rejected atomically.
    uint8_t raw[128];CHECK(encode(s,raw,sizeof raw)==128);CHECK(!encode(s,raw,127));State restored;CHECK(decode(restored,raw,128));CHECK(restored.goals==s.goals);
    for(unsigned bit=0;bit<1024;++bit){uint8_t bad[128];memcpy(bad,raw,128);bad[bit/8]^=uint8_t(1u<<(bit%8));State target=before;CHECK(!decode(target,bad,128));CHECK(target.resources[0]==before.resources[0]);}
    const unsigned offsets[]={5,8,20,32,44,45,46,47,49,50,51,52,56,60,64,72,80,123};
    for(unsigned at:offsets){uint8_t bad[128];memcpy(bad,raw,128);unsigned width=(at==8||at==20||at==32||at==52||at==56||at==60)?4:(at==64||at==72)?8:1;memset(bad+at,255,width);hash(bad);State target;CHECK(!decode(target,bad,128));}
    CHECK(!decode(restored,raw,127));CHECK(!decode(restored,nullptr,128));
    fresh(s);stamp(s,MIN_UTC+100);Away a=offline(s,MIN_UTC+110);CHECK(a.valid&&a.seconds==10&&s.pending[0]==10);
    a=offline(s,MIN_UTC+110);CHECK(a.seconds==0&&s.pending[0]==10);
    a=offline(s,MIN_UTC+90);CHECK(!a.valid&&s.pending[0]==10&&s.last_wall==MIN_UTC+110);
    stamp(s,MIN_UTC+95);CHECK(s.last_wall==MIN_UTC+110);
    a=offline(s,MAX_UTC+1);CHECK(!a.valid&&s.last_wall==0);a=offline(s,MIN_UTC+150);CHECK(!a.valid&&s.pending[0]==10);
    a=offline(s,MIN_UTC+150+86400*3);CHECK(a.valid&&a.seconds==28800&&s.pending[0]==capacity(s));
    fresh(s);stamp(s,0);a=offline(s,MIN_UTC);CHECK(!a.valid&&s.pending[0]==0);CHECK(!valid_wall(0)&&!valid_wall(MAX_UTC));
    // Real hooks: no elapsed/mono double count and no writes each tick.
    game::init();grove::State start=snapshot();unsigned saves=test_runtime::saves;
    for(unsigned i=0;i<10;++i)advance(100);
    State one=snapshot();CHECK(one.pending[0]==start.pending[0]+1);CHECK(test_runtime::saves==saves);
    // Pause -> serialize -> resume counts the suspended time exactly once.
    advance(7000,false);State paused=snapshot();CHECK(paused.pending[0]==one.pending[0]+7);
    game::tick(20);State resumed=snapshot();CHECK(resumed.pending[0]==paused.pending[0]);
    // Background -> exit serialization -> load: no duplicate interval.
    advance(5000,false);CHECK(game::serialize(raw,128)==128);State exit;CHECK(decode(exit,raw,128));
    advance(3000,false);game::init();game::load(raw,128);State loaded=snapshot();CHECK(loaded.pending[0]==exit.pending[0]+3);
    game::load(raw,128);State loaded_again=snapshot();CHECK(loaded_again.pending[0]==loaded.pending[0]);
    // Clock rollback and invalid -> valid combinations via real hooks.
    test_runtime::wall-=100;CHECK(game::serialize(raw,128)==128);State rollback;CHECK(decode(rollback,raw,128));CHECK(rollback.last_wall==loaded.last_wall);
    test_runtime::wall=0;CHECK(game::serialize(raw,128)==128);CHECK(decode(rollback,raw,128)&&rollback.last_wall==0);
    test_runtime::wall=MIN_UTC+500;game::init();game::load(raw,128);State newclock=snapshot();CHECK(newclock.pending[0]==rollback.pending[0]);
    // CANCEL and outside release never collect; reset retains progression.
    unsigned taps=newclock.taps;game::pointer(pocket::Phase::DOWN,45,183);game::pointer(pocket::Phase::CANCEL,45,183);game::pointer(pocket::Phase::UP,45,183);CHECK(snapshot().taps==taps);
    game::pointer(pocket::Phase::DOWN,45,183);game::pointer(pocket::Phase::UP,45,213);CHECK(snapshot().taps==taps);
    game::reset();tap(45,183);CHECK(snapshot().taps==taps+1);tap(45,183);CHECK(snapshot().taps==taps+1);
    State retained=snapshot();game::reset();CHECK(snapshot().collected==retained.collected);
    // Complete touch-only navigation from both subpages, including cancelled back.
    for(unsigned page=1;page<=2;++page) {
        tap(page==1?140:230,183);State entered=snapshot();CHECK(entered.view==page);
        game::pointer(pocket::Phase::DOWN,220,26);game::pointer(pocket::Phase::CANCEL,220,26);game::pointer(pocket::Phase::UP,220,26);CHECK(snapshot().view==page);
        game::pointer(pocket::Phase::DOWN,220,26);game::pointer(pocket::Phase::UP,170,26);CHECK(snapshot().view==page);
        tap(220,26);State backed=snapshot();CHECK(backed.view==0&&backed.collected==retained.collected&&backed.goals==entered.goals);
        CHECK(memcmp(backed.level,entered.level,4)==0&&memcmp(backed.resources,entered.resources,12)==0);
    }
    // The visible central firefly shares the basket reward and cooldown.
    game::init();taps=snapshot().taps;
    game::pointer(pocket::Phase::DOWN,72,102);game::pointer(pocket::Phase::CANCEL,72,102);game::pointer(pocket::Phase::UP,72,102);CHECK(snapshot().taps==taps);
    tap(72,102);CHECK(snapshot().taps==taps+1);tap(45,183);CHECK(snapshot().taps==taps+1);
    for(unsigned i=0;i<7;++i)advance(100);
    tap(45,183);CHECK(snapshot().taps==taps+2);
    tap(60,26);CHECK(snapshot().view==2);tap(220,26);CHECK(snapshot().view==0);
    // An old v1 sound-off save still silences action cues.
    fresh(s);s.sound=0;CHECK(encode(s,raw,128)==128);game::init();game::load(raw,128);
    unsigned quiet=test_runtime::tones;advance(100);tap(45,183);CHECK(test_runtime::tones==quiet&&snapshot().sound==0);
    game::key(pocket::Key::SELECT);game::key(pocket::Key::A);CHECK(snapshot().sound==1);CHECK(test_runtime::tones>quiet);
    quiet=test_runtime::tones;advance(100);CHECK(test_runtime::tones==quiet);
    game::key(pocket::Key::A);quiet=test_runtime::tones;advance(100);CHECK(snapshot().sound==0&&test_runtime::tones==quiet&&test_runtime::silences>0);
    // Render both languages, every page, upgrade silhouette and journal chapter.
    for(unsigned lang=0;lang<2;++lang) {
        pocket::set_english(lang!=0);
        fresh(s);for(unsigned i=0;i<3;++i)s.resources[i]=LIMIT;
        for(unsigned i=0;i<4;++i)while(buy(s,i)){}
        for(unsigned view=0;view<4;++view)for(unsigned selected=0;selected<4;++selected){s.view=uint8_t(view);s.selected=uint8_t(selected);if(view==0&&selected==3)s.selected=2;
            for(unsigned night=0;night<2;++night){s.cycle_ms=night?75000:0;CHECK(encode(s,raw,128)==128);game::load(raw,128);draw();}}
        game::key(pocket::Key::Y);game::key(pocket::Key::RIGHT);draw();tap(260,199);draw();
    }
    pocket::set_english(false);
    printf("PASS checks=%u ASan+UBSan draw-only/bounds\n",checks);return 0;
}
