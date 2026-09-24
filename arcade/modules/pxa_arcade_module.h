#ifndef PXA_ARCADE_MODULE_H
#define PXA_ARCADE_MODULE_H

#include <stddef.h>
#include <stdint.h>

#include "pxa_app_messages.h"
#include "pxa_canvas.h"
#include "pxa_game_screen.h"
#include "pxa_system.h"

#ifndef PXA_ARCADE_MODULE_PREFIX
#error "PXA_ARCADE_MODULE_PREFIX must be defined before including this header"
#endif

#define PXA_ARCADE_JOIN_INNER(prefix, name) prefix##name
#define PXA_ARCADE_JOIN(prefix, name) PXA_ARCADE_JOIN_INNER(prefix, name)
#define PXA_ARCADE_EXPORT(name) PXA_ARCADE_JOIN(PXA_ARCADE_MODULE_PREFIX, name)

#ifndef PXA_ARCADE_STANDALONE_TEST
#define pxa_app_start PXA_ARCADE_EXPORT(start)
#define pxa_app_on_event PXA_ARCADE_EXPORT(on_event)
#define pxa_app_stop PXA_ARCADE_EXPORT(stop)
extern uint32_t pxa_arcade_ui_generation;
extern pxa_game_screen_t pxa_arcade_screen;
const char *pxa_arcade_message(pxa_i18n_message_id_t id);
size_t pxa_arcade_text_size(const char *text);
#else
static uint32_t pxa_arcade_ui_generation;
static pxa_game_screen_t pxa_arcade_screen = {
    PXA_GAME_SCREEN_DEFAULT_WIDTH, PXA_GAME_SCREEN_DEFAULT_HEIGHT, 0, 0, 0, 0};
static inline const char *pxa_arcade_message(pxa_i18n_message_id_t id) {
    (void)id;
    return "";
}
static inline size_t pxa_arcade_text_size(const char *text) {
    size_t size = 0;
    while (text != NULL && text[size] != '\0') ++size;
    return size;
}
#endif

/* The mini-games keep their 296x240 design coordinates and hit zones. On a
 * larger panel the canvas node is pinned to a centered design-size region
 * after the first present so input stays in design pixels and the game is not
 * glued to the top-left corner. */
static inline int pxa_arcade_present(uint32_t node_id,
                                     pxa_canvas_frame_t *canvas,
                                     uint32_t *generation,
                                     uint8_t *initialized,
                                     uint8_t *commands,
                                     size_t commands_capacity,
                                     uint8_t *packet,
                                     size_t packet_capacity) {
    const uint8_t created = initialized != NULL && *initialized == 0;
    pxa_ui_transaction_t transaction = {0};
    int node_w;
    int node_h;
    int node_x;
    int node_y;
    uint32_t next;
    if (!pxa_canvas_present(node_id, canvas, generation, initialized, commands,
                            commands_capacity, packet, packet_capacity))
        return 0;
    if (!created || generation == NULL || *generation == 0) return 1;
    node_w = (int)pxa_arcade_screen.width < 296 ? (int)pxa_arcade_screen.width
                                                : 296;
    node_h = (int)pxa_arcade_screen.height < 240
                 ? (int)pxa_arcade_screen.height
                 : 240;
    node_x = (int)pxa_arcade_screen.safe_left +
             ((int)pxa_arcade_screen.width - (int)pxa_arcade_screen.safe_left -
              (int)pxa_arcade_screen.safe_right - node_w) /
                 2;
    node_y = (int)pxa_arcade_screen.safe_top +
             ((int)pxa_arcade_screen.height - (int)pxa_arcade_screen.safe_top -
              (int)pxa_arcade_screen.safe_bottom - node_h) /
                 2;
    if (node_x < 0) node_x = 0;
    if (node_y < 0) node_y = 0;
    next = *generation + 1u;
    if (next == 0) return 1;
    if (pxa_ui_transaction_begin(&transaction, next, PXA_UI_TRANSACTION_PATCH,
                                 packet, packet_capacity) &&
        pxa_ui_set_u8(&transaction, node_id, PXA_UI_PROPERTY_POSITION, 1) &&
        pxa_ui_set_length(&transaction, node_id, PXA_UI_PROPERTY_X,
                          PXA_UI_LENGTH_LOGICAL_PX, node_x) &&
        pxa_ui_set_length(&transaction, node_id, PXA_UI_PROPERTY_Y,
                          PXA_UI_LENGTH_LOGICAL_PX, node_y) &&
        pxa_ui_set_length(&transaction, node_id, PXA_UI_PROPERTY_WIDTH,
                          PXA_UI_LENGTH_LOGICAL_PX, node_w) &&
        pxa_ui_set_length(&transaction, node_id, PXA_UI_PROPERTY_HEIGHT,
                          PXA_UI_LENGTH_LOGICAL_PX, node_h) &&
        pxa_ui_transaction_commit(&transaction)) {
        *generation = next;
        return 1;
    }
    if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
    return 1;
}

#define PXA_ARCADE_MSG(id) pxa_arcade_message((id))

#endif
