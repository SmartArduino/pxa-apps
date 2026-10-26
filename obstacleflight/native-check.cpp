/* SPDX-License-Identifier: MIT. Native-only original input and explicit boundary checks. */
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <array>
#include <vector>
#include "game.cpp"
static unsigned checks=0;
static void require(bool b,const char*why){if(!b){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}++checks;}
static std::array<uint8_t,101> pack(){std::array<uint8_t,101>b{};require(arduboy_game_pack(b.data(),b.size())==101,"typed wire101");return b;}
static void advance(unsigned n){while(n){unsigned d=n>32?32:n;arduboy_pxa_advance(d);n-=d;}}
static void tap(uint8_t mask){arduboy_pxa_pointer(mask,0);advance(32);arduboy_pxa_pointer(0,1);advance(32);}
int main(){
 const char*mode=std::getenv("OBSTACLE_NATIVE_MODE");
 arduboy_pxa_boot(17);require(arduboy_pxa_state.constructors==1,"one constructor bootstrap");
 if(mode && strcmp(mode,"equivalence")==0){
  for(unsigned s=0;s<100;s++)for(float depth:{1.0f,31.0f,63.0f}){
   arduboy_pxa_new();score=(uint8_t)s;obstacleType=9;obstacleDepth=depth;
   drawRoom();drawObstacle();uint8_t wire[3237];
   require(arduboy_pxa_pack(wire,3136)==3136,"equivalence core");require(arduboy_game_pack(wire+3136,101)==101,"equivalence game");
   require(std::fwrite(wire,1,sizeof wire,stdout)==sizeof wire,"equivalence output");
  }return 0;
 }
 if(mode && strcmp(mode,"score100")==0){
  score=100;obstacleType=1;obstacleDepth=1;
  drawRoom();drawObstacle();
  require(std::isfinite(obstacleDepth),"score100 produces finite obstacleDepth");return 0;
 }
 auto initial=pack();require(screen==2 && lives==3 && score==0 && xo==64 && yo==48,"original cold title");
 require(arduboy_pxa_state.eeprom[0]==255 && arduboy_pxa_state.eeprom[1023]==255,"unused EEPROM starts FF");
 advance(64);require(arduboy_pxa_state.display_count>0 && Arduboy2Base::frameCount>0,"original title rendered");
 tap(A_BUTTON);require(screen==0 && obstacleType>=1 && obstacleType<=12,"ordinary A starts randomized obstacle");
 unsigned x=xo;tap(LEFT_BUTTON);require(xo<x,"left movement");x=xo;tap(RIGHT_BUTTON);require(xo>x,"right movement");
 unsigned y=yo;tap(UP_BUTTON);require(yo<y,"up movement");y=yo;tap(DOWN_BUTTON);require(yo>y,"down movement");
 unsigned tmp=temp;tap(A_BUTTON);require(temp==uint8_t(tmp+1),"original in-game A increments temp");
 unsigned frames=0;while(screen==0 && frames<15000){advance(16);++frames;}
 require(screen==1 && lives==0 && frames<15000,"natural original collision game over");
 tap(A_BUTTON);require(screen==0 && lives==3 && score==0,"original A restarts from game over");
 // Choose ordinary directional input from the actually observed obstacle.
 unsigned scoreframes=0;while(screen==0 && score==0 && scoreframes<5000){
  const byte tx[]={96,96,96,32,32,96,96,32,96,96,32,32,96};
  const byte ty[]={48,48,16,48,16,48,16,48,48,48,16,48,16};
  byte type=obstacleType;byte mask=0;
  if(type<=12){if(xo>tx[type]+2)mask=LEFT_BUTTON;else if(xo+2<tx[type])mask=RIGHT_BUTTON;else if(yo>ty[type]+2)mask=UP_BUTTON;else if(yo+2<ty[type])mask=DOWN_BUTTON;}
  arduboy_pxa_pointer(mask,0);advance(16);++scoreframes;
 }
 arduboy_pxa_release();require(screen==0 && score>0,"ordinary directional avoidance naturally scores");
 unsigned natural_score=score;
 auto clock=arduboy_pxa_state.clock_ms;arduboy_pxa_new();require(pack()==initial && arduboy_pxa_state.clock_ms==clock,"wrapper NEW restores original cold fields preserving clock");
 // Native edge fixture only. These direct global boundary placements never
 // appear in Guest or continuous ordinary-input native oracle.
 unsigned boundaries=0,equivalent=0;uint32_t rng=arduboy_pxa_state.rng;
 for(unsigned s=0;s<256;s++){
  arduboy_pxa_new();score=(uint8_t)s;obstacleType=1;obstacleDepth=1;float before[5];for(unsigned i=0;i<5;i++)before[i]=leftDepth[i];
  drawRoom();drawObstacle();int den=max(1,20-int(s)/5);
  require(std::isfinite(obstacleDepth) && obstacleDepth==1.0f+1.0f/den,"all byte scores finite forward obstacle speed");
  for(unsigned i=0;i<5;i++){float expected=before[i]+before[i]/den;if(expected>64)expected=1;require(std::isfinite(leftDepth[i]) && leftDepth[i]==expected,"finite exact room speed");}
  if(s<100){require(den==20-int(s)/5,"unaffected original denominator0-99");++equivalent;}
  require(arduboy_pxa_state.rng==rng,"edge speed calls preserve RNG");++boundaries;
 }
 require(arduboy_pxa_state.fault==0 && arduboy_pxa_state.dropped_steps==0,"no cooperative fault or dropped time");
 std::printf("{\"passed\":true,\"checks_passed\":%u,\"game_wire_bytes\":101,\"common_wire_bytes\":3136,\"ordinary_input_only\":true,\"ordinary_game_over_frames\":%u,\"ordinary_score\":%u,\"ordinary_score_frames\":%u,\"native_explicit_boundary_fixture\":true,\"all_score_boundaries\":%u,\"unchanged_speed_scores\":%u}\n",checks,frames,natural_score,scoreframes,boundaries,equivalent);
}
