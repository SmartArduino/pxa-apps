/* SPDX-License-Identifier: MIT. Native-only original RUNR platform boundary tests. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define SAF_GAME_ID 4
#include "../common/retro-games/saf/core.h"
static unsigned count;
static void check(int condition,const char*name){printf("{\"name\":\"%s\",\"passed\":%s}\n",name,condition?"true":"false");if(!condition)assert(condition);count++;}
int main(void){
 saf_boot();check(saf_current_game()==4&&_SAF_frame==0&&_SAF_currentRandom==0&&memory[0]==0&&RUNNER_STATE==0,"frozen bridge boot selects original RUNR with empty state");
 saf_pointer_mask(16,0);saf_pointer_mask(0,1);saf_frame();check(RUNNER_PLAYER_PHASE==7&&_SAF_buttonStates[4]==1&&saf_io.applied==16,"short real pointer pulse retained exactly original A first frame");saf_frame();check(_SAF_buttonStates[4]==0&&saf_io.applied==0&&RUNNER_PLAYER_PHASE==14,"pointer pulse released on following original frame");
 saf_boot();saf_pointer_mask(32,0);for(int i=0;i<17;i++)check(saf_frame()==1,"original B before18 retains active RUNR");check(saf_frame()==0&&saf_current_game()==5&&saf_io.menu_requested&&saf_io.applied==32,"original B18 quits only to platform pause before original menu frame");
 uint8_t saved_memory[512],saved_save[32];memcpy(saved_memory,memory,512);memcpy(saved_save,saf_io.saved,32);uint32_t saved_frame=_SAF_frame;uint8_t saved_rng=_SAF_currentRandom;check(saf_io.pointer==0&&saf_io.pending==0&&saf_io.sampled==0&&_SAF_buttonStates[5]==0,"B18 boundary clears held and pending without lost executed mask");saf_resume_selected();check(saf_current_game()==4&&!saf_io.menu_requested&&!memcmp(saved_memory,memory,512)&&!memcmp(saved_save,saf_io.saved,32)&&saved_frame==_SAF_frame&&saved_rng==_SAF_currentRandom,"CONTINUE restores original RUNR step without Init time RNG memory or save changes");
 saf_pointer_mask(3,0);saf_release_all();check(!saf_io.pointer&&!saf_io.pending&&!saf_io.controller&&!saf_io.sampled&&!_SAF_buttonStates[0],"native lifecycle CANCEL-style boundary releases all input (Host CANCEL unavailable)");
 memcpy(saved_memory,memory,512);saved_frame=_SAF_frame;saved_rng=_SAF_currentRandom;saf_select_game(4);check(_SAF_frame==saved_frame&&_SAF_currentRandom==saved_rng&&RUNNER_POSITION==0&&RUNNER_SCORE==0&&memory[0]==0,"NEW RESTART invokes original Init retains frame RNG save");for(unsigned i=1;i<464;i++)assert(memory[i]==saved_memory[i]);
 saf_boot();for(unsigned i=0;i<126;i++){memory[2*i]=1;memory[2*i+1]=72;}memory[252]=0;runnerAddObstacle(1);check(!saf_io.fault&&memory[254]==0&&memory[252]==1,"guard preserves original capacity126 append to127 and sentinel254");memcpy(saved_memory,memory,512);saved_rng=_SAF_currentRandom;runnerAddObstacle(1);check(saf_io.fault==2&&!memcmp(saved_memory,memory,512)&&saved_rng==_SAF_currentRandom,"guard refuses abnormal full127 append without uint8 wrap or RNG change");
 for(unsigned fn=0;fn<5;fn++){saf_boot();for(unsigned i=0;i<256;i+=2){memory[i]=1;memory[i+1]=72;}if(fn==0)runnerAddObstacle(1);else if(fn==1)runnerRemoveObstacle(0);else if(fn==2)runnerResolveCollisions();else if(fn==3)runnerDraw();else runnerScroll();check(saf_io.fault==2,"missing sentinel bounded guard returns fault2 for each list traversal");}
 saf_boot();runnerRemoveObstacle(1);check(saf_io.fault==2,"odd removal index rejected by exact platform guard");
 printf("{\"summary\":{\"passed\":true,\"checks\":%u,\"guest_executed\":false}}\n",count);return 0;
}
