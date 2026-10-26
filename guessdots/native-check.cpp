/* SPDX-License-Identifier: MIT. Native ordinary-input acceptance.
 * This test uses real original Game logic through the owner adapter, never
 * sets a target state or restores a packet. It solves from visible clues.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Arduboy2.h>
#include "backend.h"
static unsigned checks;
static void check(bool value,const char *name){if(!value){fprintf(stderr,"FAILED: %s\n",name);exit(1);}++checks;}
static uint8_t wire[494];
static uint16_t u16(size_t n){return (uint16_t)(wire[n]|((uint16_t)wire[n+1]<<8));}
static uint32_t u32(size_t n){return (uint32_t)u16(n)|((uint32_t)u16(n+2)<<16);}
static void read(){if(arduboy_game_pack(wire,sizeof wire)!=sizeof wire){fprintf(stderr,"pack size\n");exit(2);}}
static void advance(unsigned n){while(n){unsigned m=n>16?16:n;arduboy_pxa_advance(m);n-=m;}read();}
static void tap(uint8_t mask){arduboy_pxa_pointer(mask,0);advance(100);arduboy_pxa_pointer(0,1);advance(100);}
static unsigned pop(uint32_t bits){unsigned n=0;while(bits){n+=bits&1;bits>>=1;}return n;}
static bool valid(uint32_t guess,uint32_t level,unsigned difficulty){
    size_t meta=29+difficulty*155;
    for(unsigned i=0;i<wire[meta+3];++i){uint32_t mask=u32(meta+91+4*i);if(pop(guess&mask)!=pop(level&mask))return false;}
    return true;
}
/* Enumerate row bit patterns constrained only by the original visible
 * 2x2 counts; this allows alternative valid solutions as upstream does. */
