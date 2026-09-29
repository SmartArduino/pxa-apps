#include <stdint.h>

#include "pxa_core.h"
#include "pxa_device.h"
#include "pxa_game_render.h"
#include "pxa_window.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include "pxa_fs.h"
#include "pxa_clock.h"
#include "pxa_ui_wire.h"

#define INFO_TOKEN UINT64_C(0x123456789abcdef0)
#define INITIAL_REQUESTS 16u
#define RENDER_TOKEN (INFO_TOKEN + 100u)
#define WINDOW_TOKEN (RENDER_TOKEN + 3u)
#define PERMISSION_CHECK_TOKEN (WINDOW_TOKEN + 1u)
#define PERMISSION_ACQUIRE_TOKEN (WINDOW_TOKEN + 2u)
#define PERMISSION_RECHECK_TOKEN (WINDOW_TOKEN + 23u)
#define MAC_SCOPE_TOKEN (WINDOW_TOKEN + 3u)
#define MAC_SUCCESS_TOKEN (WINDOW_TOKEN + 4u)
#define MAC_STALE_TOKEN (WINDOW_TOKEN + 5u)
#define STORAGE_SET_TOKEN (WINDOW_TOKEN + 6u)
#define STORAGE_GET_TOKEN (WINDOW_TOKEN + 7u)
#define STORAGE_LIST_TOKEN (WINDOW_TOKEN + 8u)
#define STORAGE_REMOVE_TOKEN (WINDOW_TOKEN + 9u)
#define STORAGE_MISSING_TOKEN (WINDOW_TOKEN + 10u)
#define FS_MKDIR_TOKEN (WINDOW_TOKEN + 11u)
#define FS_OPEN_TOKEN (WINDOW_TOKEN + 12u)
#define FS_SEEK_TOKEN (WINDOW_TOKEN + 13u)
#define FS_STAT_TOKEN (WINDOW_TOKEN + 14u)
#define FS_OPEN_DIR_TOKEN (WINDOW_TOKEN + 15u)
#define FS_READ_DIR_TOKEN (WINDOW_TOKEN + 16u)
#define FS_END_DIR_TOKEN (WINDOW_TOKEN + 17u)
#define FS_RENAME_TOKEN (WINDOW_TOKEN + 18u)
#define FS_REMOVE_FILE_TOKEN (WINDOW_TOKEN + 19u)
#define FS_REMOVE_DIR_TOKEN (WINDOW_TOKEN + 20u)
#define CLOCK_NOW_TOKEN (FS_REMOVE_DIR_TOKEN + 1u)
#define UI_THEME_TOKEN (CLOCK_NOW_TOKEN + 1u)

static const char fs_dir[] = "probe-dir";
static const char fs_file[] = "probe-dir/note.txt";
static const char fs_renamed[] = "probe-dir/renamed.txt";
static uint8_t fs_contents[] = "PXA-FS-v1";

static const char storage_key[] = "migration-probe";
static uint8_t storage_value[PXA_STORAGE_MAX_VALUE];

static int32_t request_storage_set(void) {
    uint8_t packet[PXA_STORAGE_MAX_PACKET];
    for (uint32_t i = 0; i < sizeof(storage_value); ++i)
        storage_value[i] = (uint8_t)(i ^ (i >> 4));
    return pxa_storage_request_set(
        packet, sizeof(packet), STORAGE_SET_TOKEN,
        storage_key, sizeof(storage_key) - 1u,
        storage_value, sizeof(storage_value));
}

static int32_t request_permission(uint16_t opcode, uint64_t token) {
    static const char name[] = "device.identity";
    static const uint8_t scope[] = "mac.wifi.station.hardware";
    uint8_t packet[96];
    uint32_t size = 0;
    if (!pxa_permission_build(packet, sizeof(packet), opcode, token,
                                 name, sizeof(name) - 1,
                                 scope, sizeof(scope) - 1, &size))
        return -1;
    return pxa_submit(packet, size);
}

