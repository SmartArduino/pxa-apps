#define PXA_LAB_MODULE_PREFIX pxa_lab_storage_
#include "pxa_lab_module.h"

#include "pxa_storage.h"
#include "pxa_ui.h"

#define REQUEST_LIST 1u
#define REQUEST_GET 2u
#define REQUEST_SET 3u
#define REQUEST_REMOVE 4u

#define NODE_WRITE UINT32_C(20)
#define NODE_READ UINT32_C(21)
#define NODE_REFRESH UINT32_C(22)
#define NODE_DELETE UINT32_C(23)
#define NODE_KEY_BASE UINT32_C(100)
#define NODE_KEY_TEXT_BASE UINT32_C(300)

#define MAX_KEYS 14u
#define KEY_CAPACITY (PXA_STORAGE_MAX_KEY_BYTES + 1u)
#define VALUE_TEXT_CAPACITY 72u

enum storage_operation {
    STORAGE_IDLE = 0,
    STORAGE_LISTING_KEYS,
    STORAGE_LOADING_VALUES,
    STORAGE_READING_SELECTED,
    STORAGE_WRITING,
    STORAGE_REMOVING,
    STORAGE_ERROR,
};

enum storage_action {
    STORAGE_ACTION_READY = 0,
    STORAGE_ACTION_WRITE,
    STORAGE_ACTION_READ,
    STORAGE_ACTION_REFRESH,
    STORAGE_ACTION_DELETE,
    STORAGE_ACTION_SELECT,
};

typedef struct {
    char key[KEY_CAPACITY];
    char value[VALUE_TEXT_CAPACITY];
} storage_entry_t;

static uint8_t packet[1664];
static uint8_t storage_payload[128];
static storage_entry_t entries[MAX_KEYS];
static uint32_t demo_value;
static uint32_t pending_value;
static uint8_t entry_count;
static uint8_t selected_index = UINT8_MAX;
static uint8_t loading_index;
static uint8_t operation;
static uint8_t last_action;
static const char *last_error = "存储操作失败";

static size_t string_length(const char *text) {
    size_t length = 0;
    while (text != NULL && text[length] != '\0') ++length;
    return length;
}

static size_t append_text(char *output, size_t capacity, size_t offset,
                          const char *text) {
    size_t index = 0;
    if (output == NULL || capacity == 0) return 0;
    while (text != NULL && text[index] != '\0' && offset + 1u < capacity)
        output[offset++] = text[index++];
    output[offset] = '\0';
    return offset;
}

static size_t append_u32(char *output, size_t capacity, size_t offset,
                         uint32_t value) {
    char digits[10];
    size_t count = 0;
    if (value == 0) return append_text(output, capacity, offset, "0");
    while (value != 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count != 0 && offset + 1u < capacity)
        output[offset++] = digits[--count];
    output[offset] = '\0';
    return offset;
}

static int key_equals(const char *left, const char *right) {
    size_t index = 0;
    while (left[index] == right[index] && left[index] != '\0') ++index;
    return left[index] == right[index];
}

static void clear_entries(void) {
    uint8_t index;
    for (index = 0; index < MAX_KEYS; ++index) {
        entries[index].key[0] = '\0';
        entries[index].value[0] = '\0';
    }
    entry_count = 0;
    selected_index = UINT8_MAX;
    loading_index = 0;
}

static void copy_list_entries(const pxa_storage_list_result_t *result) {
    size_t offset = 0;
    clear_entries();
    while (offset + 4u <= result->records_length && entry_count < MAX_KEYS) {
        const uint16_t length = pxa_read_u16(result->records + offset + 2u);
        const uint8_t *key = result->records + offset + 4u;
        storage_entry_t *entry = &entries[entry_count];
        uint16_t index;
        if (length >= KEY_CAPACITY || offset + 4u + length > result->records_length)
            break;
        for (index = 0; index < length; ++index) entry->key[index] = (char)key[index];
        entry->key[length] = '\0';
        (void)append_text(entry->value, sizeof(entry->value), 0, "读取中...");
        ++entry_count;
        offset += 4u + length;
    }
}

