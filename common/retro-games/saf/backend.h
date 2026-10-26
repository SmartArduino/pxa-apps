/* SPDX-License-Identifier: MIT. PXA/native boundary; original SAF owns rules. */
#ifndef PXA_SAF_BACKEND_H
#define PXA_SAF_BACKEND_H
typedef struct {
    uint8_t screen[4096], saved[32];
    uint8_t pointer, controller, pending, sampled, applied, fault, menu_requested;
    uint32_t sounds[4], save_writes, pixel_writes, render_frame;
} saf_backend_t;
static saf_backend_t saf_io;
static inline void saf_platform_fault(uint8_t code) { if(!saf_io.fault)saf_io.fault=code; }
/* NONE must exist at an even index <=254. Read only this 256-byte list area. */
static inline uint16_t saf_platform_runner_end(const uint8_t *list) {
    for(uint16_t i=0;i<=254;i+=2)if(!list[i])return i;
    saf_platform_fault(2);return 256;
}
static inline void SAF_FE_drawPixel(uint8_t x,uint8_t y,uint8_t color) {
    if(x>=64||y>=64){saf_platform_fault(4);return;}
    saf_io.screen[(unsigned)y*64+x]=color;saf_io.pixel_writes++;
}
static inline uint8_t SAF_FE_buttonPressed(uint8_t button) {
    return button<7 && (saf_io.sampled&(1u<<button));
}
static inline void SAF_FE_playSound(uint8_t sound) { if(sound<4)saf_io.sounds[sound]++; }
static inline void SAF_FE_save(uint8_t index,uint8_t data) {
    if(index<32){saf_io.saved[index]=data;saf_io.save_writes++;}
}
static inline uint8_t SAF_FE_load(uint8_t index) { return index<32?saf_io.saved[index]:0; }
static inline const char *SAF_FE_extension(const char *request) { (void)request;return ""; }
#undef SAF_PLATFORM_NAME
#define SAF_PLATFORM_NAME "PXA"
#undef SAF_PLATFORM_HAS_SOUND
#define SAF_PLATFORM_HAS_SOUND 0
#undef SAF_PLATFORM_HAS_SAVES
#define SAF_PLATFORM_HAS_SAVES 0 /* session cache is provided; no disk persistence */
#endif
