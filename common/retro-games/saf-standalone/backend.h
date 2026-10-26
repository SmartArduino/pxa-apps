/* SPDX-License-Identifier: MIT. Standalone SAF callbacks, silent/session profile. */
#ifndef PXA_SAF_STANDALONE_BACKEND_H
#define PXA_SAF_STANDALONE_BACKEND_H
typedef struct {
    uint8_t screen[4096],saved[32];
    uint8_t pointer,controller,pending,pending_pointer,pending_controller,sampled,applied,fault;
    uint32_t sounds[4],save_writes,pixel_writes,render_frame;
} saf_backend_t;
static saf_backend_t saf_io;
static inline void saf_platform_fault(uint8_t code){if(!saf_io.fault)saf_io.fault=code;}
static inline void SAF_FE_drawPixel(uint8_t x,uint8_t y,uint8_t color){
    if(x>=64||y>=64){saf_platform_fault(4);return;}
    saf_io.screen[(unsigned)y*64+x]=color;saf_io.pixel_writes++;
}
static inline uint8_t SAF_FE_buttonPressed(uint8_t button){return button<7&&(saf_io.sampled&(1u<<button));}
static inline void SAF_FE_playSound(uint8_t sound){if(sound<4)saf_io.sounds[sound]++;}
static inline void SAF_FE_save(uint8_t index,uint8_t data){if(index<32){saf_io.saved[index]=data;saf_io.save_writes++;}}
static inline uint8_t SAF_FE_load(uint8_t index){return index<32?saf_io.saved[index]:0;}
static inline const char *SAF_FE_extension(const char *request){(void)request;return "";}
/* Original Chess requests the ESPboy watchdog during search. PXA has no such HW. */
static inline void wdt_reset(void){}
#undef SAF_PLATFORM_NAME
#define SAF_PLATFORM_NAME "PXA standalone"
#undef SAF_PLATFORM_HAS_SOUND
#define SAF_PLATFORM_HAS_SOUND 0
#undef SAF_PLATFORM_HAS_SAVES
#define SAF_PLATFORM_HAS_SAVES 0
#endif