static void set_entry_value(uint8_t index, const pxa_storage_get_result_t *result) {
    storage_entry_t *entry;
    size_t output = 0;
    uint16_t value_index;
    uint8_t printable = 1;
    if (index >= entry_count) return;
    entry = &entries[index];
    if (result->status == PXA_STATUS_NOT_FOUND) {
        (void)append_text(entry->value, sizeof(entry->value), 0, "不存在");
        return;
    }
    if (result->status != PXA_STATUS_OK) {
        (void)append_text(entry->value, sizeof(entry->value), 0, "不可读取");
        return;
    }
    if (result->value_length == 4u) {
        const uint32_t value = pxa_read_u32(result->value);
        (void)append_u32(entry->value, sizeof(entry->value), 0, value);
        if (key_equals(entry->key, "demo.value")) demo_value = value;
        return;
    }
    for (value_index = 0; value_index < result->value_length; ++value_index) {
        const uint8_t byte = result->value[value_index];
        if (byte < 0x20u || byte > 0x7eu) {
            printable = 0;
            break;
        }
    }
    if (!printable) {
        output = append_text(entry->value, sizeof(entry->value), output, "二进制 ");
        output = append_u32(entry->value, sizeof(entry->value), output,
                            result->value_length);
        (void)append_text(entry->value, sizeof(entry->value), output, " bytes");
        return;
    }
    output = append_text(entry->value, sizeof(entry->value), output, "\"");
    for (value_index = 0; value_index < result->value_length &&
         output + 2u < sizeof(entry->value); ++value_index)
        entry->value[output++] = (char)result->value[value_index];
    if (value_index < result->value_length)
        output = append_text(entry->value, sizeof(entry->value), output, "...");
    (void)append_text(entry->value, sizeof(entry->value), output, "\"");
}

static int create_button(pxa_ui_transaction_t *transaction, uint32_t node,
                         uint32_t parent, const char *label, uint8_t primary,
                         uint8_t enabled, uint8_t grow) {
    const uint32_t text_node = node >= NODE_KEY_BASE ?
                                   NODE_KEY_TEXT_BASE + (node - NODE_KEY_BASE) :
                                   node + UINT32_C(1000);
    return pxa_ui_create_typed(transaction, node, parent, 0,
                               PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, 36) &&
           (grow ? pxa_ui_set_u16(transaction, node, PXA_UI_PROPERTY_GROW, 1) :
                   pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_WIDTH,
                                     PXA_UI_LENGTH_FILL, 0)) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_COLUMN) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_RADIUS, 3) &&
           pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_BORDER_WIDTH,
                         primary ? 0 : 1) &&
           pxa_ui_set_theme_color(transaction, node, PXA_UI_PROPERTY_BORDER_COLOR,
                                  PXA_UI_THEME_BORDER) &&
           pxa_ui_set_theme_color(transaction, node, PXA_UI_PROPERTY_BACKGROUND,
                                  primary ? PXA_UI_THEME_PRIMARY :
                                            PXA_UI_THEME_SURFACE) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ENABLED, enabled) &&
           pxa_ui_set_event_mask(transaction, node, PXA_UI_EVENT_MASK_CLICK) &&
           pxa_ui_create(transaction, text_node, node, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, text_node, label, string_length(label)) &&
           pxa_ui_set_font_role(transaction, text_node, PXA_UI_FONT_ROLE_BODY) &&
           pxa_ui_set_theme_color(transaction, text_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  primary ? PXA_UI_THEME_ON_PRIMARY :
                                            PXA_UI_THEME_TEXT);
}

static int create_tool_row(pxa_ui_transaction_t *transaction, uint32_t node) {
    return pxa_ui_create(transaction, node, 5, 0, PXA_UI_NODE_BOX) &&
           pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_STRETCH) &&
           pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_GAP, 6);
}

static const char *status_text(void) {
    if (operation == STORAGE_ERROR) return last_error;
    if (operation == STORAGE_LISTING_KEYS) return "正在列出键";
    if (operation == STORAGE_LOADING_VALUES) return "正在读取键值对";
    if (operation == STORAGE_READING_SELECTED) return "正在读取选中键";
    if (operation == STORAGE_WRITING) return "正在写入 demo.value";
    if (operation == STORAGE_REMOVING) return "正在删除选中键";
    if (last_action == STORAGE_ACTION_WRITE) return "demo.value 已更新";
    if (last_action == STORAGE_ACTION_READ) return "选中键已读取";
    if (last_action == STORAGE_ACTION_DELETE) return "选中键已删除";
    return entry_count == 0 ? "暂无键值对" : "选择键后可读取或删除";
}

