#include "backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
static int checks=0;
static void check(bool p,const char*s){if(!p){fprintf(stderr,"FAIL %s\n",s);exit(1);}++checks;}
static std::vector<uint8_t> game(){size_t n=arduboy_game_pack(nullptr,0);std::vector<uint8_t>b(n);if(arduboy_game_pack(b.data(),b.size())!=n){fprintf(stderr,"packet write fail");exit(1);}return b;}
static void advance(unsigned ms){while(ms){unsigned n=ms>16?16:ms;arduboy_pxa_advance(n);ms-=n;}}
static void tap(uint8_t mask,unsigned ms=48){arduboy_pxa_controller(mask);advance(ms);arduboy_pxa_controller(0);advance(48);}
static unsigned word(const std::vector<uint8_t>&b,unsigned n){return b[n]|(unsigned(b[n+1])<<8);}
int main(){
 arduboy_pxa_boot(17);auto b=game();check(b.size()==429,"full original state429");
 check(arduboy_pxa_state.constructors==1&&!arduboy_pxa_state.fault,"constructor and corefault");
 check(b[2]==0&&b[38]==120,"original Logo120 frames");
 check(b[16]==0&&b[21]==0&&b[17]==1,"owner fault/audio/one original channel");
 check(b[8]==1&&b[9]==0&&b[12]==0,"independent rand initial1");
 check(arduboy_pxa_state.eeprom[768]==255,"cold FF EEPROM");
 uint8_t tooSmall[1]={77};check(!arduboy_game_pack(tooSmall,1)&&tooSmall[0]==77,"reject small pack buffer");
 advance(1930);b=game();check(b[2]==1,"original logo reaches title");
 check(b[40]==0&&b[43]==0,"original menu");
 tap(16);b=game();check(b[43]==1,"title down sound");
 tap(16);b=game();check(b[43]==2,"title down record");
 tap(8);b=game();check(b[40]==1,"original record screen");
 tap(4);b=game();check(b[40]==0,"B returns menu");
 tap(16);tap(8);b=game();check(b[40]==2,"original credit");
 tap(4);tap(16);b=game();check(b[43]==0,"menu wrap start");
 tap(8);b=game();check(b[2]==2&&b[79],"A original game and ready");
 advance(2000);b=game();check(!b[79],"ready naturally finishes");
 unsigned before=b[136],beforeScore=word(b,81),beforeColumn=b[137];arduboy_pxa_controller(64);advance(2048);arduboy_pxa_controller(0);advance(48);
 b=game();check(b[136]!=before||word(b,81)>beforeScore||b[137]!=beforeColumn,"original right movement");
 // Remain still through the naturally closing cave; no forced collision/state.
 unsigned elapsed=0;
 while(elapsed<30000){advance(16);elapsed+=16;b=game();if(b[2]==2&&b[80])break;}
 check(b[2]==2&&b[80],"natural cave crush gameover");
 check(word(b,85)>0&&word(b,85)<=480,"original480frame death counter");
 check(word(b,69)>0&&b[46]==2,"original record saved");
 check(arduboy_pxa_state.eeprom[768]=='O'&&arduboy_pxa_state.eeprom[771]==1,"original EEPROM signature");
 unsigned count=word(b,69);tap(4);b=game();check(b[2]==2&&!b[80]&&b[79],"original B death retry");
 check(word(b,69)==count,"retry preserves records");
 unsigned saved[32];for(int i=0;i<32;i++)saved[i]=arduboy_pxa_state.eeprom[768+i];
 arduboy_pxa_new();b=game();check(b[2]==0&&b[38]==120,"NEW resets originalLogo");
 check(b[8]==1&&b[12]==0&&b[79]==0&&word(b,81)==0,"NEW full owner/RNG reset");
 bool eepSame=true;for(int i=0;i<32;i++)eepSame&=saved[i]==arduboy_pxa_state.eeprom[768+i];check(eepSame,"NEW sessionEEP preserved");
 advance(1930);b=game();check(b[2]==1&&word(b,69)==count,"NEW title reloads EEPROM");
 check(b[16]==0&&b[21]==0&&!arduboy_pxa_state.fault,"final no owner/audio/corefault");
 printf("{\"passed\":true,\"checks\":%d,\"game_wire_bytes\":429,\"ordinary_input_only\":true,\"natural_game_over_ms\":%u,\"record_count\":%u,\"hardware_tested\":false}\n",checks,elapsed,count);
}
