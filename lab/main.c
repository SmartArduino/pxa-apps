#include "pxa_ui.h"
#include "pxa_app_messages.h"
#include "pxa_i18n.h"

typedef int32_t (*pxa_lab_start_fn)(const uint8_t *config, uint32_t config_length);
typedef int32_t (*pxa_lab_event_fn)(const uint8_t *event, uint32_t length);
typedef void (*pxa_lab_stop_fn)(uint32_t reason);

typedef struct {
    pxa_i18n_message_id_t name;
    pxa_i18n_message_id_t description;
    pxa_lab_start_fn start;
    pxa_lab_event_fn on_event;
    pxa_lab_stop_fn stop;
} pxa_lab_module_t;

#define PXA_LAB_DECLARE(name) \
    int32_t pxa_lab_##name##_start(const uint8_t *config, uint32_t config_length); \
    int32_t pxa_lab_##name##_on_event(const uint8_t *event, uint32_t length); \
    void pxa_lab_##name##_stop(uint32_t reason)

PXA_LAB_DECLARE(audio);
PXA_LAB_DECLARE(alpha_surface);
PXA_LAB_DECLARE(benchmark);
PXA_LAB_DECLARE(controller);
PXA_LAB_DECLARE(fs);
PXA_LAB_DECLARE(ipc);
PXA_LAB_DECLARE(kv);
PXA_LAB_DECLARE(lease);
PXA_LAB_DECLARE(light);
PXA_LAB_DECLARE(memory);
PXA_LAB_DECLARE(net);
PXA_LAB_DECLARE(permission);
PXA_LAB_DECLARE(rgb565);
PXA_LAB_DECLARE(sensor);
PXA_LAB_DECLARE(storage);
PXA_LAB_DECLARE(surface);
PXA_LAB_DECLARE(wasi);
PXA_LAB_DECLARE(work);

