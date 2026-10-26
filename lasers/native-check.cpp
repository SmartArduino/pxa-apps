#include "backend.h"
#include "owner_compat.h"
#include "vendor/platform/common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
void clearScreenGray();
static int checks=0;static void need(bool ok,const char*s){++checks;if(!ok){fprintf(stderr,"check%d %s\n",checks,s);abort();}}
static std::vector<uint8_t> game(){std::vector<uint8_t>b(arduboy_game_pack(nullptr,0));need(arduboy_game_pack(b.data(),b.size())==b.size(),"full pack");return b;}
static unsigned start(const std::vector<uint8_t>&b){return b.size()-468;}
static unsigned u16(const std::vector<uint8_t>&b,unsigned n){return b[n]|((unsigned)b[n+1]<<8);}
static unsigned value(unsigned offset){auto b=game();return u16(b,start(b)+441+offset);}
static unsigned state(){auto b=game();return b[start(b)];}
static unsigned mode(){return game()[2];}
static void advance(unsigned ms){while(ms){unsigned n=ms>32?32:ms;arduboy_pxa_advance(n);ms-=n;}}
static void tap(uint8_t b){arduboy_pxa_pointer(b,0);advance(48);arduboy_pxa_pointer(0,1);advance(48);}
static void toTitle(){advance(6500);auto b=game();need(mode()==1,"original warning mode");need(b[17+72+2+3]==51,"warning complete");tap(4);need(mode()==2,"B accepts retained warning");}
int main(){
 clearScreenGray();for(unsigned i=0;i<1024;i++)need(arduboy.getBuffer()[i]==((i&1)?0xaa:0x55),"grayphase1 1024");
 clearScreenGray();for(unsigned i=0;i<1024;i++)need(arduboy.getBuffer()[i]==((i&1)?0x55:0xaa),"grayphase2 1024");
 arduboy_pxa_boot(1);auto b=game();printf("{\"game_wire_bytes\":%zu,",b.size());need(b[0]==1&&b[1]==15&&mode()==0,"typed coldlogo");need(arduboy_pxa_state.constructors==1,"one core constructor");for(auto v:arduboy_pxa_state.eeprom)need(v==255,"fresh FF EEPROM");toTitle();tap(4);need(mode()==3&&state()==1,"B starts playing");need(value(0)==0&&value(4)==1000,"original ready score0 power1000");
 unsigned x=value(6),y=value(8);tap(32);need(value(6)<x,"left movement");tap(64);need(value(6)>=x,"right movement");tap(128);need(value(8)<y,"up movement");tap(16);need(value(8)>=y,"down movement");b=game();bool white=b[start(b)+465];tap(4);b=game();need(b[start(b)+465]!=white,"B flips color");
 advance(2500);need(value(0)>0&&value(10)>0,"natural score and laser generation");tap(8);need(state()==2,"original A pause");unsigned frames=value(2);advance(200);need(value(2)==frames,"original pause freezes gameplay");tap(4);need(state()==1,"original B continue");
 unsigned absorb=0,damage=0,whiteframes=0,blackframes=0;for(unsigned i=0;i<30000&&state()==1;i++){advance(32);b=game();unsigned g=start(b);absorb+=b[g+462]==1;damage+=b[g+462]==3;whiteframes+=b[g+465]!=0;blackframes+=b[g+465]==0;if(i%157==0)tap(4);}need(state()==3,"natural collisions death");need(damage>0,"actual hostile collision");need(value(0)>0,"positive natural score retained");need(whiteframes>0&&blackframes>0,"both colors exercised");advance(1200);tap(4);need(state()==1&&value(0)==0&&value(4)==1000,"original B death retry");
 uint8_t ee[1024];memcpy(ee,arduboy_pxa_state.eeprom,1024);unsigned clk=arduboy_pxa_state.clock_ms;arduboy_pxa_new();b=game();need(mode()==0&&b[17+72]==120,"NEW original logo120");need(clk==arduboy_pxa_state.clock_ms,"NEW preserves clock");need(!memcmp(ee,arduboy_pxa_state.eeprom,1024),"NEW session EEPROM retained");toTitle();need(arduboy_pxa_state.fault==0&&arduboy_pxa_state.dropped_steps==0,"no provider fault/drop");
 printf("\"passed\":true,\"checks\":%d,\"ordinary_input_only\":true,\"native_boots\":1,\"native_restores\":0,\"absorb_frames\":%u,\"damage_frames\":%u,\"gray_phases\":2}\n",checks,absorb,damage);
}