static int render(void) {
    char count_label[32];
    char selection_label[KEY_CAPACITY + VALUE_TEXT_CAPACITY + 8];
    char labels[MAX_KEYS][KEY_CAPACITY + VALUE_TEXT_CAPACITY + 8];
    pxa_ui_transaction_t transaction = {0};
    const uint32_t next = pxa_lab_ui_generation + 1u;
    const uint8_t idle = operation == STORAGE_IDLE || operation == STORAGE_ERROR;
    const uint8_t has_selection = selected_index < entry_count;
    uint8_t index;
    size_t offset;
    int ok;

    offset = append_text(count_label, sizeof(count_label), 0, "键值对: ");
    (void)append_u32(count_label, sizeof(count_label), offset, entry_count);
    if (has_selection) {
        offset = append_text(selection_label, sizeof(selection_label), 0,
                             entries[selected_index].key);
        offset = append_text(selection_label, sizeof(selection_label), offset, " = ");
        (void)append_text(selection_label, sizeof(selection_label), offset,
                          entries[selected_index].value);
    } else {
        (void)append_text(selection_label, sizeof(selection_label), 0, "未选择键");
    }
    for (index = 0; index < entry_count; ++index) {
        offset = append_text(labels[index], sizeof(labels[index]), 0, entries[index].key);
        offset = append_text(labels[index], sizeof(labels[index]), offset, " = ");
        (void)append_text(labels[index], sizeof(labels[index]), offset,
                          entries[index].value);
    }

    if (next == 0 || !pxa_ui_transaction_begin(&transaction, next,
            PXA_UI_TRANSACTION_REPLACE_SURFACE, packet, sizeof(packet))) return 0;
    ok = pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) &&
         pxa_ui_set_u8(&transaction, 1, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_theme_color(&transaction, 1, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create(&transaction, 2, 1, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 34) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 2, 12, 4, 12, 4) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE) &&
         pxa_ui_create(&transaction, 3, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 3, "键值存储", sizeof("键值存储") - 1u) &&
         pxa_ui_set_font_role(&transaction, 3, PXA_UI_FONT_ROLE_BODY) &&
         pxa_ui_set_theme_color(&transaction, 3, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 5, 1, 0, PXA_UI_NODE_SCROLL) &&
         pxa_ui_set_length(&transaction, 5, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u16(&transaction, 5, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_SCROLLBAR, 1) &&
         pxa_ui_set_padding(&transaction, 5, 12, 10, 12, 12) &&
         pxa_ui_set_dp(&transaction, 5, PXA_UI_PROPERTY_GAP, 6) &&
         pxa_ui_create(&transaction, 6, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 6, count_label, string_length(count_label)) &&
         pxa_ui_set_font_role(&transaction, 6, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 6, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, 7, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 7, status_text(), string_length(status_text())) &&
         pxa_ui_set_font_role(&transaction, 7, PXA_UI_FONT_ROLE_BODY) &&
         pxa_ui_set_theme_color(&transaction, 7, PXA_UI_PROPERTY_FOREGROUND,
                                operation == STORAGE_ERROR ? PXA_UI_THEME_DANGER :
                                idle ? PXA_UI_THEME_TEXT : PXA_UI_THEME_WARNING);
    for (index = 0; ok && index < entry_count; ++index)
        ok = create_button(&transaction, NODE_KEY_BASE + index, 5, labels[index],
                           (uint8_t)(has_selection && selected_index == index),
                           operation == STORAGE_IDLE, 0);
    ok = ok && pxa_ui_create(&transaction, 8, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 8, selection_label,
                         string_length(selection_label)) &&
         pxa_ui_set_font_role(&transaction, 8, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 8, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         create_tool_row(&transaction, 10) &&
         create_button(&transaction, NODE_WRITE, 10, "写入 demo +1", 1, idle, 1) &&
         create_button(&transaction, NODE_READ, 10, "读取选中", 0,
                       (uint8_t)(idle && has_selection), 1) &&
         create_tool_row(&transaction, 11) &&
         create_button(&transaction, NODE_DELETE, 11, "删除选中", 0,
                       (uint8_t)(idle && has_selection), 1) &&
         create_button(&transaction, NODE_REFRESH, 11, "刷新", 0, idle, 1);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    pxa_lab_ui_generation = next;
    return 1;
}

static int request_list(void) {
    operation = STORAGE_LISTING_KEYS;
    return pxa_storage_list(REQUEST_LIST, NULL, 0, storage_payload,
                            sizeof(storage_payload), packet, sizeof(packet));
}

static int request_load_next(void) {
    if (loading_index >= entry_count) {
        uint8_t index;
        for (index = 0; index < entry_count; ++index) {
            if (key_equals(entries[index].key, "demo.value")) {
                selected_index = index;
                break;
            }
        }
        if (selected_index == UINT8_MAX && entry_count != 0) selected_index = 0;
        operation = STORAGE_IDLE;
        return 1;
    }
    operation = STORAGE_LOADING_VALUES;
    return pxa_storage_get(REQUEST_GET, entries[loading_index].key,
                           string_length(entries[loading_index].key),
                           storage_payload, sizeof(storage_payload), packet,
                           sizeof(packet));
}

static int request_read_selected(void) {
    if (selected_index >= entry_count) return 0;
    operation = STORAGE_READING_SELECTED;
    return pxa_storage_get(REQUEST_GET, entries[selected_index].key,
                           string_length(entries[selected_index].key),
                           storage_payload, sizeof(storage_payload), packet,
                           sizeof(packet));
}

static int request_set(void) {
    static const char key[] = "demo.value";
    uint8_t encoded[4];
    pending_value = demo_value + 1u;
    encoded[0] = (uint8_t)pending_value;
    encoded[1] = (uint8_t)(pending_value >> 8);
    encoded[2] = (uint8_t)(pending_value >> 16);
    encoded[3] = (uint8_t)(pending_value >> 24);
    operation = STORAGE_WRITING;
    return pxa_storage_set(REQUEST_SET, key, sizeof(key) - 1u, encoded,
                           sizeof(encoded), storage_payload, sizeof(storage_payload),
                           packet, sizeof(packet));
}

static int request_remove_selected(void) {
    if (selected_index >= entry_count) return 0;
    operation = STORAGE_REMOVING;
    return pxa_storage_remove(REQUEST_REMOVE, entries[selected_index].key,
                              string_length(entries[selected_index].key),
                              storage_payload, sizeof(storage_payload), packet,
                              sizeof(packet));
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    clear_entries();
    demo_value = 0;
    last_action = STORAGE_ACTION_READY;
    last_error = "存储操作失败";
    return pxa_window_fullscreen() && request_list() && render()
               ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND &&
        (operation == STORAGE_IDLE || operation == STORAGE_ERROR)) {
        int accepted = 0;
        if (ui_event.node >= NODE_KEY_BASE &&
            ui_event.node < NODE_KEY_BASE + entry_count && operation == STORAGE_IDLE) {
            selected_index = (uint8_t)(ui_event.node - NODE_KEY_BASE);
            last_action = STORAGE_ACTION_SELECT;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (ui_event.node == NODE_WRITE) {
            last_action = STORAGE_ACTION_WRITE;
            accepted = request_set();
        } else if (ui_event.node == NODE_READ) {
            last_action = STORAGE_ACTION_READ;
            accepted = request_read_selected();
        } else if (ui_event.node == NODE_DELETE) {
            last_action = STORAGE_ACTION_DELETE;
            accepted = request_remove_selected();
        } else if (ui_event.node == NODE_REFRESH) {
            last_action = STORAGE_ACTION_REFRESH;
            accepted = request_list();
        } else {
            return PXA_EVENT_UNHANDLED;
        }
        if (!accepted) {
            last_error = "存储请求未被接受";
            operation = STORAGE_ERROR;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service != PXA_SERVICE_STORAGE) return PXA_EVENT_UNHANDLED;
    if (parsed.opcode == PXA_STORAGE_LIST && parsed.request_id == REQUEST_LIST) {
        pxa_storage_list_result_t result;
        if (!pxa_storage_parse_list(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK) {
            last_error = "无法列出键";
            operation = STORAGE_ERROR;
        } else {
            copy_list_entries(&result);
            if (!request_load_next()) {
                last_error = "无法读取键值对";
                operation = STORAGE_ERROR;
            }
        }
    } else if (parsed.opcode == PXA_STORAGE_GET && parsed.request_id == REQUEST_GET) {
        pxa_storage_get_result_t result;
        if (!pxa_storage_parse_get(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (operation == STORAGE_LOADING_VALUES) {
            set_entry_value(loading_index, &result);
            ++loading_index;
            if (!request_load_next()) {
                last_error = "无法继续读取键值对";
                operation = STORAGE_ERROR;
            }
        } else if (operation == STORAGE_READING_SELECTED) {
            set_entry_value(selected_index, &result);
            operation = STORAGE_IDLE;
        } else {
            return PXA_EVENT_UNHANDLED;
        }
    } else if (parsed.opcode == PXA_STORAGE_SET && parsed.request_id == REQUEST_SET) {
        int32_t status;
        if (!pxa_storage_parse_status(&parsed, PXA_STORAGE_SET, &status))
            return PXA_STATUS_INTERNAL;
        if (status != PXA_STATUS_OK) {
            last_error = "写入 demo.value 失败";
            operation = STORAGE_ERROR;
        } else {
            demo_value = pending_value;
            if (!request_list()) {
                last_error = "写入后无法刷新列表";
                operation = STORAGE_ERROR;
            }
        }
    } else if (parsed.opcode == PXA_STORAGE_REMOVE &&
               parsed.request_id == REQUEST_REMOVE) {
        int32_t status;
        if (!pxa_storage_parse_status(&parsed, PXA_STORAGE_REMOVE, &status))
            return PXA_STATUS_INTERNAL;
        if (status != PXA_STATUS_OK && status != PXA_STATUS_NOT_FOUND) {
            last_error = "删除键失败";
            operation = STORAGE_ERROR;
        } else if (!request_list()) {
            last_error = "删除后无法刷新列表";
            operation = STORAGE_ERROR;
        }
    } else {
        return PXA_EVENT_UNHANDLED;
    }
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