static uint32_t completed;
static uint8_t followup_sent;
static uint8_t render_failure_seen;
static uint64_t first_render_handle;
static uint64_t device_permission_handle;
static uint64_t file_handle;
static uint64_t directory_handle;

static int32_t request_fs_rename(void) {
    uint8_t packet[PXA_FS_MAX_RENAME_PACKET];
    uint32_t size = 0;
    if (!pxa_fs_build_rename(packet, sizeof(packet), FS_RENAME_TOKEN,
                                fs_file, sizeof(fs_file) - 1u,
                                fs_renamed, sizeof(fs_renamed) - 1u,
                                &size)) return -1;
    return pxa_submit(packet, size);
}

static int32_t request_render(uint64_t token) {
    const pxa_game_render_options_t options = {
        .width = 32, .height = 32, .buffer_count = 2,
        .scratch_mode = 1, .max_draw_bytes = 64};
    uint8_t packet[PXA_HEADER_BYTES + 12u];
    uint32_t size = 0;
    if (!pxa_game_render_build_create(packet, sizeof(packet), token,
                                          &options, &size))
        return -1;
    return pxa_submit(packet, size);
}

static int submit_clear_frame(uint64_t handle) {
    uint8_t draw_list[40];
    pxa_zero(draw_list, sizeof(draw_list));
    pxa_store_u32(draw_list, UINT32_C(0x4c525850));
    pxa_store_u16(draw_list + 4, 1);
    pxa_store_u16(draw_list + 6, 7);
    pxa_store_u32(draw_list + 8, sizeof(draw_list));
    pxa_store_u32(draw_list + 16, 1);
    pxa_store_u64(draw_list + 20, 1);
    draw_list[32] = 1;
    pxa_store_u16(draw_list + 34, 8);
    pxa_store_u16(draw_list + 36, UINT16_C(0xf800));
    if (pxa_game_render_submit(handle, draw_list,
                                  sizeof(draw_list)) !=
        (int32_t)sizeof(draw_list)) {
        (void)pxa_log_write(4, "ABI v1 render clear submit failed");
        return 0;
    }
    return 1;
}

int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    uint8_t packet[PXA_HEADER_BYTES];
    uint32_t packet_size = 0;
    (void)config;
    (void)length;
    if (!pxa_build_message(packet, sizeof(packet), PXA_LOG_SERVICE,
                              PXA_LOG_WRITE, 0, 0, 0, &packet_size) ||
        packet_size != PXA_HEADER_BYTES)
        return -1;
    packet[16] = 1;
    if (pxa_submit(packet, packet_size) != -1) return -1;
    packet[16] = 0;
    packet[4] = 1;
    if (pxa_submit(packet, packet_size) != -3) return -1;
    if (pxa_io(0, 1, 0, 0) != -1) {
        (void)pxa_log_write(4, "ABI v1 I/O gate failed");
        return -1;
    }
    if (pxa_log_write(2, "ABI v1 signed Guest reached Host") != 0)
        return -1;
    if (!pxa_build_message(packet, sizeof(packet),
                              PXA_WINDOW_SERVICE, 1, 1,
                              0, 0, &packet_size) ||
        pxa_submit(packet, packet_size) != -3) {
        (void)pxa_log_write(4, "ABI v1 Window one-way token gate failed");
        return -1;
    }
    for (uint32_t i = 0; i < INITIAL_REQUESTS; ++i) {
        if (pxa_device_request_runtime_info(INFO_TOKEN + i) != 0) {
            char error[] = "ABI v1 submit failed at 00";
            static const char hex[] = "0123456789abcdef";
            error[24] = hex[(i >> 4) & 15u];
            error[25] = hex[i & 15u];
            (void)pxa_log_write(4, error);
            return -1;
        }
    }
    if (pxa_cancel(0) != -1 ||
        pxa_cancel(INFO_TOKEN) != 0 ||
        pxa_cancel(INFO_TOKEN + 1000u) != 0) {
        (void)pxa_log_write(4, "ABI v1 cancel gate failed");
        return -1;
    }
    if (pxa_device_request_runtime_info(INFO_TOKEN) != -6) {
        (void)pxa_log_write(4, "ABI v1 duplicate gate failed");
        return -1;
    }
    if (pxa_device_request_runtime_info(
            INFO_TOKEN + INITIAL_REQUESTS) != -9) {
        (void)pxa_log_write(4, "ABI v1 capacity gate failed");
        return -1;
    }
    if (pxa_window_request_snapshot(WINDOW_TOKEN) != -9) {
        (void)pxa_log_write(4, "ABI v1 cross-service capacity gate failed");
        return -1;
    }
    return 0;
}

