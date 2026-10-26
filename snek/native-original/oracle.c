/* SPDX-License-Identifier: MIT. Native-only reference to original CC0 SNEK. */
#define main original_native_test_main
#include "test.c"
#undef main
#define SNAPSHOT_SIZE (559+4096)
void snek_qa_get(uint8_t out[SNAPSHOT_SIZE]) {
 memcpy(out,memory,512);memcpy(out+512,&_SAF_frame,4);out[516]=_SAF_currentRandom;
 memcpy(out+517,_SAF_buttonStates,7);memcpy(out+524,_SAF_saveMemory,32);
 out[556]=stepFunction==menuStep;out[557]=menuItem;out[558]=firstClick;
 memcpy(out+559,pixels,4096);
}
void snek_qa_set(const uint8_t in[SNAPSHOT_SIZE]) {
 memcpy(memory,in,512);memcpy(&_SAF_frame,in+512,4);_SAF_currentRandom=in[516];
 memcpy(_SAF_buttonStates,in+517,7);memcpy(_SAF_saveMemory,in+524,32);
 memcpy(saves,in+524,32);_SAF_saveMemoryLoaded=1;
 stepFunction=in[556]?menuStep:snakeStep;menuItem=in[557];firstClick=in[558];
 memcpy(pixels,in+559,4096);
}
void snek_qa_step(unsigned held_mask) { for(unsigned i=0;i<7;i++)rawButtons[i]=(held_mask>>i)&1;assert(SAF_FE_loop()); }
void snek_qa_init(unsigned seed) {
 reset();SAF_randomSeed(seed);snakeInit();stepFunction=snakeStep;menuItem=0;firstClick=1;snakeDraw();
}
void snek_qa_rgb(unsigned color,uint8_t out[3]) { SAF_colorToRGB(color,out,out+1,out+2); }
void snek_qa_release(void) { memset(_SAF_buttonStates,0,7);memset(rawButtons,0,7); }
void snek_qa_restart(void) { snek_qa_release();menuItem=0;firstClick=1;snakeInit();stepFunction=snakeStep;snakeDraw(); }
void snek_qa_resume(void) { snek_qa_release();stepFunction=snakeStep; }
/* Native-only convenient original SNEK field access; no Guest input. */
void snek_qa_info(int out[10]) {
 out[0]=SNAKE_HEAD_SQUARE;out[1]=SNAKE_TAIL_SQUARE;out[2]=SNAKE_FOOD_SQUARE;
 out[3]=SNAKE_SCORE;out[4]=SNAKE_MOVE_COUNT;out[5]=SNAKE_DIRECTION;
 out[6]=SNAKE_FOOD_STEPS;out[7]=SNAKE_GAME_STATE;out[8]=length();out[9]=getHiScore(0);
}
