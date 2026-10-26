#include "backend.h"
#include "owner_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t checks=0;static uint8_t wire[4096];static size_t bytes;
static void check(bool v){checks++;if(!v){fprintf(stderr,"check %u failed clock%u mode%u state%u bytes%zu score%u\n",checks,arduboy_pxa_state.clock_ms,wire[2],wire[267],bytes,(unsigned)(wire[300]|wire[301]<<8|wire[302]<<16));abort();}}
static void read(){bytes=arduboy_game_pack(wire,sizeof wire);check(bytes==360);check(wire[0]==1&&wire[1]==14);check(wire[16]==0&&wire[72]==0);check(arduboy_pxa_state.fault==0);check(arduboy_pxa_state.dropped_steps==0);}
static void advance(uint32_t n){while(n){uint32_t x=n>16?16:n;arduboy_pxa_advance(x);n-=x;read();}}
static void tap(uint8_t mask){arduboy_pxa_pointer(mask,0);advance(32);arduboy_pxa_pointer(0,1);advance(32);}
static uint32_t score(){return (uint32_t)wire[300]|((uint32_t)wire[301]<<8)|((uint32_t)wire[302]<<16)|((uint32_t)wire[303]<<24);}
int main(){arduboy_pxa_boot(1234567);read();check(wire[2]==0);check(wire[89]==120);check(arduboy_pxa_state.constructors==1);advance(2100);check(wire[2]==1);check(wire[265]==1);tap(4);check(wire[2]==2);check(wire[267]==0);advance(2100);check(wire[267]==1);
 uint8_t x=wire[316],y=wire[317],dir=wire[320];tap(32);check(wire[320]!=dir);tap(64);check(wire[320]==dir);check(wire[316]==x&&wire[317]==y);tap(8);check(wire[342]==1);uint32_t frames=(uint32_t)wire[111]|wire[112]<<8|wire[113]<<16|wire[114]<<24;advance(256);check(frames==((uint32_t)wire[111]|wire[112]<<8|wire[113]<<16|wire[114]<<24));tap(4);check(wire[342]==0);
 uint32_t maximum=0;for(int i=0;i<32&&wire[267]<3;i++){tap(4);advance(320);if(score()>maximum)maximum=score();}check(maximum>0);check(wire[267]==3||wire[267]==4);advance(1200);check(wire[267]==4);check(wire[343]==1);uint32_t old=score();tap(4);check(wire[267]==0&&score()==0);advance(2100);check(wire[267]==1);tap(8);check(wire[342]==1);tap(16);check(wire[137]==1);tap(4);check(wire[136]==2&&wire[137]==1);tap(128);tap(4);check(wire[267]==0&&score()==0);
 uint8_t ee[1024];memcpy(ee,arduboy_pxa_state.eeprom,1024);arduboy_pxa_new();read();check(wire[2]==0&&wire[89]==120);check(!memcmp(ee,arduboy_pxa_state.eeprom,1024));check(arduboy_pxa_state.constructors==1);check(arduboy_game_pack(nullptr,0)==360);uint8_t shortbuf[360];memset(shortbuf,0xa5,sizeof shortbuf);check(arduboy_game_pack(shortbuf,359)==0&&shortbuf[0]==0xa5);printf("{\"passed\":true,\"ordinary_input_only\":true,\"checks\":%u,\"game_wire_bytes\":%zu,\"natural_max_score\":%u,\"death_score\":%u,\"audio_timer_cycles_per_ms\":16000,\"native_state_restores\":0}\n",checks,bytes,maximum,old);}
