/* SPDX-License-Identifier: MIT. Private native ordinary-input oracle.
 * Compiled only for verification, never linked into the Guest. No state loader.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../common/retro-games/arduboy-batch-20261010/backend.h"
extern "C" void qa_boot(uint32_t observed_boot_seed) { arduboy_pxa_boot(observed_boot_seed); }
extern "C" void qa_new(void) { arduboy_pxa_new(); }
extern "C" void qa_release(void) { arduboy_pxa_release(); }
extern "C" void qa_pointer(uint8_t mask,int preserve_short_tap) { arduboy_pxa_pointer(mask,preserve_short_tap); }
extern "C" void qa_controller(uint8_t mask) { arduboy_pxa_controller(mask); }
extern "C" void qa_advance(uint32_t active_elapsed_ms) { arduboy_pxa_advance(active_elapsed_ms); }
extern "C" size_t qa_pack(uint8_t *out,size_t capacity) { return arduboy_pxa_pack(out,capacity); }
extern "C" size_t qa_game(uint8_t *out,size_t capacity) { return arduboy_game_pack(out,capacity); }
extern "C" void qa_pixels(uint8_t out[1024]) { memcpy(out,arduboy_pxa_state.display,1024); }
extern "C" void qa_eeprom(uint8_t out[1024]) { memcpy(out,arduboy_pxa_state.eeprom,1024); }
extern "C" uint32_t qa_clock(void) { return arduboy_pxa_state.clock_ms; }
extern "C" uint32_t qa_due(void) { return arduboy_pxa_state.next_due_ms; }
extern "C" uint32_t qa_steps(void) { return arduboy_pxa_state.steps; }
extern "C" uint32_t qa_displays(void) { return arduboy_pxa_state.display_count; }
extern "C" uint32_t qa_rng(void) { return arduboy_pxa_state.rng; }
