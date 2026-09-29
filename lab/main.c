/* Compact Core v1 diagnostics. Each probe exercises the public Guest SDK and
 * reports a completed round trip; service-specific examples live in the SDK
 * tests and the focused demo apps. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pxa_app_messages.h"
#include "pxa_ui.h"
#include "pxa_clock.h"
#include "pxa_device.h"
#include "pxa_fs.h"
#include "pxa_i18n.h"
#include "pxa_storage.h"
#include "pxa_window.h"

#define LAB_TOKEN_BASE UINT64_C(0x4c41420000000000)
#define LAB_TOKEN_DEVICE (LAB_TOKEN_BASE + 1u)
#define LAB_TOKEN_CLOCK (LAB_TOKEN_BASE + 2u)
#define LAB_TOKEN_STORAGE_SET (LAB_TOKEN_BASE + 3u)
#define LAB_TOKEN_STORAGE_GET (LAB_TOKEN_BASE + 4u)
#define LAB_TOKEN_STORAGE_REMOVE (LAB_TOKEN_BASE + 5u)
#define LAB_TOKEN_FS_OPEN (LAB_TOKEN_BASE + 6u)
#define LAB_TOKEN_FS_SEEK (LAB_TOKEN_BASE + 7u)
#define LAB_TOKEN_FS_REMOVE (LAB_TOKEN_BASE + 8u)

#define LAB_PROBE_COUNT 6u
#define LAB_STATUS_PENDING 0u
#define LAB_STATUS_PASS 1u
#define LAB_STATUS_FAIL 2u
#define LAB_RERUN_NODE 5u
#define LAB_ROW_BASE 20u
#define LAB_LABEL_BASE 40u
#define LAB_RESULT_BASE 60u

#define WASI_CLOCK_MONOTONIC 1u

__attribute__((import_module("wasi_snapshot_preview1"),
               import_name("clock_time_get"))) uint32_t
wasi_clock_time_get(uint32_t clock_id, uint64_t precision, uint64_t *timestamp);
__attribute__((import_module("wasi_snapshot_preview1"),
               import_name("random_get"))) uint32_t
wasi_random_get(uint8_t *buffer, uint32_t length);

typedef enum {
    LAB_PROBE_WASI,
    LAB_PROBE_MEMORY,
    LAB_PROBE_DEVICE,
    LAB_PROBE_CLOCK,
    LAB_PROBE_STORAGE,
    LAB_PROBE_FS
} lab_probe_t;

static const pxa_i18n_message_id_t probe_messages[LAB_PROBE_COUNT] = {
    PXA_MSG_PROBE_WASI, PXA_MSG_PROBE_ALLOCATOR, PXA_MSG_PROBE_DEVICE,
    PXA_MSG_PROBE_CLOCK, PXA_MSG_PROBE_STORAGE, PXA_MSG_PROBE_FS
};
static const char *const pass_logs[LAB_PROBE_COUNT] = {
    "Lab WASI PASS", "Lab allocator PASS", "Lab Device PASS",
    "Lab Clock PASS", "Lab Storage PASS", "Lab FS PASS"
};
static const char *const fail_logs[LAB_PROBE_COUNT] = {
    "Lab WASI FAIL", "Lab allocator FAIL", "Lab Device FAIL",
    "Lab Clock FAIL", "Lab Storage FAIL", "Lab FS FAIL"
};
static const char storage_key[] = "LabV1Probe";
static const uint8_t storage_value[] = {0x50, 0x58, 0x41, 0x31};
static const char fs_path[] = "lab-v1-probe.bin";
static uint8_t fs_value[] = {0x50, 0x58, 0x41, 0x46};

static pxa_i18n_t i18n;
static uint8_t packet[512];
static uint32_t generation;
static uint8_t status[LAB_PROBE_COUNT];
static uint8_t pending;
static uint64_t file_handle;

static int lab_render(void) {
    pxa_ui_transaction_t transaction = {0};
    const char *title = pxa_i18n_cstr(&i18n, PXA_MSG_SCREEN_TITLE);
    const char *subtitle = pxa_i18n_cstr(&i18n, PXA_MSG_SCREEN_SUBTITLE);
    const uint32_t next = generation + 1u;
    int ok;
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                  packet, sizeof(packet))) return 0;
    ok = pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) &&
         pxa_ui_set_u8(&transaction, 1, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_theme_color(&transaction, 1, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create(&transaction, 2, 1, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 58) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 2, 12, 7, 12, 7) &&
         pxa_ui_set_theme_color(&transaction, 2,
                                PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 3, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 3, title, strlen(title)) &&
         pxa_ui_set_font_role(&transaction, 3, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 3,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create_typed(&transaction, LAB_RERUN_NODE, 2, 0,
                             PXA_UI_NODE_CONTROL,
                             PXA_UI_CONTROL_BUTTON) &&
         pxa_ui_set_text(&transaction, LAB_RERUN_NODE,
                         pxa_i18n_cstr(&i18n, PXA_MSG_ACTION_RUN),
                         pxa_i18n_size(&i18n, PXA_MSG_ACTION_RUN)) &&
         pxa_ui_set_event_mask(&transaction, LAB_RERUN_NODE,
                               PXA_UI_EVENT_MASK_CLICK) &&
         pxa_ui_create(&transaction, 4, 1, 0, PXA_UI_NODE_SCROLL) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u16(&transaction, 4, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 4, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, 4, PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
         pxa_ui_set_padding(&transaction, 4, 12, 10, 12, 10) &&
         pxa_ui_set_dp(&transaction, 4, PXA_UI_PROPERTY_GAP, 6) &&
         pxa_ui_create(&transaction, 6, 4, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 6, subtitle, strlen(subtitle)) &&
         pxa_ui_set_font_role(&transaction, 6, PXA_UI_FONT_ROLE_CAPTION);
    for (uint32_t i = 0; ok && i < LAB_PROBE_COUNT; ++i) {
        uint32_t row = LAB_ROW_BASE + i;
        uint32_t label = LAB_LABEL_BASE + i;
        uint32_t result = LAB_RESULT_BASE + i;
        const char *label_text = pxa_i18n_cstr(&i18n, probe_messages[i]);
        pxa_i18n_message_id_t status_message =
            status[i] == LAB_STATUS_PASS ? PXA_MSG_STATUS_PASS :
            status[i] == LAB_STATUS_FAIL ? PXA_MSG_STATUS_FAIL :
                                           PXA_MSG_STATUS_RUNNING;
        const char *word = pxa_i18n_cstr(&i18n, status_message);
        uint8_t color = status[i] == LAB_STATUS_PASS ? PXA_UI_THEME_SUCCESS :
                        status[i] == LAB_STATUS_FAIL ? PXA_UI_THEME_DANGER :
                        PXA_UI_THEME_MUTED;
        ok = pxa_ui_create(&transaction, row, 4, 0, PXA_UI_NODE_BOX) &&
             pxa_ui_set_length(&transaction, row, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(&transaction, row, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, 46) &&
             pxa_ui_set_u8(&transaction, row, PXA_UI_PROPERTY_LAYOUT,
                           PXA_UI_LAYOUT_ROW) &&
             pxa_ui_set_u8(&transaction, row, PXA_UI_PROPERTY_ALIGN,
                           PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_padding(&transaction, row, 9, 5, 9, 5) &&
             pxa_ui_set_dp(&transaction, row, PXA_UI_PROPERTY_RADIUS, 5) &&
             pxa_ui_set_theme_color(&transaction, row,
                                    PXA_UI_PROPERTY_BACKGROUND,
                                    PXA_UI_THEME_SURFACE) &&
             pxa_ui_create(&transaction, label, row, 0, PXA_UI_NODE_TEXT) &&
             pxa_ui_set_text(&transaction, label, label_text,
                             strlen(label_text)) &&
             pxa_ui_set_u16(&transaction, label, PXA_UI_PROPERTY_GROW, 1) &&
             pxa_ui_set_theme_color(&transaction, label,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_TEXT) &&
             pxa_ui_create(&transaction, result, row, 0, PXA_UI_NODE_TEXT) &&
             pxa_ui_set_text(&transaction, result, word, strlen(word)) &&
             pxa_ui_set_theme_color(&transaction, result,
                                    PXA_UI_PROPERTY_FOREGROUND, color);
    }
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    generation = next;
    return 1;
}

static void lab_finish(lab_probe_t probe, int passed) {
    status[probe] = passed ? LAB_STATUS_PASS : LAB_STATUS_FAIL;
    (void)pxa_log_write(2, passed ? pass_logs[probe] : fail_logs[probe]);
    if (pending != 0) --pending;
}

static void lab_start_probes(void) {
    uint64_t now_ns = 0;
    uint8_t random[8] = {0};
    uint8_t *memory;
    uint8_t *resized;
    file_handle = 0;
    pending = 4;
    memset(status, LAB_STATUS_PENDING, sizeof(status));
    status[LAB_PROBE_WASI] =
        wasi_clock_time_get(WASI_CLOCK_MONOTONIC, 1, &now_ns) == 0 &&
        now_ns != 0 && wasi_random_get(random, sizeof(random)) == 0 &&
        memcmp(random, "\0\0\0\0\0\0\0\0", sizeof(random)) != 0
            ? LAB_STATUS_PASS : LAB_STATUS_FAIL;
    memory = calloc(8, 1);
    if (memory != NULL) {
        memory[0] = 0x5a;
        resized = realloc(memory, 16);
        if (resized != NULL) memory = resized;
        else {
            free(memory);
            memory = NULL;
        }
    }
    status[LAB_PROBE_MEMORY] = memory != NULL && memory[0] == 0x5a
                                   ? LAB_STATUS_PASS : LAB_STATUS_FAIL;
    free(memory);
    (void)pxa_log_write(2, status[LAB_PROBE_WASI] == LAB_STATUS_PASS
                                 ? pass_logs[LAB_PROBE_WASI]
                                 : fail_logs[LAB_PROBE_WASI]);
    (void)pxa_log_write(2, status[LAB_PROBE_MEMORY] == LAB_STATUS_PASS
                                 ? pass_logs[LAB_PROBE_MEMORY]
                                 : fail_logs[LAB_PROBE_MEMORY]);
    if (pxa_device_request_runtime_info(LAB_TOKEN_DEVICE) != 0)
        lab_finish(LAB_PROBE_DEVICE, 0);
    if (pxa_clock_now(LAB_TOKEN_CLOCK) != 0)
        lab_finish(LAB_PROBE_CLOCK, 0);
    if (pxa_storage_request_set(packet, sizeof(packet),
                                   LAB_TOKEN_STORAGE_SET, storage_key,
                                   sizeof(storage_key) - 1u, storage_value,
                                   sizeof(storage_value)) != 0)
        lab_finish(LAB_PROBE_STORAGE, 0);
    if (pxa_fs_request_open(
            LAB_TOKEN_FS_OPEN, fs_path, sizeof(fs_path) - 1u,
            PXA_FS_OPEN_READ | PXA_FS_OPEN_WRITE |
                PXA_FS_OPEN_CREATE | PXA_FS_OPEN_TRUNCATE) != 0)
        lab_finish(LAB_PROBE_FS, 0);
}

int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)pxa_i18n_init_from_start_config(&i18n, &pxa_app_i18n_bundle,
                                          config, config_length);
    if (pxa_window_fullscreen() != 0) return PXA_STATUS_INTERNAL;
    lab_start_probes();
    return lab_render() ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *data, uint32_t size) {
    pxa_event_t event;
    pxa_ui_event_data_t ui_event;
    int32_t result;
    if (!pxa_parse_event(data, size, &event)) return PXA_EVENT_UNHANDLED;
    result = pxa_i18n_handle_event(&i18n, &event);
    if (result != 0)
        return result == 1 && !lab_render() ? PXA_STATUS_INTERNAL
                                            : PXA_EVENT_HANDLED;
    if (event.token == LAB_TOKEN_DEVICE) {
        pxa_device_runtime_info_t info;
        lab_finish(LAB_PROBE_DEVICE,
                   pxa_device_parse_runtime_info(&event, LAB_TOKEN_DEVICE,
                                                    &info) &&
                   info.status == 0 && info.target[0] != '\0');
    } else if (event.token == LAB_TOKEN_CLOCK) {
        pxa_clock_now_result_t now;
        lab_finish(LAB_PROBE_CLOCK,
                   pxa_clock_parse_now(&event, LAB_TOKEN_CLOCK, &now) &&
                   now.status == 0 && now.timestamp_us != 0);
    } else if (event.token == LAB_TOKEN_STORAGE_SET) {
        if (pxa_storage_parse_status(&event, LAB_TOKEN_STORAGE_SET,
                                        PXA_STORAGE_SET, &result) &&
            result == 0 &&
            pxa_storage_request_get(LAB_TOKEN_STORAGE_GET, storage_key,
                                        sizeof(storage_key) - 1u) == 0)
            return PXA_EVENT_HANDLED;
        lab_finish(LAB_PROBE_STORAGE, 0);
    } else if (event.token == LAB_TOKEN_STORAGE_GET) {
        pxa_storage_get_result_t value;
        if (pxa_storage_parse_get(&event, LAB_TOKEN_STORAGE_GET, &value) &&
            value.status == 0 && value.value.size == sizeof(storage_value) &&
            memcmp(value.value.data, storage_value,
                   sizeof(storage_value)) == 0 &&
            pxa_storage_request_remove(LAB_TOKEN_STORAGE_REMOVE,
                                           storage_key,
                                           sizeof(storage_key) - 1u) == 0)
            return PXA_EVENT_HANDLED;
        lab_finish(LAB_PROBE_STORAGE, 0);
    } else if (event.token == LAB_TOKEN_STORAGE_REMOVE) {
        lab_finish(LAB_PROBE_STORAGE,
                   pxa_storage_parse_status(&event,
                                               LAB_TOKEN_STORAGE_REMOVE,
                                               PXA_STORAGE_REMOVE,
                                               &result) && result == 0);
    } else if (event.token == LAB_TOKEN_FS_OPEN) {
        pxa_fs_open_result_t opened;
        if (pxa_fs_parse_open(&event, LAB_TOKEN_FS_OPEN, &opened) &&
            opened.status == 0) {
            file_handle = opened.handle;
            if (pxa_fs_write(file_handle, fs_value,
                                 sizeof(fs_value)) == sizeof(fs_value) &&
                pxa_fs_request_seek(LAB_TOKEN_FS_SEEK, file_handle, 0,
                                        PXA_FS_SEEK_START) == 0)
                return PXA_EVENT_HANDLED;
        }
        if (file_handle != 0) (void)pxa_fs_close(file_handle);
        file_handle = 0;
        lab_finish(LAB_PROBE_FS, 0);
    } else if (event.token == LAB_TOKEN_FS_SEEK) {
        pxa_fs_seek_result_t seek;
        uint8_t readback[sizeof(fs_value)] = {0};
        int passed = pxa_fs_parse_seek(&event, LAB_TOKEN_FS_SEEK,
                                           &seek) && seek.status == 0 &&
                     seek.position == 0 &&
                     pxa_fs_read(file_handle, readback,
                                     sizeof(readback)) == sizeof(readback) &&
                     memcmp(readback, fs_value, sizeof(readback)) == 0;
        if (file_handle != 0) (void)pxa_fs_close(file_handle);
        file_handle = 0;
        if (passed && pxa_fs_request_path(PXA_FS_REMOVE,
                                              LAB_TOKEN_FS_REMOVE, fs_path,
                                              sizeof(fs_path) - 1u) == 0)
            return PXA_EVENT_HANDLED;
        lab_finish(LAB_PROBE_FS, 0);
    } else if (event.token == LAB_TOKEN_FS_REMOVE) {
        lab_finish(LAB_PROBE_FS,
                   pxa_fs_parse_status(&event, LAB_TOKEN_FS_REMOVE,
                                          PXA_FS_REMOVE, &result) &&
                   result == 0);
    } else if (pxa_ui_parse_event(&event, &ui_event) &&
               ui_event.kind == PXA_UI_EVENT_CLICK_KIND &&
               ui_event.node == LAB_RERUN_NODE) {
        if (pending == 0) {
            lab_start_probes();
            return lab_render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_HANDLED;
    } else {
        return PXA_EVENT_UNHANDLED;
    }
    return lab_render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    file_handle = 0;
}
