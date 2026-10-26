/* Native-only oracle; observed states may be copied here, never to Guest. */
#define main original_native_test_main
#include "native.c"
#undef main
#define SNAPSHOT_SIZE (559+4096)
void bloq_qa_get(uint8_t out[SNAPSHOT_SIZE]) {
 memcpy(out,memory,512); memcpy(out+512,&_SAF_frame,4);out[516]=_SAF_currentRandom;
 memcpy(out+517,_SAF_buttonStates,7);memcpy(out+524,_SAF_saveMemory,32);
 out[556]=stepFunction==menuStep;out[557]=menuItem;out[558]=firstClick;
 memcpy(out+559,test_screen,4096);
}
void bloq_qa_set(const uint8_t in[SNAPSHOT_SIZE]) {
 memcpy(memory,in,512);memcpy(&_SAF_frame,in+512,4);_SAF_currentRandom=in[516];
 memcpy(_SAF_buttonStates,in+517,7);memcpy(_SAF_saveMemory,in+524,32);memcpy(test_save,in+524,32);_SAF_saveMemoryLoaded=1;
 stepFunction=in[556]?menuStep:blocksStep;menuItem=in[557];firstClick=in[558];
 memcpy(test_screen,in+559,4096);
}
void bloq_qa_step(unsigned held_mask) {
 for(unsigned i=0;i<7;i++)test_keys[i]=(held_mask>>i)&1;
 SAF_FE_loop();
}
void bloq_qa_init(unsigned seed) {
 memset(test_save,0,32);memset(_SAF_saveMemory,0,32);_SAF_saveMemoryLoaded=1;
 memset(_SAF_buttonStates,0,7);memset(test_screen,0,4096);_SAF_frame=0;
 menuItem=2;firstClick=1;SAF_randomSeed(seed);blocksInit();stepFunction=blocksStep;blocksDraw();
}

void bloq_qa_rgb(unsigned colour,uint8_t out[3]) { SAF_colorToRGB(colour,out,out+1,out+2); }

void bloq_qa_release(void) { memset(_SAF_buttonStates,0,7);memset(test_keys,0,7); }
void bloq_qa_restart(void) { bloq_qa_release();menuItem=2;firstClick=1;blocksInit();stepFunction=blocksStep;blocksDraw(); }

void bloq_qa_resume(void) { bloq_qa_release();stepFunction=blocksStep; }
