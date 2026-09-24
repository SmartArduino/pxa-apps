#ifndef PXA_GAME_SCREEN_H
#define PXA_GAME_SCREEN_H

/* Shared display metrics for full-screen canvas games.
 *
 * A game keeps its gameplay field in a design space, but reads the real
 * logical screen size from the UI environment so backgrounds, HUD and hit
 * zones can reflow for the current panel (pai-touch 296x240, Watcher 412x412,
 * ...). Call pxa_game_screen_from_start() in pxa_app_start(), then
 * pxa_game_screen_handle_event() for PXA_UI_ENVIRONMENT_CHANGED and re-layout
 * when it reports a change. */
#include <stdint.h>

#include "pxa.h"
#include "pxa_ui.h"

#define PXA_GAME_SCREEN_DEFAULT_WIDTH 296u
#define PXA_GAME_SCREEN_DEFAULT_HEIGHT 240u

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t safe_left;
    uint32_t safe_top;
    uint32_t safe_right;
    uint32_t safe_bottom;
} pxa_game_screen_t;

static inline void pxa_game_screen_reset(pxa_game_screen_t *screen) {
    if (screen == NULL) return;
    screen->width = PXA_GAME_SCREEN_DEFAULT_WIDTH;
    screen->height = PXA_GAME_SCREEN_DEFAULT_HEIGHT;
    screen->safe_left = 0;
    screen->safe_top = 0;
    screen->safe_right = 0;
    screen->safe_bottom = 0;
}

static inline int pxa_game_screen_apply(pxa_game_screen_t *screen,
                                        const pxa_ui_environment_t *environment) {
    if (screen == NULL || environment == NULL || environment->width == 0 ||
        environment->height == 0)
        return 0;
    screen->width = environment->width;
    screen->height = environment->height;
    screen->safe_left = environment->safe_insets[3];
    screen->safe_top = environment->safe_insets[0];
    screen->safe_right = environment->safe_insets[1];
    screen->safe_bottom = environment->safe_insets[2];
    return 1;
}

static inline int pxa_game_screen_from_start(pxa_game_screen_t *screen,
                                             const uint8_t *config,
                                             uint32_t length) {
    pxa_ui_environment_t environment;
    pxa_game_screen_reset(screen);
    if (!pxa_ui_parse_start_environment(config, length, &environment))
        return 0;
    return pxa_game_screen_apply(screen, &environment);
}

/* Returns 1 when the event carried a new environment; the caller re-layouts
 * and re-renders. */
static inline int pxa_game_screen_handle_event(pxa_game_screen_t *screen,
                                               const pxa_event_t *event) {
    pxa_ui_environment_t environment;
    if (screen == NULL || event == NULL ||
        !pxa_ui_parse_environment_event(event, &environment))
        return 0;
    return pxa_game_screen_apply(screen, &environment);
}

/* Longest axis a centered design-space rect can use. */
static inline int pxa_game_screen_center_x(const pxa_game_screen_t *screen,
                                           int width) {
    const int available = (int)screen->width;
    if (width >= available) return 0;
    return (available - width) / 2;
}

static inline int pxa_game_screen_center_y(const pxa_game_screen_t *screen,
                                           int height) {
    const int available = (int)screen->height;
    if (height >= available) return 0;
    return (available - height) / 2;
}

#endif
