/* PXA App Store. */


#include "pxa_app_messages.h"
#include "pxa_log.h"
#include <cstdio>
static void store_ui_command_failed(unsigned opcode, int status) {
    char text[80];
    std::snprintf(text, sizeof(text), "store: UI transaction opcode=%u failed status=%d", opcode, status);
    (void)pxa_log_write(3, text);
}
#define PXA_UI_COMMAND_FAILED(opcode, status) store_ui_command_failed(opcode, status)
#include "pxa_ui.h"
#include "pxa_clock.h"
#include "pxa_device.h"
#include "pxa_i18n.h"
#include "pxa_net.h"
#include "pxa_permission.h"
#include "pxa_store_installer.h"
#include "pxa_window.h"

#include "store_client.h"
#include "search_image.h"
#include "store_json.h"
#include "store_model.h"
#include "store_layout.h"
#include "store_viewer.hpp"
#include "store_size.hpp"
#include <pxa/ui_wire.hpp>
#include "store_media.hpp"
#include <cstdlib>
#include <cstdio>

#define STORE_SCREEN_CATALOG 0
#define STORE_SCREEN_DETAIL 1
#define STORE_SCREEN_SEARCH 2
#define STORE_SCREEN_INSTALLED 3
#define STORE_SCREEN_DOWNLOADS 4
#define STORE_SCREEN_SUCCESS 5
#define STORE_SCREEN_VIEWER 6

#define STORE_IDLE 0
#define STORE_LOADING 1
#define STORE_READY 2
#define STORE_DENIED 3
#define STORE_FAILED 4

#define STORE_ERROR_NONE 0
#define STORE_ERROR_PERMISSION 1
#define STORE_ERROR_UNSUPPORTED 2
#define STORE_ERROR_UNAVAILABLE 3
#define STORE_ERROR_TIMEOUT 4
#define STORE_ERROR_LIMIT 5
#define STORE_ERROR_PROTOCOL 6
#define STORE_ERROR_NOT_FOUND 7
#define STORE_ERROR_HTTP 8
#define STORE_ERROR_FAILED 9

/* Empty catalog pages are possible because the Host applies rollout policy per
 * item, so a bounded number of pages is chained without user interaction. */
#define STORE_MAX_CHAIN 4

/* Set to 1 to trace the request state machine through the Host Log service. */
#define STORE_TRACE 0

/* Search field shape. The text-input variant reports edits through text events
 * and lets a Host input method attach to the field; it needs a Host that
 * forwards PXA_UI_EVENT_TEXT, so it stays off while the firmware in the field
 * does not carry the text payload yet. */
#define STORE_SEARCH_TEXT_INPUT 1

/* Debug: open the search screen directly so the text input and the system
 * input method can be checked without tapping the header. */
#define STORE_DEBUG_START_SEARCH 0

#define NODE_ROOT 1u
#define NODE_HEADER 2u
#define NODE_HEADER_TITLE 3u
#define NODE_REFRESH 4u
#define NODE_REFRESH_ICON 5u
#define NODE_REFRESH_LABEL 6u
#define NODE_FILTERS 7u
/* The status text lives in the footer next to the load-more button. */
#define NODE_STATUS_INLINE 49u
#define NODE_STATUS 8u
#define NODE_STATUS_TEXT 22u
#define NODE_COUNT 9u
#define NODE_LIST 10u
#define NODE_EMPTY 11u
#define NODE_FOOTER 12u
#define NODE_LOAD_MORE 13u
#define NODE_LOAD_MORE_LABEL 14u
#define NODE_BACK 15u
#define NODE_BACK_LABEL 16u
#define NODE_DETAIL_SCROLL 17u
#define NODE_DETAIL_STATUS 20u
#define NODE_HEADER_SEPARATOR 21u
#define NODE_INSTALL_BUTTON 18u
#define NODE_INSTALL_LABEL 19u
#define NODE_KIND_ALL 96u
#define NODE_KIND_BASE 100u
#define NODE_CATEGORY_ALL 97u
#define NODE_CATEGORY_BASE 120u
#define NODE_CHIP_STRIDE 4u
/* Search screen: fixed nodes 40..53, then one row per keyboard line. */
#define NODE_SEARCH_BACK 40u
#define NODE_SEARCH_BACK_LABEL 41u
#define NODE_SEARCH_BAR 42u
#define NODE_SEARCH_CLEAR 43u
#define NODE_SEARCH_CLEAR_LABEL 44u
#define NODE_SEARCH_FIELD 45u
#define NODE_SEARCH_TEXT 46u
#define NODE_SEARCH_KEYBOARD 47u
#define NODE_SEARCH_ICON 48u
#define NODE_SEARCH_SUBMIT 56u
#define NODE_SEARCH_EMPTY 53u
#define NODE_SEARCH_EMPTY_ICON 54u
#define NODE_SEARCH_EMPTY_TEXT 55u
#define NODE_SEARCH_ROW_BASE 50u
/* Catalog header search action; only used on the catalog surface. */
#define NODE_SEARCH_BUTTON 60u
#define NODE_SEARCH_BUTTON_LABEL 61u
/* Bottom tab bar and the header status line. */
#define NODE_TABBAR 62u
#define NODE_TAB_APPS 63u
#define NODE_TAB_APPS_LABEL 64u
#define NODE_TAB_APPS_ICON 1020u
#define NODE_TAB_GAMES 66u
#define NODE_TAB_GAMES_LABEL 67u
#define NODE_TAB_GAMES_ICON 1021u
#define NODE_HEADER_STATUS 69u
#define NODE_HEADER_ROW 70u
#define NODE_INSTALLED_ACTION 81u
#define NODE_INSTALLED_ACTION_LABEL 82u
#define NODE_DOWNLOADS_ACTION 83u
#define NODE_DOWNLOADS_ACTION_LABEL 84u
#define NODE_INSTALLED_ACTION_ICON 1022u
#define NODE_DOWNLOADS_ACTION_ICON 1023u
#define NODE_LIBRARY_STATUS 87u
#define NODE_LIBRARY_CHECK 88u
#define NODE_LIBRARY_CHECK_LABEL 89u
#define NODE_LIBRARY_BASE 1100u
#define NODE_LIBRARY_STRIDE 12u
#define NODE_SUCCESS_OVERLAY 1500u
#define NODE_SUCCESS_ICON 1501u
#define NODE_SUCCESS_OPEN 1502u
#define NODE_SUCCESS_LATER 1504u
#define NODE_SUCCESS_ICON_LABEL 1506u
#define NODE_SUCCESS_NAME 1507u
#define NODE_SUCCESS_BODY 1508u
#define NODE_SUCCESS_ACTIONS 1509u
#define NODE_KEY_BASE 300u
#define NODE_KEY_COUNT 28u
#define NODE_KEY_STRIDE 2u
#define NODE_DETAIL_HERO 30u
#define NODE_DETAIL_HERO_ICON 31u
#define NODE_DETAIL_HERO_LETTER 32u
#define NODE_DETAIL_HERO_INFO 33u
#define NODE_DETAIL_HERO_NAME 34u
#define NODE_DETAIL_HERO_META 35u
#define NODE_CARD_BASE 200u
#define NODE_CARD_STRIDE 12u
#define NODE_DETAIL_SECTION_BASE 900u
#define NODE_DETAIL_SECTION_STRIDE 4u
#define NODE_DETAIL_ROW_BASE 1000u
#define NODE_DETAIL_ROW_STRIDE 4u
#define NODE_TECHNICAL 1600u
#define NODE_TECHNICAL_LABEL 1601u
#define NODE_GALLERY 1610u
#define NODE_GALLERY_IMAGE 1611u
#define NODE_GALLERY_PLACEHOLDER 1612u
#define NODE_GALLERY_BAR 1613u
#define NODE_GALLERY_PREV 1614u
#define NODE_GALLERY_PREV_LABEL 1615u
#define NODE_GALLERY_COUNT 1616u
#define NODE_GALLERY_NEXT 1617u
#define NODE_GALLERY_NEXT_LABEL 1618u
#define NODE_VIEWPORT 1620u
#define NODE_VIEWER_CONTENT 1621u
#define NODE_VIEWER_IMAGE 1622u
#define NODE_VIEWER_STATUS 1623u
#define NODE_VIEWER_CONTROLS 1624u
#define NODE_VIEWER_PREV 1625u
#define NODE_VIEWER_PREV_LABEL 1626u
#define NODE_VIEWER_ZOOM 1627u
#define NODE_VIEWER_ZOOM_LABEL 1628u
#define NODE_VIEWER_NEXT 1629u
#define NODE_VIEWER_NEXT_LABEL 1630u

#define SECTION_SUMMARY 0u
#define SECTION_CHANGELOG 1u
#define SECTION_PERMISSIONS 2u
#define SECTION_SERVICES 3u
#define SECTION_INSTALL 4u

#define ROW_VERSION 0u
#define ROW_SIZE 1u
#define ROW_CHANNEL 2u
#define ROW_SEQUENCE 3u
#define ROW_PUBLISHER 4u
#define ROW_DIGEST 5u
#define ROW_PROFILE 6u
#define ROW_SDK 7u
#define ROW_PLATFORMS 8u

#define ICON_REFRESH "\xef\x80\xa1"
#define ICON_BACK "\xef\x81\x93"
/* HOME is present in both LVGL's symbol font and the product's compact
 * Font Awesome font. The previous TH_LIST (U+F00B) is not portable. */
#define ICON_APPS "\xef\x80\x95"
#define ICON_GAMES "\xef\x81\x8b"
#define ICON_INSTALLED "\xef\x80\x8c"
#define ICON_DOWNLOADS "\xef\x80\x99"

typedef struct {
    pxa_i18n_t i18n;
    store_client_t client;
    store_catalog_t catalog;
    store_app_t detail;
    store_gallery_t gallery;
    uint8_t screenshot;
    uint8_t viewer_zoom;
    uint8_t technical_expanded;
    uint8_t filters_expanded;
    uint64_t bound_icons[STORE_MAX_APPS];
    uint64_t bound_hero;
    uint64_t bound_screenshot;
    uint8_t detail_ready;
    uint8_t detail_state;
    uint8_t screen;
    uint8_t state;
    uint8_t error;
    uint8_t selected;
    uint8_t busy;
    uint8_t chain;
    uint8_t install_feedback;
    uint8_t uninstall_feedback;
    uint8_t install_refresh;
    uint8_t downloading;
    uint8_t progress_rendered;
    uint64_t downloaded_bytes;
    uint64_t download_total;
    uint8_t installed_count;
    uint8_t download_count;
    uint8_t failed_count;
    uint8_t deleting_index;
    uint8_t success_return_screen;
    uint8_t update_checking;
    uint8_t update_rendered;
    uint8_t update_checked;
    uint8_t update_failed;
    uint8_t update_detail_target;
    uint8_t update_index;
    uint8_t update_available[PXA_STORE_INSTALLED_MAX];
    store_app_t update_detail;
    pxa_store_installed_app_t installed[PXA_STORE_INSTALLED_MAX];
    pxa_store_download_entry_t downloads[PXA_STORE_DOWNLOAD_MAX];
    char failed_apps[PXA_STORE_DOWNLOAD_MAX][STORE_MAX_APP_ID];
    char completed_app_id[STORE_MAX_APP_ID];
    /* A query submitted while a catalog request is in flight is applied as
     * soon as that request completes. */
    char pending_search[STORE_MAX_QUERY];
    uint8_t pending_search_valid;
    /* A restart (chip, tab, refresh or search) requested while a catalog
     * request is in flight is replayed when that request completes. */
    uint8_t pending_restart;
    /* Node the current drag started on, so its samples share one origin. */
    uint32_t pointer_node;
    /* Set while a gesture is a drag; a drag must never count as a click. */
    uint8_t drag_active;
    /* Last pointer sample on the list, to read the drag direction even when
     * the content has no room left to scroll. */
    int32_t pointer_y;
    uint32_t detail_target;
    uint32_t detail_revision;
    uint8_t display_shape;
    uint8_t kind_index;
    uint8_t catalog_kind;
    int16_t category_entry;
    uint32_t pending_request;
    uint32_t safe_insets[4];
    uint32_t bar_insets[4];
    uint32_t density_q16;
    uint32_t corner_radii[4];
    uint32_t width;
    uint32_t height;
    int32_t auto_load_mark;
    /* Item count of the last viewport fill request, so a server that stops
     * producing items cannot spin. */
    uint8_t fill_count;
    uint8_t snapshot_received;
    uint8_t snapshot_attempts;
    store_taxonomy_t taxonomy;
    char draft[STORE_MAX_QUERY];
    int32_t list_scroll;
    int32_t detail_scroll;
    uint32_t generation;
    uint64_t request_cursor;
    char query[STORE_MAX_QUERY];
    char url[512];
    char device_id[24];
    char profile[64];
    uint8_t profile_ready;
    char scratch[384];
    char status[64];
    uint8_t packet[3072];
    uint8_t *body;
    uint32_t body_capacity;
    uint32_t body_limit;
    store::ViewerGesture viewer_gesture;
} store_state_t;

static store_state_t app;

/* Layout metrics follow the density-independent size so the same App is
 * comfortable on a 296x240 panel and on a large phone display. */
typedef struct {
    uint16_t header_height;
    uint16_t status_height;
    uint16_t card_height;
    uint16_t header_pad_h;
    uint16_t header_pad_v;
    uint16_t action_height;
    uint16_t action_pad_h;
    uint16_t chip_height;
    uint16_t chip_pad_h;
    uint16_t chip_gap;
    uint16_t chips_row_height;
    uint16_t card_pad;
    uint16_t card_gap;
    uint16_t card_radius;
    uint16_t card_icon;
    uint16_t list_pad_h;
    uint16_t list_gap;
    uint16_t tab_height;
    uint16_t hero_icon;
    uint16_t search_height;
    uint16_t key_height;
    uint16_t key_gap;
    uint8_t title_role;
} store_metrics_t;

/* header_height, status_height, card_height, header_pad_h, header_pad_v,
 * action_height, action_pad_h, chip_height, chip_pad_h, chip_gap,
 * chips_row_height, card_pad, card_gap, card_radius, card_icon, list_pad_h,
 * list_gap, tab_height, hero_icon, search_height, key_height, key_gap,
 * title_role */
static const store_metrics_t metrics_compact = {
    38, 0, 48, 10, 2, 30, 9, 26, 10, 5, 28, 5, 6, 8, 32, 10, 5, 42, 36, 32,
    25, 3, PXA_UI_FONT_ROLE_BODY};
static const store_metrics_t metrics_regular = {
    60, 18, 84, 16, 4, 32, 12, 32, 14, 8, 42, 10, 8, 8, 48, 16, 7, 48, 56,
    42, 30, 4, PXA_UI_FONT_ROLE_TITLE};
static const store_metrics_t metrics_large = {
    74, 22, 104, 22, 8, 38, 16, 38, 18, 10, 52, 14, 10, 8, 60, 22, 8, 56,
    72, 50, 34, 5, PXA_UI_FONT_ROLE_HEADLINE};

static uint16_t effective_inset(uint8_t edge);
static unsigned filter_height();

static const store_metrics_t *metrics(void) {
    if (app.width < 340u) return &metrics_compact;
    if (app.width >= 480u) return &metrics_large;
    return &metrics_regular;
}

static unsigned filter_height() { return app.filters_expanded ? metrics()->chips_row_height : 0; }


static const char *message(pxa_i18n_message_id_t id) {
    return pxa_i18n_cstr(&app.i18n, id);
}

/* The detail screen prefers the fetched single-item payload and falls back to
 * the catalog entry while the request is pending or after a failure. */
static int text_equal(const char *left, const char *right);

static const store_app_t *detail_item(void) {
    if (app.update_detail_target || app.screen == STORE_SCREEN_DETAIL) return &app.detail;
    return app.detail_ready ? &app.detail
                           : store_catalog_at_const(&app.catalog, app.selected);
}

static const pxa_store_installed_app_t *installed_for(const char *app_id) {
    for (uint8_t index = 0; index < app.installed_count; ++index)
        if (text_equal(app.installed[index].app_id, app_id))
            return &app.installed[index];
    return NULL;
}

static int has_update(const store_app_t *item) {
    const pxa_store_installed_app_t *installed = installed_for(item->app_id);
    return installed != NULL &&
        (item->release_sequence != 0 && installed->sequence != 0
            ? item->release_sequence > installed->sequence
            : !text_equal(item->version, installed->version));
}

static int newer_than_installed(const store_app_t *item,
                                const pxa_store_installed_app_t *installed) {
    return item->release_sequence != 0 && installed->sequence != 0
        ? item->release_sequence > installed->sequence
        : !text_equal(item->version, installed->version);
}

static size_t string_length(const char *value) {
    size_t size = 0;
    if (value == NULL) return 0;
    while (value[size] != '\0') ++size;
    return size;
}

static int text_equal(const char *left, const char *right) {
    size_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        ++index;
    }
    return left[index] == right[index];
}

static void copy_text(char *output, size_t capacity, const char *value) {
    size_t index = 0;
    if (capacity == 0) return;
    while (value != NULL && value[index] != '\0' && index + 1u < capacity) {
        output[index] = value[index];
        ++index;
    }
    output[index] = '\0';
}

static size_t append_text(char *output, size_t capacity, size_t offset,
                          const char *value) {
    size_t index = 0;
    if (capacity == 0) return 0;
    while (value != NULL && value[index] != '\0' && offset + 1u < capacity)
        output[offset++] = value[index++];
    output[offset] = '\0';
    return offset;
}

static size_t append_u64(char *output, size_t capacity, size_t offset,
                         uint64_t value) {
    char reverse[20];
    size_t count = 0;
    do {
        reverse[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(reverse));
    while (count != 0 && offset + 1u < capacity)
        output[offset++] = reverse[--count];
    output[offset] = '\0';
    return offset;
}

/* Diagnostic channel used while bringing up a Host. It stays compiled out of
 * release packages. */
static void store_trace(const char *tag, uint32_t value) {
#if STORE_TRACE
    char message[64];
    size_t offset = append_text(message, sizeof(message), 0, "store ");
    offset = append_text(message, sizeof(message), offset, tag);
    offset = append_text(message, sizeof(message), offset, "=");
    (void)append_u64(message, sizeof(message), offset, value);
    (void)pxa_log_write(2, message);
#else
    (void)tag;
    (void)value;
#endif
}

static void format_size(char *output, size_t capacity, uint64_t bytes) {
    store::format_package_size({output, capacity}, bytes);
}

/* Shows only the leading bytes of a long digest or key identifier. */
static void format_short_id(char *output, size_t capacity, const char *value) {
    size_t index = 0;
    if (capacity == 0) return;
    while (value != NULL && value[index] != '\0' && index + 4u < capacity &&
           index < 16u) {
        output[index] = value[index];
        ++index;
    }
    if (value != NULL && value[index] != '\0' && index + 4u < capacity) {
        output[index++] = '.';
        output[index++] = '.';
        output[index++] = '.';
    }
    output[index] = '\0';
}

static void format_service_requirement(char *output, size_t capacity,
                                       const store_service_t *service) {
    size_t offset = append_text(output, capacity, 0, service->name);
    offset = append_text(output, capacity, offset, " ");
    offset = append_u64(output, capacity, offset, service->min_major);
    offset = append_text(output, capacity, offset, ".");
    offset = append_u64(output, capacity, offset, service->min_minor);
    if (service->max_major != service->min_major ||
        service->max_minor != UINT16_MAX) {
        offset = append_text(output, capacity, offset, "-");
        offset = append_u64(output, capacity, offset, service->max_major);
        offset = append_text(output, capacity, offset, ".");
        (void)append_u64(output, capacity, offset, service->max_minor);
    }
}

static const char *error_text(void) {
    pxa_i18n_argument_t argument;
    char code[12];
    switch (app.error) {
        case STORE_ERROR_PERMISSION: return message(PXA_MSG_STATUS_PERMISSION);
        case STORE_ERROR_UNSUPPORTED: return message(PXA_MSG_STATUS_UNSUPPORTED);
        case STORE_ERROR_UNAVAILABLE: return message(PXA_MSG_STATUS_UNAVAILABLE);
        case STORE_ERROR_TIMEOUT: return message(PXA_MSG_STATUS_TIMEOUT);
        case STORE_ERROR_LIMIT: return message(PXA_MSG_STATUS_LIMIT);
        case STORE_ERROR_PROTOCOL: return message(PXA_MSG_STATUS_PROTOCOL);
        case STORE_ERROR_NOT_FOUND: return message(PXA_MSG_STATUS_NOT_FOUND);
        case STORE_ERROR_HTTP:
            code[0] = '\0';
            (void)append_u64(code, sizeof(code), 0, app.client.http_status);
            argument.name = "code";
            argument.name_size = 4u;
            argument.type = PXA_I18N_ARGUMENT_STRING;
            argument.value.string.data = code;
            argument.value.string.size = string_length(code);
            (void)pxa_i18n_format(&app.i18n, PXA_MSG_STATUS_HTTP, &argument, 1u,
                                  app.status, sizeof(app.status));
            return app.status;
        default: return message(PXA_MSG_STATUS_FAILED);
    }
}

/* Store rows show a generated icon placeholder: the first character of the
 * application name on the accent color. Guest Apps cannot decode a remote
 * image, so this keeps the list readable without shipping icons. */
static void format_icon_letter(char *output, size_t capacity,
                               const char *name) {
    size_t length = 0;
    size_t size;
    if (capacity == 0) return;
    output[0] = '\0';
    if (name == NULL || name[0] == '\0' || capacity < 2u) return;
    if ((uint8_t)name[0] < 0x80u) {
        char value = name[0];
        if (value >= 'a' && value <= 'z') value = (char)(value - 'a' + 'A');
        output[0] = value;
        length = 1;
    } else {
        size = (uint8_t)name[0] >= 0xf0u ? 4u
               : (uint8_t)name[0] >= 0xe0u ? 3u
                                           : 2u;
        while (length < size && name[length] != '\0' &&
               length + 1u < capacity)
            output[length] = name[length], ++length;
    }
    output[length] = '\0';
}

static int create_app_icon(pxa_ui_transaction_t *transaction, uint32_t box,
                           uint32_t letter, uint32_t parent, int32_t size,
                           const char *name, const char *path = "") {
    uint64_t image = store::media_handle(path);
    if (image) return pxa_ui_create(transaction, box, parent, 0, PXA_UI_NODE_IMAGE) &&
        pxa_ui_set_length(transaction, box, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, size) &&
        pxa_ui_set_length(transaction, box, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, size) &&
        pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_IMAGE_FIT, PXA_UI_IMAGE_FIT_CONTAIN) &&
        pxa_ui_set_image(transaction, box, image);
    char initial[8];
    format_icon_letter(initial, sizeof(initial), name);
    return pxa_ui_create(transaction, box, parent, 0, PXA_UI_NODE_BOX) &&
           pxa_ui_set_length(transaction, box, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_PX, (uint16_t)size) &&
           pxa_ui_set_length(transaction, box, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, (uint16_t)size) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_dp(transaction, box, PXA_UI_PROPERTY_RADIUS,
                         8) &&
           pxa_ui_set_theme_color(transaction, box, PXA_UI_PROPERTY_BACKGROUND,
                                  PXA_UI_THEME_BORDER) &&
           pxa_ui_create(transaction, letter, box, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, letter, initial,
                           string_length(initial)) &&
           pxa_ui_set_font_role(transaction, letter,
                                size >= 48 ? PXA_UI_FONT_ROLE_TITLE
                                           : PXA_UI_FONT_ROLE_BODY) &&
           pxa_ui_set_theme_color(transaction, letter,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_PRIMARY);
}

static int locale_is_chinese(void) {
    return app.i18n.locale_size >= 2u && app.i18n.locale[0] == 'z' &&
           app.i18n.locale[1] == 'h';
}

/* The device API ships both store locales; pick the one the Host reported. */
static const char *taxonomy_label(const char *label, const char *label_en) {
    if (locale_is_chinese())
        return label[0] != '\0' ? label : label_en;
    return label_en[0] != '\0' ? label_en : label;
}

/* Entry index + 1 for a canonical kind value, or 0 when the taxonomy has not
 * arrived yet. */
static uint8_t kind_entry_for(const char *value) {
    uint8_t index;
    for (index = 0; index < app.taxonomy.kind_count; ++index) {
        if (text_equal(app.taxonomy.kinds[index].value, value))
            return (uint8_t)(index + 1u);
    }
    return 0;
}

static const char *selected_kind_value(void) {
    if (app.kind_index == 0 || app.kind_index > app.taxonomy.kind_count)
        return app.catalog_kind ? "game" : PXA_STORE_DEFAULT_KIND;
    return app.taxonomy.kinds[app.kind_index - 1u].value;
}

static const char *selected_category_value(void) {
    if (app.category_entry < 0 ||
        app.category_entry >= (int16_t)app.taxonomy.category_count)
        return "";
    if (!text_equal(app.taxonomy.categories[app.category_entry].kind,
                    selected_kind_value()))
        return "";
    return app.taxonomy.categories[app.category_entry].value;
}

static uint8_t visible_category_count(void) {
    const char *kind = selected_kind_value();
    uint8_t index;
    uint8_t count = 0;
    if (kind[0] == '\0') return 0;
    for (index = 0; index < app.taxonomy.category_count; ++index) {
        if (text_equal(app.taxonomy.categories[index].kind, kind)) ++count;
    }
    return count;
}

static const char *status_text(void) {
    if (app.state == STORE_LOADING) return message(PXA_MSG_STATUS_LOADING);
    if (app.state == STORE_DENIED) return message(PXA_MSG_STATUS_DENIED);
    if (app.state == STORE_FAILED) return error_text();
    return message(PXA_MSG_STATUS_READY);
}

static pxa_i18n_message_id_t install_feedback_message(void) {
    switch (app.install_feedback) {
        case 1: return app.downloading ?
            PXA_MSG_STATUS_DOWNLOADING : PXA_MSG_STATUS_INSTALL_LOADING;
        case 2: return PXA_MSG_STATUS_INSTALL_SUCCESS;
        case 4: return PXA_MSG_STATUS_DOWNLOAD_FAILED;
        case 5: return PXA_MSG_STATUS_DOWNLOADED;
        case 6: return PXA_MSG_STATUS_STORAGE_FULL;
        case 7: return PXA_MSG_STATUS_INSTALL_CANCELLED;
        case 8: return PXA_MSG_STATUS_INSTALL_UNSUPPORTED;
        default: return PXA_MSG_STATUS_INSTALL_FAILED;
    }
}

