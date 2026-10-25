// SPDX-License-Identifier: CC0-1.0
// Event-only audio regression; the historical filename remains the runner entry.
#include "../music.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
namespace test {
struct Tone {float frequency,duration,when;};
uint32_t now=0;unsigned silences=0,peak_queue=0;
std::vector<Tone> notes;
void fresh(uint32_t start=0){now=start;notes.clear();silences=peak_queue=0;}
}
namespace pocket {
uint32_t millis(){return test::now;}
void silence(){++test::silences;}
void tone(float f,float d,float w){
    assert(std::isfinite(f)&&f>=30&&f<=6000&&d>0&&d<=.08f&&std::isfinite(w));
    assert(double(w)+.001>=double(test::now)/1000.0);
    assert(double(w)<=double(test::now)/1000.0+.386);
    unsigned active=0;for(const auto &n:test::notes)if(double(n.when)+n.duration>double(test::now)/1000.0)++active;
    test::peak_queue=std::max(test::peak_queue,active+1);assert(test::peak_queue<=16);
    test::notes.push_back({f,d,w});
}
}
int main(){
    using pocket_music::Cue;using pocket_music::Effects;
    const Cue all[]={Cue::Select,Cue::Confirm,Cue::Collect,Cue::Upgrade,Cue::Goal,Cue::Error,Cue::Order,Cue::Cook,Cue::Ready,Cue::Serve,Cue::Restock,Cue::Day,Cue::Paint,Cue::Clear,Cue::Reaction};
    std::set<std::string> signatures;
    for(Cue cue:all){
        test::fresh(1000);Effects effects;effects.reset();effects.cue(cue);
        assert(!test::notes.empty()&&test::notes.size()<=4);
        std::string signature;for(const auto &n:test::notes)signature+=std::to_string(n.frequency)+":"+std::to_string(n.duration)+",";
        signatures.insert(signature);
    }
    assert(signatures.size()==15);
    for(Cue cue:{Cue::Select,Cue::Paint}){
        test::fresh();Effects effects;effects.reset();for(test::now=0;test::now<1000;++test::now)effects.cue(cue);
        assert(test::notes.size()==(cue==Cue::Paint?7u:25u));
    }
    for(Cue reward:{Cue::Goal,Cue::Upgrade}){
        test::fresh(100);Effects effects;effects.reset();effects.cue(Cue::Collect);auto lower=test::notes.size();
        effects.cue(reward);assert(test::notes.size()==lower+4);effects.cue(reward);assert(test::notes.size()==lower+4);
        test::now+=350;effects.cue(reward);assert(test::notes.size()==lower+8);
    }
    test::fresh();Effects effects;effects.reset();
    for(test::now=0;test::now<12000;++test::now)for(Cue cue:all)effects.cue(cue);
    assert(test::notes.size()<1500);const unsigned peak=test::peak_queue;
    effects.set_enabled(false);const auto count=test::notes.size();for(Cue cue:all)effects.cue(cue);assert(test::notes.size()==count&&test::silences>=2);
    effects.set_enabled(true);effects.cue(Cue::Confirm);assert(test::notes.size()>count);
    // Reset and foreground-clock wrap retain cue access; idle time is silent.
    test::fresh(UINT32_MAX-20);effects.reset();effects.cue(Cue::Select);test::now=30;effects.reset();effects.cue(Cue::Collect);assert(test::notes.size()==4);
    test::fresh();effects.reset();for(test::now=0;test::now<60000;test::now+=20)effects.set_enabled(true);assert(test::notes.empty());
    std::printf("PASS event-only sound: 15 distinct cues; mute/reset/wrap; bounded spam queue=%u; 60s idle zero tones\n",peak);
}