int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t length) {
    pxa_event_t event;
    pxa_device_runtime_info_t info;
    pxa_game_render_create_result_t render;
    pxa_window_snapshot_t snapshot;
    pxa_permission_check_result_t checked;
    pxa_permission_acquire_result_t acquired;
    pxa_device_mac_result_t mac;
    pxa_storage_get_result_t stored;
    pxa_storage_list_result_t keys;
    int32_t storage_status = -1;
    pxa_fs_open_result_t opened_file;
    pxa_fs_seek_result_t sought;
    pxa_fs_stat_result_t file_stat;
    pxa_fs_directory_result_t directory_entry;
    pxa_clock_now_result_t clock_now;
    pxa_ui_wire_theme_t theme;
    int32_t fs_status = -1;
    if (!pxa_parse_event(bytes, length, &event)) {
        (void)pxa_log_write(4, "ABI v1 event parse failed");
        return -1;
    }
    if (event.token == WINDOW_TOKEN) {
        const pxa_window_config_t window_config = {
            .edge_to_edge = 0,
            .status_bar_mode = 0,
            .navigation_bar_mode = 0,
            .status_bar_icons = 0,
            .navigation_bar_icons = 0,
            .status_bar_color = 0xff202020u,
            .navigation_bar_color = 0xff202020u};
        if (!pxa_window_parse_snapshot(&event, WINDOW_TOKEN, &snapshot) ||
            snapshot.status != 0 || snapshot.revision == 0 ||
            snapshot.logical_width == 0 || snapshot.pixel_width == 0)
            return -1;
        if (pxa_window_configure(&window_config) != 0 ||
            pxa_window_show_toast(1000, "ABI v1 Window", 13) != 0)
            return -1;
        if (pxa_log_write(2, "ABI v1 Window configure and toast OK") != 0)
            return -1;
        if (pxa_log_write(2, "ABI v1 Window snapshot and cross-service token OK") != 0)
            return -1;
        return request_permission(PXA_PERMISSION_CHECK,
                                  PERMISSION_CHECK_TOKEN) == 0 ? 0 : -1;
    }
    if (event.token == PERMISSION_CHECK_TOKEN) {
        /* A device denies an optional declaration until the App acquires it, so
         * the first look only has to report a well-formed decision. */
        if (!pxa_permission_parse_check(
                &event, PERMISSION_CHECK_TOKEN, &checked) ||
            checked.status != 0)
            return -1;
        return request_permission(PXA_PERMISSION_ACQUIRE,
                                  PERMISSION_ACQUIRE_TOKEN) == 0 ? 0 : -1;
    }
    if (event.token == PERMISSION_ACQUIRE_TOKEN) {
        if (!pxa_permission_parse_acquire(
                &event, PERMISSION_ACQUIRE_TOKEN, &acquired) ||
            acquired.status != 0 ||
            pxa_close_handle(acquired.handle +
                                (UINT64_C(1) << 32)) != -5)
            return -1;
        device_permission_handle = acquired.handle;
        return request_permission(PXA_PERMISSION_CHECK,
                                  PERMISSION_RECHECK_TOKEN) == 0 ? 0 : -1;
    }
    if (event.token == PERMISSION_RECHECK_TOKEN) {
        if (!pxa_permission_parse_check(
                &event, PERMISSION_RECHECK_TOKEN, &checked) ||
            checked.status != 0 || checked.decision != 1)
            return -1;
        if (pxa_log_write(2, "ABI v1 Permission check, acquire and grant OK") != 0)
            return -1;
        return pxa_device_request_get_mac(
                   MAC_SCOPE_TOKEN,
                   PXA_DEVICE_MAC_WIFI_STATION_CURRENT,
                   device_permission_handle) == 0 ? 0 : -1;
    }
    if (event.token == MAC_SCOPE_TOKEN) {
        if (!pxa_device_parse_get_mac(&event, MAC_SCOPE_TOKEN, &mac) ||
            mac.status != -4) return -1;
        return pxa_device_request_get_mac(
                   MAC_SUCCESS_TOKEN,
                   PXA_DEVICE_MAC_WIFI_STATION_HARDWARE,
                   device_permission_handle) == 0 ? 0 : -1;
    }
    if (event.token == MAC_SUCCESS_TOKEN) {
        if (!pxa_device_parse_get_mac(&event, MAC_SUCCESS_TOKEN, &mac) ||
            mac.status != 0 ||
            mac.kind != PXA_DEVICE_MAC_WIFI_STATION_HARDWARE ||
            mac.mac[0] != 0x02 || mac.mac[1] != 0x50 ||
            (mac.flags & 1u) == 0 ||
            pxa_close_handle(device_permission_handle) != 0 ||
            pxa_close_handle(device_permission_handle) != -5)
            return -1;
        return pxa_device_request_get_mac(
                   MAC_STALE_TOKEN,
                   PXA_DEVICE_MAC_WIFI_STATION_HARDWARE,
                   device_permission_handle) == 0 ? 0 : -1;
    }
    if (event.token == MAC_STALE_TOKEN) {
        if (!pxa_device_parse_get_mac(&event, MAC_STALE_TOKEN, &mac) ||
            mac.status != -5) return -1;
        if (pxa_log_write(2, "ABI v1 Device MAC scoped native Handle and stale rejection OK") != 0)
            return -1;
        if (request_storage_set() != 0) return -1;
        return pxa_cancel(STORAGE_SET_TOKEN) == 0 ? 0 : -1;
    }
    if (event.token == STORAGE_SET_TOKEN) {
        if (!pxa_storage_parse_status(&event, STORAGE_SET_TOKEN,
                                         PXA_STORAGE_SET,
                                         &storage_status) ||
            storage_status != 0) return -1;
        return pxa_storage_request_get(
                   STORAGE_GET_TOKEN,
                   storage_key, sizeof(storage_key) - 1u) == 0 ? 0 : -1;
    }
    if (event.token == STORAGE_GET_TOKEN) {
        if (!pxa_storage_parse_get(&event, STORAGE_GET_TOKEN, &stored) ||
            stored.status != 0 ||
            stored.value.size != sizeof(storage_value)) return -1;
        for (uint32_t i = 0; i < sizeof(storage_value); ++i)
            if (stored.value.data[i] != storage_value[i]) return -1;
        return pxa_storage_request_list(
                   STORAGE_LIST_TOKEN,
                   "migration-prob", 14) == 0 ? 0 : -1;
    }
    if (event.token == STORAGE_LIST_TOKEN) {
        if (!pxa_storage_parse_list(&event, STORAGE_LIST_TOKEN, &keys) ||
            keys.status != 0 || keys.count == 0 ||
            keys.keys[0].size != sizeof(storage_key) - 1u) return -1;
        for (uint32_t i = 0; i < sizeof(storage_key) - 1u; ++i)
            if (keys.keys[0].data[i] != (uint8_t)storage_key[i]) return -1;
        return pxa_storage_request_remove(
                   STORAGE_REMOVE_TOKEN,
                   storage_key, sizeof(storage_key) - 1u) == 0 ? 0 : -1;
    }
    if (event.token == STORAGE_REMOVE_TOKEN) {
        if (!pxa_storage_parse_status(&event, STORAGE_REMOVE_TOKEN,
                                         PXA_STORAGE_REMOVE,
                                         &storage_status) ||
            storage_status != 0) return -1;
        return pxa_storage_request_get(
                   STORAGE_MISSING_TOKEN,
                   storage_key, sizeof(storage_key) - 1u) == 0 ? 0 : -1;
    }
    if (event.token == STORAGE_MISSING_TOKEN) {
        if (!pxa_storage_parse_get(&event, STORAGE_MISSING_TOKEN,
                                      &stored) || stored.status != -5)
            return -1;
        if (pxa_log_write(2, "ABI v1 Storage max value roundtrip and lifecycle OK") != 0)
            return -1;
        return pxa_fs_request_path(PXA_FS_MAKE_DIRECTORY,
                                      FS_MKDIR_TOKEN, fs_dir,
                                      sizeof(fs_dir) - 1u) == 0 ? 0 : -1;
    }
    if (event.token == FS_MKDIR_TOKEN) {
        if (!pxa_fs_parse_status(&event, FS_MKDIR_TOKEN,
                                    PXA_FS_MAKE_DIRECTORY,
                                    &fs_status) || fs_status != 0) return -1;
        return pxa_fs_request_open(
                   FS_OPEN_TOKEN, fs_file, sizeof(fs_file) - 1u,
                   PXA_FS_OPEN_READ | PXA_FS_OPEN_WRITE |
                   PXA_FS_OPEN_CREATE | PXA_FS_OPEN_TRUNCATE) == 0
                   ? 0 : -1;
    }
    if (event.token == FS_OPEN_TOKEN) {
        if (!pxa_fs_parse_open(&event, FS_OPEN_TOKEN, &opened_file) ||
            opened_file.status != 0 ||
            pxa_fs_close(opened_file.handle +
                            (UINT64_C(1) << 32)) != -5) return -1;
        file_handle = opened_file.handle;
        if (pxa_fs_write(file_handle, fs_contents,
                            sizeof(fs_contents) - 1u) !=
            (int32_t)(sizeof(fs_contents) - 1u)) return -1;
        return pxa_fs_request_seek(FS_SEEK_TOKEN, file_handle, 0,
                                       PXA_FS_SEEK_START) == 0 ? 0 : -1;
    }
    if (event.token == FS_SEEK_TOKEN) {
        uint8_t read_back[sizeof(fs_contents)] = {0};
        if (!pxa_fs_parse_seek(&event, FS_SEEK_TOKEN, &sought) ||
            sought.status != 0 || sought.position != 0 ||
            pxa_fs_read(file_handle, read_back,
                            sizeof(fs_contents) - 1u) !=
                (int32_t)(sizeof(fs_contents) - 1u) ||
            pxa_fs_close(file_handle) != 0 ||
            pxa_fs_read(file_handle, read_back, 1) != -5)
            return -1;
        for (uint32_t i = 0; i < sizeof(fs_contents) - 1u; ++i)
            if (read_back[i] != fs_contents[i]) return -1;
        return pxa_fs_request_path(PXA_FS_STAT, FS_STAT_TOKEN,
                                       fs_file, sizeof(fs_file) - 1u) == 0
                   ? 0 : -1;
    }
    if (event.token == FS_STAT_TOKEN) {
        if (!pxa_fs_parse_stat(&event, FS_STAT_TOKEN, &file_stat) ||
            file_stat.status != 0 ||
            file_stat.kind != PXA_FS_KIND_REGULAR ||
            file_stat.size != sizeof(fs_contents) - 1u) return -1;
        return pxa_fs_request_open(
                   FS_OPEN_DIR_TOKEN, fs_dir, sizeof(fs_dir) - 1u,
                   PXA_FS_OPEN_READ | PXA_FS_OPEN_DIRECTORY) == 0
                   ? 0 : -1;
    }
    if (event.token == FS_OPEN_DIR_TOKEN) {
        if (!pxa_fs_parse_open(&event, FS_OPEN_DIR_TOKEN,
                                  &opened_file) || opened_file.status != 0)
            return -1;
        directory_handle = opened_file.handle;
        return pxa_fs_request_read_directory(FS_READ_DIR_TOKEN,
                                                 directory_handle) == 0
                   ? 0 : -1;
    }
    if (event.token == FS_READ_DIR_TOKEN) {
        static const char expected[] = "note.txt";
        if (!pxa_fs_parse_directory(&event, FS_READ_DIR_TOKEN,
                                       &directory_entry) ||
            directory_entry.status != 0 || directory_entry.end != 0 ||
            directory_entry.name_size != sizeof(expected) - 1u ||
            directory_entry.kind != PXA_FS_KIND_REGULAR ||
            directory_entry.size != sizeof(fs_contents) - 1u) return -1;
        for (uint32_t i = 0; i < sizeof(expected) - 1u; ++i)
            if (directory_entry.name[i] != (uint8_t)expected[i]) return -1;
        return pxa_fs_request_read_directory(FS_END_DIR_TOKEN,
                                                 directory_handle) == 0
                   ? 0 : -1;
    }
    if (event.token == FS_END_DIR_TOKEN) {
        if (!pxa_fs_parse_directory(&event, FS_END_DIR_TOKEN,
                                       &directory_entry) ||
            directory_entry.status != 0 || directory_entry.end != 1 ||
            pxa_fs_close(directory_handle) != 0) return -1;
        return request_fs_rename() == 0 ? 0 : -1;
    }
    if (event.token == FS_RENAME_TOKEN) {
        if (!pxa_fs_parse_status(&event, FS_RENAME_TOKEN,
                                    PXA_FS_RENAME, &fs_status) ||
            fs_status != 0) return -1;
        return pxa_fs_request_path(PXA_FS_REMOVE,
                                       FS_REMOVE_FILE_TOKEN, fs_renamed,
                                       sizeof(fs_renamed) - 1u) == 0 ? 0 : -1;
    }
    if (event.token == FS_REMOVE_FILE_TOKEN) {
        if (!pxa_fs_parse_status(&event, FS_REMOVE_FILE_TOKEN,
                                    PXA_FS_REMOVE, &fs_status) ||
            fs_status != 0) return -1;
        return pxa_fs_request_path(PXA_FS_REMOVE,
                                       FS_REMOVE_DIR_TOKEN, fs_dir,
                                       sizeof(fs_dir) - 1u) == 0 ? 0 : -1;
    }
    if (event.token == FS_REMOVE_DIR_TOKEN) {
        if (!pxa_fs_parse_status(&event, FS_REMOVE_DIR_TOKEN,
                                    PXA_FS_REMOVE, &fs_status) ||
            fs_status != 0) return -1;
        if (pxa_log_write(2, "ABI v1 FS native Handle and file lifecycle OK") != 0)
            return -1;
        return pxa_clock_now(CLOCK_NOW_TOKEN) == 0 ? 0 : -1;
    }
    if (event.token == CLOCK_NOW_TOKEN) {
        if (!pxa_clock_parse_now(&event, CLOCK_NOW_TOKEN, &clock_now) ||
            clock_now.status != 0 || clock_now.timestamp_us == 0)
            return -1;
        if (pxa_log_write(2, "ABI v1 Clock reserved completion OK") != 0)
            return -1;
        {
            uint8_t packet[PXA_HEADER_BYTES];
            uint32_t size = 0;
            return pxa_ui_wire_build_theme_get(packet, sizeof(packet),
                                              UI_THEME_TOKEN, &size) &&
                   pxa_submit(packet, size) == 0 ? 0 : -1;
        }
    }
    if (event.token == UI_THEME_TOKEN) {
        if (!pxa_ui_wire_parse_theme(&event, UI_THEME_TOKEN, &theme) ||
            theme.status != 0 || theme.generation == 0 ||
            theme.typography_px[0] == 0) return -1;
        return pxa_log_write(2, "ABI v1 UI theme completion OK") == 0
                   ? 1 : -1;
    }
    if (event.token >= RENDER_TOKEN && event.token <= RENDER_TOKEN + 2u) {
        uint8_t telemetry[PXA_GAME_RENDER_TELEMETRY_BYTES];
        pxa_game_render_telemetry_t counters;
        if (!pxa_game_render_parse_create(&event, event.token, &render))
            return -1;
        if (event.token == RENDER_TOKEN + 1u) {
            if (render.status != -9 || render.handle != 0)
                return -1;
            render_failure_seen = 1;
            return 0;
        }
        if (render.status != 0 || (render.handle >> 32) == 0 ||
            (event.token == RENDER_TOKEN &&
             !submit_clear_frame(render.handle)) ||
            pxa_game_render_query_telemetry(render.handle,
                                                &counters) != 0) {
            (void)pxa_log_write(4, "ABI v1 render result or telemetry failed");
            return -1;
        }
        if (event.token == RENDER_TOKEN) {
            if (counters.submitted_frames != 1) {
                (void)pxa_log_write(4, "ABI v1 render submit counter failed");
                return -1;
            }
            first_render_handle = render.handle;
            if (pxa_io(render.handle + (UINT64_C(1) << 32),
                          PXA_GAME_RENDER_IO_TELEMETRY,
                          telemetry, sizeof(telemetry)) != -5 ||
                pxa_game_render_close(
                    render.handle + (UINT64_C(1) << 32)) != -5 ||
                pxa_game_render_close(render.handle) != 0 ||
                pxa_io(render.handle, PXA_GAME_RENDER_IO_TELEMETRY,
                          telemetry, sizeof(telemetry)) != -5 ||
                request_render(RENDER_TOKEN + 2u) != 0)
                return -1;
            return 0;
        }
        if (!render_failure_seen || render.handle == first_render_handle ||
            pxa_io(first_render_handle,
                      PXA_GAME_RENDER_IO_TELEMETRY, telemetry,
                      sizeof(telemetry)) != -5 ||
            pxa_game_render_close(render.handle) != 0)
            return -1;
        if (pxa_log_write(2, "ABI v1 GameRender 64-bit Handle IO and stale rejection OK") != 0)
            return -1;
        return pxa_window_request_snapshot(WINDOW_TOKEN) == 0 ? 0 : -1;
    }
    if (event.token < INFO_TOKEN ||
        event.token > INFO_TOKEN + INITIAL_REQUESTS)
        return 0;
    if (completed == 0)
        (void)pxa_log_write(2, "ABI v1 first completion reached Guest");
    if (!pxa_device_parse_runtime_info(&event, event.token, &info) ||
        info.status != 0 || info.target[0] == '\0') {
        (void)pxa_log_write(4, "ABI v1 typed decoder failed");
        return -1;
    }
    uint32_t bit = UINT32_C(1) << (uint32_t)(event.token - INFO_TOKEN);
    if ((completed & bit) != 0) {
        (void)pxa_log_write(4, "ABI v1 duplicate completion");
        return -1;
    }
    completed |= bit;
    if (!followup_sent) {
        followup_sent = 1;
        if (pxa_device_request_runtime_info(
                INFO_TOKEN + INITIAL_REQUESTS) != 0) {
            (void)pxa_log_write(4, "ABI v1 slot reuse failed");
            return -1;
        }
    }
    if (completed != (UINT32_C(1) << (INITIAL_REQUESTS + 1u)) - 1u)
        return 0;
    if (pxa_log_write(2, "ABI v1 17 token completions and reuse OK") != 0)
        return -1;
    return request_render(RENDER_TOKEN) == 0 &&
                   request_render(RENDER_TOKEN + 1u) == 0
               ? 0 : -1;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