static const char *detail_status_text(void) {
    if (app.install_feedback) return message(install_feedback_message());
    if (app.detail_state == STORE_LOADING)
        return message(PXA_MSG_STATUS_LOADING);
    if (app.detail_state == STORE_FAILED) return error_text();
    if (app.detail_state == STORE_READY) return message(PXA_MSG_STATUS_READY);
    return message(PXA_MSG_STATUS_WAITING);
}

static uint8_t status_color(void) {
    if (app.state == STORE_FAILED || app.state == STORE_DENIED)
        return PXA_UI_THEME_DANGER;
    if (app.state == STORE_READY && app.catalog.count != 0)
        return PXA_UI_THEME_SUCCESS;
    return PXA_UI_THEME_MUTED;
}

static uint8_t detail_status_color(void) {
    if (app.install_feedback == 3 || app.install_feedback == 4 || app.install_feedback == 6)
        return PXA_UI_THEME_DANGER;
    if (app.install_feedback == 1 || app.install_feedback == 7) return PXA_UI_THEME_MUTED;
    if (app.detail_state == STORE_FAILED) return PXA_UI_THEME_DANGER;
    if (app.detail_state == STORE_READY) return PXA_UI_THEME_SUCCESS;
    return PXA_UI_THEME_MUTED;
}


static uint8_t error_from_status(int32_t status) {
    switch (status) {
        case PXA_STATUS_UNSUPPORTED: return STORE_ERROR_UNSUPPORTED;
        case PXA_STATUS_DENIED: return STORE_ERROR_PERMISSION;
        case PXA_STATUS_TIMED_OUT: return STORE_ERROR_TIMEOUT;
        case PXA_STATUS_LIMIT_EXCEEDED: return STORE_ERROR_LIMIT;
        case PXA_STATUS_PROTOCOL_ERROR: return STORE_ERROR_PROTOCOL;
        case PXA_STATUS_UNAVAILABLE: return STORE_ERROR_UNAVAILABLE;
        case PXA_STATUS_NOT_FOUND: return STORE_ERROR_NOT_FOUND;
        default: return STORE_ERROR_FAILED;
    }
}

static void toast_failure(pxa_i18n_message_id_t title, int32_t status) {
    pxa_i18n_message_id_t reason = PXA_MSG_STATUS_FAILED;
    char text[PXA_WINDOW_TOAST_MAX_BYTES + 1u];
    size_t size;
    switch (error_from_status(status)) {
        case STORE_ERROR_PERMISSION: reason = PXA_MSG_STATUS_PERMISSION; break;
        case STORE_ERROR_UNSUPPORTED: reason = PXA_MSG_STATUS_UNSUPPORTED; break;
        case STORE_ERROR_UNAVAILABLE: reason = PXA_MSG_STATUS_UNAVAILABLE; break;
        case STORE_ERROR_TIMEOUT: reason = PXA_MSG_STATUS_TIMEOUT; break;
        case STORE_ERROR_LIMIT: reason = PXA_MSG_STATUS_LIMIT; break;
        case STORE_ERROR_PROTOCOL: reason = PXA_MSG_STATUS_PROTOCOL; break;
        case STORE_ERROR_NOT_FOUND: reason = PXA_MSG_STATUS_NOT_FOUND; break;
        default: break;
    }
    if (title == PXA_MSG_STATUS_INSTALL_FAILED || title == PXA_MSG_STATUS_DOWNLOAD_FAILED) {
        if (status == PXA_STATUS_DENIED) reason = PXA_MSG_STATUS_INSTALL_DENIED;
        else if (status == PXA_STATUS_BUSY) reason = PXA_MSG_STATUS_INSTALL_BUSY;
        else if (status == PXA_STATUS_RESOURCE_LIMIT) reason = PXA_MSG_STATUS_INSTALL_RESOURCES;
        else if (status == PXA_STATUS_UNSUPPORTED) reason = PXA_MSG_STATUS_INSTALL_UNSUPPORTED;
        else if (status == PXA_STATUS_QUOTA_EXCEEDED) reason = PXA_MSG_STATUS_STORAGE_FULL;
        else if (status == PXA_STATUS_INVALID_ARGUMENT) reason = PXA_MSG_STATUS_PROTOCOL;
    }
    copy_text(text, sizeof(text), message(title));
    size = string_length(text);
    if (size + 3u < sizeof(text)) {
        text[size++] = ':';
        text[size++] = ' ';
        copy_text(text + size, sizeof(text) - size, message(reason));
    }
    (void)pxa_window_show_toast(2600u, text, string_length(text));
}

static void toast_current_error(void) {
    char text[PXA_WINDOW_TOAST_MAX_BYTES + 1u];
    size_t size;
    copy_text(text, sizeof(text), message(PXA_MSG_STATUS_FAILED));
    size = string_length(text);
    if (size + 3u < sizeof(text)) {
        text[size++] = ':';
        text[size++] = ' ';
        copy_text(text + size, sizeof(text) - size, error_text());
    }
    (void)pxa_window_show_toast(2600u, text, string_length(text));
}

/* ------------------------------------------------------------------ */
/* Rendering                                                          */
/* ------------------------------------------------------------------ */

static int footer_status_text(char *output, size_t capacity);
static uint8_t catalog_columns(void);
static int create_chip(pxa_ui_transaction_t *transaction, uint32_t chip,
                       const char *text, uint8_t active);
static uint16_t effective_inset(uint8_t edge);
static uint16_t inset_padding(uint8_t edge, uint16_t margin);
static void ensure_window_snapshot(void);

static int create_action_button(pxa_ui_transaction_t *transaction, uint32_t node,
                                uint32_t label_node, const char *text,
                                uint8_t enabled) {
    const store_metrics_t *m = metrics();
    const bool symbol = node == NODE_BACK || node == NODE_LIBRARY_CHECK;
    const char *visual = node == NODE_BACK ? ICON_BACK :
                         node == NODE_LIBRARY_CHECK ? ICON_REFRESH : text;
    return pxa_ui_create_typed(transaction, node, NODE_HEADER_ROW, 0,
                                 PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
             pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_PX, m->action_height + 8u) &&
             pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, m->action_height) &&
             pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_LAYOUT,
                           PXA_UI_LAYOUT_ROW) &&
             pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_JUSTIFY,
                           PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ALIGN,
                           PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_RADIUS,
                          m->action_height / 2u) &&
             pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_BORDER_WIDTH,
                           node == NODE_BACK ? 0 : 1) &&
             pxa_ui_set_theme_color(transaction, node,
                                    PXA_UI_PROPERTY_BORDER_COLOR,
                                    PXA_UI_THEME_BORDER) &&
             (node == NODE_BACK ?
                  pxa_ui_set_rgba(transaction, node,
                                  PXA_UI_PROPERTY_BACKGROUND, 0) :
                  pxa_ui_set_theme_color(transaction, node,
                                          PXA_UI_PROPERTY_BACKGROUND,
                                          PXA_UI_THEME_SURFACE)) &&
             pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ENABLED, enabled) &&
             pxa_ui_set_event_mask(transaction, node,
                                   PXA_UI_EVENT_MASK_CLICK) &&
             pxa_ui_set_property(transaction, node,
                                 PXA_UI_PROPERTY_ACCESSIBILITY_LABEL,
                                 text, string_length(text)) &&
             (node == NODE_SEARCH_BUTTON
                  ? (pxa_ui_create(transaction, label_node, node, 0,
                                   PXA_UI_NODE_IMAGE) &&
                     pxa_ui_set_length(transaction, label_node,
                                       PXA_UI_PROPERTY_WIDTH,
                                       PXA_UI_LENGTH_PX, 20) &&
                     pxa_ui_set_length(transaction, label_node,
                                       PXA_UI_PROPERTY_HEIGHT,
                                       PXA_UI_LENGTH_PX, 20) &&
                     pxa_ui_set_u8(transaction, label_node,
                                    PXA_UI_PROPERTY_IMAGE_FIT,
                                    PXA_UI_IMAGE_FIT_CONTAIN) &&
                     store_search_image_set(transaction, label_node) &&
                     pxa_ui_set_theme_color(transaction, label_node,
                                            PXA_UI_PROPERTY_FOREGROUND,
                                            PXA_UI_THEME_PRIMARY) &&
                     pxa_ui_set_event_mask(transaction, label_node, 0))
                  : (pxa_ui_create(transaction, label_node, node, 0,
                                    PXA_UI_NODE_TEXT) &&
                     pxa_ui_set_text(transaction, label_node, visual,
                                     string_length(visual)) &&
                     pxa_ui_set_font_role(transaction, label_node,
                                          symbol ? PXA_UI_FONT_ROLE_ICON : PXA_UI_FONT_ROLE_CAPTION) &&
                     pxa_ui_set_theme_color(transaction, label_node,
                                            PXA_UI_PROPERTY_FOREGROUND,
                                            node == NODE_BACK ? PXA_UI_THEME_TEXT :
                                                                PXA_UI_THEME_PRIMARY)));
}

/* Flat title row on the page background, like the system application pages.
 * The catalog adds the request state as a second line. */
