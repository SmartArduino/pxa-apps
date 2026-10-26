/* SPDX-License-Identifier: MIT. Ordinary source-game inputs only; no state setter. */
#include "backend.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static uint8_t g[4096],c[3136];
static unsigned checks,frames,bounces;
static void require(bool v,const char*message){++checks;if(!v){std::fprintf(stderr,"FAIL %s at %u frames mode=%u state=%u score=%u\n",message,frames,g[2],g[90],g[99]|(g[100]<<8));std::exit(1);}}
static uint16_t u16(size_t p){return g[p]|((uint16_t)g[p+1]<<8);}
static int16_t i16(size_t p){return (int16_t)u16(p);}
static void pack(){require(arduboy_game_pack(g,sizeof(g))==518,"complete 518-byte owner packet");arduboy_pxa_pack(c,sizeof(c));require(c[38]==0&&c[39]==1,"fault zero, constructor exactly one");require(g[0]==1&&g[1]==8,"Hopper typed identity");require(g[16]==0&&g[21]==0,"owner and audio fault zero");}
static void step(uint8_t mask=0){arduboy_pxa_controller(mask);arduboy_pxa_advance(16);++frames;pack();}
static void tap(uint8_t mask){step(mask);step();}
static void waitMode(uint8_t mode,unsigned limit){while(g[2]!=mode&&limit--)step();require(g[2]==mode,"expected original module transition");}
static void autoMove(){
  if(g[2]!=2||g[90]!=1){step();return;}
  const unsigned first=g[251],last=g[252];unsigned target=first;
  for(unsigned i=0;i<10;i++){unsigned idx=(first+i)%10;size_t f=111+idx*14;if(g[f]&&g[f+1]+u16(101)*50>u16(99)){target=idx;break;}}
  size_t f=111+target*14;int dx=i16(f+2)-i16(104),dy=i16(f+4)-i16(106);uint8_t mask=0;
  if(dx>128)mask|=64;else if(dx<-128)mask|=32;
  if(dy>128)mask|=16;else if(dy<-128)mask|=128;
  (void)last;step(mask);
}
int main(){
  arduboy_pxa_boot(12345);pack();require(g[2]==0&&g[38]==120,"original 120-frame logo begins");
  waitMode(1,150);require(g[43]==0,"original START menu selected");
  tap(16);require(g[43]==1,"DOWN selects SOUND");tap(8);bool sound=g[42];tap(8);require(g[42]!=sound,"source sound toggle");
  tap(16);tap(8);require(g[40]==1,"RECORD page");tap(4);require(g[40]==0,"record return");
  tap(16);tap(8);require(g[40]==2,"CREDIT page");tap(4);tap(16);require(g[43]==0,"menu wrap to START");
  tap(8);require(g[2]==2&&g[90]==0,"original Ready state");
  for(unsigned i=0;i<150&&g[90]==0;i++)step();require(g[90]==1&&g[108]>0,"automatic original first platform bounce");
  tap(8);require(g[90]==4,"A pauses original game");uint8_t paused[428];std::memcpy(paused,g+90,428);for(unsigned i=0;i<60;i++)step();require(!std::memcmp(paused,g+90,428),"paused original game fields freeze");
  tap(4);require(g[90]==1&&g[98]>0,"B resumes and applies 30-frame ignore");tap(8);require(g[90]==1,"pause suppressed in ignore interval");
  int lastJump=(int8_t)g[108];for(unsigned i=0;i<2500&&g[90]==1&&u16(99)<1;i++){autoMove();int jump=(int8_t)g[108];if(jump>lastJump)++bounces;lastJump=jump;}
  require(u16(99)>=1&&bounces>=1,"ordinary steering earns platform score");
  for(unsigned i=0;i<700&&g[90]!=3;i++)step(192);require(g[90]==3,"ordinary diagonal movement reaches death");require(u16(96)>0&&u16(96)<=480,"original 480-frame death counter");
  tap(8);require(g[90]==0&&u16(99)==0,"A retries original level");
  for(unsigned i=0;i<200&&g[90]!=1;i++)step();for(unsigned i=0;i<700&&g[90]!=3;i++)step(192);require(g[90]==3,"second ordinary death");waitMode(1,500);require(g[40]==0,"death countdown returns title");
  uint8_t before[1024];std::memcpy(before,c+2112,1024);arduboy_pxa_new();pack();require(g[2]==0&&g[38]==120,"NEW resets original logo");require(!std::memcmp(before,c+2112,1024),"NEW preserves session EEPROM");
  std::printf("{\"passed\":true,\"checks\":%u,\"frames\":%u,\"platform_bounces\":%u,\"game_wire_bytes\":518,\"ordinary_input_only\":true,\"cold_boots\":1,\"state_loads\":0}\n",checks,frames,bounces);
}
