#define PXA_LAB_MODULE_PREFIX pxa_lab_fs_
#include "pxa_lab_module.h"

#include "pxa_fs.h"
#include "pxa_ui.h"

#define REQUEST_MKDIR_ROOT 1u
#define REQUEST_OPEN_DIRECTORY 2u
#define REQUEST_READ_DIRECTORY 3u
#define REQUEST_CREATE_FILE 4u
#define REQUEST_READ_FILE 5u
#define REQUEST_REMOVE 6u
#define REQUEST_CREATE_DIRECTORY 7u

#define NODE_NEW_FILE UINT32_C(20)
#define NODE_NEW_DIRECTORY UINT32_C(21)
#define NODE_OPEN UINT32_C(22)
#define NODE_REMOVE UINT32_C(23)
#define NODE_UP UINT32_C(24)
#define NODE_REFRESH UINT32_C(25)
#define NODE_ENTRY_BASE UINT32_C(100)
#define NODE_ENTRY_TEXT_BASE UINT32_C(300)

#define ROOT_DIRECTORY "lab-files"
#define MAX_ENTRIES 12u
#define NAME_CAPACITY 65u
#define PATH_CAPACITY 256u
#define PREVIEW_CAPACITY 120u

enum browser_state {
    BROWSER_IDLE = 0,
    BROWSER_MAKING_ROOT,
    BROWSER_OPENING_DIRECTORY,
    BROWSER_READING_DIRECTORY,
    BROWSER_CREATING_FILE,
    BROWSER_CREATING_DIRECTORY,
    BROWSER_READING_FILE,
    BROWSER_REMOVING,
    BROWSER_ERROR,
};

enum browser_action {
    BROWSER_ACTION_READY = 0,
    BROWSER_ACTION_LIST,
    BROWSER_ACTION_SELECT,
    BROWSER_ACTION_OPEN,
    BROWSER_ACTION_CREATE_FILE,
    BROWSER_ACTION_CREATE_DIRECTORY,
    BROWSER_ACTION_READ,
    BROWSER_ACTION_REMOVE,
    BROWSER_ACTION_UP,
};

typedef struct {
    char name[NAME_CAPACITY];
    uint64_t size;
    uint8_t kind;
} browser_entry_t;

static uint8_t packet[1664];
static uint8_t fs_payload[128];
static uint8_t read_buffer[96];
static browser_entry_t entries[MAX_ENTRIES];
static uint32_t directory_handle;
static uint32_t next_item_number;
static uint8_t entry_count;
static uint8_t state;
static uint8_t last_action;
static uint8_t selected_kind;
static char current_directory[PATH_CAPACITY] = ROOT_DIRECTORY;
static char selected_name[NAME_CAPACITY];
static char selected_path[PATH_CAPACITY];
static char preview[PREVIEW_CAPACITY] = "选择文件或文件夹";
static const char *last_error = "文件系统操作失败";

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

static void set_preview(const char *text) {
    (void)append_text(preview, sizeof(preview), 0, text);
}

static void clear_selection(void) {
    selected_name[0] = '\0';
    selected_path[0] = '\0';
    selected_kind = 0;
}

static int make_child_path(char *output, size_t capacity, const char *name) {
    const size_t directory_length = string_length(current_directory);
    const size_t name_length = string_length(name);
    size_t index;
    if (directory_length == 0 || name_length == 0 ||
        directory_length + 1u + name_length >= capacity) return 0;
    for (index = 0; index < directory_length; ++index)
        output[index] = current_directory[index];
    output[directory_length] = '/';
    for (index = 0; index < name_length; ++index)
        output[directory_length + 1u + index] = name[index];
    output[directory_length + 1u + name_length] = '\0';
    return 1;
}

static void select_entry(uint8_t index) {
    if (index >= entry_count) return;
    (void)append_text(selected_name, sizeof(selected_name), 0, entries[index].name);
    if (!make_child_path(selected_path, sizeof(selected_path), selected_name)) {
        clear_selection();
        return;
    }
    selected_kind = entries[index].kind;
}