static int create_header(pxa_ui_transaction_t *transaction, uint8_t mode,
                         const char *title, size_t title_size) {
    const store_metrics_t *m = metrics();
    uint8_t back = (uint8_t)(mode == STORE_SCREEN_DETAIL);
    uint8_t catalog = (uint8_t)(mode == STORE_SCREEN_CATALOG);
    int ok = pxa_ui_create(transaction, NODE_ROOT, 0, 0, PXA_UI_NODE_ROOT) &&
             pxa_ui_set_u8(transaction, NODE_ROOT, PXA_UI_PROPERTY_LAYOUT,
                           PXA_UI_LAYOUT_COLUMN) &&
             pxa_ui_set_theme_color(transaction, NODE_ROOT,
                                    PXA_UI_PROPERTY_BACKGROUND,
                                    PXA_UI_THEME_BACKGROUND) &&
             pxa_ui_set_padding(transaction, NODE_ROOT,
                                inset_padding(3, m->list_pad_h),
                                inset_padding(0, m->header_pad_v),
                                inset_padding(1, m->list_pad_h),
                                effective_inset(2)) &&
             pxa_ui_create(transaction, NODE_HEADER, NODE_ROOT, 0,
                           PXA_UI_NODE_BOX) &&
             pxa_ui_set_length(transaction, NODE_HEADER, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(transaction, NODE_HEADER, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, m->header_height) &&
             pxa_ui_set_u8(transaction, NODE_HEADER, PXA_UI_PROPERTY_LAYOUT,
                           PXA_UI_LAYOUT_COLUMN) &&
             pxa_ui_set_u8(transaction, NODE_HEADER, PXA_UI_PROPERTY_JUSTIFY,
                           PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_padding(transaction, NODE_HEADER, m->header_pad_h,
                               m->header_pad_v, m->header_pad_h, 4) &&
             pxa_ui_set_dp(transaction, NODE_HEADER, PXA_UI_PROPERTY_GAP, 3) &&
             pxa_ui_set_theme_color(transaction, NODE_HEADER,
                                    PXA_UI_PROPERTY_BACKGROUND,
                                    PXA_UI_THEME_BACKGROUND) &&
             pxa_ui_create(transaction, NODE_HEADER_ROW, NODE_HEADER, 0,
                           PXA_UI_NODE_BOX) &&
             pxa_ui_set_length(transaction, NODE_HEADER_ROW,
                               PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(transaction, NODE_HEADER_ROW,
                               PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                               (uint16_t)(m->header_height -
                                          m->status_height -
                                          m->header_pad_v - 6)) &&
             pxa_ui_set_u8(transaction, NODE_HEADER_ROW, PXA_UI_PROPERTY_LAYOUT,
                           PXA_UI_LAYOUT_ROW) &&
             pxa_ui_set_u8(transaction, NODE_HEADER_ROW, PXA_UI_PROPERTY_ALIGN,
                           PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_dp(transaction, NODE_HEADER_ROW, PXA_UI_PROPERTY_GAP,
                           6);
    if (!ok) return 0;
    if (back) {
        ok = create_action_button(transaction, NODE_BACK, NODE_BACK_LABEL,
                                  message(PXA_MSG_ACTION_BACK), 1);
    } else {
        ok = pxa_ui_create(transaction, NODE_HEADER_TITLE, NODE_HEADER_ROW, 0,
                           PXA_UI_NODE_TEXT) &&
             pxa_ui_set_u16(transaction, NODE_HEADER_TITLE,
                            PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_text(transaction, NODE_HEADER_TITLE, title,
                             title_size) &&
             pxa_ui_set_font_role(transaction, NODE_HEADER_TITLE, m->title_role) &&
             pxa_ui_set_theme_color(transaction, NODE_HEADER_TITLE,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_TEXT) &&
             (!catalog ||
              (create_action_button(transaction, NODE_SEARCH_BUTTON,
                                    NODE_SEARCH_BUTTON_LABEL,
                                    message(PXA_MSG_ACTION_SEARCH), 1) &&
               create_action_button(transaction, NODE_REFRESH,
                                    NODE_REFRESH_LABEL,
                                    message(PXA_MSG_ACTION_FILTERS), 1)));
    }
    if (!ok) return 0;
    if (back) {
        ok = pxa_ui_create(transaction, NODE_HEADER_TITLE, NODE_HEADER_ROW, 0,
                           PXA_UI_NODE_TEXT) &&
             pxa_ui_set_u16(transaction, NODE_HEADER_TITLE,
                            PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_text(transaction, NODE_HEADER_TITLE, title,
                             title_size) &&
             pxa_ui_set_font_role(transaction, NODE_HEADER_TITLE, m->title_role) &&
             pxa_ui_set_theme_color(transaction, NODE_HEADER_TITLE,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_TEXT);
    }
    if (!ok) return 0;
    if (catalog && m->status_height != 0) {
        char status[64];
        (void)footer_status_text(status, sizeof(status));
        ok = pxa_ui_create(transaction, NODE_HEADER_STATUS, NODE_HEADER, 0,
                           PXA_UI_NODE_TEXT) &&
             pxa_ui_set_length(transaction, NODE_HEADER_STATUS,
                               PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_CONTENT, 0) &&
             pxa_ui_set_length(transaction, NODE_HEADER_STATUS,
                               PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                               m->status_height) &&
             pxa_ui_set_text(transaction, NODE_HEADER_STATUS, status,
                             string_length(status)) &&
             pxa_ui_set_font_role(transaction, NODE_HEADER_STATUS,
                                  PXA_UI_FONT_ROLE_CAPTION) &&
             pxa_ui_set_theme_color(transaction, NODE_HEADER_STATUS,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    status_color());
    }
    return ok;
}

/* One horizontally scrollable row: kind filters first, then the categories of
 * the selected kind. */
static int create_chip(pxa_ui_transaction_t *transaction, uint32_t chip,
                       const char *text, uint8_t active) {
    const store_metrics_t *m = metrics();
    return pxa_ui_create_typed(transaction, chip, NODE_FILTERS, 0,
                               PXA_UI_NODE_CONTROL,
                               PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_length(transaction, chip, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, m->chip_height) &&
           pxa_ui_set_u8(transaction, chip, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, chip, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, chip, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_padding(transaction, chip, m->chip_pad_h, 2, m->chip_pad_h, 2) &&
           pxa_ui_set_dp(transaction, chip, PXA_UI_PROPERTY_RADIUS,
                          m->card_radius) &&
           pxa_ui_set_dp(transaction, chip, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
           pxa_ui_set_theme_color(transaction, chip,
                                  PXA_UI_PROPERTY_BORDER_COLOR,
                                  active ? PXA_UI_THEME_PRIMARY
                                         : PXA_UI_THEME_BORDER) &&
           pxa_ui_set_theme_color(transaction, chip,
                                  PXA_UI_PROPERTY_BACKGROUND,
                                  active ? PXA_UI_THEME_PRIMARY
                                         : PXA_UI_THEME_SURFACE) &&
           pxa_ui_set_event_mask(transaction, chip, PXA_UI_EVENT_MASK_CLICK) &&
           pxa_ui_create(transaction, chip + 1u, chip, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, chip + 1u, text,
                           string_length(text)) &&
           pxa_ui_set_font_role(transaction, chip + 1u,
                                PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, chip + 1u,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  active ? PXA_UI_THEME_ON_PRIMARY
                                         : PXA_UI_THEME_TEXT);
}

static int create_filters(pxa_ui_transaction_t *transaction) {
    if (!app.filters_expanded) return 1;
    const store_metrics_t *m = metrics();
    uint8_t index;
    if (!pxa_ui_create(transaction, NODE_FILTERS, NODE_ROOT, 0,
                       PXA_UI_NODE_SCROLL) ||
        !pxa_ui_set_length(transaction, NODE_FILTERS, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(transaction, NODE_FILTERS, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, m->chips_row_height) ||
        !pxa_ui_set_u8(transaction, NODE_FILTERS, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) ||
        !pxa_ui_set_u8(transaction, NODE_FILTERS, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) ||
        !pxa_ui_set_u8(transaction, NODE_FILTERS, PXA_UI_PROPERTY_SCROLL_AXIS,
                       1) ||
        !pxa_ui_set_u8(transaction, NODE_FILTERS, PXA_UI_PROPERTY_SCROLLBAR,
                       0) ||
        !pxa_ui_set_theme_color(transaction, NODE_FILTERS,
                                PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) ||
        /* Dragging the chip row is a scroll gesture, not a chip tap. */
        !pxa_ui_set_event_mask(transaction, NODE_FILTERS,
                               PXA_UI_EVENT_MASK_POINTER) ||
        !pxa_ui_set_padding(transaction, NODE_FILTERS, m->list_pad_h,
                            (uint16_t)((m->chips_row_height -
                                        m->chip_height) / 2),
                            m->list_pad_h,
                            (uint16_t)((m->chips_row_height -
                                        m->chip_height) / 2)) ||
        !pxa_ui_set_dp(transaction, NODE_FILTERS, PXA_UI_PROPERTY_GAP, m->chip_gap))
        return 0;
    if (!create_chip(transaction, NODE_CATEGORY_ALL,
                     message(PXA_MSG_FILTER_ALL),
                     (uint8_t)(app.category_entry < 0)))
        return 0;
    for (index = 0; index < app.taxonomy.category_count; ++index) {
        const store_taxonomy_entry_t *entry = &app.taxonomy.categories[index];
        if (!text_equal(entry->kind, selected_kind_value())) continue;
        if (!create_chip(transaction,
                         NODE_CATEGORY_BASE + (uint32_t)index * NODE_CHIP_STRIDE,
                         taxonomy_label(entry->label, entry->label_en),
                         (uint8_t)(app.category_entry == (int16_t)index)))
            return 0;
    }
    return 1;
}

static int footer_status_text(char *output, size_t capacity) {
    pxa_i18n_argument_t argument;
    char count[12];
    output[0] = '\0';
    if (app.state == STORE_LOADING) {
        copy_text(output, capacity, message(PXA_MSG_STATUS_LOADING));
        return 1;
    }
    if (app.state == STORE_FAILED || app.state == STORE_DENIED) {
        copy_text(output, capacity, status_text());
        return 1;
    }
    if (app.catalog.count == 0) {
        copy_text(output, capacity, message(PXA_MSG_STATUS_READY));
        return 1;
    }
    count[0] = '\0';
    (void)append_u64(count, sizeof(count), 0, app.catalog.count);
    argument.name = "count";
    argument.name_size = 5u;
    argument.type = PXA_I18N_ARGUMENT_STRING;
    argument.value.string.data = count;
    argument.value.string.size = string_length(count);
    (void)pxa_i18n_format(&app.i18n, PXA_MSG_LIST_LOADED, &argument, 1u, output,
                          capacity);
    return 1;
}

static int create_card(pxa_ui_transaction_t *transaction, uint8_t index) {
    const store_metrics_t *m = metrics();
    const store_app_t *item = store_catalog_at_const(&app.catalog, index);
    uint32_t card = NODE_CARD_BASE + (uint32_t)index * NODE_CARD_STRIDE;
    uint32_t icon = card + 1u;
    uint32_t letter = card + 2u;
    uint32_t info = card + 3u;
    uint32_t name = card + 5u;
    uint32_t version = card + 6u;
    uint32_t summary = card + 7u;
    uint32_t meta = card + 8u;
    size_t offset;
    if (!pxa_ui_create_typed(transaction, card, NODE_LIST, 0,
                             PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) ||
        !pxa_ui_set_length(transaction, card, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(transaction, card, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, m->card_height) ||
        !pxa_ui_set_u8(transaction, card, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) ||
        !pxa_ui_set_u8(transaction, card, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) ||
        !pxa_ui_set_padding(transaction, card, m->card_pad, m->card_pad,
                           m->card_pad, m->card_pad) ||
        !pxa_ui_set_dp(transaction, card, PXA_UI_PROPERTY_GAP, m->card_gap) ||
        !pxa_ui_set_dp(transaction, card, PXA_UI_PROPERTY_RADIUS, m->card_radius) ||
        !pxa_ui_set_dp(transaction, card, PXA_UI_PROPERTY_BORDER_WIDTH, 0) ||
        !pxa_ui_set_theme_color(transaction, card, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE_CONTAINER_LOW) ||
        !pxa_ui_set_theme_color(transaction, card, PXA_UI_PROPERTY_BORDER_COLOR,
                                PXA_UI_THEME_BORDER) ||
        !pxa_ui_set_event_mask(transaction, card,
                               PXA_UI_EVENT_MASK_CLICK) ||
        !create_app_icon(transaction, icon, letter, card, m->card_icon,
                        item->name, item->icon_url) ||
        !pxa_ui_create(transaction, info, card, 0, PXA_UI_NODE_BOX) ||
        !pxa_ui_set_u16(transaction, info, PXA_UI_PROPERTY_GROW, 1) ||
        !pxa_ui_set_length(transaction, info, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX,
                           (uint16_t)(m->card_height - 2u * m->card_pad)) ||
        !pxa_ui_set_u8(transaction, info, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) ||
        !pxa_ui_set_dp(transaction, info, PXA_UI_PROPERTY_GAP, 2) ||
        !pxa_ui_create(transaction, name, info, 0, PXA_UI_NODE_TEXT) ||
        !pxa_ui_set_length(transaction, name, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(transaction, name, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 18) ||
        !pxa_ui_set_text(transaction, name, item->name,
                         string_length(item->name)) ||
        !pxa_ui_set_font_role(transaction, name, PXA_UI_FONT_ROLE_LABEL) ||
        !pxa_ui_set_theme_color(transaction, name, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT))
        return 0;
    {
        uint8_t columns = catalog_columns();
        if (columns > 1u) {
            if (!pxa_ui_set_u8(transaction, card, PXA_UI_PROPERTY_ALIGN,
                               PXA_UI_ALIGN_STRETCH) ||
                !pxa_ui_set_grid_cell(transaction, card,
                                      (uint16_t)(index % columns),
                                      (uint16_t)(index / columns), 1u, 1u))
                return 0;
        }
    }
    app.bound_icons[index] = store::media_handle(item->icon_url);
    app.scratch[0] = '\0';
    offset = append_text(app.scratch, sizeof(app.scratch), 0, "v");
    offset = append_text(app.scratch, sizeof(app.scratch), offset, item->version);
    if (installed_for(item->app_id) != NULL) {
        offset = append_text(app.scratch, sizeof(app.scratch), offset, " · ");
        (void)append_text(app.scratch, sizeof(app.scratch), offset,
                          message(has_update(item) ? PXA_MSG_STATUS_UPDATE_AVAILABLE :
                                                     PXA_MSG_STATUS_INSTALLED));
    }
    if (!pxa_ui_create(transaction, version, info, 0, PXA_UI_NODE_TEXT) ||
        !pxa_ui_set_text(transaction, version, app.scratch,
                         string_length(app.scratch)) ||
        !pxa_ui_set_font_role(transaction, version, PXA_UI_FONT_ROLE_CAPTION) ||
        !pxa_ui_set_theme_color(transaction, version,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED))
        return 0;
    if (app.width < 340u) return 1;
    copy_text(app.scratch, sizeof(app.scratch), item->summary);
    if (!pxa_ui_create(transaction, summary, info, 0, PXA_UI_NODE_TEXT) ||
        !pxa_ui_set_length(transaction, summary, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(transaction, summary, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, app.width >= 480u ? 32u : 14u) ||
        !pxa_ui_set_text(transaction, summary, app.scratch,
                         string_length(app.scratch)) ||
        !pxa_ui_set_font_role(transaction, summary, PXA_UI_FONT_ROLE_CAPTION) ||
        !pxa_ui_set_theme_color(transaction, summary,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED))
        return 0;
    app.scratch[0] = '\0';
    offset = 0;

    if (item->size != 0) {
        char size[24];
        format_size(size, sizeof(size), item->size);
        if (offset != 0)
            offset = append_text(app.scratch, sizeof(app.scratch), offset,
                                 " · ");
        (void)append_text(app.scratch, sizeof(app.scratch), offset, size);
    }
    return pxa_ui_create(transaction, meta, info, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_length(transaction, meta, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_text(transaction, meta, app.scratch,
                           string_length(app.scratch)) &&
           pxa_ui_set_font_role(transaction, meta, PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, meta, PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED);
}

/* Wide displays get a multi-column card grid instead of a single list. */
static uint8_t catalog_columns(void) {
    if (app.width >= 1100u) return 3u;
    if (app.width >= 480u) return 2u;
    return 1u;
}

static int32_t catalog_view_height(void) {
    const store_metrics_t *m = metrics();
    int32_t height = (int32_t)app.height -
                     (int32_t)(m->header_height + filter_height() +
                               m->tab_height + 8u) -
                     (int32_t)inset_padding(0, m->header_pad_v) -
                     (int32_t)effective_inset(2);
    return height > 0 ? height : 0;
}

static uint8_t catalog_page_size(void) {
    const store_metrics_t *m = metrics();
    uint8_t columns = catalog_columns();
    int32_t row_height = (int32_t)m->card_height + m->list_gap;
    int32_t rows = (catalog_view_height() + row_height - 1) / row_height + 1;
    int32_t count = rows * columns;
    if (count < (int32_t)(STORE_PAGE_SIZE * columns))
        count = STORE_PAGE_SIZE * columns;
    if (count > STORE_MAX_APPS) count = STORE_MAX_APPS;
    return (uint8_t)count;
}

static int32_t catalog_content_height(void) {
    const store_metrics_t *m = metrics();
    uint8_t columns = catalog_columns();
    return (int32_t)((app.catalog.count + columns - 1u) / columns) *
           (int32_t)(m->card_height + m->list_gap);
}

// Keep media residency tied to the current viewport, with one request at a
// time. Catalog JSON has its own request, so images never block navigation.
static void sync_media(void) {
    store::MediaRequest visible[store::media_capacity];
    unsigned count = 0;
    if (app.screen == STORE_SCREEN_DETAIL || app.screen == STORE_SCREEN_VIEWER) {
        const auto *item = detail_item();
        if (item->icon_url[0]) visible[count++] = {item->icon_url};
        if (app.screenshot < app.gallery.count)
            visible[count++] = {app.gallery.screenshots[app.screenshot].url, true};
    } else if (app.screen == STORE_SCREEN_CATALOG) {
        const auto *m = metrics();
        const unsigned columns = catalog_columns();
        const unsigned pitch = m->card_height + m->list_gap;
        unsigned first = unsigned(app.list_scroll) / pitch * columns;
        unsigned last = first + (unsigned(catalog_view_height()) / pitch + 2) * columns;
        if (last > app.catalog.count) last = app.catalog.count;
        for (unsigned i = first; i < last && count < store::media_capacity; ++i) {
            const auto *item = store_catalog_at_const(&app.catalog, uint8_t(i));
            if (item->icon_url[0]) visible[count++] = {item->icon_url};
        }
    }
    store::media_update({visible, count}, app.client.network_permission);
}

static int create_catalog_list(pxa_ui_transaction_t *transaction) {
    const store_metrics_t *m = metrics();
    uint8_t index;
    int ok = pxa_ui_create(transaction, NODE_LIST, NODE_ROOT, 0,
                           PXA_UI_NODE_SCROLL) &&
             pxa_ui_set_length(transaction, NODE_LIST, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(transaction, NODE_LIST, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, 0) &&
             pxa_ui_set_u16(transaction, NODE_LIST, PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_u8(transaction, NODE_LIST, PXA_UI_PROPERTY_LAYOUT,
                           PXA_UI_LAYOUT_COLUMN) &&
             pxa_ui_set_u8(transaction, NODE_LIST, PXA_UI_PROPERTY_SCROLL_AXIS,
                           2) &&
             pxa_ui_set_u8(transaction, NODE_LIST, PXA_UI_PROPERTY_SCROLLBAR,
                           1) &&
             pxa_ui_set_event_mask(transaction, NODE_LIST,
                                   PXA_UI_EVENT_MASK_SCROLL |
                                       PXA_UI_EVENT_MASK_POINTER) &&
             pxa_ui_set_padding(transaction, NODE_LIST, m->list_pad_h, 4,
                                m->list_pad_h, 4) &&
             pxa_ui_set_dp(transaction, NODE_LIST, PXA_UI_PROPERTY_GAP, m->list_gap);
    if (!ok) return 0;
    {
        uint8_t columns = catalog_columns();
        uint8_t rows = (uint8_t)((app.catalog.count + columns - 1u) / columns);
        uint8_t footer_rows = 1;
        if (columns > 1u && rows != 0 &&
            rows + footer_rows <= PXA_UI_GRID_MAX_TRACKS) {
            pxa_ui_grid_track_t column_tracks[3];
            pxa_ui_grid_track_t row_tracks[PXA_UI_GRID_MAX_TRACKS];
            uint8_t index;
            for (index = 0; index < columns; ++index) {
                column_tracks[index].kind = PXA_UI_GRID_FRACTION;
                column_tracks[index].value = 1u;
            }
            for (index = 0; index < rows + footer_rows; ++index) {
                row_tracks[index].kind = PXA_UI_GRID_CONTENT;
                row_tracks[index].value = 0;
            }
            ok = pxa_ui_set_u8(transaction, NODE_LIST, PXA_UI_PROPERTY_LAYOUT,
                               PXA_UI_LAYOUT_GRID) &&
                 pxa_ui_set_grid_columns(transaction, NODE_LIST, column_tracks,
                                         columns) &&
                 pxa_ui_set_grid_rows(transaction, NODE_LIST, row_tracks,
                                      (uint8_t)(rows + footer_rows));
            if (!ok) return 0;
        }
    }
    /* LVGL scrolls a focusable child into view while the surface is laid out,
     * which clips the first card. Restore the tracked offset last so the
     * transaction commit overrides it. */
    ok = pxa_ui_set_dp(transaction, NODE_LIST, PXA_UI_PROPERTY_SCROLL_POSITION,
                       app.list_scroll);
    if (!ok) return 0;
    if (app.catalog.count == 0) {
        const char *text = app.state == STORE_LOADING ? message(PXA_MSG_LIST_LOADING) :
            app.state == STORE_FAILED ? error_text() : message(PXA_MSG_LIST_EMPTY);
        return pxa_ui_create(transaction, NODE_EMPTY, NODE_LIST, 0,
                             PXA_UI_NODE_TEXT) &&
               pxa_ui_set_length(transaction, NODE_EMPTY, PXA_UI_PROPERTY_WIDTH,
                                 PXA_UI_LENGTH_FILL, 0) &&
               pxa_ui_set_length(transaction, NODE_EMPTY, PXA_UI_PROPERTY_HEIGHT,
                                 PXA_UI_LENGTH_PX, 60) &&
               pxa_ui_set_padding(transaction, NODE_EMPTY, 2, 8, 2, 8) &&
               pxa_ui_set_text(transaction, NODE_EMPTY, text,
                               string_length(text)) &&
               pxa_ui_set_font_role(transaction, NODE_EMPTY,
                                    PXA_UI_FONT_ROLE_BODY) &&
               pxa_ui_set_theme_color(transaction, NODE_EMPTY,
                                      PXA_UI_PROPERTY_FOREGROUND,
                                      PXA_UI_THEME_MUTED);
    }
    for (index = 0; ok && index < app.catalog.count; ++index)
        ok = create_card(transaction, index);
    return ok;
}

static const char *catalog_footer_label() {
    return message(app.busy ? PXA_MSG_STATUS_LOADING :
        app.state == STORE_FAILED ? PXA_MSG_ACTION_RETRY : PXA_MSG_ACTION_LOAD_MORE);
}

// Keep a stable identity for insertion, but remove the end-of-catalog footer
// from layout and hit testing. Compact screens need these updates too.
static int update_catalog_footer(pxa_ui_transaction_t *transaction) {
    const bool visible = app.busy || app.state == STORE_FAILED || app.catalog.has_more;
    const char *label = catalog_footer_label();
    return pxa_ui_set_text(transaction, NODE_LOAD_MORE, label, string_length(label)) &&
        pxa_ui_set_u8(transaction, NODE_FOOTER, PXA_UI_PROPERTY_VISIBLE, visible) &&
        pxa_ui_set_event_mask(transaction, NODE_FOOTER,
            visible && !app.busy ? PXA_UI_EVENT_MASK_CLICK : 0);
}

static int create_footer(pxa_ui_transaction_t *transaction) {
    const store_metrics_t *m = metrics();
    uint8_t columns = catalog_columns();
    uint8_t rows = (uint8_t)((app.catalog.count + columns - 1u) / columns);
    /* Automatic paging with a retry/load-more fallback. */
    return pxa_ui_create(transaction, NODE_FOOTER, NODE_LIST, 0,
                         PXA_UI_NODE_BOX) &&
           (columns == 1u || rows == 0 ||
            rows + 1u > PXA_UI_GRID_MAX_TRACKS ||
            pxa_ui_set_grid_cell(transaction, NODE_FOOTER, 0, rows,
                                 columns, 1u)) &&
           pxa_ui_set_length(transaction, NODE_FOOTER, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_length(transaction, NODE_FOOTER, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, m->action_height) &&
           pxa_ui_set_u8(transaction, NODE_FOOTER, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, NODE_FOOTER, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_theme_color(transaction, NODE_FOOTER,
                                  PXA_UI_PROPERTY_BACKGROUND,
                                  PXA_UI_THEME_BACKGROUND) &&
           pxa_ui_create(transaction, NODE_LOAD_MORE, NODE_FOOTER, 0,
                         PXA_UI_NODE_TEXT) &&
           pxa_ui_set_font_role(transaction, NODE_LOAD_MORE,
                                PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, NODE_LOAD_MORE,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED) &&
           update_catalog_footer(transaction);
}

/* Four destinations pinned above the system navigation bar. */
static int create_tab(pxa_ui_transaction_t *transaction, uint32_t tab,
                      uint32_t label, uint32_t icon_node,
                      const char *icon, const char *text, uint8_t active) {
    return pxa_ui_create_typed(transaction, tab, NODE_TABBAR, 0,
                               PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_padding(transaction, tab, 0, 0, 0, 0) &&
           pxa_ui_set_u16(transaction, tab, PXA_UI_PROPERTY_GROW, 1) &&
           pxa_ui_set_length(transaction, tab, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_u8(transaction, tab, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_COLUMN) &&
           pxa_ui_set_u8(transaction, tab, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, tab, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_dp(transaction, tab, PXA_UI_PROPERTY_GAP, 1) &&
           pxa_ui_set_theme_color(transaction, tab, PXA_UI_PROPERTY_BACKGROUND,
                                  active ? PXA_UI_THEME_PRIMARY_CONTAINER :
                                           PXA_UI_THEME_BACKGROUND) &&
           pxa_ui_set_event_mask(transaction, tab, PXA_UI_EVENT_MASK_CLICK) &&
           pxa_ui_set_property(transaction, tab,
                               PXA_UI_PROPERTY_ACCESSIBILITY_LABEL,
                               text, string_length(text)) &&
           pxa_ui_create(transaction, icon_node, tab, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_length(transaction, icon_node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_CONTENT, 0) &&
           pxa_ui_set_text(transaction, icon_node, icon, string_length(icon)) &&
           pxa_ui_set_font_role(transaction, icon_node, PXA_UI_FONT_ROLE_ICON) &&
           pxa_ui_set_theme_color(transaction, icon_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  active ? PXA_UI_THEME_ON_PRIMARY_CONTAINER :
                                           PXA_UI_THEME_MUTED) &&
           pxa_ui_create(transaction, label, tab, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, label, text, string_length(text)) &&
           pxa_ui_set_font_role(transaction, label, PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, label,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  active ? PXA_UI_THEME_TEXT : PXA_UI_THEME_MUTED);
}

static int create_tabbar(pxa_ui_transaction_t *transaction, uint8_t mode) {
    const store_metrics_t *m = metrics();
    const char *apps = message(PXA_MSG_KIND_APPS);
    const char *games = message(PXA_MSG_KIND_GAMES);
    uint8_t apps_active = (uint8_t)text_equal(selected_kind_value(), "app");
    uint8_t games_active = (uint8_t)text_equal(selected_kind_value(), "game");
    if (mode != STORE_SCREEN_CATALOG && mode != STORE_SCREEN_INSTALLED &&
        mode != STORE_SCREEN_DOWNLOADS)
        return 1;
    return pxa_ui_create(transaction, NODE_TABBAR, NODE_ROOT, 0,
                         PXA_UI_NODE_BOX) &&
           pxa_ui_set_length(transaction, NODE_TABBAR, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_length(transaction, NODE_TABBAR, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, m->tab_height) &&
           pxa_ui_set_padding(transaction, NODE_TABBAR,
                              m->list_pad_h, 0, m->list_pad_h, 0) &&
           pxa_ui_set_u8(transaction, NODE_TABBAR, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_theme_color(transaction, NODE_TABBAR,
                                  PXA_UI_PROPERTY_BACKGROUND,
                                  PXA_UI_THEME_BACKGROUND) &&
           create_tab(transaction, NODE_TAB_APPS, NODE_TAB_APPS_LABEL,
                      NODE_TAB_APPS_ICON, ICON_APPS, apps,
                      (uint8_t)(app.screen == STORE_SCREEN_CATALOG && apps_active)) &&
           create_tab(transaction, NODE_TAB_GAMES, NODE_TAB_GAMES_LABEL,
                      NODE_TAB_GAMES_ICON, ICON_GAMES, games,
                      (uint8_t)(app.screen == STORE_SCREEN_CATALOG && games_active)) &&
           create_tab(transaction, NODE_INSTALLED_ACTION,
                      NODE_INSTALLED_ACTION_LABEL,
                      NODE_INSTALLED_ACTION_ICON, ICON_INSTALLED,
                      message(PXA_MSG_SCREEN_INSTALLED),
                      (uint8_t)(app.screen == STORE_SCREEN_INSTALLED)) &&
           create_tab(transaction, NODE_DOWNLOADS_ACTION,
                      NODE_DOWNLOADS_ACTION_LABEL,
                      NODE_DOWNLOADS_ACTION_ICON, ICON_DOWNLOADS,
                      message(PXA_MSG_SCREEN_DOWNLOADS),
                      (uint8_t)(app.screen == STORE_SCREEN_DOWNLOADS));
}

static int render_catalog(void) {
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = app.generation + 1u;
    int ok;
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                  app.packet, sizeof(app.packet)))
        return 0;
    ok = create_header(&transaction, STORE_SCREEN_CATALOG,
                       message(PXA_MSG_SCREEN_TITLE),
                       pxa_i18n_size(&app.i18n, PXA_MSG_SCREEN_TITLE)) &&
         create_filters(&transaction) &&
         create_catalog_list(&transaction) && create_footer(&transaction) &&
         create_tabbar(&transaction, STORE_SCREEN_CATALOG);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    return 1;
}

static int patch_catalog_status(bool include_footer = true) {
    pxa_ui_transaction_t transaction{};
    char status[64];
    uint32_t next;
    if (!include_footer && metrics()->status_height == 0) return 1;
    (void)footer_status_text(status, sizeof(status));
    next = app.generation + 1u;
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next, PXA_UI_TRANSACTION_PATCH,
                                  app.packet, sizeof(app.packet)))
        return 0;
    const bool header_ok = metrics()->status_height == 0 ||
        (pxa_ui_set_text(&transaction, NODE_HEADER_STATUS, status, string_length(status)) &&
         pxa_ui_set_theme_color(&transaction, NODE_HEADER_STATUS,
                                PXA_UI_PROPERTY_FOREGROUND, status_color()));
    if (!header_ok || (include_footer && !update_catalog_footer(&transaction)) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    return 1;
}

static int render_catalog_list(void) {
    sync_media();
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = app.generation + 1u;
    if (next == 0 ||
        !pxa_ui_transaction_begin_target(&transaction, next, next,
                                         PXA_UI_PRIMARY_SURFACE, NODE_LIST,
                                         PXA_UI_TRANSACTION_REPLACE_SUBTREE,
                                         app.packet, sizeof(app.packet)))
        return 0;
    if (!create_catalog_list(&transaction) || !create_footer(&transaction) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    return patch_catalog_status(false);
}

// Append a page to the retained scroll node. Existing cards keep their object
// identities, so an in-progress gesture survives background page completion.
static int append_catalog_page(uint8_t before) {
    sync_media();
    pxa_ui_transaction_t tx{};
    const uint32_t next = app.generation + 1;
    if (!next || !pxa_ui_transaction_begin(&tx, next, PXA_UI_TRANSACTION_PATCH,
        app.packet, sizeof(app.packet))) return 0;
    // A PATCH cannot remove and recreate the same identity. Keep the footer
    // alive, then move it after the appended cards and update it in place.
    // MOVE is also honored by deployed Hosts that ignore CREATE.before.
    int ok = 1;
    const auto columns = catalog_columns();
    if (columns > 1) {
        pxa_ui_grid_track_t rows[PXA_UI_GRID_MAX_TRACKS];
        const uint8_t count = uint8_t((app.catalog.count + columns - 1) / columns + 1);
        for (uint8_t i = 0; i < count; ++i) rows[i] = {PXA_UI_GRID_CONTENT, 0};
        ok = ok && pxa_ui_set_grid_rows(&tx, NODE_LIST, rows, count);
    }
    for (uint8_t i = before; ok && i < app.catalog.count; ++i) ok = create_card(&tx, i);
    ok = ok && pxa_ui_move(&tx, NODE_FOOTER, NODE_LIST, 0) && update_catalog_footer(&tx);
    if (columns > 1)
        ok = ok && pxa_ui_set_grid_cell(&tx, NODE_FOOTER, 0,
            (app.catalog.count + columns - 1) / columns, columns, 1);
    if (!ok || !pxa_ui_transaction_commit(&tx)) {
        if (tx.active) (void)pxa_ui_transaction_cancel(&tx);
        return 0;
    }
    app.generation = next;
    return patch_catalog_status(false);
}

static int create_library_button(pxa_ui_transaction_t *transaction,
                                 uint32_t node, uint32_t label,
                                 uint32_t parent, const char *text,
                                 uint8_t outlined) {
    return pxa_ui_create_typed(transaction, node, parent, 0,
                               PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, metrics()->action_height) &&
           pxa_ui_set_padding(transaction, node,
                              app.width < 340u ? 6 : 12, 2,
                              app.width < 340u ? 6 : 12, 2) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_RADIUS,
                         metrics()->action_height / 2u) &&
           pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
           pxa_ui_set_theme_color(transaction, node,
                                  PXA_UI_PROPERTY_BORDER_COLOR,
                                  outlined == 2u ? PXA_UI_THEME_DANGER :
                                  outlined == 0u ? PXA_UI_THEME_PRIMARY :
                                                    PXA_UI_THEME_BORDER) &&
           pxa_ui_set_theme_color(transaction, node,
                                  PXA_UI_PROPERTY_BACKGROUND,
                                  outlined == 0u ? PXA_UI_THEME_PRIMARY :
                                                   PXA_UI_THEME_SURFACE) &&
           pxa_ui_set_event_mask(transaction, node, PXA_UI_EVENT_MASK_CLICK) &&
           pxa_ui_create(transaction, label, node, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, label, text, string_length(text)) &&
           pxa_ui_set_font_role(transaction, label, PXA_UI_FONT_ROLE_LABEL) &&
           pxa_ui_set_theme_color(transaction, label,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  outlined == 0u ? PXA_UI_THEME_ON_PRIMARY :
                                  outlined == 2u ? PXA_UI_THEME_DANGER :
                                                   PXA_UI_THEME_PRIMARY);
}

static int create_library_row(pxa_ui_transaction_t *transaction,
                              uint8_t index, const char *name,
                              const char *detail, const char *action,
                              const char *secondary, const char *tertiary) {
    const store_metrics_t *m = metrics();
    const uint32_t row = NODE_LIBRARY_BASE + (uint32_t)index * NODE_LIBRARY_STRIDE;
    return pxa_ui_create(transaction, row, NODE_LIST, 0, PXA_UI_NODE_BOX) &&
           pxa_ui_set_length(transaction, row, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_length(transaction, row, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX,
                             app.width < 340u ? 94u :
                             app.width >= 480u ? 94u : 92u) &&
           pxa_ui_set_padding(transaction, row, m->card_pad, 6,
                              m->card_pad, 6) &&
           pxa_ui_set_dp(transaction, row, PXA_UI_PROPERTY_GAP, 3) &&
           pxa_ui_set_u8(transaction, row, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_COLUMN) &&
           pxa_ui_set_dp(transaction, row, PXA_UI_PROPERTY_RADIUS,
                         m->card_radius) &&
           pxa_ui_set_dp(transaction, row, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
           pxa_ui_set_theme_color(transaction, row, PXA_UI_PROPERTY_BORDER_COLOR,
                                  PXA_UI_THEME_BORDER) &&
           pxa_ui_set_theme_color(transaction, row, PXA_UI_PROPERTY_BACKGROUND,
                                  PXA_UI_THEME_SURFACE) &&
           pxa_ui_create(transaction, row + 1u, row, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, row + 1u, name, string_length(name)) &&
           pxa_ui_set_font_role(transaction, row + 1u, PXA_UI_FONT_ROLE_LABEL) &&
           pxa_ui_set_theme_color(transaction, row + 1u,
                                  PXA_UI_PROPERTY_FOREGROUND, PXA_UI_THEME_TEXT) &&
           pxa_ui_create(transaction, row + 2u, row, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, row + 2u, detail, string_length(detail)) &&
           pxa_ui_set_font_role(transaction, row + 2u, PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, row + 2u,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  secondary != NULL ? PXA_UI_THEME_PRIMARY :
                                                      PXA_UI_THEME_MUTED) &&
           (action == NULL || pxa_ui_create(transaction, row + 7u, row, 0, PXA_UI_NODE_BOX)) &&
           (action == NULL ||
           pxa_ui_set_length(transaction, row + 7u, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0)) &&
           (action == NULL ||
           pxa_ui_set_u8(transaction, row + 7u, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW)) &&
           (action == NULL || pxa_ui_set_dp(transaction, row + 7u, PXA_UI_PROPERTY_GAP, 8)) &&
           (action == NULL || create_library_button(transaction, row + 3u, row + 4u,
                                                    row + 7u, action, 0)) &&
           (secondary == NULL || create_library_button(transaction, row + 5u,
                                                       row + 6u, row + 7u,
                                                       secondary,
                                                       text_equal(secondary,
                                                           message(PXA_MSG_ACTION_DELETE)) ?
                                                           2u : 1u)) &&
           (tertiary == NULL || create_library_button(transaction, row + 8u,
                                                      row + 9u, row + 7u,
                                                      tertiary, 2u));
}

static int render_library(void) {
    const store_metrics_t *m = metrics();
    const uint8_t installed = app.screen == STORE_SCREEN_INSTALLED;
    const char *title = message(installed ? PXA_MSG_SCREEN_INSTALLED :
                                            PXA_MSG_SCREEN_DOWNLOADS);
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = app.generation + 1u;
    uint8_t active = (uint8_t)(!installed && app.downloading);
    uint8_t total = installed ? app.installed_count :
                                (uint8_t)(active + app.download_count + app.failed_count);
    int ok;
    if (next == 0 || !pxa_ui_transaction_begin(&transaction, next,
                        PXA_UI_TRANSACTION_REPLACE_SURFACE,
                        app.packet, sizeof(app.packet))) return 0;
    ok = create_header(&transaction, app.screen, title,
                       string_length(title)) &&
         (!installed || (app.width < 340u ?
          create_action_button(&transaction, NODE_LIBRARY_CHECK,
                               NODE_LIBRARY_CHECK_LABEL,
                               message(PXA_MSG_ACTION_CHECK_UPDATES), 1) :
          create_library_button(&transaction, NODE_LIBRARY_CHECK,
                                NODE_LIBRARY_CHECK_LABEL, NODE_HEADER_ROW,
                                message(PXA_MSG_ACTION_CHECK_UPDATES), 1))) &&
         pxa_ui_create(&transaction, NODE_LIST, NODE_ROOT, 0, PXA_UI_NODE_SCROLL) &&
         pxa_ui_set_length(&transaction, NODE_LIST, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, NODE_LIST, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 0) &&
         pxa_ui_set_u16(&transaction, NODE_LIST, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, NODE_LIST, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, NODE_LIST, PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
         pxa_ui_set_u8(&transaction, NODE_LIST, PXA_UI_PROPERTY_SCROLLBAR, 1) &&
         pxa_ui_set_event_mask(&transaction, NODE_LIST,
                               PXA_UI_EVENT_MASK_SCROLL |
                                   PXA_UI_EVENT_MASK_POINTER) &&
         pxa_ui_set_padding(&transaction, NODE_LIST, 4, 8, 4, 8) &&
         pxa_ui_set_dp(&transaction, NODE_LIST, PXA_UI_PROPERTY_GAP, m->list_gap);
    if (ok && installed && app.uninstall_feedback != 0u) {
        const char *feedback = message(app.uninstall_feedback == 1u ?
            PXA_MSG_STATUS_UNINSTALL_WAITING : app.uninstall_feedback == 2u ?
            PXA_MSG_STATUS_UNINSTALL_SUCCESS : app.uninstall_feedback == 4u ?
            PXA_MSG_STATUS_UNINSTALL_CANCELLED : PXA_MSG_STATUS_UNINSTALL_FAILED);
        ok = pxa_ui_create(&transaction, NODE_LIBRARY_STATUS, NODE_LIST, 0,
                           PXA_UI_NODE_TEXT) &&
             pxa_ui_set_text(&transaction, NODE_LIBRARY_STATUS, feedback,
                             string_length(feedback)) &&
             pxa_ui_set_font_role(&transaction, NODE_LIBRARY_STATUS,
                                  PXA_UI_FONT_ROLE_CAPTION);
    } else if (ok && installed && (app.update_checking || app.update_checked)) {
        const char *feedback = message(app.update_checking ?
            PXA_MSG_STATUS_CHECKING_UPDATES : app.update_failed ?
            PXA_MSG_STATUS_UPDATE_CHECK_FAILED : PXA_MSG_STATUS_UPDATES_CHECKED);
        ok = pxa_ui_create(&transaction, NODE_LIBRARY_STATUS, NODE_LIST, 0,
                           PXA_UI_NODE_TEXT) &&
             pxa_ui_set_text(&transaction, NODE_LIBRARY_STATUS, feedback,
                             string_length(feedback));
    } else if (ok && app.install_feedback != 0u) {
        const char *feedback = message(install_feedback_message());
        ok = pxa_ui_create(&transaction, NODE_LIBRARY_STATUS, NODE_LIST, 0,
                           PXA_UI_NODE_TEXT) &&
             pxa_ui_set_text(&transaction, NODE_LIBRARY_STATUS, feedback,
                             string_length(feedback)) &&
             pxa_ui_set_font_role(&transaction, NODE_LIBRARY_STATUS,
                                  PXA_UI_FONT_ROLE_CAPTION);
    }
    if (ok && total == 0) {
        const char *empty = message(installed ? PXA_MSG_LIST_NO_INSTALLED :
                                                PXA_MSG_LIST_NO_DOWNLOADS);
        ok = pxa_ui_create(&transaction, NODE_EMPTY, NODE_LIST, 0, PXA_UI_NODE_TEXT) &&
             pxa_ui_set_text(&transaction, NODE_EMPTY, empty, string_length(empty));
    }
    for (uint8_t index = 0; ok && index < total; ++index) {
        uint8_t entry_index = (uint8_t)(index - active);
        const char *name;
        const char *detail;
        const char *action;
        const char *secondary = NULL;
        const char *tertiary = NULL;
        char progress[64] = {0};
        if (active && index == 0u) {
            size_t offset = append_text(progress, sizeof(progress), 0,
                                        message(PXA_MSG_STATUS_DOWNLOADING));
            offset = append_text(progress, sizeof(progress), offset, " · ");
            offset = append_u64(progress, sizeof(progress), offset,
                                app.download_total == 0u ? 0u :
                                app.downloaded_bytes * 100u / app.download_total);
            (void)append_text(progress, sizeof(progress), offset, "%");
            name = app.completed_app_id;
            detail = progress;
            action = NULL;
        } else if (installed) {
            const pxa_store_installed_app_t *entry = &app.installed[index];
            name = entry->app_id;
            detail = entry->version;
            action = message(PXA_MSG_ACTION_OPEN);
            if (entry->uninstallable &&
                !text_equal(entry->app_id, "pxa-store"))
                tertiary = message(PXA_MSG_ACTION_UNINSTALL);
            if (app.update_available[index]) {
                detail = message(PXA_MSG_STATUS_UPDATE_AVAILABLE);
                secondary = message(PXA_MSG_ACTION_UPDATE);
            }
            for (uint8_t candidate = 0; candidate < app.catalog.count && !secondary; ++candidate) {
                const store_app_t *item = store_catalog_at_const(&app.catalog, candidate);
                if (text_equal(item->app_id, entry->app_id) && has_update(item)) {
                    detail = message(PXA_MSG_STATUS_UPDATE_AVAILABLE);
                    secondary = message(PXA_MSG_ACTION_UPDATE);
                    break;
                }
            }
        } else if (entry_index < app.download_count) {
            name = app.downloads[entry_index].app_id;
            detail = message(PXA_MSG_STATUS_DOWNLOADED);
            action = message(PXA_MSG_ACTION_INSTALL);
            secondary = message(PXA_MSG_ACTION_DELETE);
        } else {
            name = app.failed_apps[entry_index - app.download_count];
            detail = message(PXA_MSG_STATUS_DOWNLOAD_FAILED);
            action = message(PXA_MSG_ACTION_DELETE);
        }
        ok = create_library_row(&transaction, index, name, detail, action,
                                secondary, tertiary);
        if (ok && active && index == 0u) {
            uint32_t row = NODE_LIBRARY_BASE;
            ok = pxa_ui_create(&transaction, row + 10u, row, 0, PXA_UI_NODE_BOX) &&
                 pxa_ui_set_length(&transaction, row + 10u, PXA_UI_PROPERTY_WIDTH,
                                   PXA_UI_LENGTH_FILL, 0) &&
                 pxa_ui_set_length(&transaction, row + 10u, PXA_UI_PROPERTY_HEIGHT,
                                   PXA_UI_LENGTH_PX, 5) &&
                 pxa_ui_set_dp(&transaction, row + 10u, PXA_UI_PROPERTY_RADIUS, 3) &&
                 pxa_ui_set_theme_color(&transaction, row + 10u,
                                        PXA_UI_PROPERTY_BACKGROUND, PXA_UI_THEME_BORDER) &&
                 pxa_ui_create(&transaction, row + 11u, row + 10u, 0, PXA_UI_NODE_BOX) &&
                 pxa_ui_set_length(&transaction, row + 11u, PXA_UI_PROPERTY_WIDTH,
                                   PXA_UI_LENGTH_PERCENT_Q16,
                                   app.download_total == 0u ? 0 :
                                   (int32_t)(app.downloaded_bytes * 100u / app.download_total)) &&
                 pxa_ui_set_length(&transaction, row + 11u, PXA_UI_PROPERTY_HEIGHT,
                                   PXA_UI_LENGTH_PX, 5) &&
                 pxa_ui_set_dp(&transaction, row + 11u, PXA_UI_PROPERTY_RADIUS, 3) &&
                 pxa_ui_set_theme_color(&transaction, row + 11u,
                                        PXA_UI_PROPERTY_BACKGROUND, PXA_UI_THEME_PRIMARY);
        }
    }
    if (ok) ok = create_tabbar(&transaction, app.screen);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    if (active)
        app.progress_rendered = (uint8_t)(app.download_total == 0u ? 0u :
            app.downloaded_bytes * 100u / app.download_total);
    return 1;
}

static int patch_download_progress(void) {
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = app.generation + 1u;
    uint8_t percent = (uint8_t)(app.download_total == 0u ? 0u :
        app.downloaded_bytes * 100u / app.download_total);
    char progress[64];
    size_t offset;
    if (percent > 100u) percent = 100u;
    if (percent == app.progress_rendered) return 1;
    offset = append_text(progress, sizeof(progress), 0,
                         message(PXA_MSG_STATUS_DOWNLOADING));
    offset = append_text(progress, sizeof(progress), offset, " · ");
    offset = append_u64(progress, sizeof(progress), offset, percent);
    (void)append_text(progress, sizeof(progress), offset, "%");
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next, PXA_UI_TRANSACTION_PATCH,
                                  app.packet, sizeof(app.packet)))
        return 0;
    if (!pxa_ui_set_text(&transaction, NODE_LIBRARY_BASE + 2u, progress,
                         string_length(progress)) ||
        !pxa_ui_set_length(&transaction, NODE_LIBRARY_BASE + 11u,
                            PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PERCENT_Q16,
                            percent) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    app.progress_rendered = percent;
    return 1;
}

static int render_success(void) {
    const store_metrics_t *m = metrics();
    pxa_ui_transaction_t transaction = {0};
    const char *title = message(PXA_MSG_STATUS_INSTALL_SUCCESS);
    const char *body = message(PXA_MSG_DIALOG_INSTALL_SUCCESS);
    const store_app_t *item = detail_item();
    const char *name = item != NULL &&
        text_equal(item->app_id, app.completed_app_id) ?
        item->name : app.completed_app_id;
    uint32_t next = app.generation + 1u;
    int ok;
    if (next == 0 || !pxa_ui_transaction_begin(&transaction, next,
                        PXA_UI_TRANSACTION_REPLACE_SURFACE,
                        app.packet, sizeof(app.packet))) return 0;
    ok = create_header(&transaction, STORE_SCREEN_DETAIL, title,
                       string_length(title)) &&
         pxa_ui_create(&transaction, NODE_LIST, NODE_ROOT, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, NODE_LIST, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u16(&transaction, NODE_LIST, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, NODE_LIST, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, NODE_LIST, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, NODE_LIST, PXA_UI_PROPERTY_GAP, 10) &&
         pxa_ui_create(&transaction, NODE_SUCCESS_OVERLAY, NODE_LIST, 0,
                       PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, NODE_SUCCESS_OVERLAY,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, NODE_SUCCESS_OVERLAY,
                           PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                           app.width < 340u ? 110 : 160) &&
         pxa_ui_set_u8(&transaction, NODE_SUCCESS_OVERLAY,
                       PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, NODE_SUCCESS_OVERLAY,
                       PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_u8(&transaction, NODE_SUCCESS_OVERLAY,
                       PXA_UI_PROPERTY_JUSTIFY, PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, NODE_SUCCESS_OVERLAY,
                        PXA_UI_PROPERTY_GAP, 4) &&
         pxa_ui_set_dp(&transaction, NODE_SUCCESS_OVERLAY,
                        PXA_UI_PROPERTY_RADIUS, m->card_radius + 6) &&
         pxa_ui_set_theme_color(&transaction, NODE_SUCCESS_OVERLAY,
                                PXA_UI_PROPERTY_BACKGROUND, PXA_UI_THEME_SURFACE) &&
         pxa_ui_create(&transaction, NODE_SUCCESS_ICON, NODE_SUCCESS_OVERLAY, 0,
                       PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, NODE_SUCCESS_ICON,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 36) &&
         pxa_ui_set_length(&transaction, NODE_SUCCESS_ICON,
                           PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 36) &&
         pxa_ui_set_u8(&transaction, NODE_SUCCESS_ICON,
                       PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, NODE_SUCCESS_ICON,
                       PXA_UI_PROPERTY_JUSTIFY, PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_u8(&transaction, NODE_SUCCESS_ICON,
                       PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, NODE_SUCCESS_ICON,
                        PXA_UI_PROPERTY_RADIUS, 18) &&
         pxa_ui_set_theme_color(&transaction, NODE_SUCCESS_ICON,
                                PXA_UI_PROPERTY_BACKGROUND, PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, NODE_SUCCESS_ICON_LABEL, NODE_SUCCESS_ICON,
                       0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, NODE_SUCCESS_ICON_LABEL,
                         ICON_INSTALLED, string_length(ICON_INSTALLED)) &&
         pxa_ui_set_font_role(&transaction, NODE_SUCCESS_ICON_LABEL,
                              PXA_UI_FONT_ROLE_ICON) &&
         pxa_ui_set_theme_color(&transaction, NODE_SUCCESS_ICON_LABEL,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, NODE_SUCCESS_NAME, NODE_SUCCESS_OVERLAY,
                       0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, NODE_SUCCESS_NAME, name,
                         string_length(name)) &&
         pxa_ui_set_font_role(&transaction, NODE_SUCCESS_NAME,
                              PXA_UI_FONT_ROLE_LABEL) &&
         pxa_ui_set_theme_color(&transaction, NODE_SUCCESS_NAME,
                                PXA_UI_PROPERTY_FOREGROUND, PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, NODE_SUCCESS_BODY, NODE_SUCCESS_OVERLAY,
                       0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, NODE_SUCCESS_BODY, body,
                         string_length(body)) &&
         pxa_ui_set_font_role(&transaction, NODE_SUCCESS_BODY,
                              PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, NODE_SUCCESS_BODY,
                                PXA_UI_PROPERTY_FOREGROUND, PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, NODE_SUCCESS_ACTIONS, NODE_LIST, 0,
                       PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, NODE_SUCCESS_ACTIONS,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u8(&transaction, NODE_SUCCESS_ACTIONS,
                       PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_dp(&transaction, NODE_SUCCESS_ACTIONS,
                        PXA_UI_PROPERTY_GAP, 8) &&
         create_library_button(&transaction, NODE_SUCCESS_OPEN,
                               NODE_SUCCESS_OPEN + 1u, NODE_SUCCESS_ACTIONS,
                               message(PXA_MSG_ACTION_OPEN), 0) &&
         create_library_button(&transaction, NODE_SUCCESS_LATER,
                               NODE_SUCCESS_LATER + 1u, NODE_SUCCESS_ACTIONS,
                               message(PXA_MSG_ACTION_DONE), 1) &&
         pxa_ui_set_u16(&transaction, NODE_SUCCESS_OPEN,
                        PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u16(&transaction, NODE_SUCCESS_LATER,
                        PXA_UI_PROPERTY_GROW, 1);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    return 1;
}

static int create_section(pxa_ui_transaction_t *transaction, uint8_t index,
                          const char *title, const char *body,
                          uint8_t warning) {
    const store_metrics_t *m = metrics();
    uint32_t box = NODE_DETAIL_SECTION_BASE +
                   (uint32_t)index * NODE_DETAIL_SECTION_STRIDE;
    uint32_t title_node = box + 1u;
    uint32_t body_node = box + 2u;
    return pxa_ui_create(transaction, box, NODE_DETAIL_SCROLL, 0,
                         PXA_UI_NODE_BOX) &&
           pxa_ui_set_length(transaction, box, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_COLUMN) &&
           pxa_ui_set_padding(transaction, box, m->card_pad + 2, m->card_pad,
                           m->card_pad + 2, m->card_pad) &&
           pxa_ui_set_dp(transaction, box, PXA_UI_PROPERTY_GAP, 4) &&
           pxa_ui_set_dp(transaction, box, PXA_UI_PROPERTY_RADIUS, m->card_radius) &&
           pxa_ui_set_dp(transaction, box, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
           pxa_ui_set_theme_color(transaction, box, PXA_UI_PROPERTY_BACKGROUND,
                                  PXA_UI_THEME_SURFACE) &&
           pxa_ui_set_theme_color(transaction, box,
                                  PXA_UI_PROPERTY_BORDER_COLOR,
                                  warning ? PXA_UI_THEME_WARNING
                                          : PXA_UI_THEME_BORDER) &&
           pxa_ui_create(transaction, title_node, box, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, title_node, title,
                           string_length(title)) &&
           pxa_ui_set_font_role(transaction, title_node,
                                PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, title_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  warning ? PXA_UI_THEME_WARNING
                                          : PXA_UI_THEME_MUTED) &&
           pxa_ui_create(transaction, body_node, box, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_length(transaction, body_node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_text(transaction, body_node, body,
                           string_length(body)) &&
           pxa_ui_set_font_role(transaction, body_node,
                                PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, body_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_TEXT);
}

static int create_row(pxa_ui_transaction_t *transaction, uint8_t index,
                      const char *label, const char *value) {
    uint32_t box = NODE_DETAIL_ROW_BASE +
                   (uint32_t)index * NODE_DETAIL_ROW_STRIDE;
    uint32_t label_node = box + 1u;
    uint32_t value_node = box + 2u;
    return pxa_ui_create(transaction, box, NODE_DETAIL_SCROLL, 0,
                         PXA_UI_NODE_BOX) &&
           pxa_ui_set_length(transaction, box, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_START) &&
           pxa_ui_set_padding(transaction, box, 2, 0, 2, 0) &&
           pxa_ui_set_dp(transaction, box, PXA_UI_PROPERTY_GAP, 8) &&
           pxa_ui_create(transaction, label_node, box, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_length(transaction, label_node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_PX, 86) &&
           pxa_ui_set_text(transaction, label_node, label,
                           string_length(label)) &&
           pxa_ui_set_font_role(transaction, label_node,
                                PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, label_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED) &&
           pxa_ui_create(transaction, value_node, box, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_u16(transaction, value_node, PXA_UI_PROPERTY_GROW, 1) &&
           pxa_ui_set_text(transaction, value_node, value,
                           string_length(value)) &&
           pxa_ui_set_font_role(transaction, value_node,
                                PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, value_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_TEXT);
}

static void build_permissions_text(char *output, size_t capacity) {
    const store_app_t *item = detail_item();
    size_t offset = 0;
    uint8_t index;
    output[0] = '\0';
    if (item->permission_count == 0) {
        copy_text(output, capacity, message(PXA_MSG_DETAIL_NONE));
        return;
    }
    for (index = 0; index < item->permission_count; ++index) {
        const store_permission_t *permission = &item->permissions[index];
        if (index != 0) offset = append_text(output, capacity, offset, "\n");
        offset = append_text(output, capacity, offset, permission->name);
        if (permission->scope[0] != '\0') {
            offset = append_text(output, capacity, offset, " (");
            offset = append_text(output, capacity, offset, permission->scope);
            offset = append_text(output, capacity, offset, ")");
        }
        offset = append_text(output, capacity, offset, " · ");
        (void)append_text(output, capacity, offset,
                          permission->required
                              ? message(PXA_MSG_DETAIL_REQUIRED)
                              : message(PXA_MSG_DETAIL_OPTIONAL));
    }
}

static void build_services_text(char *output, size_t capacity) {
    const store_app_t *item = detail_item();
    size_t offset = 0;
    uint8_t index;
    output[0] = '\0';
    if (item->service_count == 0) {
        copy_text(output, capacity, message(PXA_MSG_DETAIL_NONE));
        return;
    }
    for (index = 0; index < item->service_count; ++index) {
        if (index != 0) offset = append_text(output, capacity, offset, "\n");
        format_service_requirement(output + offset, capacity - offset,
                                   &item->services[index]);
        offset = string_length(output);
    }
}

static void build_install_text(char *output, size_t capacity) {
    const store_app_t *item = detail_item();
    char size[24];
    size_t offset = 0;
    output[0] = '\0';
    format_size(size, sizeof(size), item->size);
    offset = append_text(output, capacity, offset, size);
    offset = append_text(output, capacity, offset, "\n");
    offset = append_text(output, capacity, offset,
                         message(PXA_MSG_INSTALL_HOST_CONFIRM));
    if (app.install_feedback != 0) {
        offset = append_text(output, capacity, offset, "\n");
        (void)append_text(output, capacity, offset,
                          message(install_feedback_message()));
    }
}

static store::ViewerGeometry viewer_geometry(void) {
    const auto *m = metrics();
    const auto &shot = app.gallery.screenshots[app.screenshot];
    const auto horizontal = inset_padding(3, m->list_pad_h) +
                            inset_padding(1, m->list_pad_h) + 2u * m->list_pad_h;
    const auto vertical = inset_padding(0, m->header_pad_v) + effective_inset(2) +
                          m->header_height + std::max(36u, unsigned(m->action_height)) + 12u;
    return store::viewer_geometry(app.width > horizontal ? app.width - horizontal : 1u,
        app.height > vertical ? app.height - vertical : 1u,
        shot.width, shot.height, app.viewer_zoom);
}

static int create_viewer_content(pxa_ui_transaction_t *tx) {
    const auto &shot = app.gallery.screenshots[app.screenshot];
    const auto handle = store::media_handle(shot.url, true);
    const auto size = viewer_geometry();
    int ok = pxa_ui_create(tx, NODE_VIEWER_CONTENT, NODE_VIEWPORT, 0, PXA_UI_NODE_BOX) &&
        pxa_ui_set_length(tx, NODE_VIEWER_CONTENT, PXA_UI_PROPERTY_WIDTH,
                          PXA_UI_LENGTH_PX, size.canvas_width) &&
        pxa_ui_set_length(tx, NODE_VIEWER_CONTENT, PXA_UI_PROPERTY_HEIGHT,
                          PXA_UI_LENGTH_PX, size.canvas_height) &&
        pxa_ui_set_padding(tx, NODE_VIEWER_CONTENT, 0, 0, 0, 0) &&
        pxa_ui_set_u8(tx, NODE_VIEWER_CONTENT, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
        pxa_ui_set_u8(tx, NODE_VIEWER_CONTENT, PXA_UI_PROPERTY_JUSTIFY, PXA_UI_ALIGN_CENTER) &&
        pxa_ui_set_u8(tx, NODE_VIEWER_CONTENT, PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER);
    if (handle) {
        ok = ok && pxa_ui_create(tx, NODE_VIEWER_IMAGE, NODE_VIEWER_CONTENT, 0, PXA_UI_NODE_IMAGE) &&
            pxa_ui_set_length(tx, NODE_VIEWER_IMAGE, PXA_UI_PROPERTY_WIDTH,
                              PXA_UI_LENGTH_PX, size.image_width) &&
            pxa_ui_set_length(tx, NODE_VIEWER_IMAGE, PXA_UI_PROPERTY_HEIGHT,
                              PXA_UI_LENGTH_PX, size.image_height) &&
            pxa_ui_set_image(tx, NODE_VIEWER_IMAGE, handle) &&
            pxa_ui_set_u8(tx, NODE_VIEWER_IMAGE, PXA_UI_PROPERTY_IMAGE_FIT, PXA_UI_IMAGE_FIT_CONTAIN);
    } else {
        const char *text = message(store::media_failed(shot.url, true) ?
            PXA_MSG_MEDIA_RETRY : PXA_MSG_MEDIA_LOADING);
        ok = ok && pxa_ui_create(tx, NODE_VIEWER_STATUS, NODE_VIEWER_CONTENT, 0, PXA_UI_NODE_TEXT) &&
            pxa_ui_set_length(tx, NODE_VIEWER_STATUS, PXA_UI_PROPERTY_WIDTH,
                              PXA_UI_LENGTH_FILL, 0) &&
            pxa_ui_set_u8(tx, NODE_VIEWER_STATUS, PXA_UI_PROPERTY_TEXT_ALIGN, 1) &&
            pxa_ui_set_text(tx, NODE_VIEWER_STATUS, text, string_length(text)) &&
            pxa_ui_set_font_role(tx, NODE_VIEWER_STATUS, PXA_UI_FONT_ROLE_CAPTION) &&
            pxa_ui_set_theme_color(tx, NODE_VIEWER_STATUS, PXA_UI_PROPERTY_FOREGROUND, PXA_UI_THEME_MUTED);
    }
    app.bound_screenshot = handle;
    return ok;
}

static int create_gallery(pxa_ui_transaction_t *tx) {
    if (!app.gallery.count) return 1;
    const auto &shot = app.gallery.screenshots[app.screenshot];
    const auto handle = store::media_handle(shot.url, true);
    const unsigned usable = app.height > effective_inset(0) + effective_inset(2) ?
        app.height - effective_inset(0) - effective_inset(2) : app.height;
    const unsigned height = usable < 240 ? 80 : 176;
    const char *placeholder = message(store::media_failed(shot.url, true) ?
        PXA_MSG_MEDIA_RETRY : PXA_MSG_MEDIA_LOADING);
    char count[24];
    std::snprintf(count, sizeof(count), "%u / %u", app.screenshot + 1, app.gallery.count);
    int ok = pxa_ui_create(tx, NODE_GALLERY, NODE_DETAIL_SCROLL, 0, PXA_UI_NODE_BOX) &&
        pxa_ui_set_length(tx, NODE_GALLERY, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
        pxa_ui_set_u8(tx, NODE_GALLERY, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
        pxa_ui_set_dp(tx, NODE_GALLERY, PXA_UI_PROPERTY_GAP, 4) &&
        pxa_ui_create_typed(tx, NODE_GALLERY_IMAGE, NODE_GALLERY, 0,
            handle ? PXA_UI_NODE_IMAGE : PXA_UI_NODE_CONTROL,
            handle ? 0 : PXA_UI_CONTROL_BUTTON) &&
        pxa_ui_set_length(tx, NODE_GALLERY_IMAGE, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
        pxa_ui_set_length(tx, NODE_GALLERY_IMAGE, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, height) &&
        pxa_ui_set_event_mask(tx, NODE_GALLERY_IMAGE,
            PXA_UI_EVENT_MASK_CLICK | PXA_UI_EVENT_MASK_POINTER) &&
        pxa_ui_set_property(tx, NODE_GALLERY_IMAGE, PXA_UI_PROPERTY_ACCESSIBILITY_LABEL,
            message(PXA_MSG_MEDIA_OPEN), string_length(message(PXA_MSG_MEDIA_OPEN)));
    if (handle) ok = ok && pxa_ui_set_image(tx, NODE_GALLERY_IMAGE, handle) &&
        pxa_ui_set_u8(tx, NODE_GALLERY_IMAGE, PXA_UI_PROPERTY_IMAGE_FIT, PXA_UI_IMAGE_FIT_CONTAIN);
    else ok = ok &&
        pxa_ui_set_u8(tx, NODE_GALLERY_IMAGE, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
        pxa_ui_set_u8(tx, NODE_GALLERY_IMAGE, PXA_UI_PROPERTY_JUSTIFY, PXA_UI_ALIGN_CENTER) &&
        pxa_ui_set_u8(tx, NODE_GALLERY_IMAGE, PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
        pxa_ui_set_theme_color(tx, NODE_GALLERY_IMAGE, PXA_UI_PROPERTY_BACKGROUND, PXA_UI_THEME_SURFACE_CONTAINER_LOW) &&
        pxa_ui_create(tx, NODE_GALLERY_PLACEHOLDER, NODE_GALLERY_IMAGE, 0, PXA_UI_NODE_TEXT) &&
        pxa_ui_set_text(tx, NODE_GALLERY_PLACEHOLDER, placeholder, string_length(placeholder)) &&
        pxa_ui_set_font_role(tx, NODE_GALLERY_PLACEHOLDER, PXA_UI_FONT_ROLE_CAPTION) &&
        pxa_ui_set_theme_color(tx, NODE_GALLERY_PLACEHOLDER, PXA_UI_PROPERTY_FOREGROUND, PXA_UI_THEME_MUTED);
    if (app.gallery.count == 1) {
        app.bound_screenshot = handle;
        return ok;
    }
    ok = ok && pxa_ui_create(tx, NODE_GALLERY_BAR, NODE_GALLERY, 0, PXA_UI_NODE_BOX) &&
        pxa_ui_set_length(tx, NODE_GALLERY_BAR, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
        pxa_ui_set_u8(tx, NODE_GALLERY_BAR, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
        pxa_ui_set_u8(tx, NODE_GALLERY_BAR, PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
        create_library_button(tx, NODE_GALLERY_PREV, NODE_GALLERY_PREV_LABEL, NODE_GALLERY_BAR, "‹", 1) &&
        pxa_ui_set_u8(tx, NODE_GALLERY_PREV, PXA_UI_PROPERTY_ENABLED, app.screenshot > 0) &&
        pxa_ui_create(tx, NODE_GALLERY_COUNT, NODE_GALLERY_BAR, 0, PXA_UI_NODE_TEXT) &&
        pxa_ui_set_u16(tx, NODE_GALLERY_COUNT, PXA_UI_PROPERTY_GROW, 1) &&
        pxa_ui_set_u8(tx, NODE_GALLERY_COUNT, PXA_UI_PROPERTY_TEXT_ALIGN, 1) &&
        pxa_ui_set_text(tx, NODE_GALLERY_COUNT, count, string_length(count)) &&
        pxa_ui_set_font_role(tx, NODE_GALLERY_COUNT, PXA_UI_FONT_ROLE_CAPTION) &&
        pxa_ui_set_theme_color(tx, NODE_GALLERY_COUNT, PXA_UI_PROPERTY_FOREGROUND, PXA_UI_THEME_MUTED) &&
        create_library_button(tx, NODE_GALLERY_NEXT, NODE_GALLERY_NEXT_LABEL, NODE_GALLERY_BAR, "›", 1) &&
        pxa_ui_set_u8(tx, NODE_GALLERY_NEXT, PXA_UI_PROPERTY_ENABLED, app.screenshot + 1 < app.gallery.count);
    app.bound_screenshot = handle;
    return ok;
}

static int render_detail(void) {
    const store_metrics_t *m = metrics();
    pxa_ui_transaction_t transaction = {0};
    const store_app_t *item = detail_item();
    uint32_t next = app.generation + 1u;
    char id_text[STORE_MAX_KEY_ID + 8];
    char size_text[32];
    char sequence_text[24];
    char sdk_text[40];
    int ok;
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                  app.packet, sizeof(app.packet)))
        return 0;
    ok = create_header(&transaction, STORE_SCREEN_DETAIL, item->name,
                       string_length(item->name)) &&
         pxa_ui_create(&transaction, NODE_DETAIL_SCROLL, NODE_ROOT, 0,
                       PXA_UI_NODE_SCROLL) &&
         pxa_ui_set_length(&transaction, NODE_DETAIL_SCROLL,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, NODE_DETAIL_SCROLL,
                           PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 0) &&
         pxa_ui_set_u16(&transaction, NODE_DETAIL_SCROLL, PXA_UI_PROPERTY_GROW,
                        1) &&
         pxa_ui_set_u8(&transaction, NODE_DETAIL_SCROLL, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, NODE_DETAIL_SCROLL,
                       PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
         pxa_ui_set_u8(&transaction, NODE_DETAIL_SCROLL,
                       PXA_UI_PROPERTY_SCROLLBAR, 1) &&
         pxa_ui_set_event_mask(&transaction, NODE_DETAIL_SCROLL,
                               PXA_UI_EVENT_MASK_SCROLL) &&
         pxa_ui_set_padding(&transaction, NODE_DETAIL_SCROLL, 12, 8, 12, 8) &&
         pxa_ui_set_dp(&transaction, NODE_DETAIL_SCROLL, PXA_UI_PROPERTY_GAP,
                       8) &&
         pxa_ui_set_dp(&transaction, NODE_DETAIL_SCROLL,
                       PXA_UI_PROPERTY_SCROLL_POSITION, app.detail_scroll);
    if (app.detail_state != STORE_READY || app.install_feedback) ok = ok &&
         pxa_ui_create(&transaction, NODE_DETAIL_STATUS, NODE_DETAIL_SCROLL, 0,
                       PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, NODE_DETAIL_STATUS,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, NODE_DETAIL_STATUS, detail_status_text(),
                         string_length(detail_status_text())) &&
         pxa_ui_set_font_role(&transaction, NODE_DETAIL_STATUS,
                              PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, NODE_DETAIL_STATUS,
                                PXA_UI_PROPERTY_FOREGROUND,
                                detail_status_color());
    /* Icon and release line; the header already carries the display name. */
    {
        char hero_meta[64];
        char size_text[24];
        size_t offset;
        format_size(size_text, sizeof(size_text), item->size);
        hero_meta[0] = '\0';
        offset = append_text(hero_meta, sizeof(hero_meta), 0, "v");
        offset = append_text(hero_meta, sizeof(hero_meta), offset,
                             item->version);
        if (item->size != 0) {
            offset = append_text(hero_meta, sizeof(hero_meta), offset, " · ");
            (void)append_text(hero_meta, sizeof(hero_meta), offset, size_text);
        }
        ok = ok && pxa_ui_create(&transaction, NODE_DETAIL_HERO,
                                 NODE_DETAIL_SCROLL, 0, PXA_UI_NODE_BOX) &&
             pxa_ui_set_length(&transaction, NODE_DETAIL_HERO,
                               PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_u8(&transaction, NODE_DETAIL_HERO,
                           PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
             pxa_ui_set_u8(&transaction, NODE_DETAIL_HERO,
                           PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_padding(&transaction, NODE_DETAIL_HERO, 2, 2, 2, 6) &&
             pxa_ui_set_dp(&transaction, NODE_DETAIL_HERO, PXA_UI_PROPERTY_GAP,
                           12) &&
             create_app_icon(&transaction, NODE_DETAIL_HERO_ICON,
                             NODE_DETAIL_HERO_LETTER, NODE_DETAIL_HERO,
                             m->hero_icon, item->name, item->icon_url) &&
             pxa_ui_create(&transaction, NODE_DETAIL_HERO_INFO,
                           NODE_DETAIL_HERO, 0, PXA_UI_NODE_BOX) &&
             pxa_ui_set_u16(&transaction, NODE_DETAIL_HERO_INFO,
                            PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_u8(&transaction, NODE_DETAIL_HERO_INFO,
                           PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
             pxa_ui_set_dp(&transaction, NODE_DETAIL_HERO_INFO,
                           PXA_UI_PROPERTY_GAP, 3) &&
             pxa_ui_create(&transaction, NODE_DETAIL_HERO_META,
                           NODE_DETAIL_HERO_INFO, 0, PXA_UI_NODE_TEXT) &&
             pxa_ui_set_length(&transaction, NODE_DETAIL_HERO_META,
                               PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_text(&transaction, NODE_DETAIL_HERO_META, hero_meta,
                             string_length(hero_meta)) &&
             pxa_ui_set_font_role(&transaction, NODE_DETAIL_HERO_META,
                                  PXA_UI_FONT_ROLE_CAPTION) &&
             pxa_ui_set_theme_color(&transaction, NODE_DETAIL_HERO_META,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_MUTED);
    }
    ok = ok && create_gallery(&transaction);
    app.scratch[0] = '\0';
    (void)append_text(app.scratch, sizeof(app.scratch), 0, item->summary);
    ok = ok && create_section(&transaction, SECTION_SUMMARY,
                              message(PXA_MSG_DETAIL_SUMMARY), app.scratch, 0);
    if (ok) {
        copy_text(app.scratch, sizeof(app.scratch),
                  item->changelog[0] != '\0' ? item->changelog
                                             : message(PXA_MSG_DETAIL_NONE));
        ok = create_section(&transaction, SECTION_CHANGELOG,
                            message(PXA_MSG_DETAIL_CHANGELOG), app.scratch, 0);
    }
    if (ok) ok = create_library_button(&transaction, NODE_TECHNICAL,
        NODE_TECHNICAL_LABEL, NODE_DETAIL_SCROLL,
        message(app.technical_expanded ? PXA_MSG_DETAIL_LESS : PXA_MSG_DETAIL_TECHNICAL), 1);
    if (app.technical_expanded) {
    store::format_package_bytes(size_text, item->size);
    sequence_text[0] = '\0';
    (void)append_u64(sequence_text, sizeof(sequence_text), 0,
                     item->release_sequence);
    sdk_text[0] = '\0';
    {
        size_t offset = append_text(sdk_text, sizeof(sdk_text), 0,
                                    item->min_sdk);
        offset = append_text(sdk_text, sizeof(sdk_text), offset, " - ");
        (void)append_text(sdk_text, sizeof(sdk_text), offset, item->target_sdk);
    }
    format_short_id(id_text, sizeof(id_text), item->publisher_key_id);
    ok = ok && create_row(&transaction, ROW_VERSION,
                          message(PXA_MSG_DETAIL_VERSION), item->version) &&
         create_row(&transaction, ROW_SIZE, message(PXA_MSG_DETAIL_SIZE),
                    size_text) &&
         create_row(&transaction, ROW_CHANNEL, message(PXA_MSG_DETAIL_CHANNEL),
                    item->channel) &&
         create_row(&transaction, ROW_SEQUENCE,
                    message(PXA_MSG_DETAIL_SEQUENCE), sequence_text) &&
         create_row(&transaction, ROW_PUBLISHER,
                    message(PXA_MSG_DETAIL_PUBLISHER), id_text);
    format_short_id(id_text, sizeof(id_text), item->sha256);
    ok = ok && create_row(&transaction, ROW_DIGEST, message(PXA_MSG_DETAIL_DIGEST),
                          id_text) &&
         create_row(&transaction, ROW_PROFILE, message(PXA_MSG_DETAIL_PROFILE),
                    item->target_profile) &&
         create_row(&transaction, ROW_SDK, message(PXA_MSG_DETAIL_SDK),
                    sdk_text) &&
         create_row(&transaction, ROW_PLATFORMS,
                    message(PXA_MSG_DETAIL_PLATFORMS), item->platforms);
    if (ok) {
        build_permissions_text(app.scratch, sizeof(app.scratch));
        ok = create_section(&transaction, SECTION_PERMISSIONS,
                            message(PXA_MSG_DETAIL_PERMISSIONS), app.scratch,
                            0);
    }
    if (ok) {
        build_services_text(app.scratch, sizeof(app.scratch));
        ok = create_section(&transaction, SECTION_SERVICES,
                            message(PXA_MSG_DETAIL_SERVICES), app.scratch, 0);
    }
    if (ok) {
        build_install_text(app.scratch, sizeof(app.scratch));
        ok = create_section(&transaction, SECTION_INSTALL,
                            message(PXA_MSG_INSTALL_TITLE), app.scratch, 1);
    }
    }
    if (ok) {
        const pxa_store_installed_app_t *current = installed_for(item->app_id);
        const char *label = message(current == NULL ? PXA_MSG_ACTION_INSTALL :
                                    has_update(item) ? PXA_MSG_ACTION_UPDATE :
                                                       PXA_MSG_ACTION_OPEN);
        ok = pxa_ui_create(&transaction, NODE_FOOTER, NODE_ROOT, 0,
                           PXA_UI_NODE_BOX) &&
             pxa_ui_set_length(&transaction, NODE_FOOTER, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_u8(&transaction, NODE_FOOTER, PXA_UI_PROPERTY_LAYOUT,
                           PXA_UI_LAYOUT_COLUMN) &&
             pxa_ui_set_padding(&transaction, NODE_FOOTER, 12, 2, 12, 6) &&
             pxa_ui_set_theme_color(&transaction, NODE_FOOTER,
                                    PXA_UI_PROPERTY_BACKGROUND,
                                    PXA_UI_THEME_BACKGROUND) &&
             pxa_ui_create_typed(&transaction, NODE_INSTALL_BUTTON, NODE_FOOTER,
                                 0, PXA_UI_NODE_CONTROL,
                                 PXA_UI_CONTROL_BUTTON) &&
             pxa_ui_set_length(&transaction, NODE_INSTALL_BUTTON,
                               PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(&transaction, NODE_INSTALL_BUTTON,
                               PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                               (uint16_t)(m->action_height + 8)) &&
             pxa_ui_set_u8(&transaction, NODE_INSTALL_BUTTON,
                           PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
             pxa_ui_set_u8(&transaction, NODE_INSTALL_BUTTON,
                           PXA_UI_PROPERTY_JUSTIFY, PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_u8(&transaction, NODE_INSTALL_BUTTON,
                           PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_dp(&transaction, NODE_INSTALL_BUTTON,
                           PXA_UI_PROPERTY_RADIUS,
                           (m->action_height + 8) / 2) &&
             pxa_ui_set_dp(&transaction, NODE_INSTALL_BUTTON,
                           PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
             pxa_ui_set_theme_color(&transaction, NODE_INSTALL_BUTTON,
                                    PXA_UI_PROPERTY_BORDER_COLOR,
                                    PXA_UI_THEME_PRIMARY) &&
             pxa_ui_set_theme_color(&transaction, NODE_INSTALL_BUTTON,
                                    PXA_UI_PROPERTY_BACKGROUND,
                                    PXA_UI_THEME_PRIMARY) &&
             pxa_ui_set_u8(&transaction, NODE_INSTALL_BUTTON,
                           PXA_UI_PROPERTY_ENABLED, (uint8_t)(!app.busy && !app.downloading)) &&
             pxa_ui_set_event_mask(&transaction, NODE_INSTALL_BUTTON,
                                   PXA_UI_EVENT_MASK_CLICK) &&
             pxa_ui_create(&transaction, NODE_INSTALL_LABEL,
                           NODE_INSTALL_BUTTON, 0, PXA_UI_NODE_TEXT) &&
             pxa_ui_set_text(&transaction, NODE_INSTALL_LABEL, label,
                             string_length(label)) &&
             pxa_ui_set_font_role(&transaction, NODE_INSTALL_LABEL,
                                  PXA_UI_FONT_ROLE_BODY) &&
             pxa_ui_set_theme_color(&transaction, NODE_INSTALL_LABEL,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_ON_PRIMARY);
    }
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    app.bound_hero = store::media_handle(item->icon_url);
    return 1;
}

static int render_viewer(void) {
    if (!app.gallery.count) {
        app.screen = STORE_SCREEN_DETAIL;
        return render_detail();
    }
    const auto *m = metrics();
    char title[64], zoom[24];
    std::snprintf(title, sizeof(title), "%s %u / %u", message(PXA_MSG_MEDIA_SCREENSHOTS),
                  app.screenshot + 1, app.gallery.count);
    if (app.viewer_zoom == 1) copy_text(zoom, sizeof(zoom), message(PXA_MSG_MEDIA_ZOOM_FIT));
    else std::snprintf(zoom, sizeof(zoom), "%u×", app.viewer_zoom);
    pxa_ui_transaction_t tx{};
    const auto next = app.generation + 1;
    if (!next || !pxa_ui_transaction_begin(&tx, next, PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                           app.packet, sizeof(app.packet))) return 0;
    int ok = create_header(&tx, STORE_SCREEN_DETAIL, title, string_length(title)) &&
        pxa_ui_create(&tx, NODE_VIEWPORT, NODE_ROOT, 0, PXA_UI_NODE_SCROLL) &&
        pxa_ui_set_length(&tx, NODE_VIEWPORT, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
        pxa_ui_set_length(&tx, NODE_VIEWPORT, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 0) &&
        pxa_ui_set_u16(&tx, NODE_VIEWPORT, PXA_UI_PROPERTY_GROW, 1) &&
        pxa_ui_set_u8(&tx, NODE_VIEWPORT, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
        pxa_ui_set_u8(&tx, NODE_VIEWPORT, PXA_UI_PROPERTY_SCROLL_AXIS, app.viewer_zoom > 1 ? 3 : 0) &&
        pxa_ui_set_u8(&tx, NODE_VIEWPORT, PXA_UI_PROPERTY_SCROLLBAR, 0) &&
        pxa_ui_set_padding(&tx, NODE_VIEWPORT, m->list_pad_h, 0, m->list_pad_h, 0) &&
        pxa_ui_set_event_mask(&tx, NODE_VIEWPORT, PXA_UI_EVENT_MASK_POINTER) &&
        create_viewer_content(&tx) &&
        pxa_ui_create(&tx, NODE_VIEWER_CONTROLS, NODE_ROOT, 0, PXA_UI_NODE_BOX) &&
        pxa_ui_set_length(&tx, NODE_VIEWER_CONTROLS, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
        pxa_ui_set_length(&tx, NODE_VIEWER_CONTROLS, PXA_UI_PROPERTY_HEIGHT,
                          PXA_UI_LENGTH_PX, std::max(36u, unsigned(m->action_height)) + 12u) &&
        pxa_ui_set_padding(&tx, NODE_VIEWER_CONTROLS, m->list_pad_h, 6, m->list_pad_h, 6) &&
        pxa_ui_set_u8(&tx, NODE_VIEWER_CONTROLS, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
        pxa_ui_set_dp(&tx, NODE_VIEWER_CONTROLS, PXA_UI_PROPERTY_GAP, 6);
    if (app.gallery.count > 1) ok = ok &&
        create_library_button(&tx, NODE_VIEWER_PREV, NODE_VIEWER_PREV_LABEL, NODE_VIEWER_CONTROLS, "‹", 1) &&
        pxa_ui_set_length(&tx, NODE_VIEWER_PREV, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 36) &&
        pxa_ui_set_u8(&tx, NODE_VIEWER_PREV, PXA_UI_PROPERTY_ENABLED, app.screenshot > 0);
    ok = ok && create_library_button(&tx, NODE_VIEWER_ZOOM, NODE_VIEWER_ZOOM_LABEL,
                                     NODE_VIEWER_CONTROLS, zoom, 1) &&
        pxa_ui_set_u16(&tx, NODE_VIEWER_ZOOM, PXA_UI_PROPERTY_GROW, 1) &&
        pxa_ui_set_property(&tx, NODE_VIEWER_ZOOM, PXA_UI_PROPERTY_ACCESSIBILITY_LABEL,
            message(PXA_MSG_MEDIA_ZOOM), string_length(message(PXA_MSG_MEDIA_ZOOM)));
    if (app.gallery.count > 1) ok = ok &&
        create_library_button(&tx, NODE_VIEWER_NEXT, NODE_VIEWER_NEXT_LABEL, NODE_VIEWER_CONTROLS, "›", 1) &&
        pxa_ui_set_length(&tx, NODE_VIEWER_NEXT, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 36) &&
        pxa_ui_set_u8(&tx, NODE_VIEWER_NEXT, PXA_UI_PROPERTY_ENABLED, app.screenshot + 1 < app.gallery.count);
    for (const auto node : {NODE_VIEWER_PREV, NODE_VIEWER_ZOOM, NODE_VIEWER_NEXT}) {
        if (node != NODE_VIEWER_ZOOM && app.gallery.count == 1) continue;
        ok = ok && pxa_ui_set_length(&tx, node, PXA_UI_PROPERTY_HEIGHT,
            PXA_UI_LENGTH_PX, std::max(36u, unsigned(m->action_height)));
    }
    if (!ok || !pxa_ui_transaction_commit(&tx)) {
        if (tx.active) (void)pxa_ui_transaction_cancel(&tx);
        return 0;
    }
    app.generation = next;
    return 1;
}

// Image completion only replaces the affected icon/preview subtree. It must
// never rebuild the list or disturb a drag, focus or scroll offset.
static int patch_media(void) {
    auto replace_icon = [](uint32_t box, uint32_t letter, uint32_t parent,
                           int size, const store_app_t *item) {
        pxa_ui_transaction_t tx{};
        const uint32_t next = app.generation + 1;
        if (!next || !pxa_ui_transaction_begin_target(&tx, next, next,
            PXA_UI_PRIMARY_SURFACE, box, PXA_UI_TRANSACTION_REPLACE_SUBTREE,
            app.packet, sizeof(app.packet))) return 0;
        if (!create_app_icon(&tx, box, letter, parent, size, item->name, item->icon_url) ||
            !pxa_ui_transaction_commit(&tx)) {
            if (tx.active) (void)pxa_ui_transaction_cancel(&tx);
            return 0;
        }
        app.generation = next;
        return 1;
    };
    if (app.screen == STORE_SCREEN_CATALOG) {
        for (uint8_t i = 0; i < app.catalog.count; ++i) {
            const auto *item = store_catalog_at_const(&app.catalog, i);
            const auto handle = store::media_handle(item->icon_url);
            if (handle == app.bound_icons[i]) continue;
            uint32_t card = NODE_CARD_BASE + i * NODE_CARD_STRIDE;
            if (!replace_icon(card + 1, card + 2, card, metrics()->card_icon, item)) return 0;
            app.bound_icons[i] = handle;
        }
    } else if (app.screen == STORE_SCREEN_VIEWER && app.gallery.count) {
        pxa_ui_transaction_t tx{};
        const auto next = app.generation + 1;
        if (!next || !pxa_ui_transaction_begin_target(&tx, next, next,
            PXA_UI_PRIMARY_SURFACE, NODE_VIEWER_CONTENT, PXA_UI_TRANSACTION_REPLACE_SUBTREE,
            app.packet, sizeof(app.packet))) return 0;
        if (!create_viewer_content(&tx) || !pxa_ui_transaction_commit(&tx)) {
            if (tx.active) (void)pxa_ui_transaction_cancel(&tx);
            return 0;
        }
        app.generation = next;
    } else if (app.screen == STORE_SCREEN_DETAIL) {
        const auto *item = detail_item();
        const auto handle = store::media_handle(item->icon_url);
        if (handle != app.bound_hero) {
            if (!replace_icon(NODE_DETAIL_HERO_ICON, NODE_DETAIL_HERO_LETTER,
                NODE_DETAIL_HERO, metrics()->hero_icon, item)) return 0;
            app.bound_hero = handle;
        }
        if (app.gallery.count) {
            pxa_ui_transaction_t tx{};
            const uint32_t next = app.generation + 1;
            if (!next || !pxa_ui_transaction_begin_target(&tx, next, next,
                PXA_UI_PRIMARY_SURFACE, NODE_GALLERY, PXA_UI_TRANSACTION_REPLACE_SUBTREE,
                app.packet, sizeof(app.packet))) return 0;
            if (!create_gallery(&tx) || !pxa_ui_transaction_commit(&tx)) {
                if (tx.active) (void)pxa_ui_transaction_cancel(&tx);
                return 0;
            }
            app.generation = next;
        }
    }
    return 1;
}

/* Rows of the compact on-screen keyboard: 7 keys per row, 28 keys total. */
static const char *keyboard_labels[4] = {
    "abcdefg",
    "hijklmn",
    "opqrstu",
    "vwxyz..",
};

static const char *keyboard_key_text(uint8_t key, char *scratch,
                                     size_t capacity) {
    if (key < 26u) {
        scratch[0] = keyboard_labels[key / 7u][key % 7u];
        scratch[1] = '\0';
        return scratch;
    }
    if (key == 26u) return message(PXA_MSG_KEY_DELETE);
    return message(PXA_MSG_ACTION_SEARCH);
}

static int create_key(pxa_ui_transaction_t *transaction, uint32_t parent,
                      uint8_t key) {
    const store_metrics_t *m = metrics();
    uint32_t box = NODE_KEY_BASE + (uint32_t)key * NODE_KEY_STRIDE;
    uint32_t label = box + 1u;
    char scratch[8];
    const char *text = keyboard_key_text(key, scratch, sizeof(scratch));
    uint8_t accent = (uint8_t)(key == 27u);
    return pxa_ui_create_typed(transaction, box, parent, 0,
                               PXA_UI_NODE_CONTROL,
                               PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_length(transaction, box, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_u16(transaction, box, PXA_UI_PROPERTY_GROW, 1) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, box, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_dp(transaction, box, PXA_UI_PROPERTY_RADIUS, 8) &&
           pxa_ui_set_theme_color(transaction, box, PXA_UI_PROPERTY_BACKGROUND,
                                  accent ? PXA_UI_THEME_PRIMARY
                                         : PXA_UI_THEME_SURFACE) &&
           pxa_ui_set_event_mask(transaction, box, PXA_UI_EVENT_MASK_CLICK) &&
           pxa_ui_create(transaction, label, box, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, label, text, string_length(text)) &&
           pxa_ui_set_font_role(transaction, label,
                                key < 26u ? PXA_UI_FONT_ROLE_BODY
                                          : PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, label,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  accent ? PXA_UI_THEME_ON_PRIMARY
                                         : PXA_UI_THEME_TEXT);
}

static int create_search_clear(pxa_ui_transaction_t *transaction) {
    const char *label = message(PXA_MSG_ACTION_CLEAR);
    return pxa_ui_create_typed(transaction, NODE_SEARCH_CLEAR, NODE_SEARCH_BAR,
                               0, PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_length(transaction, NODE_SEARCH_CLEAR,
                             PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 28) &&
           pxa_ui_set_length(transaction, NODE_SEARCH_CLEAR,
                             PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 28) &&
           pxa_ui_set_u8(transaction, NODE_SEARCH_CLEAR,
                         PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, NODE_SEARCH_CLEAR,
                         PXA_UI_PROPERTY_JUSTIFY, PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, NODE_SEARCH_CLEAR,
                         PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_dp(transaction, NODE_SEARCH_CLEAR,
                         PXA_UI_PROPERTY_RADIUS, 14) &&
           pxa_ui_set_dp(transaction, NODE_SEARCH_CLEAR,
                         PXA_UI_PROPERTY_BORDER_WIDTH, 0) &&
           pxa_ui_set_theme_color(transaction, NODE_SEARCH_CLEAR,
                                  PXA_UI_PROPERTY_BACKGROUND,
                                  PXA_UI_THEME_BACKGROUND) &&
           pxa_ui_set_u8(transaction, NODE_SEARCH_CLEAR,
                         PXA_UI_PROPERTY_VISIBLE,
                         (uint8_t)(app.draft[0] != '\0')) &&
           pxa_ui_set_event_mask(transaction, NODE_SEARCH_CLEAR,
                                 PXA_UI_EVENT_MASK_CLICK) &&
           pxa_ui_set_property(transaction, NODE_SEARCH_CLEAR,
                               PXA_UI_PROPERTY_ACCESSIBILITY_LABEL,
                               label, string_length(label)) &&
           pxa_ui_create(transaction, NODE_SEARCH_CLEAR_LABEL,
                         NODE_SEARCH_CLEAR, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, NODE_SEARCH_CLEAR_LABEL,
                           "×", sizeof("×") - 1u) &&
           pxa_ui_set_font_role(transaction, NODE_SEARCH_CLEAR_LABEL,
                                PXA_UI_FONT_ROLE_TITLE) &&
           pxa_ui_set_theme_color(transaction, NODE_SEARCH_CLEAR_LABEL,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED);
}

static int render_search(void) {
    const store_metrics_t *m = metrics();
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = app.generation + 1u;
    uint16_t bar_height = m->search_height + 4u;
    uint16_t left_padding = effective_inset(3);
    uint16_t right_padding = effective_inset(1);
    if (bar_height > 46u) bar_height = 46u;
    if (app.display_shape == PXA_UI_DISPLAY_SHAPE_ROUNDED_RECTANGLE) {
        uint16_t left_offset = (uint16_t)(store::layout::pixels_to_dp(
            app.corner_radii[0], app.density_q16) / 8u);
        uint16_t right_offset = (uint16_t)(store::layout::pixels_to_dp(
            app.corner_radii[1], app.density_q16) / 8u);
        if (left_offset > 8u) left_offset = 8u;
        if (right_offset > 8u) right_offset = 8u;
        left_padding = left_padding > left_offset ?
            left_padding - left_offset : 0u;
        right_padding += right_offset;
    }
    if (left_padding < m->list_pad_h) left_padding = m->list_pad_h;
    if (right_padding < m->list_pad_h) right_padding = m->list_pad_h;
    uint8_t row;
    int ok;
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                  app.packet, sizeof(app.packet)))
        return 0;
    ok = pxa_ui_create(&transaction, NODE_ROOT, 0, 0, PXA_UI_NODE_ROOT) &&
         pxa_ui_set_u8(&transaction, NODE_ROOT, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, NODE_ROOT, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, NODE_ROOT,
                                PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_set_padding(&transaction, NODE_ROOT,
                            left_padding,
                            inset_padding(0, m->header_pad_v),
                            right_padding,
                            inset_padding(2, 6)) &&
         pxa_ui_create(&transaction, NODE_HEADER, NODE_ROOT, 0,
                       PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, NODE_HEADER, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, NODE_HEADER,
                           PXA_UI_PROPERTY_MAX_WIDTH, PXA_UI_LENGTH_PX, 520) &&
         pxa_ui_set_u8(&transaction, NODE_HEADER, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, NODE_HEADER, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, NODE_HEADER, PXA_UI_PROPERTY_GAP, 2) &&
         pxa_ui_set_theme_color(&transaction, NODE_HEADER,
                                PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create_typed(&transaction, NODE_SEARCH_BACK, NODE_HEADER, 0,
                             PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_BACK,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 32) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_BACK,
                           PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                           bar_height) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_BACK, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_BACK, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_BACK, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, NODE_SEARCH_BACK, 0, 0, 0, 0) &&
         pxa_ui_set_dp(&transaction, NODE_SEARCH_BACK, PXA_UI_PROPERTY_RADIUS,
                       m->card_radius) &&
         pxa_ui_set_dp(&transaction, NODE_SEARCH_BACK,
                       PXA_UI_PROPERTY_BORDER_WIDTH, 0) &&
         pxa_ui_set_rgba(&transaction, NODE_SEARCH_BACK,
                          PXA_UI_PROPERTY_BACKGROUND, 0) &&
         pxa_ui_set_event_mask(&transaction, NODE_SEARCH_BACK,
                               PXA_UI_EVENT_MASK_CLICK) &&
         pxa_ui_set_property(&transaction, NODE_SEARCH_BACK,
                             PXA_UI_PROPERTY_ACCESSIBILITY_LABEL,
                             message(PXA_MSG_ACTION_BACK),
                             string_length(message(PXA_MSG_ACTION_BACK))) &&
         pxa_ui_create(&transaction, NODE_SEARCH_BACK_LABEL, NODE_SEARCH_BACK, 0,
                       PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, NODE_SEARCH_BACK_LABEL,
                         ICON_BACK, string_length(ICON_BACK)) &&
         pxa_ui_set_font_role(&transaction, NODE_SEARCH_BACK_LABEL,
                              PXA_UI_FONT_ROLE_ICON) &&
         pxa_ui_set_theme_color(&transaction, NODE_SEARCH_BACK_LABEL,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, NODE_SEARCH_BAR, NODE_HEADER, 0,
                       PXA_UI_NODE_BOX) &&
         pxa_ui_set_u16(&transaction, NODE_SEARCH_BAR, PXA_UI_PROPERTY_GROW,
                        1) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_BAR,
                           PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                           bar_height) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_BAR, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_BAR, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, NODE_SEARCH_BAR, 12, 0, 6, 0) &&
         pxa_ui_set_dp(&transaction, NODE_SEARCH_BAR, PXA_UI_PROPERTY_GAP, 6) &&
         pxa_ui_set_dp(&transaction, NODE_SEARCH_BAR, PXA_UI_PROPERTY_RADIUS,
                       14) &&
         pxa_ui_set_dp(&transaction, NODE_SEARCH_BAR,
                       PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
         pxa_ui_set_theme_color(&transaction, NODE_SEARCH_BAR,
                                PXA_UI_PROPERTY_BORDER_COLOR,
                                PXA_UI_THEME_BORDER) &&
         pxa_ui_set_theme_color(&transaction, NODE_SEARCH_BAR,
                                PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE_CONTAINER_HIGH) &&
         pxa_ui_create_typed(&transaction, NODE_SEARCH_SUBMIT, NODE_SEARCH_BAR, 0,
                             PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_SUBMIT,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 36) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_SUBMIT,
                           PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, bar_height - 2u) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_SUBMIT, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_SUBMIT, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_SUBMIT, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, NODE_SEARCH_SUBMIT, 0, 0, 0, 0) &&
         pxa_ui_set_dp(&transaction, NODE_SEARCH_SUBMIT,
                       PXA_UI_PROPERTY_BORDER_WIDTH, 0) &&
         pxa_ui_set_rgba(&transaction, NODE_SEARCH_SUBMIT,
                         PXA_UI_PROPERTY_BACKGROUND, 0) &&
         pxa_ui_set_event_mask(&transaction, NODE_SEARCH_SUBMIT,
                               PXA_UI_EVENT_MASK_CLICK) &&
         pxa_ui_set_property(&transaction, NODE_SEARCH_SUBMIT,
                             PXA_UI_PROPERTY_ACCESSIBILITY_LABEL,
                             (const uint8_t *)message(PXA_MSG_ACTION_SEARCH),
                             string_length(message(PXA_MSG_ACTION_SEARCH))) &&
         pxa_ui_create(&transaction, NODE_SEARCH_ICON, NODE_SEARCH_SUBMIT, 0,
                       PXA_UI_NODE_IMAGE) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_ICON,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 20) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_ICON,
                           PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 20) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_ICON,
                        PXA_UI_PROPERTY_IMAGE_FIT,
                        PXA_UI_IMAGE_FIT_CONTAIN) &&
         store_search_image_set(&transaction, NODE_SEARCH_ICON) &&
         pxa_ui_set_theme_color(&transaction, NODE_SEARCH_ICON,
                                 PXA_UI_PROPERTY_FOREGROUND,
                                 PXA_UI_THEME_PRIMARY);
    if (!ok) goto failed;
    /* Search field. */
    {
        const char *text = app.draft[0] != '\0'
                               ? app.draft
                               : message(PXA_MSG_SEARCH_PLACEHOLDER);
        uint8_t empty = (uint8_t)(app.draft[0] == '\0');
        (void)text;
        (void)empty;
#if STORE_SEARCH_TEXT_INPUT
        /* The real text input keeps the system input method and text events. */
        ok = pxa_ui_create_typed(&transaction, NODE_SEARCH_FIELD,
                                 NODE_SEARCH_BAR, 0,
                                 PXA_UI_NODE_CONTROL,
                                 PXA_UI_CONTROL_TEXT_INPUT) &&
             pxa_ui_set_u16(&transaction, NODE_SEARCH_FIELD,
                            PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_length(&transaction, NODE_SEARCH_FIELD,
                               PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                               bar_height - 2u) &&
             pxa_ui_set_padding(&transaction, NODE_SEARCH_FIELD, 2, 4, 2, 4) &&
             pxa_ui_set_dp(&transaction, NODE_SEARCH_FIELD,
                           PXA_UI_PROPERTY_RADIUS, 0) &&
             pxa_ui_set_dp(&transaction, NODE_SEARCH_FIELD,
                           PXA_UI_PROPERTY_BORDER_WIDTH, 0) &&
             pxa_ui_set_rgba(&transaction, NODE_SEARCH_FIELD,
                              PXA_UI_PROPERTY_BACKGROUND, 0) &&
             pxa_ui_set_event_mask(&transaction, NODE_SEARCH_FIELD,
                                   PXA_UI_EVENT_MASK_CLICK |
                                       PXA_UI_EVENT_MASK_TEXT) &&
             pxa_ui_set_u8(&transaction, NODE_SEARCH_FIELD,
                            pxa::ui::protocol::text_single_line, 1) &&
             pxa_ui_set_u32(&transaction, NODE_SEARCH_FIELD,
                             pxa::ui::protocol::text_max_bytes, STORE_MAX_QUERY - 1u) &&
             pxa_ui_set_text(&transaction, NODE_SEARCH_FIELD, app.draft,
                             string_length(app.draft)) &&
             pxa_ui_create(&transaction, NODE_SEARCH_TEXT, NODE_SEARCH_FIELD,
                           0, PXA_UI_NODE_TEXT) &&
             pxa_ui_set_u8(&transaction, NODE_SEARCH_TEXT,
                           PXA_UI_PROPERTY_POSITION, 1) &&
             pxa_ui_set_length(&transaction, NODE_SEARCH_TEXT,
                               PXA_UI_PROPERTY_X, PXA_UI_LENGTH_PX, 2) &&
             pxa_ui_set_length(&transaction, NODE_SEARCH_TEXT,
                               PXA_UI_PROPERTY_Y, PXA_UI_LENGTH_PX,
                               m == &metrics_compact ? 3u : 9u) &&
             pxa_ui_set_text(&transaction, NODE_SEARCH_TEXT,
                             empty ? message(PXA_MSG_SEARCH_PLACEHOLDER) : "",
                             empty ? pxa_i18n_size(
                                 &app.i18n, PXA_MSG_SEARCH_PLACEHOLDER) : 0) &&
             pxa_ui_set_font_role(&transaction, NODE_SEARCH_TEXT,
                                  PXA_UI_FONT_ROLE_LABEL) &&
             pxa_ui_set_theme_color(&transaction, NODE_SEARCH_TEXT,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_MUTED);
#else
        ok = pxa_ui_create_typed(&transaction, NODE_SEARCH_FIELD,
                                 NODE_SEARCH_BAR, 0,
                                 PXA_UI_NODE_CONTROL,
                                 PXA_UI_CONTROL_BUTTON) &&
             pxa_ui_set_u16(&transaction, NODE_SEARCH_FIELD,
                            PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_length(&transaction, NODE_SEARCH_FIELD,
                               PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                               bar_height - 2u) &&
             pxa_ui_set_u8(&transaction, NODE_SEARCH_FIELD,
                           PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
             pxa_ui_set_u8(&transaction, NODE_SEARCH_FIELD,
                           PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_padding(&transaction, NODE_SEARCH_FIELD, 2, 4, 2, 4) &&
             pxa_ui_set_dp(&transaction, NODE_SEARCH_FIELD,
                           PXA_UI_PROPERTY_RADIUS, 0) &&
             pxa_ui_set_dp(&transaction, NODE_SEARCH_FIELD,
                           PXA_UI_PROPERTY_BORDER_WIDTH, 0) &&
             pxa_ui_set_rgba(&transaction, NODE_SEARCH_FIELD,
                              PXA_UI_PROPERTY_BACKGROUND, 0) &&
             pxa_ui_set_event_mask(&transaction, NODE_SEARCH_FIELD,
                                   PXA_UI_EVENT_MASK_CLICK) &&
             pxa_ui_create(&transaction, NODE_SEARCH_TEXT, NODE_SEARCH_FIELD, 0,
                           PXA_UI_NODE_TEXT) &&
             pxa_ui_set_u16(&transaction, NODE_SEARCH_TEXT,
                            PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_text(&transaction, NODE_SEARCH_TEXT, text,
                             string_length(text)) &&
             pxa_ui_set_theme_color(&transaction, NODE_SEARCH_TEXT,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    empty ? PXA_UI_THEME_MUTED
                                          : PXA_UI_THEME_TEXT);
#endif
    }
    if (!ok || !create_search_clear(&transaction)) goto failed;
#if STORE_SEARCH_TEXT_INPUT
    ok = pxa_ui_create(&transaction, NODE_SEARCH_EMPTY, NODE_ROOT, 0,
                       PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_EMPTY,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_EMPTY,
                       PXA_UI_PROPERTY_VISIBLE,
                       (uint8_t)(app.draft[0] == '\0')) &&
         pxa_ui_set_u16(&transaction, NODE_SEARCH_EMPTY, PXA_UI_PROPERTY_GROW,
                        1) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_EMPTY, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_EMPTY, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_EMPTY, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, NODE_SEARCH_EMPTY, PXA_UI_PROPERTY_GAP,
                       10) &&
         pxa_ui_create(&transaction, NODE_SEARCH_EMPTY_ICON,
                       NODE_SEARCH_EMPTY, 0, PXA_UI_NODE_IMAGE) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_EMPTY_ICON,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 36) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_EMPTY_ICON,
                           PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 36) &&
         store_search_image_set(&transaction, NODE_SEARCH_EMPTY_ICON) &&
         pxa_ui_set_theme_color(&transaction, NODE_SEARCH_EMPTY_ICON,
                                 PXA_UI_PROPERTY_FOREGROUND,
                                 PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, NODE_SEARCH_EMPTY_TEXT,
                       NODE_SEARCH_EMPTY, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, NODE_SEARCH_EMPTY_TEXT,
                         message(PXA_MSG_SEARCH_PLACEHOLDER),
                         pxa_i18n_size(&app.i18n,
                                       PXA_MSG_SEARCH_PLACEHOLDER)) &&
         pxa_ui_set_font_role(&transaction, NODE_SEARCH_EMPTY_TEXT,
                              PXA_UI_FONT_ROLE_LABEL) &&
         pxa_ui_set_theme_color(&transaction, NODE_SEARCH_EMPTY_TEXT,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED);
    if (!ok) goto failed;
#endif
#if !STORE_SEARCH_TEXT_INPUT
    ok = pxa_ui_create(&transaction, NODE_SEARCH_KEYBOARD, NODE_ROOT, 0,
                       PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, NODE_SEARCH_KEYBOARD,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u16(&transaction, NODE_SEARCH_KEYBOARD,
                        PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, NODE_SEARCH_KEYBOARD,
                       PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_padding(&transaction, NODE_SEARCH_KEYBOARD, m->list_pad_h, 4,
                       m->list_pad_h, 8) &&
         pxa_ui_set_dp(&transaction, NODE_SEARCH_KEYBOARD, PXA_UI_PROPERTY_GAP,
                       m->key_gap);
    for (row = 0; ok && row < 4u; ++row) {
        uint32_t row_node = NODE_SEARCH_ROW_BASE + row;
        uint8_t column;
        ok = pxa_ui_create(&transaction, row_node, NODE_SEARCH_KEYBOARD, 0,
                           PXA_UI_NODE_BOX) &&
             pxa_ui_set_length(&transaction, row_node, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(&transaction, row_node, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, 0) &&
             pxa_ui_set_u16(&transaction, row_node, PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_u8(&transaction, row_node, PXA_UI_PROPERTY_LAYOUT,
                           PXA_UI_LAYOUT_ROW) &&
             pxa_ui_set_dp(&transaction, row_node, PXA_UI_PROPERTY_GAP,
                       m->key_gap);
        for (column = 0; ok && column < 7u; ++column) {
            uint8_t key = (uint8_t)(row * 7u + column);
            if (key == 27u && column != 6u) {
                /* only the last cell of the last row holds the search key */
                ok = pxa_ui_create(&transaction, NODE_KEY_BASE +
                                   (uint32_t)key * NODE_KEY_STRIDE, row_node, 0,
                                   PXA_UI_NODE_BOX) &&
                     pxa_ui_set_u16(&transaction, NODE_KEY_BASE +
                                    (uint32_t)key * NODE_KEY_STRIDE,
                                    PXA_UI_PROPERTY_GROW, 1);
                continue;
            }
            ok = create_key(&transaction, row_node, key);
        }
    }
#endif /* !STORE_SEARCH_TEXT_INPUT */
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
failed:
        store_trace("search build ok", ok ? 1u : 0u);
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    return 1;
}

#if STORE_SEARCH_TEXT_INPUT
static int sync_search_decoration(uint8_t update_field) {
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = app.generation + 1u;
    uint8_t empty = (uint8_t)(app.draft[0] == '\0');
    const char *hint = message(PXA_MSG_SEARCH_PLACEHOLDER);
    if (app.screen != STORE_SCREEN_SEARCH || next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_PATCH, app.packet,
                                  sizeof(app.packet)))
        return 0;
    if ((update_field && !pxa_ui_set_text(&transaction, NODE_SEARCH_FIELD,
                                          app.draft, string_length(app.draft))) ||
        !pxa_ui_set_text(&transaction, NODE_SEARCH_TEXT,
                         empty ? hint : "",
                         empty ? string_length(hint) : 0) ||
        !pxa_ui_set_u8(&transaction, NODE_SEARCH_CLEAR,
                        PXA_UI_PROPERTY_VISIBLE, !empty) ||
        !pxa_ui_set_u8(&transaction, NODE_SEARCH_EMPTY,
                        PXA_UI_PROPERTY_VISIBLE, empty) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    return 1;
}

static int patch_search_images(void) {
    pxa_ui_transaction_t transaction{};
    const auto next = app.generation + 1u;
    if (!next || !pxa_ui_transaction_begin(&transaction, next,
            PXA_UI_TRANSACTION_PATCH, app.packet, sizeof(app.packet))) return 0;
    if (!store_search_image_set(&transaction, NODE_SEARCH_ICON) ||
        !store_search_image_set(&transaction, NODE_SEARCH_EMPTY_ICON) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    return 1;
}

/* Copies the text the system input method reported into the draft query. */
static void adopt_field_text(const char *text, size_t length) {
    size_t copy = length < sizeof(app.draft) ? length : sizeof(app.draft) - 1u;
    size_t index;
    for (index = 0; index < copy; ++index) app.draft[index] = text[index];
    app.draft[copy] = '\0';
}
#endif

static int render(void) {
    sync_media();
    if (app.screen == STORE_SCREEN_SEARCH) return render_search();
    if (app.screen == STORE_SCREEN_DETAIL) return render_detail();
    if (app.screen == STORE_SCREEN_VIEWER) return render_viewer();
    if (app.screen == STORE_SCREEN_INSTALLED || app.screen == STORE_SCREEN_DOWNLOADS)
        return render_library();
    if (app.screen == STORE_SCREEN_SUCCESS) return render_success();
    return render_catalog();
}

/* ------------------------------------------------------------------ */
/* Catalog requests                                                   */
/* ------------------------------------------------------------------ */

#define STORE_DRAG_DELTA 6

/* LVGL scrolls a newly focused card into view after the surface layout, which
 * would leave the list scrolled on entry. Re-apply the tracked offset with a
 * patch transaction so the commit's scroll restore wins. */
static int restore_scroll(uint32_t node, int32_t offset) {
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = app.generation + 1u;
    int ok;
    if (next == 0 ||
        !pxa_ui_transaction_begin_target(&transaction, next, next,
                                         PXA_UI_PRIMARY_SURFACE, 0,
                                         PXA_UI_TRANSACTION_PATCH, app.packet,
                                         sizeof(app.packet)))
        return 0;
    ok = pxa_ui_set_dp(&transaction, node, PXA_UI_PROPERTY_SCROLL_POSITION,
                       offset);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    app.generation = next;
    return 1;
}

static int close_viewer(void) {
    app.viewer_gesture.cancel();
    app.screen = STORE_SCREEN_DETAIL;
    return render() && restore_scroll(NODE_DETAIL_SCROLL, app.detail_scroll);
}

static int viewer_page(int delta) {
    const int selected = int(app.screenshot) + delta;
    if (selected < 0 || selected >= app.gallery.count) return 1;
    app.screenshot = uint8_t(selected);
    app.viewer_zoom = 1;
    app.viewer_gesture.cancel();
    return render();
}

static int restart_catalog(const char *query);

static int restart_pending_catalog(void) {
    const char *query = app.pending_search_valid ? app.pending_search : app.query;
    app.pending_search_valid = 0;
    app.pending_restart = 0;
    return restart_catalog(query);
}

static int fail_request(uint8_t error) {
    store_trace("catalog failure", error);
    (void)pxa_clock_set_period(0);
    if (app.client.body_handle != 0)
        (void)store_client_close_body(&app.client);
    app.state = STORE_FAILED;
    app.error = error;
    app.busy = 0;
    if (app.pending_restart || app.pending_search_valid)
        return restart_pending_catalog();
    toast_current_error();
    return app.screen == STORE_SCREEN_SEARCH || render();
}

static int ensure_body_capacity(void) {
    if (app.body_limit == 0) app.body_limit = 4096u;
    if (app.body_capacity >= app.body_limit) return 1;
    auto *buffer = static_cast<uint8_t *>(std::realloc(app.body, app.body_limit));
    if (!buffer) return 0;
    app.body = buffer;
    app.body_capacity = app.body_limit;
    return 1;
}

static int retry_larger_body(void) {
    if (app.body_limit >= PXA_NET_MAX_RESPONSE_BODY_BYTES) return 0;
    if (app.client.body_handle != 0)
        (void)store_client_close_body(&app.client);
    app.body_limit *= 2u;
    if (app.body_limit > PXA_NET_MAX_RESPONSE_BODY_BYTES)
        app.body_limit = PXA_NET_MAX_RESPONSE_BODY_BYTES;
    if (!ensure_body_capacity()) return 0;
    return store_client_fetch(&app.client, app.pending_request, app.url,
                              string_length(app.url), app.body_limit,
                              app.packet, sizeof(app.packet));
}

static int start_catalog_fetch(void) {
    store_trace("catalog fetch cursor", (uint32_t)app.request_cursor);
    app.busy = 1;
    app.state = STORE_LOADING;
    app.pending_request = STORE_REQUEST_CATALOG;
    if (!store_client_build_catalog_url(app.url, sizeof(app.url),
                                        app.profile,
                                        app.device_id, app.request_cursor,
                                        app.query, selected_kind_value(),
                                        selected_category_value(),
                                        catalog_page_size()))
        return 0;
    // Include taxonomy and UTF-8 summaries when budgeting the first page;
    // avoid fetching the same page twice merely to discover its byte size.
    const uint32_t estimate = uint32_t(catalog_page_size()) * 1536u + 2048u;
    if (!app.body_limit) app.body_limit = 4096;
    while (app.body_limit < estimate && app.body_limit < PXA_NET_MAX_RESPONSE_BODY_BYTES)
        app.body_limit *= 2;
    if (!ensure_body_capacity()) return 0;
    return store_client_fetch(
        &app.client, STORE_REQUEST_CATALOG, app.url, string_length(app.url),
        app.body_limit, app.packet, sizeof(app.packet));
}

/* Fetches the full metadata for one catalog entry. The compact catalog page
 * omits compatibility detail, so the detail screen requests it separately. */
static int start_detail_fetch(void) {
    const store_app_t *item = detail_item();
    store_trace("detail fetch index", app.selected);
    app.detail_target = app.detail_revision;
    app.detail_ready = 0;
    app.detail_state = STORE_LOADING;
    if (!app.client.has_network_permission) {
        app.detail_state = STORE_FAILED;
        return 0;
    }
    if (!store_client_build_app_url(app.url, sizeof(app.url), app.profile, item->app_id,
                                    app.device_id))
        return 0;
    app.pending_request = STORE_REQUEST_DETAIL;
    if (!ensure_body_capacity()) return 0;
    if (!store_client_fetch(
            &app.client, STORE_REQUEST_DETAIL, app.url, string_length(app.url),
            app.body_limit, app.packet, sizeof(app.packet)))
        return 0;
    app.busy = 1;
    return 1;
}

static int next_update_check(void) {
    while (app.update_index < app.installed_count) {
        const char *app_id = app.installed[app.update_index].app_id;
        if (store_client_build_app_url(app.url, sizeof(app.url), app.profile, app_id,
                                       app.device_id) &&
            ensure_body_capacity() &&
            store_client_fetch(&app.client, STORE_REQUEST_UPDATE_CHECK,
                               app.url, string_length(app.url),
                               app.body_limit, app.packet,
                               sizeof(app.packet))) {
            app.pending_request = STORE_REQUEST_UPDATE_CHECK;
            app.busy = 1;
            if (app.update_rendered) return 1;
            app.update_rendered = 1;
            return render();
        }
        app.update_failed = 1;
        ++app.update_index;
    }
    app.update_checking = 0;
    app.update_checked = 1;
    app.busy = 0;
    app.pending_request = 0;
    if (app.pending_restart) {
        app.pending_restart = 0;
        return restart_catalog(app.query);
    }
    return render();
}

static int skip_update_check(int failed) {
    (void)pxa_clock_set_period(0);
    if (app.client.body_handle != 0 &&
        !store_client_close_body(&app.client))
        failed = 1;
    if (failed) app.update_failed = 1;
    ++app.update_index;
    return next_update_check();
}

static int finish_update_check(int failed) {
    (void)pxa_clock_set_period(0);
    if (app.client.body_handle != 0 &&
        !store_client_close_body(&app.client))
        failed = 1;
    if (!failed &&
        ((app.client.response_flags & PXA_NET_BODY_LENGTH_KNOWN) != 0 &&
         app.client.response_length != app.client.response_size)) failed = 1;
    if (!failed &&
        (!store_json_parse_app_detail(app.body, app.client.response_size,
                                      &app.update_detail) ||
         !text_equal(app.update_detail.app_id,
                     app.installed[app.update_index].app_id))) failed = 1;
    if (!failed)
        app.update_available[app.update_index] = (uint8_t)newer_than_installed(
            &app.update_detail, &app.installed[app.update_index]);
    else
        app.update_failed = 1;
    ++app.update_index;
    return next_update_check();
}

/* Queues the permission, identity and catalog steps in order. A send failure
 * means the Host has no such service, which the SDK helper reports only as
 * failure. */
/* Queues the next step of the catalog sequence: device permission, device
 * identity, network permission, then the catalog request itself. Result
 * handlers call this to continue, so it must not check app.busy. */
static int advance_catalog_request(void) {
    if (!app.profile_ready) {
        if (pxa_device_request_runtime_info(
                STORE_REQUEST_RUNTIME_INFO) != PXA_STATUS_OK) {
            (void)fail_request(STORE_ERROR_UNSUPPORTED);
            return 0;
        }
        return 1;
    }
    if (app.profile[0] == '\0') {
        (void)fail_request(STORE_ERROR_UNSUPPORTED);
        return 0;
    }
    if (!app.client.has_device_permission) {
        store_trace("acquire device permission", 1);
        if (!store_client_acquire_device_permission(
                app.packet, sizeof(app.packet))) {
            (void)fail_request(STORE_ERROR_UNSUPPORTED);
            return 0;
        }
        return 1;
    }
    if (!app.client.has_mac) {
        store_trace("fetch device mac", 1);
        if (!store_client_fetch_mac(&app.client)) {
            (void)fail_request(STORE_ERROR_UNSUPPORTED);
            return 0;
        }
        return 1;
    }
    if (!app.client.has_network_permission) {
        store_trace("acquire network permission", 1);
        if (!store_client_acquire_network_permission(
                app.packet, sizeof(app.packet))) {
            (void)fail_request(STORE_ERROR_UNSUPPORTED);
            return 0;
        }
        return 1;
    }
    if (!start_catalog_fetch()) {
        (void)fail_request(STORE_ERROR_UNSUPPORTED);
        return 0;
    }
    return 1;
}

static int start_catalog_request(void) {
    if (app.busy) {
        /* Do not drop the request: replay it when the running one finishes. */
        app.pending_restart = 1;
        return 1;
    }
    app.busy = 1;
    app.state = STORE_LOADING;
    app.error = STORE_ERROR_NONE;
    return advance_catalog_request();
}

/* Drops the last UTF-8 codepoint from the draft query. */
static void draft_backspace(void) {
    size_t length = string_length(app.draft);
    if (length == 0) return;
    --length;
    while (length > 0 && ((uint8_t)app.draft[length] & 0xc0u) == 0x80u) --length;
    app.draft[length] = '\0';
}

static int open_search(void) {
    copy_text(app.draft, sizeof(app.draft), app.query);
    app.screen = STORE_SCREEN_SEARCH;
    return render();
}

static int apply_search(void) {
    app.screen = STORE_SCREEN_CATALOG;
    if (text_equal(app.query, app.draft)) return render();
    if (app.busy) {
        /* The catalog is still loading; keep the query and apply it when the
         * running request completes instead of dropping the user's input. */
        copy_text(app.pending_search, sizeof(app.pending_search), app.draft);
        app.pending_search_valid = 1;
        return render();
    }
    return restart_catalog(app.draft);
}

static int select_kind(uint8_t chip_index) {
    app.kind_index = chip_index;
    if (chip_index != 0u && chip_index <= app.taxonomy.kind_count)
        app.catalog_kind = (uint8_t)text_equal(
            app.taxonomy.kinds[chip_index - 1u].value, "game");
    app.category_entry = -1;
    app.list_scroll = 0;
    return restart_catalog(app.query);
}

static int select_category(int16_t entry) {
    app.category_entry = entry;
    app.filters_expanded = 0;
    app.list_scroll = 0;
    return restart_catalog(app.query);
}

static int restart_catalog(const char *query) {
    copy_text(app.query, sizeof(app.query), query == NULL ? "" : query);
    app.catalog.head = 0;
    app.catalog.count = 0;
    app.catalog.dropped = 0;
    app.catalog.has_more = 0;
    app.catalog.next_cursor = 0;
    app.request_cursor = 0;
    app.auto_load_mark = 0;
    app.chain = 0;
    app.selected = 0;
    app.install_feedback = 0;
    app.uninstall_feedback = 0;
    app.install_refresh = 0;
    app.list_scroll = 0;
    app.detail_scroll = 0;
    return start_catalog_request();
}

static int continue_body(void);
static int fail_detail(uint8_t error);

/* A page that does not fill the list leaves it barely scrollable, which would
 * stall the incremental loading that the scroll offset drives. Keep fetching
 * while the content is shorter than the viewport plus one card. */
static int catalog_needs_fill(void) {
    const store_metrics_t *m = metrics();
    if (!app.catalog.has_more) return 0;
    if (app.catalog.count == app.fill_count) return 0;
    return catalog_content_height() <
           catalog_view_height() + (int32_t)(m->card_height + m->list_gap);
}

static int finish_catalog_body(void) {
    uint8_t before = app.catalog.count;
    uint8_t moved;
    uint8_t window_shifted;
    (void)pxa_clock_set_period(0);
    if (app.client.body_handle != 0 &&
        !store_client_close_body(&app.client))
        return fail_request(STORE_ERROR_FAILED);
    if ((app.client.response_flags & PXA_NET_BODY_LENGTH_KNOWN) != 0 &&
        app.client.response_length != app.client.response_size)
        return fail_request(STORE_ERROR_PROTOCOL);
    app.catalog.has_more = 0;
    if (!store_json_parse_catalog(app.body, app.client.response_size,
                                  &app.catalog, &app.taxonomy))
        return fail_request(STORE_ERROR_PROTOCOL);
    window_shifted = app.catalog.dropped;
    moved = (uint8_t)(app.catalog.count != before || window_shifted);
    if (app.kind_index > app.taxonomy.kind_count)
        app.kind_index = kind_entry_for(app.catalog_kind ? "game" :
                                        PXA_STORE_DEFAULT_KIND);
    if (app.category_entry >= (int16_t)app.taxonomy.category_count)
        app.category_entry = -1;
    app.request_cursor = app.catalog.next_cursor;
    if (app.catalog.dropped) {
        /* Eviction can displace several cards in a page. Preserve the row offset. */
        store_metrics_t metrics_value = *metrics();
        int32_t removed = ((app.catalog.dropped + catalog_columns() - 1) / catalog_columns()) *
            (metrics_value.card_height + metrics_value.list_gap);
        app.list_scroll -= removed;
        if (app.list_scroll < 0) app.list_scroll = 0;
        app.catalog.dropped = 0;
    }
    app.busy = 0;
    if (app.pending_restart || app.pending_search_valid)
        return restart_pending_catalog();
    if (!moved && app.catalog.has_more &&
        app.chain < STORE_MAX_CHAIN) {
        ++app.chain;
        app.state = STORE_READY;
        if (app.screen != STORE_SCREEN_SEARCH && !render()) return 0;
        if (!start_catalog_fetch()) return fail_request(STORE_ERROR_UNSUPPORTED);
        return app.screen != STORE_SCREEN_CATALOG || patch_catalog_status();
    }
    app.state = STORE_READY;
    if (moved) app.auto_load_mark = -1;
    ensure_window_snapshot();
    if (app.screen != STORE_SCREEN_SEARCH && !(app.screen == STORE_SCREEN_CATALOG && before != 0u ?
          window_shifted ? render_catalog_list() : append_catalog_page(before) : render())) return 0;
    if (app.screen == STORE_SCREEN_CATALOG) {
        if (store::media_changed() && !patch_media()) return 0;
        if (window_shifted) (void)restore_scroll(NODE_LIST, app.list_scroll);
    }
    if (app.screen == STORE_SCREEN_DETAIL && app.detail_state == STORE_IDLE) {
        if (!start_detail_fetch()) return fail_detail(STORE_ERROR_UNAVAILABLE);
        return render();
    }
    if (app.screen == STORE_SCREEN_CATALOG && catalog_needs_fill()) {
        app.fill_count = app.catalog.count;
        app.request_cursor = app.catalog.next_cursor;
        app.chain = 0;
        if (!start_catalog_fetch()) return fail_request(STORE_ERROR_UNSUPPORTED);
        return patch_catalog_status();
    }
    return 1;
}

// Navigation stays responsive while a JSON response is in flight. A late
// detail belongs to its selection revision, never to a reused ring index.
static bool detail_request_stale() {
    return app.detail_target != app.detail_revision || app.pending_restart ||
           app.pending_search_valid;
}
static int resume_detail_navigation() {
    app.busy = 0;
    app.detail_ready = 0;
    if (app.pending_restart || app.pending_search_valid) {
        const bool search = app.pending_search_valid;
        app.pending_restart = 0;
        app.pending_search_valid = 0;
        if (!restart_catalog(search ? app.pending_search : app.query)) return 0;
    } else if (app.screen == STORE_SCREEN_DETAIL) {
        app.detail_state = STORE_IDLE;
        if (!start_detail_fetch()) return fail_detail(STORE_ERROR_UNAVAILABLE);
    }
    return render();
}

static int fail_detail(uint8_t error) {
    store_trace("detail failure", error);
    (void)pxa_clock_set_period(0);
    if (app.client.body_handle != 0)
        (void)store_client_close_body(&app.client);
    app.busy = 0;
    if (app.install_refresh) {
        app.install_refresh = 0;
        app.install_feedback = 3;
    }
    if (detail_request_stale()) return resume_detail_navigation();
    app.detail_state = STORE_FAILED;
    app.detail_ready = 0;
    app.error = error;
    toast_current_error();
    return render();
}

static int32_t send_package_request(void) {
    const store_app_t *item = detail_item();
    if (!app.detail_ready) return PXA_STATUS_BAD_STATE;
    if (item->size == 0 || item->sha256[0] == '\0' ||
        !store_client_build_download_url(app.url, sizeof(app.url),
                                         item->download_ticket_url))
        return PXA_STATUS_PROTOCOL_ERROR;
    const int32_t status = pxa_store_request_package_status(PXA_STORE_INSTALL_REQUEST,
                           STORE_REQUEST_INSTALL,
                           item->app_id,
                           item->download_ticket_url, item->sha256,
                           item->size, app.packet, sizeof(app.packet));
    if (status == PXA_STATUS_OK) {
        app.downloading = 1u;
        app.downloaded_bytes = 0u;
        app.download_total = item->size;
    }
    else {
        char text[96];
        std::snprintf(text, sizeof(text), "store: install request rejected status=%ld",
                      (long)status);
        (void)pxa_log_write(3, text);
    }
    return status;
}

static int finish_detail_body(void) {
    (void)pxa_clock_set_period(0);
    if (app.client.body_handle != 0 &&
        !store_client_close_body(&app.client))
        return fail_detail(STORE_ERROR_FAILED);
    if ((app.client.response_flags & PXA_NET_BODY_LENGTH_KNOWN) != 0 &&
        app.client.response_length != app.client.response_size)
        return fail_detail(STORE_ERROR_PROTOCOL);
    app.busy = 0;
    if (detail_request_stale()) return resume_detail_navigation();
    if (!store_json_parse_app_detail(app.body, app.client.response_size,
                                     &app.detail, &app.gallery))
        return fail_detail(STORE_ERROR_PROTOCOL);
    if (app.screenshot >= app.gallery.count) app.screenshot = 0;
    app.detail_ready = 1;
    app.detail_state = STORE_READY;
    if (app.install_refresh) {
        app.install_refresh = 0;
        const int32_t status = send_package_request();
        if (status != PXA_STATUS_OK) {
            app.install_feedback = 3;
            toast_failure(PXA_MSG_STATUS_INSTALL_FAILED, status);
        }
    }
    store_trace("detail ready", 1);
    if (!render()) return 0;
    if (app.screen == STORE_SCREEN_DETAIL)
        (void)restore_scroll(NODE_DETAIL_SCROLL, app.detail_scroll);
    return 1;
}

static int continue_body(void) {
    int result;
    store_trace("continue body", app.client.response_size);
    result = store_client_consume_body(
        &app.client, app.body, app.body_capacity);
    if (result == STORE_BODY_WAITING) return 1;
    if (result == STORE_BODY_DONE) {
        if (app.pending_request == STORE_REQUEST_UPDATE_CHECK)
            return finish_update_check(0);
        if (app.pending_request == STORE_REQUEST_DETAIL)
            return finish_detail_body();
        return finish_catalog_body();
    }
    if (result == STORE_BODY_LIMIT && retry_larger_body()) return 1;
    if (app.pending_request == STORE_REQUEST_UPDATE_CHECK)
        return finish_update_check(1);
    if (app.pending_request == STORE_REQUEST_DETAIL)
        return fail_detail(result == STORE_BODY_LIMIT ? STORE_ERROR_LIMIT
                                                      : STORE_ERROR_FAILED);
    return fail_request(result == STORE_BODY_LIMIT ? STORE_ERROR_LIMIT
                                                   : STORE_ERROR_FAILED);
}

static int handle_json_result(const pxa_event_t *parsed) {
    pxa_net_result_t result;
    uint8_t detail = (uint8_t)(parsed->token == STORE_REQUEST_DETAIL);
    if (!pxa_net_parse_result(parsed, parsed->token,
                                  PXA_NET_HTTP_REQUEST, &result)) {
        if (parsed->token == STORE_REQUEST_UPDATE_CHECK)
            return finish_update_check(1);
        return detail ? fail_detail(STORE_ERROR_PROTOCOL)
                      : fail_request(STORE_ERROR_PROTOCOL);
    }
    if (detail && detail_request_stale()) {
        app.client.body_handle = result.body_handle;
        if (app.client.body_handle) (void)store_client_close_body(&app.client);
        return resume_detail_navigation();
    }
    store_trace("HTTP result", (uint32_t)result.status);
    store_trace("body limit", app.body_limit);
    if (result.status == PXA_STATUS_LIMIT_EXCEEDED && retry_larger_body())
        return 1;
    if (parsed->token == STORE_REQUEST_UPDATE_CHECK) {
        app.client.response_flags = result.flags;
        app.client.response_length = result.body_length;
        app.client.body_handle = result.body_handle;
        if (result.status == PXA_STATUS_OK && result.status_code == 404)
            return skip_update_check(0);
        if (result.status != PXA_STATUS_OK || result.status_code != 200 ||
            result.body_handle == 0) return skip_update_check(1);
        return continue_body();
    }
    if (result.status != PXA_STATUS_OK) {
        toast_failure(PXA_MSG_STATUS_FAILED, result.status);
        if (detail && app.install_refresh) {
            app.install_refresh = 0;
            app.install_feedback = 3;
        }
        app.error = error_from_status(result.status);
        if (detail) {
            app.detail_state = STORE_FAILED;
            app.detail_ready = 0;
        } else {
            app.state = STORE_FAILED;
        }
        app.busy = 0;
        if (app.pending_restart || app.pending_search_valid)
            return restart_pending_catalog();
        return app.screen == STORE_SCREEN_SEARCH || render();
    }
    app.client.http_status = result.status_code;
    app.client.response_flags = result.flags;
    app.client.response_length = result.body_length;
    app.client.body_handle = result.body_handle;
    if (result.status_code != 200 || result.body_handle == 0) {
        if (detail && app.install_refresh) {
            app.install_refresh = 0;
            app.install_feedback = 3;
        }
        if (result.body_handle != 0)
            (void)store_client_close_body(&app.client);
        app.error = result.status_code == 404 ? STORE_ERROR_NOT_FOUND
                                              : STORE_ERROR_HTTP;
        toast_failure(PXA_MSG_STATUS_FAILED,
                      result.status_code == 404 ? PXA_STATUS_NOT_FOUND : PXA_STATUS_IO_ERROR);
        if (detail) {
            app.detail_state = STORE_FAILED;
            app.detail_ready = 0;
        } else {
            app.state = STORE_FAILED;
        }
        app.busy = 0;
        if (app.pending_restart || app.pending_search_valid)
            return restart_pending_catalog();
        return app.screen == STORE_SCREEN_SEARCH || render();
    }
    return continue_body();
}

static int handle_permission_result(const pxa_event_t *parsed) {
    pxa_permission_acquire_result_t result;
    if (!pxa_permission_parse_acquire(parsed, parsed->token, &result))
        return fail_request(STORE_ERROR_PROTOCOL);
    if (result.status != PXA_STATUS_OK) {
        app.state = STORE_DENIED;
        app.busy = 0;
        return render();
    }
    if (parsed->token == STORE_REQUEST_DEVICE_PERMISSION) {
        app.client.device_permission = result.handle;
        app.client.has_device_permission = 1;
    } else {
        app.client.network_permission = result.handle;
        app.client.has_network_permission = 1;
    }
    if (!advance_catalog_request()) return PXA_EVENT_HANDLED;
    return PXA_EVENT_HANDLED;
}

static int handle_mac_result(const pxa_event_t *parsed) {
    pxa_device_mac_result_t result;
    if (!pxa_device_parse_get_mac(parsed, STORE_REQUEST_DEVICE_MAC,
                                     &result) ||
        result.status != PXA_STATUS_OK ||
        result.kind != PXA_DEVICE_MAC_WIFI_STATION_HARDWARE) {
        /* A stable device identity only affects rollout bucketing. */
        copy_text(app.device_id, sizeof(app.device_id), "");
    } else {
        (void)pxa_device_format_mac_colon(result.mac, app.device_id,
                                          sizeof(app.device_id));
    }
    app.client.has_mac = 1;
    if (!advance_catalog_request()) return PXA_EVENT_HANDLED;
    return PXA_EVENT_HANDLED;
}

static int handle_runtime_info(const pxa_event_t *parsed) {
    pxa_device_runtime_info_t info;
    app.profile_ready = 1;
    if (pxa_device_parse_runtime_info(
            parsed, STORE_REQUEST_RUNTIME_INFO, &info) &&
        info.status == PXA_STATUS_OK) {
        (void)store_client_profile_for_device(app.profile,
                                               sizeof(app.profile),
                                               info.target, info.architecture,
                                               info.engine,
                                               info.formats);
    }
    if (!advance_catalog_request()) return PXA_EVENT_HANDLED;
    return PXA_EVENT_HANDLED;
}

/* Hide the status bar. Keep system navigation visible so the Host reports
 * its real reserved region instead of overlaying the Store destinations. */
static int configure_window(void) {
    pxa_window_config_t config = {0};
    config.status_bar_mode = 1;
    config.navigation_bar_mode = 0;
    return pxa_window_configure(&config) == PXA_STATUS_OK;
}

static void apply_environment(const pxa_ui_environment_t *environment) {
    uint8_t index;
    app.density_q16 = environment->density_q16;
    for (index = 0; index < 4u; ++index)
        app.safe_insets[index] = environment->safe_insets[index];
    app.display_shape = environment->display_shape;
    for (index = 0; index < 4u; ++index)
        app.corner_radii[index] = environment->corner_radii[index];
    if (environment->width != 0)
        app.width = store::layout::pixels_to_dp(environment->width, app.density_q16);
    if (environment->height != 0)
        app.height = store::layout::pixels_to_dp(environment->height, app.density_q16);
}

/* The UI environment reports the panel's physical safe area. The Window
 * service additionally reports the status and navigation bar insets, so an
 * ordinary application window uses the larger value per edge. */
static uint16_t effective_inset(uint8_t edge) {
    uint32_t safe = store::layout::pixels_to_dp(app.safe_insets[edge], app.density_q16);
    uint32_t bar = store::layout::pixels_to_dp(app.bar_insets[edge], app.density_q16);
    uint32_t shape = 0;
    uint32_t value;
    if (app.display_shape == PXA_UI_DISPLAY_SHAPE_CIRCLE) {
        uint32_t diameter = app.width < app.height ? app.width : app.height;
        shape = diameter / 6u;
        if (edge == 1u || edge == 3u)
            shape += (app.width - diameter) / 2u;
        else
            shape += (app.height - diameter) / 2u;
    } else if (app.display_shape == PXA_UI_DISPLAY_SHAPE_ROUNDED_RECTANGLE) {
        uint32_t first = app.corner_radii[edge];
        uint32_t second = app.corner_radii[(edge + 1u) % 4u];
        if (edge == 1u) {
            first = app.corner_radii[1];
            second = app.corner_radii[2];
        } else if (edge == 2u) {
            first = app.corner_radii[2];
            second = app.corner_radii[3];
        } else if (edge == 3u) {
            first = app.corner_radii[3];
            second = app.corner_radii[0];
        }
        shape = store::layout::pixels_to_dp(first > second ? first : second,
                                           app.density_q16) / 3u;
    }
    value = safe > bar ? safe : bar;
    if (shape > value) value = shape;
    if (value > 512u) value = 512u;
    return (uint16_t)value;
}

/* The screen's own margins already keep content away from an edge, so the
 * root only adds the part of the safe area they do not cover. Without this the
 * gesture strip and the layout margin stack up and the content drifts. */
static uint16_t inset_padding(uint8_t edge, uint16_t margin) {
    uint16_t inset = effective_inset(edge);
    return inset > margin ? (uint16_t)(inset - margin) : 0;
}

static int request_window_snapshot(void);

/* Applies a window snapshot: the panel safe area and the system bar insets. */
static int parse_window_snapshot(const pxa_event_t *event,
                                 int with_status) {
    pxa_window_snapshot_t view;
    if (!(with_status ? pxa_window_parse_snapshot(
                            event, STORE_REQUEST_WINDOW_SNAPSHOT, &view)
                     : pxa_window_parse_metrics_changed(event, &view)) ||
        view.status != PXA_STATUS_OK) return 0;
    store::layout::copy_window_insets(app.safe_insets, view.safe_insets);
    store::layout::copy_window_insets(app.bar_insets, view.system_bar_insets);
    app.snapshot_received = 1;
    return 1;
}

/* Some Hosts only have a snapshot once the component is fully active, so the
 * request is best effort and retried a few times. */
static void ensure_window_snapshot(void) {
    if (app.snapshot_received || app.snapshot_attempts >= 3u) return;
    ++app.snapshot_attempts;
    (void)request_window_snapshot();
}

static int request_window_snapshot(void) {
    return pxa_window_request_snapshot(STORE_REQUEST_WINDOW_SNAPSHOT) ==
           PXA_STATUS_OK;
}

/* ------------------------------------------------------------------ */
/* Application lifecycle                                              */
/* ------------------------------------------------------------------ */

extern "C" int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)pxa_i18n_init_from_start_config(&app.i18n, &pxa_app_i18n_bundle,
                                          config, config_length);
    store_client_init(&app.client);
    store::media_start();
    store_search_image_request();
    app.installed_count = 0;
    app.download_count = 0;
    app.failed_count = 0;
    app.update_checking = 0;
    app.update_rendered = 0;
    app.update_checked = 0;
    app.update_detail_target = 0;
    (void)pxa_store_manage(PXA_STORE_INSTALLED_LIST_REQUEST, STORE_REQUEST_INSTALLED,
                           NULL, app.packet, sizeof(app.packet));
    (void)pxa_store_manage(PXA_STORE_DOWNLOAD_LIST_REQUEST, STORE_REQUEST_DOWNLOADS,
                           NULL, app.packet, sizeof(app.packet));
    app.screen = STORE_SCREEN_CATALOG;
#if STORE_DEBUG_START_SEARCH
    app.screen = STORE_SCREEN_SEARCH;
#endif
    app.state = STORE_IDLE;
    app.error = STORE_ERROR_NONE;
    app.selected = 0;
    app.busy = 0;
    app.chain = 0;
    app.install_feedback = 0;
    app.uninstall_feedback = 0;
    app.install_refresh = 0;
    app.generation = 0;
    app.request_cursor = 0;
    app.catalog.head = 0;
    app.catalog.count = 0;
    app.catalog.dropped = 0;
    app.catalog.has_more = 0;
    app.catalog.next_cursor = 0;
    app.auto_load_mark = 0;
    app.detail_ready = 0;
    app.detail_state = STORE_IDLE;
    app.detail_target = 0;
    app.pending_request = 0;
    app.kind_index = 0;
    app.catalog_kind = 0;
    app.category_entry = -1;
    app.list_scroll = 0;
    app.detail_scroll = 0;
    app.width = 0;
    app.height = 0;
    app.density_q16 = 65536u;
    app.draft[0] = '\0';
    app.taxonomy.kind_count = 0;
    app.taxonomy.category_count = 0;
    copy_text(app.query, sizeof(app.query), "");
    copy_text(app.device_id, sizeof(app.device_id), "");
    app.profile[0] = '\0';
    app.profile_ready = 0;
    store_trace("start", 1);
    {
        pxa_ui_environment_t environment;
        if (pxa_ui_parse_start_environment(config, config_length, &environment))
            apply_environment(&environment);
    }
    if (!configure_window()) return PXA_STATUS_INTERNAL;
    ensure_window_snapshot();
    if (start_catalog_request()) {
        if (!render()) return PXA_STATUS_INTERNAL;
    }
    return PXA_STATUS_OK;
}

extern "C" int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    if (store::media_event({reinterpret_cast<const std::byte *>(event), length})) {
        return !store::media_changed() || patch_media() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) {
        store_trace("unparsed event length", length);
        return PXA_EVENT_UNHANDLED;
    }
    store_trace("event service", parsed.service);
    store_trace("event opcode", parsed.opcode);
    if (store_search_image_on_event(&parsed)) {
        if (store_search_image_handle()) {
            pxa_log_write(2,"store: search image ready");
#if STORE_SEARCH_TEXT_INPUT
            if (app.screen == STORE_SCREEN_SEARCH)
                return patch_search_images() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
#endif
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (store_search_image_error()) pxa_log_write(3,"store: search image unavailable");
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service==PXA_SERVICE_SYSTEM && parsed.opcode==PXA_SYSTEM_LIFECYCLE_EVENT && parsed.payload_size==1) {
        store_search_image_pause(parsed.payload[0]==PXA_SYSTEM_LIFECYCLE_BACKGROUND);
        store::media_pause(parsed.payload[0]==PXA_SYSTEM_LIFECYCLE_BACKGROUND);
        return PXA_EVENT_HANDLED;
    }
    {
        int locale_result = pxa_i18n_handle_event(&app.i18n, &parsed);
        if (locale_result != 0)
            return locale_result == 1 && !render() ? PXA_STATUS_INTERNAL
                                                   : PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_WINDOW_SERVICE &&
        parsed.opcode == PXA_WINDOW_GET_SNAPSHOT &&
        parsed.token == STORE_REQUEST_WINDOW_SNAPSHOT) {
        if (!parse_window_snapshot(&parsed, 1))
            return PXA_EVENT_UNHANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_WINDOW_SERVICE &&
        parsed.opcode == PXA_WINDOW_METRICS_CHANGED) {
        /* metrics-changed carries a complete snapshot without a status. */
        if (!parse_window_snapshot(&parsed, 0))
            return PXA_EVENT_UNHANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_UI_SERVICE &&
        parsed.opcode == PXA_UI_ENVIRONMENT_CHANGED) {
        pxa_ui_environment_t environment;
        if (!pxa_ui_parse_environment_event(&parsed, &environment))
            return PXA_EVENT_UNHANDLED;
        apply_environment(&environment);
        ensure_window_snapshot();
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (pxa_window_is_back_requested(&parsed)) {
        if (app.screen == STORE_SCREEN_VIEWER)
            return close_viewer() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        if (app.screen == STORE_SCREEN_SUCCESS) {
            app.screen = app.success_return_screen;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (app.screen == STORE_SCREEN_INSTALLED || app.screen == STORE_SCREEN_DOWNLOADS) {
            app.screen = STORE_SCREEN_CATALOG;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (app.screen == STORE_SCREEN_SEARCH) {
            app.screen = STORE_SCREEN_CATALOG;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (app.screen == STORE_SCREEN_DETAIL) {
            app.screen = STORE_SCREEN_CATALOG;
            app.install_feedback = 0;
            app.install_refresh = 0;
            app.detail_ready = 0;
            app.detail_state = STORE_IDLE;
            app.update_detail_target = 0;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_UNHANDLED;
    }
    if (parsed.service == PXA_PERMISSION_SERVICE &&
        parsed.opcode == PXA_PERMISSION_ACQUIRE)
        return handle_permission_result(&parsed);
    if (parsed.service == PXA_DEVICE_SERVICE &&
        parsed.opcode == PXA_DEVICE_GET_RUNTIME_INFO &&
        parsed.token == STORE_REQUEST_RUNTIME_INFO)
        return handle_runtime_info(&parsed);
    if (parsed.service == PXA_DEVICE_SERVICE &&
        parsed.opcode == PXA_DEVICE_GET_MAC &&
        parsed.token == STORE_REQUEST_DEVICE_MAC)
        return handle_mac_result(&parsed);
    if (parsed.service == PXA_NET_SERVICE &&
        parsed.opcode == PXA_NET_HTTP_REQUEST &&
        (parsed.token == STORE_REQUEST_CATALOG ||
         parsed.token == STORE_REQUEST_DETAIL ||
         parsed.token == STORE_REQUEST_UPDATE_CHECK))
        return handle_json_result(&parsed);
    if (parsed.service == PXA_SERVICE_STORE_INSTALLER &&
        parsed.opcode == PXA_STORE_INSTALLED_LIST_REQUEST &&
        parsed.token == STORE_REQUEST_INSTALLED) {
        size_t count = 0;
        if (pxa_store_parse_installed(&parsed, app.installed,
                                      PXA_STORE_INSTALLED_MAX, &count)) {
            app.installed_count = (uint8_t)count;
        }
        if (app.screen == STORE_SCREEN_CATALOG && app.catalog.count == 0u)
            return PXA_EVENT_HANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_STORE_INSTALLER &&
        parsed.opcode == PXA_STORE_UNINSTALL_REQUEST &&
        parsed.token == STORE_REQUEST_UNINSTALL) {
        int32_t status;
        app.uninstall_feedback = pxa_store_uninstall_result(&parsed, &status) ?
            status == PXA_STATUS_OK ? 2u :
            status == PXA_STATUS_DENIED ? 4u : 3u : 3u;
        if (app.uninstall_feedback == 2u) {
            app.update_checked = 0;
            for (uint8_t index = 0; index < PXA_STORE_INSTALLED_MAX; ++index)
                app.update_available[index] = 0;
            (void)pxa_store_manage(PXA_STORE_INSTALLED_LIST_REQUEST,
                                   STORE_REQUEST_INSTALLED, NULL,
                                   app.packet, sizeof(app.packet));
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_STORE_INSTALLER &&
        parsed.opcode == PXA_STORE_DOWNLOAD_LIST_REQUEST &&
        parsed.token == STORE_REQUEST_DOWNLOADS) {
        size_t count = 0;
        if (pxa_store_parse_downloads(&parsed, app.downloads,
                                      PXA_STORE_DOWNLOAD_MAX, &count))
            app.download_count = (uint8_t)count;
        if (app.screen != STORE_SCREEN_DOWNLOADS) return PXA_EVENT_HANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_STORE_INSTALLER &&
        parsed.opcode == PXA_STORE_DELETE_REQUEST &&
        parsed.token == STORE_REQUEST_DELETE) {
        if (parsed.payload != NULL && parsed.payload_size == 4u &&
            (int32_t)pxa_load_u32(parsed.payload) == PXA_STATUS_OK) {
            (void)pxa_store_manage(PXA_STORE_DOWNLOAD_LIST_REQUEST,
                                   STORE_REQUEST_DOWNLOADS, NULL,
                                   app.packet, sizeof(app.packet));
        }
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SERVICE_STORE_INSTALLER &&
        parsed.opcode == PXA_STORE_DOWNLOAD_PROGRESS) {
        pxa_store_token_t request_id;
        uint64_t received, total;
        if (pxa_store_parse_download_progress(&parsed, &request_id, &received, &total) &&
            (request_id == STORE_REQUEST_DOWNLOAD || request_id == STORE_REQUEST_INSTALL) &&
            app.downloading) {
            app.downloaded_bytes = received;
            app.download_total = total;
            if (app.screen == STORE_SCREEN_DOWNLOADS)
                return patch_download_progress() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SERVICE_STORE_INSTALLER &&
        parsed.opcode == PXA_STORE_DOWNLOAD_REQUEST &&
        parsed.token == STORE_REQUEST_DOWNLOAD) {
        app.downloading = 0u;
        int32_t status = PXA_STATUS_PROTOCOL_ERROR;
        char filename[20];
        if (!pxa_store_download_result(&parsed, &status, filename) ||
            status != PXA_STATUS_OK) {
            toast_failure(PXA_MSG_STATUS_DOWNLOAD_FAILED, status);
            app.install_feedback = 4;
            if (app.failed_count < PXA_STORE_DOWNLOAD_MAX)
                copy_text(app.failed_apps[app.failed_count++], STORE_MAX_APP_ID,
                          app.completed_app_id);
        } else {
            (void)pxa_store_manage(PXA_STORE_DOWNLOAD_LIST_REQUEST,
                                   STORE_REQUEST_DOWNLOADS, NULL,
                                   app.packet, sizeof(app.packet));
            app.install_feedback = 5;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_STORE_INSTALLER &&
        (parsed.opcode == PXA_STORE_INSTALL_FILE_REQUEST ||
         parsed.opcode == PXA_STORE_INSTALL_REQUEST) &&
        parsed.token == STORE_REQUEST_INSTALL) {
        app.downloading = 0u;
        if (parsed.opcode == PXA_STORE_INSTALL_FILE_REQUEST)
            (void)pxa_store_manage(PXA_STORE_DOWNLOAD_LIST_REQUEST,
                                   STORE_REQUEST_DOWNLOADS, NULL,
                                   app.packet, sizeof(app.packet));
        int32_t status = PXA_STATUS_PROTOCOL_ERROR;
        app.install_feedback = pxa_store_install_result(&parsed, &status) ?
            status == PXA_STATUS_OK ? 2u :
            status == PXA_STATUS_QUOTA_EXCEEDED ? 6u :
            status == PXA_STATUS_DENIED ? 7u :
            status == PXA_STATUS_UNSUPPORTED ? 8u : 3u : 3u;
        if (app.install_feedback == 3u || app.install_feedback == 6u)
            toast_failure(app.install_feedback == 6u ? PXA_MSG_STATUS_STORAGE_FULL :
                          PXA_MSG_STATUS_INSTALL_FAILED, status);
        if (app.install_feedback == 2u) {
            app.success_return_screen = app.screen;
            app.screen = STORE_SCREEN_SUCCESS;
            app.update_checked = 0;
            for (uint8_t index = 0; index < PXA_STORE_INSTALLED_MAX; ++index)
                app.update_available[index] = 0;
            (void)pxa_store_manage(PXA_STORE_INSTALLED_LIST_REQUEST,
                                   STORE_REQUEST_INSTALLED, NULL,
                                   app.packet, sizeof(app.packet));
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_CLOCK_SERVICE &&
        parsed.opcode == PXA_CLOCK_TICK) {
        if (app.client.stream_waiting && app.client.body_handle != 0) {
            app.client.stream_waiting = 0;
            return continue_body() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_UNHANDLED;
    }
    if (!pxa_ui_parse_event(&parsed, &ui_event)) return PXA_EVENT_UNHANDLED;
    if (ui_event.kind == PXA_UI_EVENT_POINTER_KIND) {
        pxa_ui_pointer_data_t pointer;
        if (app.screen == STORE_SCREEN_VIEWER && ui_event.node == NODE_VIEWPORT &&
            pxa_ui_parse_pointer(&parsed, &pointer)) {
            auto &gesture = app.viewer_gesture;
            if (pointer.phase == PXA_POINTER_DOWN) gesture.begin(pointer.x, pointer.y);
            else if (pointer.phase == PXA_POINTER_MOVE) gesture.move(pointer.x, pointer.y);
            else if (pointer.phase == PXA_POINTER_UP) {
                const bool active = gesture.active;
                const auto action = gesture.end(pointer.x, pointer.y, pointer.timestamp_us, app.viewer_zoom);
                if (action == store::ViewerAction::next || action == store::ViewerAction::previous)
                    return viewer_page(action == store::ViewerAction::next ? 1 : -1)
                        ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
                if (action == store::ViewerAction::zoom) {
                    app.viewer_zoom = app.viewer_zoom == 1 ? 2 : 1;
                    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
                }
                if (active && !gesture.moved && app.gallery.count &&
                    store::media_failed(app.gallery.screenshots[app.screenshot].url, true)) {
                    store::media_retry();
                    return patch_media() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
                }
            } else gesture.cancel();
            return PXA_EVENT_HANDLED;
        }
        if (app.screen == STORE_SCREEN_DETAIL && ui_event.node == NODE_GALLERY_IMAGE &&
            pxa_ui_parse_pointer(&parsed, &pointer)) {
            if (pointer.phase == PXA_POINTER_DOWN) {
                app.viewer_gesture.begin(pointer.x, pointer.y);
                app.drag_active = 0;
            } else if (pointer.phase == PXA_POINTER_MOVE) {
                app.viewer_gesture.move(pointer.x, pointer.y);
                app.drag_active = app.viewer_gesture.moved;
            } else if (pointer.phase != PXA_POINTER_UP) {
                app.viewer_gesture.cancel();
                app.drag_active = 1;
            }
            return PXA_EVENT_HANDLED;
        }
        if ((ui_event.node == NODE_LIST || ui_event.node == NODE_FILTERS ||
             ui_event.node == NODE_HEADER) &&
            pxa_ui_parse_pointer(&parsed, &pointer)) {
            if (pointer.phase == PXA_POINTER_DOWN ||
                app.pointer_node != ui_event.node) {
                app.pointer_node = ui_event.node;
                app.pointer_y = pointer.y;
                app.drag_active = 0;
            } else if (pointer.phase == PXA_POINTER_MOVE) {
                int32_t dy = pointer.y - app.pointer_y;
                if (dy >= STORE_DRAG_DELTA || dy <= -STORE_DRAG_DELTA) {
                    app.pointer_y = pointer.y;
                    app.drag_active = 1;
                }

            }
        }
        return PXA_EVENT_HANDLED;
    }
    if (ui_event.kind == PXA_UI_EVENT_SCROLL_KIND) {
        if (ui_event.node == NODE_LIST && app.screen == STORE_SCREEN_CATALOG) {
            int32_t raw = ui_event.value;
            int32_t offset = raw > 0 ? raw : 0;
            app.list_scroll = offset;
            // Focus can also scroll a partially visible card into view. Only
            // pointer movement identifies a drag; scrolling alone is no reason
            // to discard the subsequent legitimate card click.
            sync_media();
            if (store::media_changed() && !patch_media()) return PXA_STATUS_INTERNAL;
            if (!app.busy && app.catalog.has_more &&
                offset > app.auto_load_mark &&
                offset + catalog_view_height() +
                    (int32_t)(metrics()->card_height + metrics()->list_gap) >=
                    catalog_content_height()) {
                app.auto_load_mark = offset;
                app.request_cursor = app.catalog.next_cursor;
                app.chain = 0;
                if (!start_catalog_request() || !patch_catalog_status())
                    return PXA_STATUS_INTERNAL;
            }
        } else if (ui_event.node == NODE_DETAIL_SCROLL) {
            app.detail_scroll = ui_event.value > 0 ? ui_event.value : 0;
        }
        return PXA_EVENT_HANDLED;
    }
#if STORE_SEARCH_TEXT_INPUT
    if (ui_event.kind == PXA_UI_EVENT_TEXT_KIND &&
        app.screen == STORE_SCREEN_SEARCH &&
        ui_event.node == NODE_SEARCH_FIELD) {
        char text[PXA_UI_EVENT_TEXT_MAX_BYTES + 1u];
        size_t length;
        if (!pxa_ui_event_text(&ui_event, text, sizeof(text)))
            return PXA_EVENT_HANDLED;
        length = string_length(text);
        if (!text_equal(text, app.draft)) {
            adopt_field_text(text, length);
            if (!sync_search_decoration(0)) return PXA_STATUS_INTERNAL;
        }
        /* The field already shows the text the input method wrote; editing
         * never restarts a search in flight, the query is applied when the
         * input is submitted or searched. */
        return PXA_EVENT_HANDLED;
    }
#endif
    if (ui_event.kind != PXA_UI_EVENT_CLICK_KIND) return PXA_EVENT_UNHANDLED;
    /* A drag is not a click. When the list cannot scroll - a short page, or
     * content already at its end - the release still arrives as a click on
     * the card under the finger, which opened a detail instead of scrolling. */
    if (app.drag_active) {
        app.drag_active = 0;
        if (ui_event.node == NODE_LIST || ui_event.node == NODE_FILTERS ||
            ui_event.node == NODE_GALLERY_IMAGE ||
            ui_event.node == NODE_CATEGORY_ALL ||
            (ui_event.node >= NODE_CATEGORY_BASE &&
             ui_event.node < NODE_CATEGORY_BASE +
                              STORE_MAX_TAXONOMY * NODE_CHIP_STRIDE) ||
            (ui_event.node >= NODE_CARD_BASE &&
             ui_event.node < NODE_CARD_BASE +
                              STORE_MAX_APPS * NODE_CARD_STRIDE))
            return PXA_EVENT_HANDLED;
    }
    if (ui_event.node == NODE_SEARCH_BUTTON &&
        app.screen == STORE_SCREEN_CATALOG) {
        if (!open_search()) return PXA_EVENT_HANDLED;
        return PXA_EVENT_HANDLED;
    }
    if (app.screen == STORE_SCREEN_SEARCH) {
        if (ui_event.node == NODE_SEARCH_BACK) {
            app.screen = STORE_SCREEN_CATALOG;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (ui_event.node == NODE_SEARCH_CLEAR) {
            app.draft[0] = '\0';
#if STORE_SEARCH_TEXT_INPUT
            return sync_search_decoration(1) ? PXA_EVENT_HANDLED
                                             : PXA_STATUS_INTERNAL;
#else
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
#endif
        }
        if (ui_event.node == NODE_SEARCH_FIELD || ui_event.node == NODE_SEARCH_SUBMIT) {
            if (!apply_search()) return PXA_EVENT_HANDLED;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (ui_event.node >= NODE_KEY_BASE &&
            ui_event.node < NODE_KEY_BASE +
                               NODE_KEY_COUNT * NODE_KEY_STRIDE &&
            (ui_event.node - NODE_KEY_BASE) % NODE_KEY_STRIDE == 0) {
            uint8_t key = (uint8_t)((ui_event.node - NODE_KEY_BASE) /
                                    NODE_KEY_STRIDE);
            if (key < 26u) {
                size_t length = string_length(app.draft);
                if (length + 1u < sizeof(app.draft)) {
                    app.draft[length] = (char)('a' + key);
                    app.draft[length + 1u] = '\0';
                }
            } else if (key == 26u) {
                draft_backspace();
            } else {
                if (!apply_search()) return PXA_EVENT_HANDLED;
                return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
            }
#if STORE_SEARCH_TEXT_INPUT
            return sync_search_decoration(1) ? PXA_EVENT_HANDLED
                                       : PXA_STATUS_INTERNAL;
#else
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
#endif
        }
        return PXA_EVENT_HANDLED;
    }
    if (ui_event.node == NODE_TAB_APPS || ui_event.node == NODE_TAB_GAMES) {
        if (app.busy && app.pending_request == STORE_REQUEST_UPDATE_CHECK)
            return PXA_EVENT_HANDLED;
        uint8_t requested_kind = (uint8_t)(ui_event.node == NODE_TAB_GAMES);
        uint8_t next = kind_entry_for(requested_kind ? "game" : "app");
        if (app.screen != STORE_SCREEN_CATALOG) {
            app.screen = STORE_SCREEN_CATALOG;
            if (app.kind_index == next && app.catalog_kind == requested_kind)
                return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        } else if (app.kind_index == next && app.catalog_kind == requested_kind) {
            if (!app.busy) {
                store::media_retry();
                return restart_catalog(app.query) && render() ?
                    PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
            }
            return PXA_EVENT_HANDLED;
        }
        app.catalog_kind = requested_kind;
        if (!select_kind(next)) return PXA_EVENT_HANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_CATEGORY_ALL) {
        if (app.category_entry < 0) return PXA_EVENT_HANDLED;
        if (!select_category(-1)) return PXA_EVENT_HANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node >= NODE_CATEGORY_BASE &&
        ui_event.node <
            NODE_CATEGORY_BASE + STORE_MAX_TAXONOMY * NODE_CHIP_STRIDE &&
        (ui_event.node - NODE_CATEGORY_BASE) % NODE_CHIP_STRIDE == 0) {
        int16_t entry = (int16_t)((ui_event.node - NODE_CATEGORY_BASE) /
                                  NODE_CHIP_STRIDE);
        if (entry >= (int16_t)app.taxonomy.category_count ||
            app.category_entry == entry)
            return PXA_EVENT_HANDLED;
        if (!select_category(entry)) return PXA_EVENT_HANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_TECHNICAL && app.screen == STORE_SCREEN_DETAIL) {
        app.technical_expanded = !app.technical_expanded;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (app.screen == STORE_SCREEN_VIEWER) {
        if (ui_event.node == NODE_BACK)
            return close_viewer() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        if (ui_event.node == NODE_VIEWER_PREV || ui_event.node == NODE_VIEWER_NEXT)
            return viewer_page(ui_event.node == NODE_VIEWER_NEXT ? 1 : -1)
                ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        if (ui_event.node == NODE_VIEWER_ZOOM) {
            app.viewer_zoom = uint8_t(app.viewer_zoom % 3 + 1);
            app.viewer_gesture.cancel();
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_HANDLED;
    }
    if (app.screen == STORE_SCREEN_DETAIL && ui_event.node == NODE_GALLERY_IMAGE && app.gallery.count) {
        app.screen = STORE_SCREEN_VIEWER;
        app.viewer_zoom = 1;
        app.viewer_gesture.cancel();
        app.drag_active = 0;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (app.screen == STORE_SCREEN_DETAIL && (ui_event.node == NODE_GALLERY_PREV ||
        ui_event.node == NODE_GALLERY_NEXT)) {
        if (ui_event.node == NODE_GALLERY_PREV && app.screenshot) --app.screenshot;
        if (ui_event.node == NODE_GALLERY_NEXT && app.screenshot + 1 < app.gallery.count) ++app.screenshot;
        store::media_retry();
        sync_media();
        return patch_media() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_REFRESH && app.screen == STORE_SCREEN_CATALOG) {
        app.filters_expanded = !app.filters_expanded;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_FOOTER && app.screen == STORE_SCREEN_CATALOG && !app.busy) {
        if (app.catalog.has_more) {
            app.request_cursor = app.catalog.next_cursor;
            return start_catalog_request() && patch_catalog_status() ?
                PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        store_search_image_retry();
        store::media_retry();
        if (!restart_catalog(app.query)) return PXA_EVENT_HANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_INSTALLED_ACTION ||
        ui_event.node == NODE_DOWNLOADS_ACTION) {
        if ((ui_event.node == NODE_INSTALLED_ACTION &&
             app.screen == STORE_SCREEN_INSTALLED) ||
            (ui_event.node == NODE_DOWNLOADS_ACTION &&
             app.screen == STORE_SCREEN_DOWNLOADS)) return PXA_EVENT_HANDLED;
        app.screen = ui_event.node == NODE_INSTALLED_ACTION ?
            STORE_SCREEN_INSTALLED : STORE_SCREEN_DOWNLOADS;
        (void)pxa_store_manage(app.screen == STORE_SCREEN_INSTALLED ?
                               PXA_STORE_INSTALLED_LIST_REQUEST :
                               PXA_STORE_DOWNLOAD_LIST_REQUEST,
                               app.screen == STORE_SCREEN_INSTALLED ?
                               STORE_REQUEST_INSTALLED : STORE_REQUEST_DOWNLOADS,
                               NULL, app.packet, sizeof(app.packet));
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_LIBRARY_CHECK &&
        app.screen == STORE_SCREEN_INSTALLED) {
        if (app.busy || !app.client.has_network_permission) {
            app.update_failed = 1;
            app.update_checked = 1;
        } else {
            app.update_checking = 1;
            app.update_rendered = 0;
            app.update_checked = 0;
            app.update_failed = 0;
            app.update_index = 0;
            for (uint8_t index = 0; index < PXA_STORE_INSTALLED_MAX; ++index)
                app.update_available[index] = 0;
            return next_update_check() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_BACK) {
        app.screen = app.screen == STORE_SCREEN_SUCCESS ?
                     app.success_return_screen : STORE_SCREEN_CATALOG;
        app.install_feedback = 0;
        app.install_refresh = 0;
        app.detail_ready = 0;
        app.detail_state = STORE_IDLE;
        app.update_detail_target = 0;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_SUCCESS_LATER) {
        app.screen = app.success_return_screen;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node == NODE_SUCCESS_OPEN) {
        (void)pxa_store_manage(PXA_STORE_LAUNCH_REQUEST, STORE_REQUEST_LAUNCH,
                               app.completed_app_id, app.packet, sizeof(app.packet));
        return PXA_EVENT_HANDLED;
    }
    if ((app.screen == STORE_SCREEN_INSTALLED || app.screen == STORE_SCREEN_DOWNLOADS) &&
        ui_event.node >= NODE_LIBRARY_BASE &&
        ui_event.node < NODE_LIBRARY_BASE +
                            (PXA_STORE_DOWNLOAD_MAX * 2u + 1u) * NODE_LIBRARY_STRIDE) {
        uint8_t index = (uint8_t)((ui_event.node - NODE_LIBRARY_BASE) /
                                  NODE_LIBRARY_STRIDE);
        uint8_t action = (uint8_t)((ui_event.node - NODE_LIBRARY_BASE) %
                                   NODE_LIBRARY_STRIDE);
        if (app.screen == STORE_SCREEN_DOWNLOADS && app.downloading) {
            if (index == 0u) return PXA_EVENT_HANDLED;
            --index;
        }
        if (app.screen == STORE_SCREEN_INSTALLED && index < app.installed_count) {
            if (action == 3u) {
                (void)pxa_store_manage(PXA_STORE_LAUNCH_REQUEST,
                                       STORE_REQUEST_LAUNCH,
                                       app.installed[index].app_id,
                                       app.packet, sizeof(app.packet));
                return PXA_EVENT_HANDLED;
            }
            if (action == 5u && !app.busy) {
                int available = app.update_available[index];
                for (uint8_t candidate = 0; candidate < app.catalog.count; ++candidate) {
                    const store_app_t *item = store_catalog_at_const(&app.catalog, candidate);
                    if (text_equal(item->app_id, app.installed[index].app_id) &&
                        has_update(item)) available = 1;
                }
                if (!available) return PXA_EVENT_HANDLED;
                app.screen = STORE_SCREEN_DETAIL;
                app.detail_ready = 0;
                app.detail_state = STORE_IDLE;
                app.update_detail_target = 1;
                ++app.detail_revision;
                app.gallery = {};
                app.screenshot = 0;
                app.technical_expanded = 0;
                app.detail_scroll = 0;
                app.detail = (store_app_t){0};
                copy_text(app.detail.app_id, sizeof(app.detail.app_id),
                          app.installed[index].app_id);
                (void)start_detail_fetch();
                return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
            }
            if (action == 8u && app.installed[index].uninstallable &&
                !text_equal(app.installed[index].app_id, "pxa-store") &&
                app.uninstall_feedback != 1u) {
                app.uninstall_feedback = pxa_store_manage(
                    PXA_STORE_UNINSTALL_REQUEST, STORE_REQUEST_UNINSTALL,
                    app.installed[index].app_id, app.packet,
                    sizeof(app.packet)) ? 1u : 3u;
                return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
            }
        } else if (app.screen == STORE_SCREEN_DOWNLOADS &&
                   index < app.download_count && action == 3u &&
                   app.install_feedback != 1u) {
            copy_text(app.completed_app_id, sizeof(app.completed_app_id),
                      app.downloads[index].app_id);
            app.install_feedback = pxa_store_install_file(STORE_REQUEST_INSTALL,
                                        app.downloads[index].filename,
                                        app.packet, sizeof(app.packet)) ? 1u : 3u;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        } else if (app.screen == STORE_SCREEN_DOWNLOADS &&
                   index < app.download_count && action == 5u) {
            (void)pxa_store_manage(PXA_STORE_DELETE_REQUEST, STORE_REQUEST_DELETE,
                                   app.downloads[index].filename,
                                   app.packet, sizeof(app.packet));
            return PXA_EVENT_HANDLED;
        } else if (app.screen == STORE_SCREEN_DOWNLOADS &&
                   index >= app.download_count &&
                   index < app.download_count + app.failed_count && action == 3u) {
            const uint8_t failed = (uint8_t)(index - app.download_count);
            for (uint8_t next = failed + 1u; next < app.failed_count; ++next)
                copy_text(app.failed_apps[next - 1u], STORE_MAX_APP_ID,
                          app.failed_apps[next]);
            --app.failed_count;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_HANDLED;
    }
    if (ui_event.node == NODE_INSTALL_BUTTON) {
        if (app.install_feedback == 1) return PXA_EVENT_HANDLED;
        const store_app_t *item = detail_item();
        if (installed_for(item->app_id) != NULL && !has_update(item)) {
            (void)pxa_store_manage(PXA_STORE_LAUNCH_REQUEST, STORE_REQUEST_LAUNCH,
                                   item->app_id, app.packet, sizeof(app.packet));
            return PXA_EVENT_HANDLED;
        }
        copy_text(app.completed_app_id, sizeof(app.completed_app_id), item->app_id);
        if (!app.detail_ready || app.busy) {
            app.install_feedback = 3;
            toast_failure(PXA_MSG_STATUS_INSTALL_FAILED, PXA_STATUS_BUSY);
        } else {
            app.install_feedback = 1;
            app.install_refresh = 1;
            if (!start_detail_fetch()) {
                app.install_feedback = 3;
                app.install_refresh = 0;
                app.busy = 0;
                toast_failure(PXA_MSG_STATUS_INSTALL_FAILED, PXA_STATUS_UNAVAILABLE);
            }
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (ui_event.node >= NODE_CARD_BASE &&
        ui_event.node < NODE_CARD_BASE + STORE_MAX_APPS * NODE_CARD_STRIDE &&
        (ui_event.node - NODE_CARD_BASE) % NODE_CARD_STRIDE == 0) {
        uint8_t index =
            (uint8_t)((ui_event.node - NODE_CARD_BASE) / NODE_CARD_STRIDE);
        if (index >= app.catalog.count) return PXA_EVENT_HANDLED;
        if (app.busy && app.pending_request == STORE_REQUEST_UPDATE_CHECK)
            return PXA_EVENT_HANDLED;
        app.selected = index;
        app.detail = *store_catalog_at_const(&app.catalog, index);
        ++app.detail_revision;
        app.detail_ready = 0;
        app.detail_state = STORE_IDLE;
        app.gallery = {};
        app.screenshot = 0;
        app.technical_expanded = 0;
        app.detail_scroll = 0;
        app.install_feedback = 0;
        app.install_refresh = 0;
        app.screen = STORE_SCREEN_DETAIL;
        app.update_detail_target = 0;
        if (app.busy) return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        (void)start_detail_fetch();
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    return PXA_EVENT_UNHANDLED;
}

extern "C" void pxa_app_stop(uint32_t reason) {
    (void)reason;
    store_search_image_stop();
    store::media_stop();
    if (app.client.body_handle) (void)store_client_close_body(&app.client);
    std::free(app.body);
    app.body = nullptr;
    app.body_capacity = 0;
}
