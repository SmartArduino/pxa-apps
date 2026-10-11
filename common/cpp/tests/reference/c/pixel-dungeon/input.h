#ifndef PD_INPUT_H
#define PD_INPUT_H

#include <stdint.h>

#include "game.h"
#include "layout.h"
#include "pxa_ui.h"

/* Translates pointer events in render-buffer coordinates into game actions. */
void pd_input_pointer(pd_game_t *game, pd_layout_t *layout, int x, int y,
                      uint8_t pointer_id, uint8_t phase, uint64_t timestamp_us);

/* Controller/keyboard-style input: a direction pad plus face buttons. */
void pd_input_controller(pd_game_t *game, uint32_t buttons, uint32_t previous);

int pd_input_back(pd_game_t *game);

#endif
