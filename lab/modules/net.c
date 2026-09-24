#define PXA_LAB_MODULE_PREFIX pxa_lab_net_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_net.h"
#include "pxa_permission.h"

#define REQUEST_PERMISSION UINT32_C(1)
#define REQUEST_NET_BASE UINT32_C(100)
#define NODE_RUN_ALL UINT32_C(40)
#define NORMAL_REQUEST_TIMEOUT_MS UINT32_C(15000)

#define CASE_GET UINT8_C(0)
#define CASE_HEAD UINT8_C(1)
#define CASE_POST UINT8_C(2)
#define CASE_PUT UINT8_C(3)
#define CASE_PATCH UINT8_C(4)
#define CASE_DELETE UINT8_C(5)
#define CASE_LEGACY UINT8_C(6)
#define CASE_NOT_FOUND UINT8_C(7)
#define CASE_REDIRECT UINT8_C(8)
#define CASE_TIMEOUT UINT8_C(9)
#define CASE_SERVICE_UNAVAILABLE UINT8_C(10)
#define CASE_LIMIT UINT8_C(11)
#define CASE_COUNT UINT8_C(12)

#define RESPONSE_BODY_FLAGS \
    (PXA_NET_RESPONSE_BODY_PRESENT | PXA_NET_RESPONSE_BODY_LENGTH_KNOWN)

typedef struct {
    const char *label;
    const char *url;
    uint16_t method;
    int32_t expected_status;
    uint16_t expected_http_status;
    uint32_t expected_flags;
    const char *expected_header;
    uint32_t button_node;
    uint8_t legacy;
    uint8_t body_optional;
} net_case_t;

static const net_case_t cases[CASE_COUNT] = {
    {"GET", "https://postman-echo.com/get?case=get", PXA_NET_METHOD_GET,
     PXA_STATUS_OK, 200, RESPONSE_BODY_FLAGS, "etag", 20, 0, 0},
    {"HEAD", "https://postman-echo.com/get?case=head", PXA_NET_METHOD_HEAD,
     PXA_STATUS_OK, 200, PXA_NET_RESPONSE_BODY_LENGTH_KNOWN, NULL, 21, 0, 0},
    {"POST", "https://postman-echo.com/post?case=post", PXA_NET_METHOD_POST,
     PXA_STATUS_OK, 200, RESPONSE_BODY_FLAGS, "etag", 22, 0, 0},
    {"PUT", "https://postman-echo.com/put?case=put", PXA_NET_METHOD_PUT,
     PXA_STATUS_OK, 200, RESPONSE_BODY_FLAGS, "etag", 23, 0, 0},
    {"PATCH", "https://postman-echo.com/patch?case=patch",
     PXA_NET_METHOD_PATCH, PXA_STATUS_OK, 200, RESPONSE_BODY_FLAGS, "etag", 24,
     0, 0},
    {"DELETE", "https://postman-echo.com/delete?case=delete",
     PXA_NET_METHOD_DELETE, PXA_STATUS_OK, 200, RESPONSE_BODY_FLAGS, "etag", 25,
     0, 0},
    {"LEGACY", "https://postman-echo.com/get?case=legacy",
     PXA_NET_METHOD_GET, PXA_STATUS_OK, 200, 0, NULL, 41, 1, 0},
    {"HTTP 404", "https://postman-echo.com/status/404?case=http-404",
     PXA_NET_METHOD_GET, PXA_STATUS_OK, 404, RESPONSE_BODY_FLAGS, "etag", 42, 0, 0},
    {"HTTP 302",
     "https://postman-echo.com/redirect-to?url=%2Fget&status_code=302&case=redirect",
     PXA_NET_METHOD_GET, PXA_STATUS_OK, 302, RESPONSE_BODY_FLAGS, "location", 43,
     0, 1},
    {"TIMEOUT", "https://postman-echo.com/delay/3?case=timeout",
     PXA_NET_METHOD_GET, PXA_STATUS_TIMED_OUT, 0, 0, NULL, 44, 0, 0},
    {"HTTP 503", "https://postman-echo.com/status/503?case=service-503",
     PXA_NET_METHOD_GET, PXA_STATUS_OK, 503, RESPONSE_BODY_FLAGS, "etag", 45, 0, 0},
    {"LIMIT", "https://postman-echo.com/get?case=limit",
     PXA_NET_METHOD_GET, PXA_STATUS_LIMIT_EXCEEDED, 0, 0, NULL, 46, 0, 0},
};