#define PXA_LAB_MODULE(name, title, detail) \
    {title, detail, pxa_lab_##name##_start, pxa_lab_##name##_on_event, pxa_lab_##name##_stop}

static const pxa_lab_module_t modules[] = {
    PXA_LAB_MODULE(audio, PXA_MSG_MODULE_AUDIO_NAME, PXA_MSG_MODULE_AUDIO_DESCRIPTION),
    PXA_LAB_MODULE(benchmark, PXA_MSG_MODULE_BENCHMARK_NAME, PXA_MSG_MODULE_BENCHMARK_DESCRIPTION),
    PXA_LAB_MODULE(fs, PXA_MSG_MODULE_FS_NAME, PXA_MSG_MODULE_FS_DESCRIPTION),
    PXA_LAB_MODULE(ipc, PXA_MSG_MODULE_IPC_NAME, PXA_MSG_MODULE_IPC_DESCRIPTION),
    PXA_LAB_MODULE(kv, PXA_MSG_MODULE_KV_NAME, PXA_MSG_MODULE_KV_DESCRIPTION),
    PXA_LAB_MODULE(lease, PXA_MSG_MODULE_LEASE_NAME, PXA_MSG_MODULE_LEASE_DESCRIPTION),
    PXA_LAB_MODULE(light, PXA_MSG_MODULE_LIGHT_NAME, PXA_MSG_MODULE_LIGHT_DESCRIPTION),
    PXA_LAB_MODULE(memory, PXA_MSG_MODULE_MEMORY_NAME, PXA_MSG_MODULE_MEMORY_DESCRIPTION),
    PXA_LAB_MODULE(net, PXA_MSG_MODULE_NET_NAME, PXA_MSG_MODULE_NET_DESCRIPTION),
    PXA_LAB_MODULE(permission, PXA_MSG_MODULE_PERMISSION_NAME, PXA_MSG_MODULE_PERMISSION_DESCRIPTION),
    PXA_LAB_MODULE(sensor, PXA_MSG_MODULE_SENSOR_NAME, PXA_MSG_MODULE_SENSOR_DESCRIPTION),
    PXA_LAB_MODULE(storage, PXA_MSG_MODULE_STORAGE_NAME, PXA_MSG_MODULE_STORAGE_DESCRIPTION),
    PXA_LAB_MODULE(surface, PXA_MSG_MODULE_SURFACE_NAME, PXA_MSG_MODULE_SURFACE_DESCRIPTION),
    PXA_LAB_MODULE(alpha_surface, PXA_MSG_MODULE_ALPHA_SURFACE_NAME, PXA_MSG_MODULE_ALPHA_SURFACE_DESCRIPTION),
    PXA_LAB_MODULE(wasi, PXA_MSG_MODULE_WASI_NAME, PXA_MSG_MODULE_WASI_DESCRIPTION),
    PXA_LAB_MODULE(work, PXA_MSG_MODULE_WORK_NAME, PXA_MSG_MODULE_WORK_DESCRIPTION),
    PXA_LAB_MODULE(rgb565, PXA_MSG_MODULE_RGB565_NAME, PXA_MSG_MODULE_RGB565_DESCRIPTION),
    PXA_LAB_MODULE(controller, PXA_MSG_MODULE_CONTROLLER_NAME, PXA_MSG_MODULE_CONTROLLER_DESCRIPTION)
};

#define PXA_LAB_MODULE_COUNT ((uint8_t)(sizeof(modules) / sizeof(modules[0])))
#define PXA_LAB_MENU_LIST_NODE UINT32_C(5)
#define PXA_LAB_MENU_ITEM_NODE_BASE UINT32_C(20)
#define PXA_LAB_MENU_TITLE_NODE_BASE UINT32_C(40)
#define PXA_LAB_MENU_DETAIL_NODE_BASE UINT32_C(60)

static uint8_t packet[2304];
uint32_t pxa_lab_ui_generation;
static uint8_t selected_module;
static uint8_t active_module = UINT8_MAX;
static int32_t menu_scroll_y;
static pxa_i18n_t i18n;

const char *pxa_lab_message(pxa_i18n_message_id_t id) {
    return pxa_i18n_cstr(&i18n, id);
}

static size_t string_length(const char *value) {
    size_t size = 0;
    while (value != NULL && value[size] != '\0') ++size;
    return size;
}

static int create_menu_item(pxa_ui_transaction_t *transaction,
                            uint8_t module_index) {
    const pxa_lab_module_t *module = &modules[module_index];
    const uint32_t item_node = PXA_LAB_MENU_ITEM_NODE_BASE + module_index;
    const uint32_t title_node = PXA_LAB_MENU_TITLE_NODE_BASE + module_index;
    const uint32_t detail_node = PXA_LAB_MENU_DETAIL_NODE_BASE + module_index;

    return pxa_ui_create_typed(transaction, item_node, PXA_LAB_MENU_LIST_NODE,
                               0, PXA_UI_NODE_CONTROL,
                               PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_length(transaction, item_node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_length(transaction, item_node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, 58) &&
           pxa_ui_set_u8(transaction, item_node, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_COLUMN) &&
           pxa_ui_set_u8(transaction, item_node, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, item_node, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_START) &&
           pxa_ui_set_padding(transaction, item_node, 12, 7, 12, 7) &&
           pxa_ui_set_dp(transaction, item_node, PXA_UI_PROPERTY_RADIUS, 6) &&
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
           pxa_ui_create(transaction, title_node, item_node, 0,
                         PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, title_node,
                           pxa_lab_message(module->name),
                           pxa_i18n_size(&i18n, module->name)) &&
           pxa_ui_set_font_role(transaction, title_node,
                                PXA_UI_FONT_ROLE_BODY) &&
           pxa_ui_set_theme_color(transaction, title_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_TEXT) &&
           pxa_ui_create(transaction, detail_node, item_node, 0,
                         PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, detail_node,
                           pxa_lab_message(module->description),
                           pxa_i18n_size(&i18n, module->description)) &&
           pxa_ui_set_font_role(transaction, detail_node,
                                PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, detail_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED);
}

static int render_menu(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t next = pxa_lab_ui_generation + 1u;
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
                           PXA_UI_LENGTH_PX, 68) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 2, 22, 14, 14, 6) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 3, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 3,
                         pxa_lab_message(PXA_MSG_SCREEN_TITLE),
                         pxa_i18n_size(&i18n, PXA_MSG_SCREEN_TITLE)) &&
         pxa_ui_set_font_role(&transaction, 3, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 3, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 4, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 4,
                         pxa_lab_message(PXA_MSG_SCREEN_SUBTITLE),
                         pxa_i18n_size(&i18n, PXA_MSG_SCREEN_SUBTITLE)) &&
         pxa_ui_set_font_role(&transaction, 4, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 4, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, PXA_LAB_MENU_LIST_NODE, 1, 0,
                       PXA_UI_NODE_SCROLL) &&
         pxa_ui_set_length(&transaction, PXA_LAB_MENU_LIST_NODE,
                           PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u16(&transaction, PXA_LAB_MENU_LIST_NODE,
                        PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, PXA_LAB_MENU_LIST_NODE,
                       PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, PXA_LAB_MENU_LIST_NODE,
                       PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
         pxa_ui_set_u8(&transaction, PXA_LAB_MENU_LIST_NODE,
                       PXA_UI_PROPERTY_SCROLLBAR, 1) &&
         pxa_ui_set_event_mask(&transaction, PXA_LAB_MENU_LIST_NODE,
                               PXA_UI_EVENT_MASK_SCROLL) &&
         pxa_ui_set_padding(&transaction, PXA_LAB_MENU_LIST_NODE,
                            12, 10, 12, 12) &&
         pxa_ui_set_dp(&transaction, PXA_LAB_MENU_LIST_NODE,
                       PXA_UI_PROPERTY_GAP, 7);
    for (index = 0; ok && index < PXA_LAB_MODULE_COUNT; ++index)
        ok = create_menu_item(&transaction, index);
    if (ok && menu_scroll_y > 0)
        ok = pxa_ui_set_dp(&transaction, PXA_LAB_MENU_LIST_NODE,
                           PXA_UI_PROPERTY_SCROLL_POSITION, menu_scroll_y);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    pxa_lab_ui_generation = next;
    return 1;
}

static int open_selected(const uint8_t *config, uint32_t config_length) {
    const pxa_lab_module_t *module = &modules[selected_module];
    int32_t status = module->start(config, config_length);
    if (status != PXA_STATUS_OK) return 0;
    active_module = selected_module;
    return 1;
}

static int return_to_menu(void) {
    if (active_module != UINT8_MAX) {
        const uint8_t previous_module = active_module;
        if (!render_menu()) return 0;
        modules[previous_module].stop(0);
        active_module = UINT8_MAX;
        return 1;
    }
    return render_menu();
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)pxa_i18n_init_from_start_config(
        &i18n, &pxa_app_i18n_bundle, config, config_length);
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
            if (locale_result == 1 && active_module != UINT8_MAX) {
                int32_t status;
                modules[active_module].stop(0);
                status = modules[active_module].start(NULL, 0);
                return status == PXA_STATUS_OK ? PXA_EVENT_HANDLED : status;
            }
            return locale_result == 1 && !render_menu()
                       ? PXA_STATUS_INTERNAL : PXA_EVENT_HANDLED;
        }
    }
    if (active_module != UINT8_MAX) {
        if (parsed.service == PXA_SERVICE_WINDOW &&
            parsed.opcode == PXA_WINDOW_BACK_REQUESTED) {
            return return_to_menu() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return modules[active_module].on_event(event, length);
    }
    if (!pxa_ui_parse_event(&parsed, &ui_event)) return PXA_EVENT_UNHANDLED;
    if (ui_event.node == PXA_LAB_MENU_LIST_NODE &&
        ui_event.kind == PXA_UI_EVENT_SCROLL_KIND) {
        menu_scroll_y = ui_event.value > 0 ? ui_event.value : 0;
        return PXA_EVENT_HANDLED;
    }
    if (ui_event.node >= PXA_LAB_MENU_ITEM_NODE_BASE &&
        ui_event.node < PXA_LAB_MENU_ITEM_NODE_BASE + PXA_LAB_MODULE_COUNT &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND) {
        selected_module = (uint8_t)(ui_event.node -
                                    PXA_LAB_MENU_ITEM_NODE_BASE);
        return open_selected(NULL, 0) ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    if (active_module != UINT8_MAX)
        modules[active_module].stop(reason);
}
