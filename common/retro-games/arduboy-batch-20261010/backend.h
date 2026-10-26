/* SPDX-License-Identifier: MIT. Independent PXA frontend/owner contract. */
#ifndef ARDUBOY_PXA_BACKEND_H
#define ARDUBOY_PXA_BACKEND_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ARDU_FB_BYTES 1024
#define ARDU_EEPROM_BYTES 1024
#define ARDU_GAME_MAX_BYTES 4096
#define ARDU_ORIGINAL_BUTTON_MASK 252
/* Owner calls only original game init/logic or an explicit original continuation. */
void arduboy_game_boot(void);
void arduboy_game_step(void);
size_t arduboy_game_pack(uint8_t *out,size_t capacity);
const char *arduboy_game_name(void);
void arduboy_pxa_wait(uint32_t milliseconds);
uint32_t arduboy_pxa_millis(void);
void arduboy_pxa_boot(uint32_t observed_clock_seed);
void arduboy_pxa_new(void);
void arduboy_pxa_advance(uint32_t active_elapsed_ms);
void arduboy_pxa_pointer(uint8_t original_mask,int preserve_short_tap);
void arduboy_pxa_controller(uint8_t original_mask);
void arduboy_pxa_release(void);
size_t arduboy_pxa_pack(uint8_t *out,size_t capacity);
uint32_t arduboy_pxa_hash(const uint8_t *data,size_t bytes);
typedef struct {
 uint32_t clock_ms,next_due_ms,steps,seed,rng,display_count,display_step,dropped_steps;
 uint8_t pointer,controller,pending_pointer,pending_controller,applied,sampled,fault,constructors;
 uint16_t tone1,tone2;
 uint8_t display[ARDU_FB_BYTES],eeprom[ARDU_EEPROM_BYTES];
} arduboy_pxa_state_t;
extern arduboy_pxa_state_t arduboy_pxa_state;
#ifdef __cplusplus
}
#endif
#endif