static uint8_t packet[1664];
static uint8_t request_payload[1024];
static uint32_t permission_handle;
static uint32_t body_handle;
static uint32_t active_request_id;
static uint32_t response_flags;
static uint64_t response_length;
static uint32_t body_bytes;
static uint16_t response_status_code;
static int32_t response_status;
static uint8_t current_case;
static uint8_t queued_case;
static uint8_t waiting;
static uint8_t waiting_permission;
static uint8_t stream_waiting;
static uint8_t suite_running;
static uint8_t queued_suite;
static uint8_t suite_index;
static uint8_t suite_passed;
static uint8_t suite_failed;
static uint8_t permission_denied;
static uint8_t has_result;
static uint8_t last_passed;
static uint8_t response_has_header;
static uint8_t response_metadata_valid;

static size_t string_length(const char *value) {
    size_t size = 0;
    if (value == NULL) return 0;
    while (value[size] != '\0') ++size;
    return size;
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

static size_t append_u32(char *output, size_t capacity, size_t offset,
                         uint32_t value) {
    char reverse[10];
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

static int bytes_equal_text(const uint8_t *bytes, uint16_t length,
                            const char *text) {
    uint16_t index = 0;
    if (bytes == NULL || text == NULL) return 0;
    while (text[index] != '\0') {
        if (index >= length || bytes[index] != (uint8_t)text[index]) return 0;
        ++index;
    }
    return index == length;
}

static const char *status_name(int32_t status) {
    switch (status) {
        case PXA_STATUS_OK: return "OK";
        case PXA_STATUS_DENIED: return "DENIED";
        case PXA_STATUS_WOULD_BLOCK: return "WOULD_BLOCK";
        case PXA_STATUS_CANCELLED: return "CANCELLED";
        case PXA_STATUS_TIMED_OUT: return "TIMED_OUT";
        case PXA_STATUS_UNAVAILABLE: return "UNAVAILABLE";
        case PXA_STATUS_IO_ERROR: return "IO_ERROR";
        case PXA_STATUS_PROTOCOL_ERROR: return "PROTOCOL_ERROR";
        case PXA_STATUS_LIMIT_EXCEEDED: return "LIMIT_EXCEEDED";
        default: return "ERROR";
    }
}

static int close_handle(uint32_t handle) {
    uint8_t payload[4];
    pxa_writer_t writer;
    if (handle == 0) return 1;
    payload[0] = (uint8_t)handle;
    payload[1] = (uint8_t)(handle >> 8);
    payload[2] = (uint8_t)(handle >> 16);
    payload[3] = (uint8_t)(handle >> 24);
    pxa_writer_init(&writer, packet, sizeof(packet));
    return pxa_message(&writer, PXA_SERVICE_CORE, PXA_CORE_CLOSE_HANDLE, 0,
                       payload, sizeof(payload)) &&
           pxa_control(writer.data, (uint32_t)writer.length) == PXA_STATUS_OK;
}

static int create_button(pxa_ui_transaction_t *transaction, uint32_t node,
                         uint32_t parent, const char *label, uint8_t primary) {
    const uint32_t text_node = node + UINT32_C(100);
    return pxa_ui_create_typed(transaction, node, parent, 0,
                               PXA_UI_NODE_CONTROL,
                               PXA_UI_CONTROL_BUTTON) &&
           pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, 36) &&
           pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_MIN_WIDTH,
                             PXA_UI_LENGTH_PX, 0) &&
           pxa_ui_set_u16(transaction, node, PXA_UI_PROPERTY_GROW, 1) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_COLUMN) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_JUSTIFY,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_CENTER) &&
           pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_RADIUS, 5) &&
           pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_BORDER_WIDTH,
                         primary ? 0 : 1) &&
           pxa_ui_set_theme_color(transaction, node, PXA_UI_PROPERTY_BORDER_COLOR,
                                  PXA_UI_THEME_BORDER) &&
           pxa_ui_set_theme_color(transaction, node, PXA_UI_PROPERTY_BACKGROUND,
                                  primary ? PXA_UI_THEME_PRIMARY :
                                            PXA_UI_THEME_SURFACE) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ENABLED,
                         (uint8_t)!waiting) &&
           pxa_ui_set_event_mask(transaction, node,
                                 PXA_UI_EVENT_MASK_CLICK) &&
           pxa_ui_create(transaction, text_node, node, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, text_node, label, string_length(label)) &&
           pxa_ui_set_font_role(transaction, text_node, PXA_UI_FONT_ROLE_BODY) &&
           pxa_ui_set_theme_color(transaction, text_node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  primary ? PXA_UI_THEME_ON_PRIMARY :
                                            PXA_UI_THEME_TEXT);
}

