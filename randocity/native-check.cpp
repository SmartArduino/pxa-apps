/* SPDX-License-Identifier: MIT. Ordinary original input and software API checks;
 * no target-state setter, saved-state loader, or Game field mutation.
 */
#include <Arduboy2.h>
#include "backend.h"
#include "vendor/platform/ArduboyTones.h"
#include "vendor/platform/Records.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
static uint8_t b[120];
static uint16_t u16(unsigned n){return b[n]|((uint16_t)b[n+1]<<8);}
static uint32_t u32(unsigned n){return u16(n)|((uint32_t)u16(n+2)<<16);}
static int32_t i32(unsigned n){uint32_t value=u32(n);int32_t result;memcpy(&result,&value,4);return result;}
static void check(bool yes,const char*name){if(!yes){fprintf(stderr,"FAILED: %s mode=%u menu=%d pos=%d,%d speed=%d\n",name,b[23],(int8_t)b[25],i32(2),i32(6),(int8_t)b[18]);exit(1);}++checks;}
static void read(){check(arduboy_game_pack(b,sizeof b)==sizeof b,"canonical 120-byte full Game pack");--checks;}
static void advance(unsigned n){while(n){unsigned step=n>16?16:n;arduboy_pxa_advance(step);n-=step;}read();}
static void tap(uint8_t mask,unsigned hold=80){arduboy_pxa_pointer(mask,0);advance(hold);arduboy_pxa_pointer(0,1);advance(hold);}
int main(){
  arduboy_pxa_boot(17);read();
  check(arduboy_game_pack(nullptr,0)==120&&arduboy_pxa_pack(nullptr,0)==3136,"full typed 3136+120 bytes");
  check(b[23]==0&&b[18]==0&&b[119]==1,"original declared cold globals and setup title");
  check(b[65]==99&&b[66]==0&&u32(67)==0xFADED&&u16(71)==0&&u32(73)==0xDECAF,"original invalid-EEPROM records validate defaults");
  bool erased=true;for(uint8_t v:arduboy_pxa_state.eeprom)erased&=v==255;check(erased,"validate does not invent a disk save or EEPROM write");
  ArduboyTones::tone(0,10);check(ArduboyTones::playing(),"original rest counts as playing");advance(9);check(ArduboyTones::playing(),"rest prescaler8 OCR3999, floor4 toggles plus extra interrupt remains active at9ms");advance(1);check(!ArduboyTones::playing(),"rest reaches original END interrupt at10ms");
  advance(100);check(b[1]==0&&i32(2)>0&&i32(6)>0,"original title procedural city scroll");
  tap(A_BUTTON);check(b[23]==4&&b[25]==0,"ordinary A opens actual original main menu");
  tap(B_BUTTON);check(b[23]==1,"ordinary B FREE PLAY");
  int32_t oldX=i32(2),oldY=i32(6);tap(B_BUTTON);check(i32(2)!=oldX||i32(6)!=oldY,"ordinary B accelerates and moves original player");
  tap(RIGHT_BUTTON);check(b[0]==1,"RIGHT rotates original eight directions by one");tap(LEFT_BUTTON);check(b[0]==0,"LEFT rotates back north");
  tap(A_BUTTON);check((int8_t)b[18]>=0,"A brakes with original nonnegative speed");
  tap(DOWN_BUTTON|A_BUTTON);check(b[23]==4&&b[24]==1,"real DOWN+A leaves FREE PLAY to original menu");
  tap(DOWN_BUTTON);tap(DOWN_BUTTON);tap(B_BUTTON);check(b[23]==8,"original records menu");tap(A_BUTTON);check(b[23]==4,"records A exit");
  tap(DOWN_BUTTON);tap(B_BUTTON);check(b[23]==9,"original VIEW MAP");
  int32_t savedX=i32(10),savedY=i32(14);tap(RIGHT_BUTTON);check(i32(2)!=savedX,"map RIGHT scrolls actual camera");tap(A_BUTTON);check(b[23]==4&&i32(2)==savedX&&i32(6)==savedY,"map A restores original saved player coordinates");
  tap(DOWN_BUTTON);tap(B_BUTTON);check(b[23]==11&&b[34]==1,"original HELP page1");tap(RIGHT_BUTTON);check(b[34]==2,"help RIGHT page2");for(int i=0;i<8;++i)tap(DOWN_BUTTON);check(b[34]==6,"original help clamp at6");tap(A_BUTTON);
  tap(UP_BUTTON);tap(UP_BUTTON);tap(UP_BUTTON);tap(B_BUTTON);check(b[23]==5&&b[25]==0,"original NEW GAME submenu");
  tap(B_BUTTON);check(b[23]==2&&b[0]==0&&b[78]==0&&b[79]==0&&b[80]==1,"original RACE TO EDGE starts count-up");
  advance(6000);check(b[83]==2&&b[78]>0,"actual original30-frame countdown and race clock");
  tap(DOWN_BUTTON|A_BUTTON);check(b[23]==7&&b[24]==2,"original race pause captures actual mode");
  uint8_t seconds=b[78],minutes=b[79];advance(2000);check(b[78]==seconds&&b[79]==minutes,"original pause freezes gameTimer while core clock advances");tap(A_BUTTON);check(b[23]==2,"pause A returns race");
  unsigned driving=0;while(b[23]==2&&driving<600000){tap(B_BUTTON,40);driving+=80;}
  check(b[23]==12&&b[24]==2,"ordinary repeated B reaches city boundary and original race GAME OVER");
  check(b[45]&&b[65]<99,"actual completed race sets a new original record");
  uint8_t saved[1024];memcpy(saved,arduboy_pxa_state.eeprom,1024);
  check(saved[480]==b[65]&&saved[481]==b[66]&&!memcmp(saved+482,b+67,4)&&saved[492]==b[77],"explicit AVR13-byte EEPROM record, no native structure padding");
  tap(A_BUTTON);tap(DOWN_BUTTON);tap(B_BUTTON);tap(DOWN_BUTTON);tap(B_BUTTON);check(b[23]==3&&u16(35)==0&&b[79]==2,"original CRAZY COURIER starts two-minute countdown and zero score");
  advance(130000);check(b[23]==12&&b[24]==3&&u16(35)==0&&!b[45],"real original courier timer expiry and zero-score loss, no invented win");
  check(!memcmp(saved,arduboy_pxa_state.eeprom,1024),"zero courier score does not overwrite actual race EEPROM record");
  tap(A_BUTTON);tap(B_BUTTON);tap(DOWN_BUTTON);tap(B_BUTTON);check(b[23]==3,"ordinary menu begins a second courier route");
  advance(6000);uint32_t courierStart=arduboy_pxa_millis();uint8_t lastMask=0;unsigned courierFrames=0;
  for(;courierFrames<4000&&!u16(35);++courierFrames){
    check(b[23]==3,"courier route remains in the original active game");--checks;
    int32_t dx=i32(37)-i32(2),dy=i32(41)-i32(6);bool xAxis=abs(dx)>14;int32_t delta=xAxis?dx:dy;
    uint8_t direction=xAxis?(delta>0?2:6):(delta>0?4:0),mask=0;int speed=(int8_t)b[18];
    if(b[0]!=direction){
      if(speed>0)mask=lastMask!=A_BUTTON?A_BUTTON:0;
      else {unsigned clockwise=(direction-b[0])&7;uint8_t turn=clockwise<=4?RIGHT_BUTTON:LEFT_BUTTON;mask=lastMask!=turn?turn:0;}
    }else{
      int distance=abs(delta),desired=distance>=900?12:distance>=200?6:2;if(distance<=14)desired=0;
      if(speed<desired)mask=lastMask!=B_BUTTON?B_BUTTON:0;
      else if(speed>desired)mask=lastMask!=A_BUTTON?A_BUTTON:0;
    }
    arduboy_pxa_pointer(mask,0);advance(33);lastMask=mask;
  }
  uint32_t courierMs=arduboy_pxa_millis()-courierStart;uint16_t deliveredScore=u16(35);
  check(deliveredScore>0&&b[79]==2&&b[78]==0,"ordinary adaptive direction and accelerator route earns an actual courier pickup and resets the original timer");
  while((int8_t)b[18]>0){tap(A_BUTTON,33);}
  advance(130000);check(b[23]==12&&b[24]==3&&u16(35)==deliveredScore&&b[45],"original courier timeout saves the real positive delivery record");
  check(u16(71)==deliveredScore&&arduboy_pxa_state.eeprom[486]==(uint8_t)deliveredScore&&arduboy_pxa_state.eeprom[487]==(uint8_t)(deliveredScore>>8),"actual courier record uses exact AVR two-byte score at486");
  memcpy(saved,arduboy_pxa_state.eeprom,1024);
  Records rule{};rule.validate();check(rule.setCC(0x12345,25)&&rule.crazyCourierScore==25&&!rule.setCC(0x12345,25)&&rule.checksum==rule.calcChecksum(),"original Records public score rule accepts only a strictly better score with authentic checksum");
  uint32_t storedRaceSeed=(uint32_t)saved[482]|((uint32_t)saved[483]<<8)|((uint32_t)saved[484]<<16)|((uint32_t)saved[485]<<24);
  arduboy_pxa_new();read();check(b[23]==0&&b[18]==0&&b[65]<99&&u32(67)==storedRaceSeed,"NEW original setup preserves actual EEPROM record and reconstructs cold title");
  check(!memcmp(saved,arduboy_pxa_state.eeprom,1024),"NEW retains all1024 EEPROM bytes");
  printf("{\"passed\":true,\"checks\":%u,\"game_wire_bytes\":120,\"core_wire_bytes\":3136,\"ordinary_input_only\":true,\"race_game_over\":true,\"courier_timer_game_over\":true,\"courier_actual_pickup_score\":%u,\"courier_route_virtual_ms\":%u,\"courier_input_frames\":%u,\"state_loads\":0,\"original_frame_period_ms\":33,\"race_driving_virtual_ms\":%u}\n",checks,deliveredScore,courierMs,courierFrames,driving);
}
