/* SPDX-License-Identifier: MIT. Original standalone FE init/loop and typed wire. */
#ifndef PXA_SAF_STANDALONE_CORE_H
#define PXA_SAF_STANDALONE_CORE_H
#include <stdint.h>
#include <string.h>
#define SAF_PLATFORM_PXA 1
#ifndef SAF_ENTRY_ID
#define SAF_ENTRY_ID 1
#endif
#if SAF_ENTRY_ID == 1
#include "vendor/platform/utd.ino"
#elif SAF_ENTRY_ID == 3
#include "vendor/platform/chess.ino"
#else
#error Unsupported standalone entry (1 microTD, 3 Chess; not minigames IDs)
#endif
static inline uint32_t saf_hash(const uint8_t *bytes,unsigned count){uint32_t h=2166136261u;while(count--)h=(h^*bytes++)*16777619u;return h;}
static inline void saf_release_all(void){saf_io.pointer=saf_io.controller=saf_io.pending=saf_io.pending_pointer=saf_io.pending_controller=saf_io.sampled=0;memset(_SAF_buttonStates,0,sizeof _SAF_buttonStates);}
static inline void saf_pointer_mask(uint8_t mask,int preserve_short_tap){
    mask&=127;uint8_t previous=saf_io.pointer;saf_io.pending_pointer|=mask&~previous;
    if(mask||!preserve_short_tap)saf_io.pending_pointer&=mask;
    saf_io.pointer=mask;saf_io.pending=saf_io.pending_pointer|saf_io.pending_controller;
}
static inline void saf_controller_mask(uint8_t mask){mask&=127;saf_io.pending_controller|=mask&~saf_io.controller;saf_io.controller=mask;saf_io.pending=saf_io.pending_pointer|saf_io.pending_controller;}
static inline unsigned saf_current_state(void){
#if SAF_ENTRY_ID == 1
    return UTD_gameState;
#else
    return state;
#endif
}
static inline unsigned saf_menu_item(void){
#if SAF_ENTRY_ID == 1
    return UTD_menuItem;
#else
    return menuItem;
#endif
}
static inline void saf_render_initial(void){
    saf_io.render_frame=_SAF_frame;
#if SAF_ENTRY_ID == 1
    UTD_draw();
#else
    orientBoard();if(state==STATE_MENU)drawMenu();else drawBoard();
#endif
}
/* Original menus/keys remain inside each standalone game, including B hold. */
static inline void saf_new_game(void){
    saf_release_all();saf_io.fault=0;SAF_FE_init();
#if SAF_ENTRY_ID == 3
    state=STATE_MENU;
#endif
    saf_render_initial();
}
static inline void saf_restart_game(void){
    saf_release_all();saf_io.fault=0;
#if SAF_ENTRY_ID == 1
    UTD_loadMap(UTD_mapIndex<UTD_MAPS?UTD_mapIndex:0);
#else
    initGame();state=STATE_PLAYING;
#endif
    saf_render_initial();
}
static inline void saf_resume(void){saf_release_all();}
static inline void saf_boot(void){
    memset(&saf_io,0,sizeof saf_io);memset(_SAF_buttonStates,0,sizeof _SAF_buttonStates);
    memset(_SAF_saveMemory,0,sizeof _SAF_saveMemory);_SAF_saveMemoryLoaded=0;_SAF_frame=0;_SAF_currentRandom=0;
    saf_new_game();
}
static inline uint8_t saf_frame(void){
    saf_io.sampled=(uint8_t)(saf_io.pointer|saf_io.controller|saf_io.pending);saf_io.applied=saf_io.sampled;
    saf_io.render_frame=_SAF_frame;uint8_t result=SAF_FE_loop();saf_io.pending=saf_io.pending_pointer=saf_io.pending_controller=0;return result;
}
static inline uint16_t saf_rgb565(uint8_t color){uint8_t r,g,b;SAF_colorToRGB(color,&r,&g,&b);return (uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));}
static inline void saf_put_u16(uint8_t **p,uint16_t v){*(*p)++=(uint8_t)v;*(*p)++=(uint8_t)(v>>8);}
static inline void saf_put_u32(uint8_t **p,uint32_t v){saf_put_u16(p,(uint16_t)v);saf_put_u16(p,(uint16_t)(v>>16));}
static inline void saf_put_bytes(uint8_t **p,const void *data,unsigned bytes){memcpy(*p,data,bytes);*p+=bytes;}
static inline int saf_pointer_in(const uint8_t *ptr,const uint8_t *base,unsigned bytes,uint16_t *offset){
    uintptr_t value=(uintptr_t)ptr,start=(uintptr_t)base;if(value<start||value>start+bytes)return 0;*offset=(uint16_t)(value-start);return 1;
}
static inline void saf_put_image_pointer(uint8_t **p,const uint8_t *ptr,unsigned role){
    uint8_t tag=0;uint16_t offset=0;
    if(ptr){
#if SAF_ENTRY_ID == 1
        if(role==0&&saf_pointer_in(ptr,UTD_ramImage,sizeof UTD_ramImage,&offset))tag=1;
        else if(role==0&&saf_pointer_in(ptr,UTD_logo,sizeof UTD_logo,&offset))tag=3;
        else if(role==1&&saf_pointer_in(ptr,UTD_ramMask,sizeof UTD_ramMask,&offset))tag=2;
#else
        if(role==0&&saf_pointer_in(ptr,image,sizeof image,&offset))tag=1;
        else if(role==1&&saf_pointer_in(ptr,imageMask,sizeof imageMask,&offset))tag=2;
#endif
        else{saf_platform_fault(6);offset=0;tag=255;}
    }
    *(*p)++=tag;saf_put_u16(p,offset);
}
/* Read-only diagnostic snapshot. No pointers, native padding or state setter. */
static inline unsigned saf_pack_state(uint8_t out[1024]){
    uint8_t *p=out;saf_put_u32(&p,_SAF_frame);*p++=_SAF_currentRandom;
    saf_put_bytes(&p,_SAF_buttonStates,7);saf_put_bytes(&p,_SAF_saveMemory,32);*p++=_SAF_saveMemoryLoaded;
#if SAF_ENTRY_ID == 1
    const uint8_t fields[]={UTD_gameState,UTD_mapIndex,UTD_sound,UTD_backColor,UTD_menuItem,UTD_cameraPos,UTD_cursorPos,(uint8_t)UTD_lives,UTD_updateCounter,UTD_creepCount};
    saf_put_bytes(&p,fields,sizeof fields);saf_put_u16(&p,UTD_money);saf_put_u16(&p,UTD_round);
    saf_put_bytes(&p,UTD_map,34);for(unsigned i=0;i<128;i++)saf_put_u16(&p,UTD_mapTiles[i]);
    for(unsigned i=0;i<50;i++){*p++=UTD_creeps[i].typeFreeze;*p++=UTD_creeps[i].healthLives;*p++=UTD_creeps[i].pathStart;saf_put_u16(&p,(uint16_t)UTD_creeps[i].pathPosition);}
    saf_put_bytes(&p,UTD_ramImage,10);saf_put_bytes(&p,UTD_ramMask,10);
#else
    const uint8_t fields[]={state,menuItem,playerW,playerB,variant,soundOn,visualSelectedSquare,boardFlipped,(uint8_t)previousMove[0],(uint8_t)previousMove[1],(uint8_t)selectedPiece,(uint8_t)aiCountDown,SCL_currentRandom8};
    saf_put_bytes(&p,fields,sizeof fields);saf_put_u16(&p,SCL_currentRandom16);
    saf_put_bytes(&p,game.board,sizeof game.board);saf_put_bytes(&p,game.record,sizeof game.record);
    saf_put_u16(&p,game.state);saf_put_u16(&p,game.ply);for(unsigned i=0;i<16;i++)saf_put_u32(&p,game.prevMoves[i]);
    if(game.startState && game.startState!=(const char*)variantStartState)saf_platform_fault(6);
    *p++=game.startState?1:0;saf_put_bytes(&p,variantStartState,sizeof variantStartState);
    saf_put_bytes(&p,allowedSquares,sizeof allowedSquares);saf_put_bytes(&p,image,sizeof image);saf_put_bytes(&p,imageMask,sizeof imageMask);
    if(_SCL_staticEvaluationFunction&&_SCL_staticEvaluationFunction!=SCL_boardEvaluateStatic)saf_platform_fault(6);
    *p++=_SCL_staticEvaluationFunction?1:0;*p++=(uint8_t)_SCL_depthHardLimit;saf_put_u16(&p,(uint16_t)_SCL_currentPositionScore);
#endif
    saf_put_image_pointer(&p,_SAF_drawnImage.image,0);saf_put_image_pointer(&p,_SAF_drawnImage.binaryMask,1);saf_put_image_pointer(&p,_SAF_drawnImage.palette,2);
    const uint8_t scratch[]={_SAF_drawnImage.mode,_SAF_drawnImage.transparentColor,_SAF_drawnImage.binaryLine,_SAF_drawnImage.binaryMaskLine,_SAF_drawnImage.binaryPosition,_SAF_drawnImage.binaryColor1,_SAF_drawnImage.binaryColor2,_SAF_drawnImage.rleCount,_SAF_drawnImage.rleLastColor};
    saf_put_bytes(&p,scratch,sizeof scratch);
    return (unsigned)(p-out);
}
#endif