static int create_row(pxa_ui_transaction_t *transaction, uint32_t node) {
    return pxa_ui_create(transaction, node, 5, 0, PXA_UI_NODE_BOX) &&
           pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_LAYOUT,
                         PXA_UI_LAYOUT_ROW) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_ALIGN,
                         PXA_UI_ALIGN_STRETCH) &&
           pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_GAP, 6);
}

static void build_status(char *summary, size_t summary_capacity,
                         char *detail, size_t detail_capacity) {
    size_t offset = 0;
    summary[0] = '\0';
    detail[0] = '\0';
    if (permission_denied) {
        (void)append_text(summary, summary_capacity, 0, "PERMISSION DENIED");
        (void)append_text(detail, detail_capacity, 0, "net.client");
        return;
    }
    if (waiting) {
        if (waiting_permission) {
            (void)append_text(summary, summary_capacity, 0, "PERMISSION");
        } else if (suite_running) {
            offset = append_text(summary, summary_capacity, 0, "RUN ");
            offset = append_u32(summary, summary_capacity, offset,
                                (uint32_t)suite_index + 1u);
            offset = append_text(summary, summary_capacity, offset, "/");
            (void)append_u32(summary, summary_capacity, offset, CASE_COUNT);
        } else {
            offset = append_text(summary, summary_capacity, 0,
                                 cases[current_case].label);
            (void)append_text(summary, summary_capacity, offset,
                              stream_waiting ? " STREAM" : " RUNNING");
        }
    } else if (suite_passed != 0 || suite_failed != 0) {
        offset = append_text(summary, summary_capacity, 0, "PASS ");
        offset = append_u32(summary, summary_capacity, offset, suite_passed);
        offset = append_text(summary, summary_capacity, offset, "  FAIL ");
        (void)append_u32(summary, summary_capacity, offset, suite_failed);
    } else {
        (void)append_text(summary, summary_capacity, 0, has_result ?
                          (last_passed ? "PASS" : "FAIL") : "READY");
    }
    if (!has_result) {
        (void)append_text(detail, detail_capacity, 0,
                          "HTTPS  |  NET ABI 0.2");
        return;
    }
    offset = append_text(detail, detail_capacity, 0, cases[current_case].label);
    offset = append_text(detail, detail_capacity, offset, "  ");
    if (response_status == PXA_STATUS_OK) {
        offset = append_text(detail, detail_capacity, offset, "HTTP ");
        offset = append_u32(detail, detail_capacity, offset,
                            response_status_code);
        offset = append_text(detail, detail_capacity, offset, "  BODY ");
        offset = append_u32(detail, detail_capacity, offset, body_bytes);
        if (response_has_header)
            (void)append_text(detail, detail_capacity, offset, "  HEADER");
    } else {
        (void)append_text(detail, detail_capacity, offset,
                          status_name(response_status));
    }
}