static int selected_entry_is(uint8_t index) {
    size_t character = 0;
    if (index >= entry_count || selected_name[0] == '\0') return 0;
    while (entries[index].name[character] == selected_name[character] &&
           entries[index].name[character] != '\0') ++character;
    return entries[index].name[character] == selected_name[character];
}

static int selected_entry_is_listed(void) {
    uint8_t index;
    for (index = 0; index < entry_count; ++index)
        if (selected_entry_is(index)) return 1;
    return 0;
}

static void note_item_number(const pxa_fs_directory_result_t *result,
                             const char *prefix, const char *suffix) {
    const size_t prefix_length = string_length(prefix);
    const size_t suffix_length = string_length(suffix);
    const size_t digits_end = result->name_length - suffix_length;
    uint32_t value = 0;
    size_t index;
    if (result->name_length <= prefix_length + suffix_length) return;
    for (index = 0; index < prefix_length; ++index)
        if (result->name[index] != (uint8_t)prefix[index]) return;
    for (index = digits_end; index < result->name_length; ++index)
        if (result->name[index] != (uint8_t)suffix[index - digits_end]) return;
    for (index = prefix_length; index < digits_end; ++index) {
        const uint8_t digit = result->name[index];
        if (digit < '0' || digit > '9' ||
            value > (UINT32_MAX - (uint32_t)(digit - '0')) / 10u) return;
        value = value * 10u + (uint32_t)(digit - '0');
    }
    if (value > next_item_number) next_item_number = value;
}

static void add_entry(const pxa_fs_directory_result_t *result) {
    browser_entry_t *entry;
    uint16_t index;
    if (result->kind == PXA_FS_KIND_REGULAR)
        note_item_number(result, "file-", ".txt");
    else if (result->kind == PXA_FS_KIND_DIRECTORY)
        note_item_number(result, "folder-", "");
    if (entry_count >= MAX_ENTRIES || result->name_length >= NAME_CAPACITY) return;
    entry = &entries[entry_count++];
    for (index = 0; index < result->name_length; ++index)
        entry->name[index] = (char)result->name[index];
    entry->name[result->name_length] = '\0';
    entry->kind = result->kind;
    entry->size = result->size;
}

static void set_read_preview(int32_t length) {
    size_t output = append_text(preview, sizeof(preview), 0, "内容: ");
    int32_t index;
    for (index = 0; index < length && output + 1u < sizeof(preview); ++index) {
        const uint8_t value = read_buffer[index];
        preview[output++] = value >= 0x20u && value <= 0x7eu ?
                            (char)value : ' ';
    }
    preview[output] = '\0';
}

static void make_entry_label(char *output, size_t capacity,
                             const browser_entry_t *entry) {
    size_t offset = append_text(output, capacity, 0,
                                entry->kind == PXA_FS_KIND_DIRECTORY ?
                                    "[文件夹] " : "[文件] ");
    offset = append_text(output, capacity, offset, entry->name);
    if (entry->kind == PXA_FS_KIND_REGULAR) {
        offset = append_text(output, capacity, offset, "  ");
        if (entry->size > UINT32_MAX)
            (void)append_text(output, capacity, offset, ">4GB");
        else {
            offset = append_u32(output, capacity, offset, (uint32_t)entry->size);
            (void)append_text(output, capacity, offset, " B");
        }
    }
}

static int create_button(pxa_ui_transaction_t *transaction, uint32_t node,
                         uint32_t parent, const char *label, uint8_t primary,
                         uint8_t enabled, uint8_t grow) {
    const uint32_t text_node = node >= NODE_ENTRY_BASE ?
                                   NODE_ENTRY_TEXT_BASE + (node - NODE_ENTRY_BASE) :
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
    if (state == BROWSER_ERROR) return last_error;
    if (state == BROWSER_MAKING_ROOT) return "正在准备私有目录";
    if (state == BROWSER_OPENING_DIRECTORY || state == BROWSER_READING_DIRECTORY)
        return "正在读取目录";
    if (state == BROWSER_CREATING_FILE) return "正在新建文件";
    if (state == BROWSER_CREATING_DIRECTORY) return "正在新建文件夹";
    if (state == BROWSER_READING_FILE) return "正在读取文件";
    if (state == BROWSER_REMOVING) return "正在删除";
    if (last_action == BROWSER_ACTION_CREATE_FILE) return "文件已新建";
    if (last_action == BROWSER_ACTION_CREATE_DIRECTORY) return "文件夹已新建";
    if (last_action == BROWSER_ACTION_REMOVE) return "已删除选中项";
    if (last_action == BROWSER_ACTION_UP) return "已返回上级目录";
    if (last_action == BROWSER_ACTION_READ) return "文件内容已读取";
    return entry_count == 0 ? "此文件夹为空" : "选择项目后可打开或删除";
}

