/* Native only: compare final frozen platform guards with byte-identical raw core. */
#include <stdint.h>
#include <string.h>
#define SAF_GAME_ID 2
#include "../../saf/core.h"
#define SNAPSHOT_SIZE (559+4096)
void bloq_platform_get(uint8_t out[SNAPSHOT_SIZE]) {
 memcpy(out,memory,512);memcpy(out+512,&_SAF_frame,4);out[516]=_SAF_currentRandom;
 memcpy(out+517,_SAF_buttonStates,7);memcpy(out+524,_SAF_saveMemory,32);
 out[556]=stepFunction==menuStep;out[557]=menuItem;out[558]=firstClick;memcpy(out+559,saf_io.screen,4096);
}
void bloq_platform_init(void) { saf_boot(); }
void bloq_platform_step(unsigned mask) { saf_io.sampled=(uint8_t)mask;SAF_FE_loop(); }
unsigned bloq_platform_empty_fall(void) { memset(memory,0,150);return blocksFallStep(); }
unsigned bloq_platform_empty_drop(void) { memset(memory,0,150);memset(_SAF_buttonStates,0,7);_SAF_buttonStates[4]=1;blocksStep();return BLOCKS_SCORE; }