static int render(void) {
    char summary[64];
    char detail[128];
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = pxa_lab_ui_generation + 1u;
    uint8_t result_color = !has_result ? PXA_UI_THEME_MUTED :
                           last_passed ? PXA_UI_THEME_SUCCESS :
                                         PXA_UI_THEME_DANGER;
    int ok;
    build_status(summary, sizeof(summary), detail, sizeof(detail));
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
                           PXA_UI_LENGTH_PX, 42) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 2, 12, 6, 12, 6) &&
         pxa_ui_set_dp(&transaction, 2, PXA_UI_PROPERTY_GAP, 8) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 3, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_icon(&transaction, 3, 5) &&
         pxa_ui_set_theme_color(&transaction, 3, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 4, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 4, "Network ABI Lab", 15) &&
         pxa_ui_set_font_role(&transaction, 4, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 4, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 5, 1, 0, PXA_UI_NODE_SCROLL) &&
         pxa_ui_set_length(&transaction, 5, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 5, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 0) &&
         pxa_ui_set_u16(&transaction, 5, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_SCROLLBAR, 1) &&
         pxa_ui_set_padding(&transaction, 5, 12, 10, 12, 12) &&
         pxa_ui_set_dp(&transaction, 5, PXA_UI_PROPERTY_GAP, 8) &&
         pxa_ui_create(&transaction, 6, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 6, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 6, summary, string_length(summary)) &&
         pxa_ui_set_font_role(&transaction, 6, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 6, PXA_UI_PROPERTY_FOREGROUND,
                                waiting ? PXA_UI_THEME_WARNING : result_color) &&
         pxa_ui_create(&transaction, 7, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 7, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 7, detail, string_length(detail)) &&
         pxa_ui_set_theme_color(&transaction, 7, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         create_row(&transaction, 10) &&
         create_button(&transaction, 20, 10, "GET", 0) &&
         create_button(&transaction, 21, 10, "HEAD", 0) &&
         create_button(&transaction, 22, 10, "POST", 0) &&
         create_row(&transaction, 11) &&
         create_button(&transaction, 23, 11, "PUT", 0) &&
         create_button(&transaction, 24, 11, "PATCH", 0) &&
         create_button(&transaction, 25, 11, "DELETE", 0) &&
         create_row(&transaction, 12) &&
         create_button(&transaction, NODE_RUN_ALL, 12, "RUN ALL", 1) &&
         create_button(&transaction, 41, 12, "LEGACY", 0) &&
         create_row(&transaction, 13) &&
         create_button(&transaction, 42, 13, "HTTP 404", 0) &&
         create_button(&transaction, 43, 13, "HTTP 302", 0) &&
         create_row(&transaction, 14) &&
         create_button(&transaction, 44, 14, "TIMEOUT", 0) &&
         create_button(&transaction, 45, 14, "HTTP 503", 0) &&
         create_button(&transaction, 46, 14, "LIMIT", 0);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    pxa_lab_ui_generation = next;
    return 1;
}

static int request_permission(void) {
    static const char name[] = "net.client";
    static const uint8_t scope[] = "https://postman-echo.com";
    waiting = 1;
    waiting_permission = 1;
    permission_denied = 0;
    return pxa_permission_acquire(REQUEST_PERMISSION, name, sizeof(name) - 1u,
                                  scope, sizeof(scope) - 1u, request_payload,
                                  sizeof(request_payload), packet,
                                  sizeof(packet));
}

