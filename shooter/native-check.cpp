/* SPDX-License-Identifier: MIT. Ordinary-input native Shooter gameplay checks. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <array>
#include <Arduboy2.h>
#include "backend.h"

static unsigned checks=0;
static uint32_t simulated=0;
static void require(bool value,const char *label){if(!value){std::fprintf(stderr,"FAIL %s clock=%u\n",label,arduboy_pxa_millis());std::exit(1);}++checks;}
static std::array<uint8_t,127> pack(){std::array<uint8_t,127>a{};if(arduboy_game_pack(a.data(),a.size())!=127){std::fprintf(stderr,"pack failure\n");std::exit(2);}return a;}
static float f32(const std::array<uint8_t,127>&a,size_t offset){float value;uint32_t bits=(uint32_t)a[offset]|((uint32_t)a[offset+1]<<8)|((uint32_t)a[offset+2]<<16)|((uint32_t)a[offset+3]<<24);memcpy(&value,&bits,4);return value;}
static void advance(uint32_t ms){while(ms){uint32_t step=ms>16?16:ms;arduboy_pxa_advance(step);ms-=step;simulated+=step;if(arduboy_pxa_state.fault){std::fprintf(stderr,"foundation fault=%u\n",arduboy_pxa_state.fault);std::exit(3);}}}
static void hold(uint8_t mask,uint32_t ms){arduboy_pxa_pointer(mask,1);advance(ms);arduboy_pxa_pointer(0,1);advance(32);}
static void tap(uint8_t mask){hold(mask,32);}
static bool bullets_active(const std::array<uint8_t,127>&a){return a[25]||a[36]||a[47];}
static bool pixels(){for(uint8_t value:arduboy_pxa_state.display)if(value)return true;return false;}
int main(){
    arduboy_pxa_boot(17);require(arduboy_pxa_state.constructors==1,"single C++ constructor");
    require(arduboy_game_pack(nullptr,0)==127,"typed game packet has127 bytes");
    uint8_t small[126]{};require(arduboy_game_pack(small,sizeof small)==0,"undersized game pack rejected");
    auto initial=pack();require(initial[0]==0 && initial[6]==0 && initial[7]==0 && initial[9]==32,"original cold globals");
    advance(64);require(pack()[0]==0 && pixels() && arduboy_pxa_state.display_count>0,"original title raster");
    tap(A_BUTTON);require(pack()[0]==1,"original A menu start");
    hold(UP_BUTTON,400);require(pack()[9]==16,"tank upper bound through UP");
    hold(DOWN_BUTTON,600);require(pack()[9]==42,"tank lower bound through DOWN");
    hold(LEFT_BUTTON,450);require(std::fabs(f32(pack(),1)+1.22f)<0.000001f,"original gun left clamp");
    hold(RIGHT_BUTTON,900);require(std::fabs(f32(pack(),1)-1.22f)<0.000001f,"original gun right clamp");
    tap(B_BUTTON);require(bullets_active(pack()),"original B creates a bullet");
    tap(A_BUTTON);require(pack()[0]==2,"original A pause");
    auto paused=pack();advance(1000);auto afterpause=pack();
    require(std::memcmp(paused.data()+1,afterpause.data()+1,4)==0 && paused[6]==afterpause[6] && paused[8]==afterpause[8] && paused[9]==afterpause[9] && std::memcmp(paused.data()+15,afterpause.data()+15,98)==0,"pause retains gun score terrain and entities");
    tap(A_BUTTON);require(pack()[0]==1 && pack()[9]==afterpause[9],"original resume retains tank");
    tap(A_BUTTON);tap(B_BUTTON);require(pack()[0]==4,"original paused B quits to GameOver");
    advance(64);require(pack()[6]==0 && !bullets_active(pack()) && pixels(),"original GameOver reset and bitmap");
    tap(A_BUTTON);require(pack()[0]==0,"GameOver A returns menu");
    tap(DOWN_BUTTON);tap(DOWN_BUTTON);tap(A_BUTTON);require(pack()[0]==3,"original credits menu");
    tap(A_BUTTON);require(pack()[0]==0,"credits A returns menu");
    arduboy_pxa_new();advance(64);tap(DOWN_BUTTON);bool sound=Arduboy2Base::audio.enabled();tap(A_BUTTON);require(Arduboy2Base::audio.enabled()!=sound && pack()[0]==0,"original sound menu toggles audio flag");
    arduboy_pxa_new();advance(64);tap(A_BUTTON);
    unsigned frames=0;while(pack()[0]==1 && frames<7500){advance(16);++frames;}
    require(pack()[0]==4 && frames>0 && frames<7500,"natural enemy or pit collision GameOver");
    require(pixels() && !arduboy_pxa_state.fault && !arduboy_pxa_state.dropped_steps,"natural loss renders without fault or catch-up loss");
    const uint32_t clockbefore=arduboy_pxa_millis();arduboy_pxa_new();auto restarted=pack();
    require(restarted==initial && arduboy_pxa_millis()==clockbefore && arduboy_pxa_state.constructors==1,"NEW reproduces original cold game state retaining common clock");
    unsigned scored_sessions=0;unsigned maximum_score=0;unsigned deaths=0;
    for(unsigned round=0;round<12;round++){
        arduboy_pxa_new();advance(64);tap(A_BUTTON);
        unsigned frame=0;
        while(pack()[0]==1 && frame<4000){
            arduboy_pxa_pointer((frame%2)==0?B_BUTTON:0,1);advance(16);auto a=pack();if(a[6]>maximum_score)maximum_score=a[6];++frame;
        }
        if(maximum_score)++scored_sessions;
        if(pack()[0]==4)++deaths;
        arduboy_pxa_release();
    }
    require(maximum_score>0,"ordinary B fire naturally hits UFO and scores");
    require(deaths>0 && !arduboy_pxa_state.fault && !arduboy_pxa_state.dropped_steps,"twelve natural retries remain sanitizer clean");
    require(arduboy_pxa_pack(nullptr,0)==3136,"canonical common packet has3136 bytes");
    std::printf("{\"passed\":true,\"checks_passed\":%u,\"game_wire_bytes\":127,\"common_wire_bytes\":3136,\"ordinary_input_only\":true,\"simulated_ms\":%u,\"natural_loss_frames\":%u,\"natural_score\":%u,\"deaths\":%u,\"scored_sessions\":%u}\n",checks,simulated,frames,maximum_score,deaths,scored_sessions);
    return 0;
}
