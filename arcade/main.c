#include "pxa_ui.h"
#include "pxa_app_messages.h"
#include "pxa_game_screen.h"
#include "pxa_i18n.h"

typedef int32_t (*pxa_arcade_start_fn)(const uint8_t *config, uint32_t config_length);
typedef int32_t (*pxa_arcade_event_fn)(const uint8_t *event, uint32_t length);
typedef void (*pxa_arcade_stop_fn)(uint32_t reason);

typedef struct {
    pxa_i18n_message_id_t name;
    pxa_i18n_message_id_t description;
    const char *icon_path;
    pxa_arcade_start_fn start;
    pxa_arcade_event_fn on_event;
    pxa_arcade_stop_fn stop;
} pxa_arcade_game_t;

#define PXA_ARCADE_DECLARE(name) \
    int32_t pxa_arcade_##name##_start(const uint8_t *config, uint32_t config_length); \
    int32_t pxa_arcade_##name##_on_event(const uint8_t *event, uint32_t length); \
    void pxa_arcade_##name##_stop(uint32_t reason)

PXA_ARCADE_DECLARE(brick_breaker);
PXA_ARCADE_DECLARE(flappy_bird);
PXA_ARCADE_DECLARE(jump_jump);
PXA_ARCADE_DECLARE(jumping);
PXA_ARCADE_DECLARE(minesweeper);
PXA_ARCADE_DECLARE(tetris);