static int begin_case(uint8_t case_index) {
    static const char accept_name[] = "accept";
    static const uint8_t accept_value[] = "application/json";
    static const char content_type_name[] = "content-type";
    static const uint8_t content_type_value[] = "application/json";
    static const uint8_t request_body[] = "{\"source\":\"pxa-net-lab\"}";
    const char *wanted_headers[1];
    uint16_t wanted_header_lengths[1];
    pxa_net_header_t headers[2];
    pxa_net_http_request_t request = {0};
    const net_case_t *test;
    uint8_t has_body;
    if (case_index >= CASE_COUNT || permission_handle == 0) return 0;
    test = &cases[case_index];
    current_case = case_index;
    waiting = 1;
    has_result = 0;
    waiting_permission = 0;
    stream_waiting = 0;
    body_handle = 0;
    body_bytes = 0;
    response_status = PXA_STATUS_INTERNAL;
    response_status_code = 0;
    response_flags = 0;
    response_length = 0;
    response_has_header = 0;
    response_metadata_valid = 0;
    active_request_id = REQUEST_NET_BASE + (uint32_t)case_index;
    if (test->legacy) {
        return pxa_net_fetch_get(active_request_id, test->url,
                                 string_length(test->url), permission_handle,
                                 1024, request_payload,
                                 sizeof(request_payload), packet,
                                 sizeof(packet));
    }
    headers[0].name = accept_name;
    headers[0].name_length = sizeof(accept_name) - 1u;
    headers[0].value = accept_value;
    headers[0].value_length = sizeof(accept_value) - 1u;
    headers[1].name = content_type_name;
    headers[1].name_length = sizeof(content_type_name) - 1u;
    headers[1].value = content_type_value;
    headers[1].value_length = sizeof(content_type_value) - 1u;
    has_body = (uint8_t)(test->method != PXA_NET_METHOD_GET &&
                         test->method != PXA_NET_METHOD_HEAD);
    request.method = test->method;
    request.url = test->url;
    request.url_length = (uint16_t)string_length(test->url);
    request.permission_handle = permission_handle;
    request.max_response_bytes = case_index == CASE_LIMIT ? 4u : 1024u;
    request.timeout_ms = case_index == CASE_TIMEOUT ? 100u :
                                                     NORMAL_REQUEST_TIMEOUT_MS;
    request.headers = headers;
    request.header_count = has_body ? 2u : 1u;
    request.body = has_body ? request_body : NULL;
    request.body_length = has_body ? sizeof(request_body) - 1u : 0;
    if (test->expected_header != NULL) {
        wanted_headers[0] = test->expected_header;
        wanted_header_lengths[0] = (uint16_t)string_length(
            test->expected_header);
        request.wanted_response_headers = wanted_headers;
        request.wanted_response_header_lengths = wanted_header_lengths;
        request.wanted_response_header_count = 1;
    }
    return pxa_net_http_request(active_request_id, &request, request_payload,
                                sizeof(request_payload), packet,
                                sizeof(packet));
}

static int complete_case(uint8_t passed) {
    waiting = 0;
    stream_waiting = 0;
    has_result = 1;
    last_passed = passed;
    if (!suite_running) return 1;
    if (passed) ++suite_passed;
    else ++suite_failed;
    if ((uint8_t)(suite_index + 1u) >= CASE_COUNT) {
        suite_running = 0;
        return 1;
    }
    ++suite_index;
    if (!begin_case(suite_index)) {
        suite_running = 0;
        waiting = 0;
        ++suite_failed;
        last_passed = 0;
    }
    return 1;
}

static int consume_body(uint8_t metadata_valid) {
    uint8_t bytes[64];
    for (;;) {
        int32_t count = pxa_io(body_handle, PXA_IO_READ, bytes, sizeof(bytes));
        if (count == PXA_STATUS_WOULD_BLOCK) {
            response_metadata_valid = metadata_valid;
            stream_waiting = 1;
            return pxa_clock_set_period(50);
        }
        if (count < 0) {
            response_status = count;
            if (!close_handle(body_handle)) return 0;
            body_handle = 0;
            (void)pxa_clock_set_period(0);
            return complete_case(0);
        }
        if (count == 0) {
            uint8_t valid = metadata_valid;
            if (!close_handle(body_handle)) return 0;
            body_handle = 0;
            (void)pxa_clock_set_period(0);
            if ((response_flags & PXA_NET_RESPONSE_BODY_LENGTH_KNOWN) != 0 &&
                response_length != body_bytes) valid = 0;
            return complete_case(valid);
        }
        if ((uint32_t)count > PXA_NET_MAX_RESPONSE_BODY_BYTES - body_bytes) {
            if (!close_handle(body_handle)) return 0;
            body_handle = 0;
            return complete_case(0);
        }
        body_bytes += (uint32_t)count;
    }
}

