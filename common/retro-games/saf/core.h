/* SPDX-License-Identifier: MIT. Thin entry/lifecycle around original minigames. */
#ifndef PXA_SAF_CORE_H
#define PXA_SAF_CORE_H
#include <stdint.h>
#include <string.h>
#define SAF_PLATFORM_PXA 1
#include "vendor/platform/minigames.ino"
#ifndef SAF_GAME_ID
#define SAF_GAME_ID 0
#endif
static inline uint32_t saf_hash(const uint8_t *data,unsigned count) {
    uint32_t value=2166136261u;for(unsigned i=0;i<count;i++)value=(value^data[i])*16777619u;return value;
}
static inline unsigned saf_current_game(void) {
    if(stepFunction==snakeStep)return 0;if(stepFunction==minesStep)return 1;
    if(stepFunction==blocksStep)return 2;if(stepFunction==g2048Step)return 3;
    if(stepFunction==runnerStep)return 4;return 5;
}
static inline void saf_release_all(void) {
    saf_io.pointer=saf_io.controller=saf_io.pending=saf_io.sampled=0;
    memset(_SAF_buttonStates,0,sizeof(_SAF_buttonStates));
}
static inline void saf_pointer_mask(uint8_t mask,int preserve_short_tap) {
    mask&=127;uint8_t old=saf_io.pointer;
    saf_io.pending|=mask&~old;
    if(mask||!preserve_short_tap)saf_io.pending&=(uint8_t)(mask|saf_io.controller);
    saf_io.pointer=mask;
}
static inline void saf_controller_mask(uint8_t mask) {
    mask&=127;saf_io.pending|=mask&~saf_io.controller;saf_io.controller=mask;
}
static inline void saf_render_initial(unsigned game) {
    saf_io.render_frame=_SAF_frame;
    if(game==0)snakeDraw();else if(game==2)blocksDraw();else if(game==4)runnerDraw();
}
/* NEW and RESTART intentionally retain original virtual time, RNG and scores. */
static inline void saf_select_game(unsigned game) {
    saf_release_all();saf_io.fault=0;saf_io.menu_requested=0;menuItem=(uint8_t)game;firstClick=1;
    if(game==0){snakeInit();stepFunction=snakeStep;}
    else if(game==2){blocksInit();stepFunction=blocksStep;}
    else if(game==4){runnerInit();stepFunction=runnerStep;}
    else{saf_platform_fault(5);return;}
    saf_render_initial(game);
}
/* Consume original B-long menu at the platform boundary, without another game. */
static inline void saf_resume_selected(void) {
    saf_release_all();saf_io.menu_requested=0;
    if(SAF_GAME_ID==0)stepFunction=snakeStep;
    else if(SAF_GAME_ID==2)stepFunction=blocksStep;
    else if(SAF_GAME_ID==4)stepFunction=runnerStep;
}
static inline void saf_boot(void) {
    memset(&saf_io,0,sizeof(saf_io));memset(memory,0,sizeof(memory));
    memset(_SAF_buttonStates,0,sizeof(_SAF_buttonStates));
    memset(_SAF_saveMemory,0,sizeof(_SAF_saveMemory));_SAF_saveMemoryLoaded=0;
    _SAF_frame=0;_SAF_currentRandom=0;menuItem=0;firstClick=0;
    SAF_FE_init();saf_select_game(SAF_GAME_ID);
}
static inline uint8_t saf_frame(void) {
    saf_io.sampled=(uint8_t)(saf_io.pointer|saf_io.controller|saf_io.pending);
    saf_io.applied=saf_io.sampled; /* retain the actual executed mask across B18 clear */
    saf_io.render_frame=_SAF_frame;
    uint8_t result=SAF_FE_loop();saf_io.pending=0;
    if(saf_current_game()!=SAF_GAME_ID){saf_io.menu_requested=1;saf_release_all();return 0;}
    return result;
}
static inline uint16_t saf_rgb565(uint8_t color) {
    uint8_t r,g,b;SAF_colorToRGB(color,&r,&g,&b);
    return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
#endif