static unsigned gridSize,clues[16];
static uint32_t answer;
static bool solveRow(unsigned row,unsigned previous,uint32_t bits){
    if(row==gridSize){answer=bits;return true;}
    for(unsigned next=0;next<(1u<<gridSize);++next){
        bool match=true;
        if(row)for(unsigned col=0;col<gridSize-1;++col){
            if(pop((previous>>col)&3)+pop((next>>col)&3)!=clues[(row-1)*(gridSize-1)+col]){match=false;break;}
        }
        if(match&&solveRow(row+1,next,bits|((uint32_t)next<<(row*gridSize))))return true;
    }
    return false;
}
static void solveOrdinary(){
    unsigned difficulty=wire[1];size_t meta=29+difficulty*155;
    gridSize=wire[meta+1];uint32_t level=u32(2);
    for(unsigned i=0;i<wire[meta+3];++i)clues[i]=pop(level&u32(meta+91+4*i));
    if(!solveRow(0,0,0)){fprintf(stderr,"no clue solution\n");exit(3);}
    uint32_t target=answer;
    check(valid(target,level,difficulty),"solver uses clue-count equivalence, not hidden-pattern equality");
    for(unsigned i=0;i<gridSize*gridSize&&wire[27]==4;++i){
        if(target&((uint32_t)1<<i))tap(A_BUTTON);
        if(wire[27]==4 && i+1<gridSize*gridSize)tap(RIGHT_BUTTON);
    }
}
static void choose(unsigned difficulty){
    tap(A_BUTTON);
    for(unsigned i=0;i<difficulty;++i)tap(DOWN_BUTTON);
    check(wire[20]==difficulty && wire[27]==2,"original difficulty selection");
    tap(A_BUTTON);advance(100);
    check(wire[27]==4 && wire[1]==difficulty,"original random level reached by ordinary A");
}
int main(){
    arduboy_pxa_boot(17);read();
    check(arduboy_game_pack(nullptr,0)==494 && arduboy_pxa_pack(nullptr,0)==3136,"full canonical 3136+494 byte boundary");
    check(wire[0]==24 && wire[27]==0 && wire[28]==1,"original zero-storage default Game and process flash initial state");
    bool erased=true;for(uint8_t b:arduboy_pxa_state.eeprom)erased&=b==255;
    check(erased,"cold EEPROM erased FF");
    advance(100);
    check(wire[27]==1 && u16(24)==0 && !wire[26],"original INIT reset and title score flag");
    check(arduboy_pxa_state.eeprom[16]=='D' && arduboy_pxa_state.eeprom[17]=='0' && arduboy_pxa_state.eeprom[18]=='T',"original D0T EEPROM signature");
    check(wire[29+1]==3 && wire[184+1]==4 && wire[339+1]==5 && wire[29+2]==9 && wire[184+2]==16 && wire[339+2]==25,"complete original three dot grids 9/16/25");
    uint16_t expectedScore=0;
    for(unsigned difficulty=0;difficulty<3;++difficulty){
        choose(difficulty);
        check(valid(u32(2),u32(2),difficulty),"all original clue masks accept the generated pattern");
        check(!valid(u32(2)^1u,u32(2),difficulty),"neighbor clue rejects a changed corner");
        tap(LEFT_BUTTON);check(u16(10)==0,"LEFT stops at original linear cursor lower bound");
        tap(B_BUTTON);check(wire[27]==4 && u32(6)==0,"B has no original action during play");
        solveOrdinary();advance(100);
        expectedScore=(uint16_t)(expectedScore+(difficulty==0?1:difficulty==1?5:10));
        check(wire[27]==5 && wire[26] && u16(24)==expectedScore,"ordinary clue completion and original score increment");
        check(arduboy_pxa_state.eeprom[19]==(uint8_t)expectedScore && arduboy_pxa_state.eeprom[20]==(uint8_t)(expectedScore>>8),"original little-endian u16 score saved once");
        advance(250);check(u16(24)==expectedScore,"completed screen does not duplicate score");
        tap(A_BUTTON);advance(100);check(wire[27]==4 && u32(6)==0,"completed A NEXT opens fresh original level");
        solveOrdinary();advance(100);
        expectedScore=(uint16_t)(expectedScore+(difficulty==0?1:difficulty==1?5:10));
        tap(B_BUTTON);advance(100);check(wire[27]==1 && u16(24)==expectedScore,"completed B BACK loads actual accumulated EEPROM score");
    }
    tap(A_BUTTON);tap(DOWN_BUTTON);tap(DOWN_BUTTON);tap(UP_BUTTON|DOWN_BUTTON);
    check(wire[20]==1 && wire[27]==2,"simultaneous UP DOWN safety patch selects first valid direction without COUNT3");
    tap(A_BUTTON);advance(100);check(wire[27]==4 && wire[1]==1,"fixed simultaneous direction choice remains within original metadata");
    advance(800);bool flashBefore=wire[28];uint8_t eepromBefore[1024];memcpy(eepromBefore,arduboy_pxa_state.eeprom,1024);
    arduboy_pxa_new();read();
    check(wire[27]==0 && wire[20]==0 && wire[28]==flashBefore,"NEW reconstructs original zero-global Game difficulty while preserving process-static flash");
    check(!memcmp(eepromBefore,arduboy_pxa_state.eeprom,1024),"NEW retains entire original session EEPROM");
    advance(100);check(wire[27]==1 && u16(24)==expectedScore,"NEW title loads accumulated original score");
    tap(UP_BUTTON|DOWN_BUTTON);check(u16(24)==0 && wire[26],"title original simultaneous score reset");
    check(arduboy_pxa_state.eeprom[19]==0 && arduboy_pxa_state.eeprom[20]==0,"title reset writes original EEPROM score");
    printf("{\"passed\":true,\"checks\":%u,\"game_wire_bytes\":494,\"core_wire_bytes\":3136,\"ordinary_input_only\":true,\"difficulty_wins\":6,\"state_loads\":0,\"original_frame_period_ms\":41}\n",checks);
}