static int handle_http_result(const pxa_event_t *event) {
    pxa_net_http_result_t result;
    const net_case_t *test = &cases[current_case];
    uint8_t valid;
    uint8_t flags_valid;
    uint16_t index;
    if (!pxa_net_parse_http_result(event, &result)) {
        response_status = PXA_STATUS_PROTOCOL_ERROR;
        return complete_case(0);
    }
    response_status = result.status;
    valid = (uint8_t)(result.status == test->expected_status);
    if (result.status != PXA_STATUS_OK) return complete_case(valid);
    response_status_code = result.status_code;
    response_flags = result.flags;
    response_length = result.body_length;
    body_handle = result.body_handle;
    for (index = 0; index < result.header_count; ++index) {
        const pxa_net_header_view_t *header = &result.headers[index];
        if (bytes_equal_text(header->name, header->name_length,
                             test->expected_header)) {
            response_has_header = 1;
        }
    }
    flags_valid = (uint8_t)(result.flags == test->expected_flags ||
                            (test->body_optional &&
                             result.flags ==
                                 (test->expected_flags &
                                  ~PXA_NET_RESPONSE_BODY_PRESENT)));
    valid = (uint8_t)(valid && result.status_code == test->expected_http_status &&
                      flags_valid &&
                      result.header_count ==
                          (uint16_t)(test->expected_header != NULL) &&
                      (test->expected_header == NULL || response_has_header));
    response_metadata_valid = valid;
    if ((result.flags & PXA_NET_RESPONSE_BODY_PRESENT) == 0)
        return complete_case((uint8_t)(valid && result.body_handle == 0 &&
                             result.body_length == 0));
    return consume_body(valid);
}

static int handle_legacy_result(const pxa_event_t *event) {
    pxa_net_fetch_result_t result;
    uint8_t valid;
    if (!pxa_net_parse_fetch(event, &result)) {
        response_status = PXA_STATUS_PROTOCOL_ERROR;
        return complete_case(0);
    }
    response_status = result.status;
    valid = (uint8_t)(result.status == PXA_STATUS_OK && result.status_code == 200);
    if (result.status != PXA_STATUS_OK) return complete_case(0);
    response_status_code = result.status_code;
    body_handle = result.body_handle;
    response_flags = PXA_NET_RESPONSE_BODY_PRESENT;
    response_metadata_valid = valid;
    return consume_body(valid);
}

static int start_selected(uint8_t case_index, uint8_t run_suite) {
    queued_case = case_index;
    queued_suite = run_suite;
    permission_denied = 0;
    suite_passed = 0;
    suite_failed = 0;
    if (permission_handle == 0) return request_permission();
    suite_running = run_suite;
    suite_index = 0;
    return begin_case(run_suite ? CASE_GET : case_index);
}

static int node_case(uint32_t node, uint8_t *case_index) {
    uint8_t index;
    for (index = 0; index < CASE_COUNT; ++index) {
        if (cases[index].button_node == node) {
            *case_index = index;
            return 1;
        }
    }
    return 0;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    return pxa_window_fullscreen() && render() ? PXA_STATUS_OK :
                                                PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND && !waiting) {
        uint8_t case_index;
        int started = 0;
        if (ui_event.node == NODE_RUN_ALL)
            started = start_selected(CASE_GET, 1);
        else if (node_case(ui_event.node, &case_index))
            started = start_selected(case_index, 0);
        else
            return PXA_EVENT_UNHANDLED;
        return started && render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_PERMISSION &&
        parsed.opcode == PXA_PERMISSION_ACQUIRE &&
        parsed.request_id == REQUEST_PERMISSION) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(&parsed, &result))
            return PXA_STATUS_INTERNAL;
        waiting_permission = 0;
        if (result.status != PXA_STATUS_OK) {
            waiting = 0;
            permission_denied = 1;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        permission_handle = result.handle;
        suite_running = queued_suite;
        suite_index = 0;
        if (!begin_case(queued_suite ? CASE_GET : queued_case))
            return PXA_STATUS_INTERNAL;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_NET &&
        parsed.request_id == active_request_id && waiting) {
        int handled = cases[current_case].legacy
                          ? (parsed.opcode == PXA_NET_FETCH &&
                             handle_legacy_result(&parsed))
                          : (parsed.opcode == PXA_NET_HTTP_REQUEST &&
                             handle_http_result(&parsed));
        return handled && render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_CLOCK &&
        parsed.opcode == PXA_CLOCK_TICK && stream_waiting && body_handle != 0) {
        stream_waiting = 0;
        return consume_body(response_metadata_valid) && render()
                   ? PXA_EVENT_HANDLED
                   : PXA_STATUS_INTERNAL;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