static int at_root_directory(void) {
    static const char root[] = ROOT_DIRECTORY;
    size_t index = 0;
    while (current_directory[index] == root[index] && root[index] != '\0') ++index;
    return current_directory[index] == root[index];
}

static int render(void) {
    char path_label[PATH_CAPACITY + 12];
    char selection_label[NAME_CAPACITY + 24];
    char entry_labels[MAX_ENTRIES][NAME_CAPACITY + 20];
    pxa_ui_transaction_t transaction = {0};
    const uint32_t next = pxa_lab_ui_generation + 1u;
    const uint8_t idle = state == BROWSER_IDLE || state == BROWSER_ERROR;
    const uint8_t has_selection = selected_path[0] != '\0';
    const char *selected_type = selected_kind == PXA_FS_KIND_DIRECTORY ?
                                    "文件夹" : "文件";
    uint8_t index;
    size_t offset;
    int ok;

    offset = append_text(path_label, sizeof(path_label), 0, "路径: /");
    (void)append_text(path_label, sizeof(path_label), offset, current_directory);
    offset = append_text(selection_label, sizeof(selection_label), 0,
                         has_selection ? selected_type : "未选择");
    if (has_selection) {
        offset = append_text(selection_label, sizeof(selection_label), offset, ": ");
        (void)append_text(selection_label, sizeof(selection_label), offset,
                          selected_name);
    }
    for (index = 0; index < entry_count; ++index)
        make_entry_label(entry_labels[index], sizeof(entry_labels[index]),
                         &entries[index]);

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
         pxa_ui_set_text(&transaction, 3, "私有文件", sizeof("私有文件") - 1u) &&
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
         pxa_ui_set_text(&transaction, 6, path_label, string_length(path_label)) &&
         pxa_ui_set_font_role(&transaction, 6, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 6, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, 7, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 7, status_text(), string_length(status_text())) &&
         pxa_ui_set_font_role(&transaction, 7, PXA_UI_FONT_ROLE_BODY) &&
         pxa_ui_set_theme_color(&transaction, 7, PXA_UI_PROPERTY_FOREGROUND,
                                state == BROWSER_ERROR ? PXA_UI_THEME_DANGER :
                                idle ? PXA_UI_THEME_TEXT : PXA_UI_THEME_WARNING);
    for (index = 0; ok && index < entry_count; ++index)
        ok = create_button(&transaction, NODE_ENTRY_BASE + index, 5,
                           entry_labels[index],
                           (uint8_t)(has_selection && selected_entry_is(index)),
                           state == BROWSER_IDLE, 0);
    ok = ok && pxa_ui_create(&transaction, 8, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 8, selection_label,
                         string_length(selection_label)) &&
         pxa_ui_set_font_role(&transaction, 8, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 8, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, 9, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 9, preview, string_length(preview)) &&
         pxa_ui_set_font_role(&transaction, 9, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 9, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         create_tool_row(&transaction, 10) &&
         create_button(&transaction, NODE_NEW_FILE, 10, "新建文件", 1, idle, 1) &&
         create_button(&transaction, NODE_NEW_DIRECTORY, 10, "新建文件夹", 0,
                       idle, 1) &&
         create_tool_row(&transaction, 11) &&
         create_button(&transaction, NODE_OPEN, 11, "打开", 0,
                       (uint8_t)(idle && has_selection), 1) &&
         create_button(&transaction, NODE_REMOVE, 11, "删除", 0,
                       (uint8_t)(idle && has_selection), 1) &&
         create_tool_row(&transaction, 12) &&
         create_button(&transaction, NODE_UP, 12, "上级目录", 0,
                       (uint8_t)(idle && !at_root_directory()), 1) &&
         create_button(&transaction, NODE_REFRESH, 12, "刷新", 0, idle, 1);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    pxa_lab_ui_generation = next;
    return 1;
}

static int request_make_root(void) {
    static const char root[] = ROOT_DIRECTORY;
    state = BROWSER_MAKING_ROOT;
    return pxa_fs_make_directory(REQUEST_MKDIR_ROOT, root, sizeof(root) - 1u,
                                 fs_payload, sizeof(fs_payload), packet,
                                 sizeof(packet));
}

static int request_list(void) {
    uint8_t index;
    for (index = 0; index < MAX_ENTRIES; ++index) entries[index].name[0] = '\0';
    entry_count = 0;
    directory_handle = 0;
    state = BROWSER_OPENING_DIRECTORY;
    return pxa_fs_open(REQUEST_OPEN_DIRECTORY, current_directory,
                       string_length(current_directory),
                       PXA_FS_OPEN_READ | PXA_FS_OPEN_DIRECTORY, fs_payload,
                       sizeof(fs_payload), packet, sizeof(packet));
}

static int build_new_name(const char *prefix, const char *suffix) {
    size_t offset;
    if (next_item_number == UINT32_MAX) return 0;
    ++next_item_number;
    offset = append_text(selected_name, sizeof(selected_name), 0, prefix);
    offset = append_u32(selected_name, sizeof(selected_name), offset,
                        next_item_number);
    (void)append_text(selected_name, sizeof(selected_name), offset, suffix);
    return make_child_path(selected_path, sizeof(selected_path), selected_name);
}

static int request_create_file(void) {
    if (!build_new_name("file-", ".txt")) return 0;
    selected_kind = PXA_FS_KIND_REGULAR;
    state = BROWSER_CREATING_FILE;
    return pxa_fs_open(REQUEST_CREATE_FILE, selected_path,
                       string_length(selected_path),
                       PXA_FS_OPEN_WRITE | PXA_FS_OPEN_CREATE |
                           PXA_FS_OPEN_EXCLUSIVE,
                       fs_payload, sizeof(fs_payload), packet, sizeof(packet));
}

static int request_create_directory(void) {
    if (!build_new_name("folder-", "")) return 0;
    selected_kind = PXA_FS_KIND_DIRECTORY;
    state = BROWSER_CREATING_DIRECTORY;
    return pxa_fs_make_directory(REQUEST_CREATE_DIRECTORY, selected_path,
                                 string_length(selected_path), fs_payload,
                                 sizeof(fs_payload), packet, sizeof(packet));
}

static int request_read_file(void) {
    if (selected_path[0] == '\0' || selected_kind != PXA_FS_KIND_REGULAR) return 0;
    state = BROWSER_READING_FILE;
    return pxa_fs_open(REQUEST_READ_FILE, selected_path, string_length(selected_path),
                       PXA_FS_OPEN_READ, fs_payload, sizeof(fs_payload), packet,
                       sizeof(packet));
}

static int request_remove(void) {
    if (selected_path[0] == '\0') return 0;
    state = BROWSER_REMOVING;
    return pxa_fs_remove(REQUEST_REMOVE, selected_path, string_length(selected_path),
                         fs_payload, sizeof(fs_payload), packet, sizeof(packet));
}

static int request_go_up(void) {
    size_t length = string_length(current_directory);
    size_t name_start;
    size_t index;
    if (at_root_directory()) return 0;
    name_start = length;
    while (name_start != 0 && current_directory[name_start - 1u] != '/')
        --name_start;
    if (name_start == 0 || length - name_start >= sizeof(selected_name)) return 0;
    for (index = 0; index < length - name_start; ++index)
        selected_name[index] = current_directory[name_start + index];
    selected_name[length - name_start] = '\0';
    (void)append_text(selected_path, sizeof(selected_path), 0, current_directory);
    selected_kind = PXA_FS_KIND_DIRECTORY;
    while (length != 0 && current_directory[length - 1u] != '/') --length;
    if (length == 0) return 0;
    current_directory[length - 1u] = '\0';
    state = BROWSER_OPENING_DIRECTORY;
    return request_list();
}

static int request_open_selected(void) {
    if (selected_path[0] == '\0') return 0;
    if (selected_kind == PXA_FS_KIND_REGULAR) {
        last_action = BROWSER_ACTION_READ;
        return request_read_file();
    }
    if (selected_kind != PXA_FS_KIND_DIRECTORY ||
        string_length(selected_path) >= sizeof(current_directory)) return 0;
    (void)append_text(current_directory, sizeof(current_directory), 0, selected_path);
    clear_selection();
    set_preview("已进入文件夹");
    last_action = BROWSER_ACTION_OPEN;
    return request_list();
}

static void close_directory(void) {
    if (directory_handle != 0)
        (void)pxa_fs_close(directory_handle, packet, sizeof(packet));
    directory_handle = 0;
}

static void finish_directory_listing(void) {
    if (directory_handle != 0 && !pxa_fs_close(directory_handle, packet,
                                                sizeof(packet))) {
        directory_handle = 0;
        last_error = "关闭目录失败";
        state = BROWSER_ERROR;
        return;
    }
    directory_handle = 0;
    if (!selected_entry_is_listed()) clear_selection();
    state = BROWSER_IDLE;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    (void)append_text(current_directory, sizeof(current_directory), 0,
                      ROOT_DIRECTORY);
    clear_selection();
    entry_count = 0;
    next_item_number = 0;
    last_action = BROWSER_ACTION_READY;
    last_error = "文件系统操作失败";
    set_preview("选择项目后使用下方工具栏操作");
    if (!pxa_window_fullscreen() || !request_make_root() || !render())
        return PXA_STATUS_INTERNAL;
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND &&
        (state == BROWSER_IDLE || state == BROWSER_ERROR)) {
        int accepted = 0;
        if (ui_event.node >= NODE_ENTRY_BASE &&
            ui_event.node < NODE_ENTRY_BASE + entry_count && state == BROWSER_IDLE) {
            select_entry((uint8_t)(ui_event.node - NODE_ENTRY_BASE));
            last_action = BROWSER_ACTION_SELECT;
            set_preview(selected_kind == PXA_FS_KIND_DIRECTORY ?
                        "文件夹已选择，可打开或删除空文件夹" :
                        "文件已选择，可打开、读取或删除");
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (ui_event.node == NODE_NEW_FILE) {
            last_action = BROWSER_ACTION_CREATE_FILE;
            accepted = request_create_file();
        } else if (ui_event.node == NODE_NEW_DIRECTORY) {
            last_action = BROWSER_ACTION_CREATE_DIRECTORY;
            accepted = request_create_directory();
        } else if (ui_event.node == NODE_OPEN) {
            accepted = request_open_selected();
        } else if (ui_event.node == NODE_REMOVE) {
            last_action = BROWSER_ACTION_REMOVE;
            set_preview("正在删除选中项");
            accepted = request_remove();
        } else if (ui_event.node == NODE_UP) {
            last_action = BROWSER_ACTION_UP;
            set_preview("正在返回上级目录");
            accepted = request_go_up();
        } else if (ui_event.node == NODE_REFRESH) {
            last_action = BROWSER_ACTION_LIST;
            accepted = request_list();
        } else {
            return PXA_EVENT_UNHANDLED;
        }
        if (!accepted) {
            last_error = "文件系统请求未被接受";
            state = BROWSER_ERROR;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service != PXA_SERVICE_FS) return PXA_EVENT_UNHANDLED;
    if (parsed.opcode == PXA_FS_MAKE_DIRECTORY &&
        parsed.request_id == REQUEST_MKDIR_ROOT) {
        int32_t status;
        if (!pxa_fs_parse_status_result(&parsed, PXA_FS_MAKE_DIRECTORY, &status))
            return PXA_STATUS_INTERNAL;
        if (status != PXA_STATUS_OK && status != PXA_STATUS_BUSY) {
            last_error = "无法创建私有目录";
            state = BROWSER_ERROR;
        } else if (!request_list()) {
            last_error = "无法打开私有目录";
            state = BROWSER_ERROR;
        }
    } else if (parsed.opcode == PXA_FS_OPEN &&
               parsed.request_id == REQUEST_OPEN_DIRECTORY) {
        pxa_fs_open_result_t result;
        if (!pxa_fs_parse_open_result(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK ||
            !pxa_fs_read_directory(REQUEST_READ_DIRECTORY, result.handle,
                                   packet, sizeof(packet))) {
            if (result.status == PXA_STATUS_OK)
                (void)pxa_fs_close(result.handle, packet, sizeof(packet));
            last_error = "无法读取目录";
            state = BROWSER_ERROR;
        } else {
            directory_handle = result.handle;
            state = BROWSER_READING_DIRECTORY;
        }
    } else if (parsed.opcode == PXA_FS_READ_DIRECTORY &&
               parsed.request_id == REQUEST_READ_DIRECTORY) {
        pxa_fs_directory_result_t result;
        if (!pxa_fs_parse_directory_result(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK) {
            close_directory();
            last_error = "目录条目读取失败";
            state = BROWSER_ERROR;
        } else if (result.end) {
            finish_directory_listing();
        } else {
            add_entry(&result);
            if (!pxa_fs_read_directory(REQUEST_READ_DIRECTORY, directory_handle,
                                       packet, sizeof(packet))) {
                close_directory();
                last_error = "无法继续读取目录";
                state = BROWSER_ERROR;
            }
        }
    } else if (parsed.opcode == PXA_FS_OPEN &&
               parsed.request_id == REQUEST_CREATE_FILE) {
        pxa_fs_open_result_t result;
        static uint8_t content[] = "PXA private file created from ABI Lab\n";
        if (!pxa_fs_parse_open_result(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK ||
            pxa_fs_write(result.handle, content, sizeof(content) - 1u) !=
                (int32_t)(sizeof(content) - 1u) ||
            !pxa_fs_close(result.handle, packet, sizeof(packet)) || !request_list()) {
            last_error = "新建文件失败";
            state = BROWSER_ERROR;
        } else {
            set_preview("文件已创建，正在刷新目录");
        }
    } else if (parsed.opcode == PXA_FS_MAKE_DIRECTORY &&
               parsed.request_id == REQUEST_CREATE_DIRECTORY) {
        int32_t status;
        if (!pxa_fs_parse_status_result(&parsed, PXA_FS_MAKE_DIRECTORY, &status))
            return PXA_STATUS_INTERNAL;
        if (status != PXA_STATUS_OK || !request_list()) {
            last_error = "新建文件夹失败";
            state = BROWSER_ERROR;
        } else {
            set_preview("文件夹已创建，正在刷新目录");
        }
    } else if (parsed.opcode == PXA_FS_OPEN && parsed.request_id == REQUEST_READ_FILE) {
        pxa_fs_open_result_t result;
        int32_t read_length;
        if (!pxa_fs_parse_open_result(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK) {
            last_error = "无法打开文件";
            state = BROWSER_ERROR;
        } else {
            read_length = pxa_fs_read(result.handle, read_buffer, sizeof(read_buffer));
            if (read_length < 0 || !pxa_fs_close(result.handle, packet,
                                                  sizeof(packet))) {
                last_error = "读取文件失败";
                state = BROWSER_ERROR;
            } else {
                set_read_preview(read_length);
                state = BROWSER_IDLE;
            }
        }
    } else if (parsed.opcode == PXA_FS_REMOVE && parsed.request_id == REQUEST_REMOVE) {
        int32_t status;
        if (!pxa_fs_parse_status_result(&parsed, PXA_FS_REMOVE, &status))
            return PXA_STATUS_INTERNAL;
        if (status != PXA_STATUS_OK && status != PXA_STATUS_NOT_FOUND) {
            last_error = "删除失败：文件夹必须为空";
            state = BROWSER_ERROR;
        } else {
            clear_selection();
            if (!request_list()) {
                last_error = "删除后无法刷新目录";
                state = BROWSER_ERROR;
            } else {
                set_preview("已删除，正在刷新目录");
            }
        }
    } else {
        return PXA_EVENT_UNHANDLED;
    }
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    close_directory();
}