#define PXA_ARCADE_GAME(name, title, detail, icon) \
    {title, detail, icon, \
     pxa_arcade_##name##_start, pxa_arcade_##name##_on_event, \
     pxa_arcade_##name##_stop}

static const pxa_arcade_game_t games[] = {
    PXA_ARCADE_GAME(brick_breaker, PXA_MSG_GAME_BRICK_BREAKER_NAME,
                    PXA_MSG_GAME_BRICK_BREAKER_DESCRIPTION,
                    "assets/brick-breaker/icon.png"),
    PXA_ARCADE_GAME(flappy_bird, PXA_MSG_GAME_FLAPPY_BIRD_NAME,
                    PXA_MSG_GAME_FLAPPY_BIRD_DESCRIPTION,
                    "assets/flappy-bird/icon.png"),
    PXA_ARCADE_GAME(jump_jump, PXA_MSG_GAME_JUMP_JUMP_NAME,
                    PXA_MSG_GAME_JUMP_JUMP_DESCRIPTION,
                    "assets/jump-jump/icon.png"),
    PXA_ARCADE_GAME(jumping, PXA_MSG_GAME_JUMPING_NAME,
                    PXA_MSG_GAME_JUMPING_DESCRIPTION,
                    "assets/jumping/icon.png"),
    PXA_ARCADE_GAME(minesweeper, PXA_MSG_GAME_MINESWEEPER_NAME,
                    PXA_MSG_GAME_MINESWEEPER_DESCRIPTION,
                    "assets/minesweeper-music/icon.png"),
    PXA_ARCADE_GAME(tetris, PXA_MSG_GAME_TETRIS_NAME,
                    PXA_MSG_GAME_TETRIS_DESCRIPTION,
                    "assets/tetris/icon.png")
};

#define PXA_ARCADE_GAME_COUNT ((uint8_t)(sizeof(games) / sizeof(games[0])))
#define PXA_ARCADE_MENU_LIST_NODE UINT32_C(5)
#define PXA_ARCADE_MENU_ITEM_NODE_BASE UINT32_C(20)
#define PXA_ARCADE_MENU_TITLE_NODE_BASE UINT32_C(40)
#define PXA_ARCADE_MENU_DETAIL_NODE_BASE UINT32_C(60)
#define PXA_ARCADE_MENU_ICON_NODE_BASE UINT32_C(80)
#define PXA_ARCADE_MENU_CONTENT_NODE_BASE UINT32_C(100)

static uint8_t packet[4096];
uint32_t pxa_arcade_ui_generation;
pxa_game_screen_t pxa_arcade_screen;
static uint8_t active_game = UINT8_MAX;
static int32_t menu_scroll_y;
static pxa_i18n_t i18n;

const char *pxa_arcade_message(pxa_i18n_message_id_t id) {
    return pxa_i18n_cstr(&i18n, id);
}

size_t pxa_arcade_text_size(const char *text) {
    size_t size = 0;
    while (text != NULL && text[size] != '\0') ++size;
    return size;
}

static size_t string_length(const char *value) {
    size_t size = 0;
    while (value != NULL && value[size] != '\0') ++size;
    return size;
}

static int create_menu_item(pxa_ui_transaction_t *transaction,
                            uint8_t game_index) {
    const pxa_arcade_game_t *game = &games[game_index];
    const uint32_t item_node = PXA_ARCADE_MENU_ITEM_NODE_BASE + game_index;
    const uint32_t title_node = PXA_ARCADE_MENU_TITLE_NODE_BASE + game_index;
    const uint32_t detail_node = PXA_ARCADE_MENU_DETAIL_NODE_BASE + game_index;
    const uint32_t icon_node = PXA_ARCADE_MENU_ICON_NODE_BASE + game_index;
    const uint32_t content_node = PXA_ARCADE_MENU_CONTENT_NODE_BASE + game_index;

    return pxa_ui_create_typed(transaction, item_node,
                               PXA_ARCADE_MENU_LIST_NODE, 0,
                               PXA_UI_NODE_CONTROL,
                               PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_length(transaction, item_node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_length(transaction, item_node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, 64) &&
           pxa_ui_set_u8(transaction, item_node, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, item_node, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_padding(transaction, item_node, 10, 8, 10, 8) &&
           pxa_ui_set_dp(transaction, item_node, PXA_UI_PROPERTY_GAP, 11) &&
           pxa_ui_set_dp(transaction, item_node, PXA_UI_PROPERTY_RADIUS, 8) &&
           pxa_ui_set_dp(transaction, item_node,
                         PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
           pxa_ui_set_theme_color(transaction, item_node,
                                  PXA_UI_PROPERTY_BACKGROUND,
                                  PXA_UI_THEME_SURFACE) &&
           pxa_ui_set_theme_color(transaction, item_node,
                                  PXA_UI_PROPERTY_BORDER_COLOR,
                                  PXA_UI_THEME_BORDER) &&
           pxa_ui_set_event_mask(transaction, item_node,
                                 PXA_UI_EVENT_MASK_CLICK) &&
           pxa_ui_create(transaction, icon_node, item_node, 0,
                         PXA_UI_NODE_IMAGE) &&
           pxa_ui_set_length(transaction, icon_node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_PX, 44) &&
           pxa_ui_set_length(transaction, icon_node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, 44) &&
           pxa_ui_set_u8(transaction, icon_node, PXA_UI_PROPERTY_IMAGE_FIT,
                         PXA_UI_IMAGE_FIT_CONTAIN) &&
           pxa_ui_set_property(transaction, icon_node, PXA_UI_PROPERTY_ASSET,
                               game->icon_path, string_length(game->icon_path)) &&
           pxa_ui_set_event_mask(transaction, icon_node, 0) &&
           pxa_ui_create(transaction, content_node, item_node, 0,
                         PXA_UI_NODE_BOX) &&
           pxa_ui_set_event_mask(transaction, content_node, 0) &&
           pxa_ui_set_u16(transaction, content_node, PXA_UI_PROPERTY_GROW, 1) &&
           pxa_ui_set_u8(transaction, content_node, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_COLUMN) &&
           pxa_ui_set_u8(transaction, content_node, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, content_node, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_START) &&
           pxa_ui_create(transaction, title_node, content_node, 0,
                         PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, title_node,
                           pxa_arcade_message(game->name),
                           pxa_i18n_size(&i18n, game->name)) &&
           pxa_ui_set_font_role(transaction, title_node,
                                PXA_UI_FONT_ROLE_BODY) &&
           pxa_ui_set_theme_color(transaction, title_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_TEXT) &&
           pxa_ui_set_event_mask(transaction, title_node, 0) &&
           pxa_ui_create(transaction, detail_node, content_node, 0,
                         PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, detail_node,
                           pxa_arcade_message(game->description),
                           pxa_i18n_size(&i18n, game->description)) &&
           pxa_ui_set_font_role(transaction, detail_node,
                                PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, detail_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED) &&
           pxa_ui_set_event_mask(transaction, detail_node, 0);
}

static int render_menu(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t next = pxa_arcade_ui_generation + 1u;
    uint8_t index;
    int ok;

    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                  packet, sizeof(packet))) {
        return 0;
    }
    ok = pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) &&
         pxa_ui_set_u8(&transaction, 1, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_theme_color(&transaction, 1, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create(&transaction, 2, 1, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 72) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 2, 22, 12, 14, 7) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 3, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 3,
                         pxa_arcade_message(PXA_MSG_SCREEN_TITLE),
                         pxa_i18n_size(&i18n, PXA_MSG_SCREEN_TITLE)) &&
         pxa_ui_set_font_role(&transaction, 3, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 3, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 4, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 4,
                         pxa_arcade_message(PXA_MSG_SCREEN_SUBTITLE),
                         pxa_i18n_size(&i18n, PXA_MSG_SCREEN_SUBTITLE)) &&
         pxa_ui_set_font_role(&transaction, 4, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 4, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, PXA_ARCADE_MENU_LIST_NODE, 1, 0,
                       PXA_UI_NODE_SCROLL) &&
         pxa_ui_set_length(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u16(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                        PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                       PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                       PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
         pxa_ui_set_u8(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                       PXA_UI_PROPERTY_SCROLLBAR, 1) &&
         pxa_ui_set_event_mask(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                               PXA_UI_EVENT_MASK_SCROLL) &&
         pxa_ui_set_padding(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                            12, 9, 12, 12) &&
         pxa_ui_set_dp(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                       PXA_UI_PROPERTY_GAP, 8);
    for (index = 0; ok && index < PXA_ARCADE_GAME_COUNT; ++index)
        ok = create_menu_item(&transaction, index);
    if (ok && menu_scroll_y > 0)
        ok = pxa_ui_set_dp(&transaction, PXA_ARCADE_MENU_LIST_NODE,
                           PXA_UI_PROPERTY_SCROLL_POSITION, menu_scroll_y);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    pxa_arcade_ui_generation = next;
    return 1;
}

static int open_game(uint8_t game_index) {
    if (games[game_index].start(NULL, 0) != PXA_STATUS_OK) return 0;
    active_game = game_index;
    return 1;
}

static int return_to_menu(void) {
    if (active_game != UINT8_MAX) {
        games[active_game].stop(0);
        active_game = UINT8_MAX;
    }
    return render_menu();
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)pxa_i18n_init_from_start_config(
        &i18n, &pxa_app_i18n_bundle, config, config_length);
    pxa_game_screen_from_start(&pxa_arcade_screen, config, config_length);
    return pxa_window_fullscreen() && render_menu()
               ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    {
        int locale_result = pxa_i18n_handle_event(&i18n, &parsed);
        if (locale_result != 0) {
            if (active_game != UINT8_MAX)
                return games[active_game].on_event(event, length);
            return locale_result == 1 && !render_menu()
                       ? PXA_STATUS_INTERNAL
                       : PXA_EVENT_HANDLED;
        }
    }
    if (pxa_game_screen_handle_event(&pxa_arcade_screen, &parsed) &&
        active_game == UINT8_MAX) {
        return render_menu() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (active_game != UINT8_MAX) {
        if (parsed.service == PXA_SERVICE_WINDOW &&
            parsed.opcode == PXA_WINDOW_BACK_REQUESTED)
            return return_to_menu() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        return games[active_game].on_event(event, length);
    }
    if (!pxa_ui_parse_event(&parsed, &ui_event)) return PXA_EVENT_UNHANDLED;
    if (ui_event.node == PXA_ARCADE_MENU_LIST_NODE &&
        ui_event.kind == PXA_UI_EVENT_SCROLL_KIND) {
        menu_scroll_y = ui_event.value > 0 ? ui_event.value : 0;
        return PXA_EVENT_HANDLED;
    }
    if (ui_event.node >= PXA_ARCADE_MENU_ITEM_NODE_BASE &&
        ui_event.node < PXA_ARCADE_MENU_ITEM_NODE_BASE + PXA_ARCADE_GAME_COUNT &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND) {
        return open_game((uint8_t)(ui_event.node -
                                   PXA_ARCADE_MENU_ITEM_NODE_BASE))
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    if (active_game != UINT8_MAX) games[active_game].stop(reason);
}
