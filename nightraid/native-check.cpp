/* SPDX-License-Identifier: MIT. Ordinary-input tests; no game-state setters. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include "../common/retro-games/arduboy-batch-20261010/backend.h"
static uint8_t b[394];static int checks;static bool inversion_seen;
static void ck(bool condition,const char*name){if(!condition){fprintf(stderr,"FAIL %s check%d state%d score%d cursor%d,%d\n",name,checks,b[0],b[374]|b[375]<<8,b[386],b[387]);exit(2);}++checks;}
static void read(){if(arduboy_game_pack(b,sizeof b)!=sizeof b)abort();}
static void frame(uint8_t mask=0){arduboy_pxa_controller(mask);arduboy_pxa_advance(16);read();inversion_seen|=b[393]!=0;
 uint8_t core[3136];arduboy_pxa_pack(core,sizeof core);
 if(arduboy_pxa_state.display_count)for(int i=0;i<1024;++i)if(core[1088+i]!=(uint8_t)(core[64+i]^(b[393]?255:0)))abort();
}
static void frames(int n,uint8_t mask=0){while(n--)frame(mask);}
static int score(){return b[374]|b[375]<<8;}
static float f32(size_t p){float v;memcpy(&v,b+p,4);return v;}
static int outgoing(){int n=0;for(int i=8;i<14;++i)n+=b[66+22*i+18];return n;}
static void start(){frame(8);frame();ck(b[0]==1,"A starts original MENU");}
static void cursor(int x,int y){for(int n=0;n<220&&(b[386]!=x||b[387]!=y);++n){uint8_t m=(b[386]>x?32:b[386]<x?64:0)|(b[387]>y?128:b[387]<y?16:0);frame(m);}}
int main(){
 arduboy_pxa_boot(17);read();ck(arduboy_pxa_state.constructors==1&&arduboy_pxa_state.fault==0,"one constructor no fault");
 ck(arduboy_game_pack(nullptr,0)==394&&arduboy_game_pack(b,393)==0,"explicit complete owner394 capacity");
 ck(b[0]==0&&b[386]==63&&b[387]==26&&b[389]==150,"cold original globals");
 ck(arduboy_pxa_state.eeprom[400]==111&&arduboy_pxa_state.eeprom[401]==0,"AVR16 marker400");
 ck(arduboy_pxa_state.eeprom[420]==0&&arduboy_pxa_state.eeprom[421]==0&&arduboy_pxa_state.eeprom[422]==0&&arduboy_pxa_state.eeprom[419]==255&&arduboy_pxa_state.eeprom[423]==255,"original overlapping AVR16 zero writes420421");
 frames(2);ck(arduboy_pxa_state.display_count>0,"original title rendered");start();
 ck(b[382]==0&&b[383]==0,"A B physical releases rearm original edges");
 bool rubble=false;for(int i=12;i<44;++i)rubble|=b[i]!=0;ck(rubble,"real software random rubble");
 frame(32);ck(b[386]==62,"left cursor");frame(64);ck(b[386]==63,"right cursor");frame(128);ck(b[387]==25,"up cursor");frame(16);ck(b[387]==26,"down cursor");
 frame(8);ck(outgoing()==1,"A launches original turret");frame(8);ck(outgoing()==1,"A hold never repeatfires");frame();frame(4);ck(outgoing()==2,"B launches other original turret");frame();
 ck(b[66+8*22+21]==0&&b[66+9*22+21]==0,"defined outgoing scored flags");
 frame(140);ck(b[0]==3&&b[390]==1,"original UP A B pause");frame();ck(b[390]==0,"pause release unlock");
 uint8_t frozen[394];memcpy(frozen,b,394);auto clock=arduboy_pxa_state.clock_ms;frames(12);ck(memcmp(frozen,b,394)==0&&arduboy_pxa_state.clock_ms>clock,"original paused owner state frozen with running clock");
 frame(8);ck(b[0]==1&&b[382]==1&&b[383]==1,"original A pause resumes and rearms");frame();
 arduboy_pxa_new();read();ck(b[0]==0&&b[386]==63&&b[387]==26&&b[374]==0,"NEW cold original global resets");start();
 bool spawned=false;
 for(int n=0;n<200&& !spawned;++n){frame();for(int i=0;i<8;++i)spawned|=b[66+22*i+18]!=0;}
 ck(spawned,"original150frame incoming spawn");
 for(int n=0;n<5000&&score()==0&&b[0]==1;++n){
  read();int target=-1;for(int i=0;i<8;++i)if(b[66+22*i+18]&&!b[66+22*i+21]&&b[66+22*i+19]==0){target=i;break;}
  if(target<0){frame();continue;}
  int p=66+22*target;float y=f32(p+5),x=f32(p+9);int tx=int(std::round(x+(int(b[p+3])-int(b[p]))*0.14f));
  int ty=int(std::round(y+8));tx=tx<0?0:tx>127?127:tx;ty=ty<8?8:ty>56?56:ty;
  cursor(tx,ty);frame(8);frame();frame(4);frames(30);
 }
 ck(score()>0,"ordinary aimed shots create original collision score");int scored=score();
 // Let authentic incoming missiles destroy the cities/turrets, then original
 // 240-frame pending loss must write the high score to EEPROM420.
 int lossFrames=0;while(b[0]!=2&&lossFrames<30000){frame();++lossFrames;}
 ck(b[0]==2&&b[384]&&b[385]>=240,"genuine original city/turret loss GAMEOVER");
 ck((b[2]|b[3]<<8)==score()&&score()>=scored,"original high score updated");
 ck((arduboy_pxa_state.eeprom[420]|arduboy_pxa_state.eeprom[421]<<8)==score()&&arduboy_pxa_state.eeprom[422]==0,"authentic little endian highscore420");
 int best=score();frame(8);ck(b[0]==0&&score()==0&&b[388]==1,"original GAMEOVER A returns MENU resetGame");frame();
 auto savedClock=arduboy_pxa_state.clock_ms;arduboy_pxa_new();read();ck(b[0]==0&&(b[2]|b[3]<<8)==best&&arduboy_pxa_state.clock_ms==savedClock,"NEW preserves session EEPROM best and active clock");
 ck(inversion_seen,"real original explosion OLED inversion observed and display XOR checked");
 ck(arduboy_pxa_state.dropped_steps==0&&arduboy_pxa_state.fault==0,"ordinary16ms driver no dropped steps");
 printf("{\"passed\":true,\"checks\":%d,\"ordinary_input_only\":true,\"game_wire_bytes\":394,\"total_wire_bytes\":3530,\"actual_collision_score\":%d,\"saved_best\":%d,\"actual_loss_frames\":%d,\"original_fps\":60,\"period_ms\":16}\n",checks,scored,best,lossFrames);
}
