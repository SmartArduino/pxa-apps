#include "pxa_device.h"
#include "pxa_app_messages.h"
#include "pxa_i18n.h"
#include "pxa_net.h"
#include "pxa_permission.h"
#include "pxa_ui.h"

#define REQUEST_DEVICE_PERMISSION UINT32_C(1)
#define REQUEST_DEVICE_MAC UINT32_C(2)
#define REQUEST_NETWORK_PERMISSION UINT32_C(3)
#define REQUEST_WEATHER UINT32_C(4)
#define NODE_REFRESH UINT32_C(18)
#define AUTO_REFRESH_TICKS UINT16_C(600)

#define WEATHER_IDLE UINT8_C(0)
#define WEATHER_LOADING UINT8_C(1)
#define WEATHER_READY UINT8_C(2)
#define WEATHER_DENIED UINT8_C(3)
#define WEATHER_ERROR UINT8_C(4)

#define ICON_LOCATION "\xef\x8f\x85"
#define ICON_CLOCK "\xef\x80\x97"
#define ICON_REFRESH "\xef\x80\xa1"
#define ICON_SUN "\xef\x86\x85"
#define ICON_MOON "\xef\x86\x86"
#define ICON_CLOUD "\xef\x83\x82"
#define ICON_CLOUD_SUN "\xef\x9d\x86"
#define ICON_CLOUD_BOLT "\xef\x9d\xac"
#define ICON_CLOUD_RAIN "\xef\x9c\xbd"
#define ICON_SNOW "\xef\x8b\x9c"
#define ICON_SMOG "\xef\x9d\x9f"
#define ICON_WIND "\xef\x9c\xae"
#define ICON_INFO "\xef\x81\x9a"

#if defined(__has_include)
#if __has_include("weather_config.h")
#include "weather_config.h"
#endif
#endif
#ifndef PXA_WEATHER_API_KEY
#define PXA_WEATHER_API_KEY ""
#endif

static const char weather_url_prefix[] =
    "http://8.166.128.230:3100/api/weather?"
    "key=" PXA_WEATHER_API_KEY "&mac=";

static uint8_t packet[3072];
static uint8_t request_payload[1024];
static uint8_t response_body[1024];
static char weather_url[128];
static uint32_t generation;
static uint32_t device_permission_handle;
static uint32_t network_permission_handle;
static uint32_t body_handle;
static uint32_t response_size;
static uint16_t http_status;
static uint64_t response_length;
static uint8_t response_flags;
static uint8_t state;
static uint8_t stream_waiting;
static uint8_t has_mac;
static uint8_t has_weather;
static uint16_t auto_refresh_ticks;
static int32_t temperature;
static int32_t weather_code = -1;
static char city[64];
static char weather_text[32];
static char last_update[64];
static pxa_i18n_t i18n;

static const char *message(pxa_i18n_message_id_t id) {
    return pxa_i18n_cstr(&i18n, id);
}

static size_t string_length(const char *value) {
    size_t size = 0;
    if (value == NULL) return 0;
    while (value[size] != '\0') ++size;
    return size;
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

static int hex_value(uint8_t value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static int append_codepoint(char *output, size_t capacity, size_t *offset,
                            uint32_t codepoint) {
    if (codepoint <= UINT32_C(0x7f)) {
        if (*offset + 1u >= capacity) return 0;
        output[(*offset)++] = (char)codepoint;
    } else if (codepoint <= UINT32_C(0x7ff)) {
        if (*offset + 2u >= capacity) return 0;
        output[(*offset)++] = (char)(0xc0u | (codepoint >> 6));
        output[(*offset)++] = (char)(0x80u | (codepoint & 0x3fu));
    } else if (codepoint <= UINT32_C(0xffff) &&
               (codepoint < UINT32_C(0xd800) || codepoint > UINT32_C(0xdfff))) {
        if (*offset + 3u >= capacity) return 0;
        output[(*offset)++] = (char)(0xe0u | (codepoint >> 12));
        output[(*offset)++] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
        output[(*offset)++] = (char)(0x80u | (codepoint & 0x3fu));
    } else {
        return 0;
    }
    output[*offset] = '\0';
    return 1;
}

static int decode_json_string(const uint8_t *data, size_t size, size_t offset,
                              char *output, size_t capacity) {
    size_t output_size = 0;
    if (data == NULL || output == NULL || capacity == 0 || offset >= size ||
        data[offset] != '"') return 0;
    ++offset;
    output[0] = '\0';
    while (offset < size) {
        uint8_t value = data[offset++];
        if (value == '"') return 1;
        if (value < 0x20u) return 0;
        if (value != '\\') {
            if (output_size + 1u >= capacity) return 0;
            output[output_size++] = (char)value;
            output[output_size] = '\0';
            continue;
        }
        if (offset >= size) return 0;
        value = data[offset++];
        if (value == '"' || value == '\\' || value == '/') {
            if (output_size + 1u >= capacity) return 0;
            output[output_size++] = (char)value;
            output[output_size] = '\0';
        } else if (value == 'b' || value == 'f' || value == 'n' ||
                   value == 'r' || value == 't') {
            char decoded = value == 'b' ? '\b' : value == 'f' ? '\f' :
                           value == 'n' ? '\n' : value == 'r' ? '\r' : '\t';
            if (output_size + 1u >= capacity) return 0;
            output[output_size++] = decoded;
            output[output_size] = '\0';
        } else if (value == 'u') {
            uint32_t codepoint = 0;
            size_t index;
            if (size - offset < 4u) return 0;
            for (index = 0; index < 4u; ++index) {
                int digit = hex_value(data[offset + index]);
                if (digit < 0) return 0;
                codepoint = (codepoint << 4) | (uint32_t)digit;
            }
            offset += 4u;
            if (!append_codepoint(output, capacity, &output_size, codepoint))
                return 0;
        } else {
            return 0;
        }
    }
    return 0;
}

static int find_json_string(const uint8_t *data, size_t size, const char *key,
                            char *output, size_t capacity) {
    size_t key_size = string_length(key);
    size_t offset;
    if (key_size == 0 || size < key_size + 3u) return 0;
    for (offset = 0; offset + key_size + 2u <= size; ++offset) {
        size_t index;
        size_t value_offset;
        if (data[offset] != '"' || data[offset + key_size + 1u] != '"')
            continue;
        for (index = 0; index < key_size; ++index) {
            if (data[offset + 1u + index] != (uint8_t)key[index]) break;
        }
        if (index != key_size) continue;
        value_offset = offset + key_size + 2u;
        while (value_offset < size && (data[value_offset] == ' ' ||
               data[value_offset] == '\t' || data[value_offset] == '\r' ||
               data[value_offset] == '\n')) ++value_offset;
        if (value_offset >= size || data[value_offset++] != ':') continue;
        while (value_offset < size && (data[value_offset] == ' ' ||
               data[value_offset] == '\t' || data[value_offset] == '\r' ||
               data[value_offset] == '\n')) ++value_offset;
        return decode_json_string(data, size, value_offset, output, capacity);
    }
    return 0;
}

static int parse_i32(const char *text, int32_t *output) {
    size_t offset = 0;
    int sign = 1;
    int32_t value = 0;
    if (text == NULL || output == NULL) return 0;
    if (text[offset] == '-') {
        sign = -1;
        ++offset;
    } else if (text[offset] == '+') {
        ++offset;
    }
    if (text[offset] < '0' || text[offset] > '9') return 0;
    while (text[offset] >= '0' && text[offset] <= '9') {
        if (value > 1000000) return 0;
        value = value * 10 + (int32_t)(text[offset++] - '0');
    }
    if (text[offset] != '\0') return 0;
    *output = sign < 0 ? -value : value;
    return 1;
}

static int parse_weather_response(void) {
    char parsed_city[sizeof(city)];
    char parsed_weather[sizeof(weather_text)];
    char parsed_update[sizeof(last_update)];
    char parsed_temperature[16];
    char parsed_code[16];
    int32_t parsed_temp;
    int32_t parsed_weather_code;
    if (!find_json_string(response_body, response_size, "name", parsed_city,
                          sizeof(parsed_city)) ||
        !find_json_string(response_body, response_size, "text", parsed_weather,
                          sizeof(parsed_weather)) ||
        !find_json_string(response_body, response_size, "temperature",
                          parsed_temperature, sizeof(parsed_temperature)) ||
        !find_json_string(response_body, response_size, "code", parsed_code,
                          sizeof(parsed_code)) ||
        !find_json_string(response_body, response_size, "last_update",
                          parsed_update, sizeof(parsed_update)) ||
        !parse_i32(parsed_temperature, &parsed_temp) ||
        !parse_i32(parsed_code, &parsed_weather_code) ||
        parsed_weather_code < 0 || parsed_weather_code > 99) return 0;
    copy_text(city, sizeof(city), parsed_city);
    copy_text(weather_text, sizeof(weather_text), parsed_weather);
    copy_text(last_update, sizeof(last_update), parsed_update);
    temperature = parsed_temp;
    weather_code = parsed_weather_code;
    has_weather = 1;
    return 1;
}

static const char *weather_icon(void) {
    if (!has_weather) return ICON_CLOUD_SUN;
    if (weather_code == 0 || weather_code == 2 || weather_code == 38)
        return ICON_SUN;
    if (weather_code == 1 || weather_code == 3) return ICON_MOON;
    if (weather_code >= 4 && weather_code <= 8) return ICON_CLOUD_SUN;
    if (weather_code == 9) return ICON_CLOUD;
    if (weather_code == 11 || weather_code == 12) return ICON_CLOUD_BOLT;
    if ((weather_code >= 10 && weather_code <= 20)) return ICON_CLOUD_RAIN;
    if (weather_code >= 21 && weather_code <= 25) return ICON_SNOW;
    if (weather_code >= 26 && weather_code <= 31) return ICON_SMOG;
    if (weather_code >= 32 && weather_code <= 36) return ICON_WIND;
    return ICON_CLOUD;
}

static const char *weather_summary(void) {
    if (!has_weather) return message(PXA_MSG_SUMMARY_LOCATING);
    if (weather_code == 11 || weather_code == 12)
        return message(PXA_MSG_SUMMARY_THUNDERSTORM);
    if (weather_code >= 10 && weather_code <= 20)
        return message(PXA_MSG_SUMMARY_RAIN);
    if (weather_code >= 21 && weather_code <= 25)
        return message(PXA_MSG_SUMMARY_SNOW);
    if (weather_code >= 26 && weather_code <= 31)
        return message(PXA_MSG_SUMMARY_LOW_VISIBILITY);
    if (weather_code >= 32 && weather_code <= 36)
        return message(PXA_MSG_SUMMARY_WIND);
    if (temperature >= 35)
        return message(PXA_MSG_SUMMARY_VERY_HOT);
    if (temperature >= 28)
        return message(PXA_MSG_SUMMARY_WARM);
    if (temperature <= 5)
        return message(PXA_MSG_SUMMARY_COLD);
    if (temperature <= 15)
        return message(PXA_MSG_SUMMARY_COOL);
    return message(PXA_MSG_SUMMARY_COMFORTABLE);
}

static const char *temperature_feel(void) {
    if (!has_weather) return "--";
    if (temperature >= 35) return message(PXA_MSG_FEEL_HOT);
    if (temperature >= 28) return message(PXA_MSG_FEEL_WARM);
    if (temperature >= 22) return message(PXA_MSG_FEEL_COMFORTABLE);
    if (temperature >= 15) return message(PXA_MSG_FEEL_COOL);
    if (temperature >= 5) return message(PXA_MSG_FEEL_COLD);
    return message(PXA_MSG_FEEL_VERY_COLD);
}

static const char *day_phase(void) {
    if (!has_weather) return message(PXA_MSG_SCREEN_PHASE_CONDITIONS);
    return weather_code == 1 || weather_code == 3
               ? message(PXA_MSG_SCREEN_PHASE_NIGHT)
               : message(PXA_MSG_SCREEN_PHASE_DAY);
}

static uint8_t weather_accent(void) {
    if (!has_weather) return PXA_UI_THEME_MUTED;
    if (weather_code >= 10 && weather_code <= 36) return PXA_UI_THEME_PRIMARY;
    return PXA_UI_THEME_WARNING;
}

static void format_temperature(char *output, size_t capacity) {
    size_t offset = 0;
    uint32_t magnitude;
    if (!has_weather) {
        copy_text(output, capacity, "-- C");
        return;
    }
    if (temperature < 0) {
        offset = append_text(output, capacity, offset, "-");
        magnitude = (uint32_t)(-(temperature + 1)) + 1u;
    } else {
        magnitude = (uint32_t)temperature;
    }
    offset = append_u32(output, capacity, offset, magnitude);
    (void)append_text(output, capacity, offset, "\xc2\xb0" "C");
}

static void format_update(char *output, size_t capacity) {
    size_t size = string_length(last_update);
    size_t offset = 0;
    char timestamp[12];
    pxa_i18n_argument_t argument;
    if (!has_weather || size < 16u) {
        copy_text(output, capacity, message(PXA_MSG_UPDATE_PROVIDER));
        return;
    }
    timestamp[offset++] = last_update[5];
    timestamp[offset++] = last_update[6];
    timestamp[offset++] = '-';
    timestamp[offset++] = last_update[8];
    timestamp[offset++] = last_update[9];
    timestamp[offset++] = ' ';
    timestamp[offset++] = last_update[11];
    timestamp[offset++] = last_update[12];
    timestamp[offset++] = ':';
    timestamp[offset++] = last_update[14];
    timestamp[offset++] = last_update[15];
    timestamp[offset] = '\0';
    argument.name = "time";
    argument.name_size = 4u;
    argument.type = PXA_I18N_ARGUMENT_STRING;
    argument.value.string.data = timestamp;
    argument.value.string.size = offset;
    (void)pxa_i18n_format(&i18n, PXA_MSG_UPDATE_TIME, &argument, 1u,
                          output, capacity);
}

static const char *status_text(void) {
    if (state == WEATHER_LOADING) return message(PXA_MSG_STATUS_UPDATING);
    if (state == WEATHER_READY)
        return message(PXA_MSG_STATUS_READY);
    if (state == WEATHER_DENIED)
        return message(PXA_MSG_STATUS_PERMISSION);
    if (http_status == 400) return message(PXA_MSG_STATUS_INVALID_DEVICE);
    if (http_status == 401) return message(PXA_MSG_STATUS_AUTHENTICATION);
    if (http_status == 503) return message(PXA_MSG_STATUS_UNAVAILABLE);
    if (http_status != 0) return message(PXA_MSG_STATUS_UNEXPECTED);
    if (state == WEATHER_ERROR) return message(PXA_MSG_STATUS_FAILED);
    return message(PXA_MSG_STATUS_WAITING);
}

static int schedule_auto_refresh(void) {
    auto_refresh_ticks = 0;
    return pxa_clock_set_period(1000);
}

static int render(void) {
    char temperature_text[24];
    char update_text[48];
    const char *display_city = has_weather ? city :
        message(PXA_MSG_LOCATION_LOCATING);
    const char *display_weather = has_weather ? weather_text :
        message(PXA_MSG_WEATHER_WAITING);
    const char *summary = weather_summary();
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = generation + 1u;
    uint8_t status_color = state == WEATHER_READY ? PXA_UI_THEME_SUCCESS :
                           state == WEATHER_ERROR || state == WEATHER_DENIED
                               ? PXA_UI_THEME_DANGER
                               : PXA_UI_THEME_MUTED;
    int ok;
    format_temperature(temperature_text, sizeof(temperature_text));
    format_update(update_text, sizeof(update_text));
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
         pxa_ui_set_padding(&transaction, 2, 14, 5, 14, 5) &&
         pxa_ui_set_dp(&transaction, 2, PXA_UI_PROPERTY_GAP, 6) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 3, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 3, ICON_CLOUD_SUN,
                         sizeof(ICON_CLOUD_SUN) - 1u) &&
         pxa_ui_set_font_role(&transaction, 3, PXA_UI_FONT_ROLE_ICON) &&
         pxa_ui_set_theme_color(&transaction, 3, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 4, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 4, message(PXA_MSG_SCREEN_TITLE),
                         pxa_i18n_size(&i18n, PXA_MSG_SCREEN_TITLE)) &&
         pxa_ui_set_font_role(&transaction, 4, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 4, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 21, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u16(&transaction, 21, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_text(&transaction, 21, day_phase(), string_length(day_phase())) &&
         pxa_ui_set_font_role(&transaction, 21, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_u8(&transaction, 21, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_END) &&
         pxa_ui_set_theme_color(&transaction, 21, PXA_UI_PROPERTY_FOREGROUND,
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
         pxa_ui_set_event_mask(&transaction, 5, PXA_UI_EVENT_MASK_SCROLL) &&
         pxa_ui_set_padding(&transaction, 5, 12, 10, 12, 12) &&
         pxa_ui_set_dp(&transaction, 5, PXA_UI_PROPERTY_GAP, 8) &&
         pxa_ui_create(&transaction, 6, 5, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 6, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u8(&transaction, 6, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 6, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, 6, PXA_UI_PROPERTY_GAP, 6) &&
         pxa_ui_create(&transaction, 7, 6, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 7, ICON_LOCATION,
                         sizeof(ICON_LOCATION) - 1u) &&
         pxa_ui_set_font_role(&transaction, 7, PXA_UI_FONT_ROLE_ICON) &&
         pxa_ui_set_theme_color(&transaction, 7, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 8, 6, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 8, display_city,
                         string_length(display_city)) &&
         pxa_ui_set_font_role(&transaction, 8, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 8, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 9, 5, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 9, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 9, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 88) &&
         pxa_ui_set_u8(&transaction, 9, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 9, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 9, 12, 10, 12, 10) &&
         pxa_ui_set_dp(&transaction, 9, PXA_UI_PROPERTY_GAP, 12) &&
         pxa_ui_set_dp(&transaction, 9, PXA_UI_PROPERTY_RADIUS, 8) &&
         pxa_ui_set_dp(&transaction, 9, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
         pxa_ui_set_theme_color(&transaction, 9, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE) &&
         pxa_ui_set_theme_color(&transaction, 9, PXA_UI_PROPERTY_BORDER_COLOR,
                                PXA_UI_THEME_BORDER) &&
         pxa_ui_create(&transaction, 10, 9, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 10, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_PX, 54) &&
         pxa_ui_set_text(&transaction, 10, weather_icon(),
                         string_length(weather_icon())) &&
         pxa_ui_set_font_role(&transaction, 10, PXA_UI_FONT_ROLE_ICON) &&
         pxa_ui_set_u8(&transaction, 10, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, 10, PXA_UI_PROPERTY_FOREGROUND,
                                weather_accent()) &&
         pxa_ui_create(&transaction, 11, 9, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_u16(&transaction, 11, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 11, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_dp(&transaction, 11, PXA_UI_PROPERTY_GAP, 3) &&
         pxa_ui_create(&transaction, 12, 11, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 12, temperature_text,
                         string_length(temperature_text)) &&
         pxa_ui_set_font_role(&transaction, 12, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 12, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 13, 11, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 13, display_weather,
                         string_length(display_weather)) &&
         pxa_ui_set_theme_color(&transaction, 13, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, 22, 5, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 22, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u8(&transaction, 22, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_padding(&transaction, 22, 10, 8, 10, 8) &&
         pxa_ui_set_dp(&transaction, 22, PXA_UI_PROPERTY_GAP, 8) &&
         pxa_ui_set_dp(&transaction, 22, PXA_UI_PROPERTY_RADIUS, 6) &&
         pxa_ui_set_theme_color(&transaction, 22, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE) &&
         pxa_ui_create(&transaction, 23, 22, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 23, ICON_INFO, sizeof(ICON_INFO) - 1u) &&
         pxa_ui_set_font_role(&transaction, 23, PXA_UI_FONT_ROLE_ICON) &&
         pxa_ui_set_theme_color(&transaction, 23, PXA_UI_PROPERTY_FOREGROUND,
                                weather_accent()) &&
         pxa_ui_create(&transaction, 24, 22, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u16(&transaction, 24, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_text(&transaction, 24, summary, string_length(summary)) &&
         pxa_ui_set_font_role(&transaction, 24, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 24, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 25, 5, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 25, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u8(&transaction, 25, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_dp(&transaction, 25, PXA_UI_PROPERTY_GAP, 8) &&
         pxa_ui_create(&transaction, 26, 25, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_u16(&transaction, 26, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 26, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_padding(&transaction, 26, 10, 8, 10, 8) &&
         pxa_ui_set_dp(&transaction, 26, PXA_UI_PROPERTY_RADIUS, 6) &&
         pxa_ui_set_theme_color(&transaction, 26, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE) &&
         pxa_ui_create(&transaction, 27, 26, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 27, message(PXA_MSG_DETAIL_FEELS_LIKE),
                         pxa_i18n_size(&i18n, PXA_MSG_DETAIL_FEELS_LIKE)) &&
         pxa_ui_set_font_role(&transaction, 27, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 27, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, 28, 26, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 28, temperature_feel(),
                         string_length(temperature_feel())) &&
         pxa_ui_set_font_role(&transaction, 28, PXA_UI_FONT_ROLE_BODY) &&
         pxa_ui_set_theme_color(&transaction, 28, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 29, 25, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_u16(&transaction, 29, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 29, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_padding(&transaction, 29, 10, 8, 10, 8) &&
         pxa_ui_set_dp(&transaction, 29, PXA_UI_PROPERTY_RADIUS, 6) &&
         pxa_ui_set_theme_color(&transaction, 29, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE) &&
         pxa_ui_create(&transaction, 30, 29, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 30,
                         message(PXA_MSG_DETAIL_UPDATE_POLICY),
                         pxa_i18n_size(&i18n,
                                       PXA_MSG_DETAIL_UPDATE_POLICY)) &&
         pxa_ui_set_font_role(&transaction, 30, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 30, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, 31, 29, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 31, message(PXA_MSG_DETAIL_AUTO_SYNC),
                         pxa_i18n_size(&i18n, PXA_MSG_DETAIL_AUTO_SYNC)) &&
         pxa_ui_set_font_role(&transaction, 31, PXA_UI_FONT_ROLE_BODY) &&
         pxa_ui_set_theme_color(&transaction, 31, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 14, 5, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 14, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u8(&transaction, 14, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 14, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, 14, PXA_UI_PROPERTY_GAP, 6) &&
         pxa_ui_create(&transaction, 15, 14, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 15, ICON_CLOCK,
                         sizeof(ICON_CLOCK) - 1u) &&
         pxa_ui_set_font_role(&transaction, 15, PXA_UI_FONT_ROLE_ICON) &&
         pxa_ui_set_theme_color(&transaction, 15, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, 16, 14, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 16, update_text,
                         string_length(update_text)) &&
         pxa_ui_set_theme_color(&transaction, 16, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create(&transaction, 17, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 17, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 17, status_text(),
                         string_length(status_text())) &&
         pxa_ui_set_theme_color(&transaction, 17, PXA_UI_PROPERTY_FOREGROUND,
                                status_color) &&
         pxa_ui_create_typed(&transaction, NODE_REFRESH, 5, 0,
                             PXA_UI_NODE_CONTROL,
                             PXA_UI_CONTROL_BUTTON) &&
         pxa_ui_set_length(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 31) &&
         pxa_ui_set_u8(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_u8(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_GAP, 7) &&
         pxa_ui_set_dp(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_RADIUS, 5) &&
         pxa_ui_set_dp(&transaction, NODE_REFRESH,
                       PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
         pxa_ui_set_theme_color(&transaction, NODE_REFRESH,
                                PXA_UI_PROPERTY_BORDER_COLOR,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_set_u8(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_ENABLED,
                       (uint8_t)(state != WEATHER_LOADING)) &&
         pxa_ui_set_event_mask(&transaction, NODE_REFRESH,
                               PXA_UI_EVENT_MASK_CLICK) &&
         pxa_ui_set_theme_color(&transaction, NODE_REFRESH,
                                PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 19, NODE_REFRESH, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 19, ICON_REFRESH,
                         sizeof(ICON_REFRESH) - 1u) &&
         pxa_ui_set_font_role(&transaction, 19, PXA_UI_FONT_ROLE_ICON) &&
         pxa_ui_set_theme_color(&transaction, 19, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 20, NODE_REFRESH, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 20,
                         state == WEATHER_LOADING
                             ? message(PXA_MSG_ACTION_UPDATING)
                             : message(PXA_MSG_ACTION_UPDATE_NOW),
                         string_length(state == WEATHER_LOADING
                             ? message(PXA_MSG_ACTION_UPDATING)
                             : message(PXA_MSG_ACTION_UPDATE_NOW))) &&
         pxa_ui_set_theme_color(&transaction, 20, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    generation = next;
    return 1;
}

static int acquire_device_permission(void) {
    static const char permission_name[] = "device.identity";
    static const uint8_t permission_scope[] = "mac.wifi.station.hardware";
    state = WEATHER_LOADING;
    return pxa_permission_acquire(
        REQUEST_DEVICE_PERMISSION, permission_name, sizeof(permission_name) - 1u,
        permission_scope, sizeof(permission_scope) - 1u, request_payload,
        sizeof(request_payload), packet, sizeof(packet));
}

static int acquire_network_permission(void) {
    static const char permission_name[] = "net.client";
    static const uint8_t permission_scope[] = "http://8.166.128.230:3100";
    state = WEATHER_LOADING;
    return pxa_permission_acquire(
        REQUEST_NETWORK_PERMISSION, permission_name, sizeof(permission_name) - 1u,
        permission_scope, sizeof(permission_scope) - 1u, request_payload,
        sizeof(request_payload), packet, sizeof(packet));
}

static int fetch_device_mac(void) {
    state = WEATHER_LOADING;
    return pxa_device_get_mac(
        REQUEST_DEVICE_MAC, PXA_DEVICE_MAC_KIND_WIFI_STATION_HARDWARE,
        device_permission_handle, request_payload, sizeof(request_payload),
        packet, sizeof(packet));
}

static int set_weather_mac(const uint8_t mac[6]) {
    size_t prefix_size = sizeof(weather_url_prefix) - 1u;
    if (prefix_size + 18u > sizeof(weather_url)) return 0;
    copy_text(weather_url, sizeof(weather_url), weather_url_prefix);
    return pxa_device_format_mac_colon(mac, weather_url + prefix_size,
                                       sizeof(weather_url) - prefix_size);
}

static int fetch_weather(void) {
    static const char accept_name[] = "accept";
    static const uint8_t accept_value[] = "application/json";
    static const pxa_net_header_t headers[] = {
        {accept_name, sizeof(accept_name) - 1u, accept_value,
         sizeof(accept_value) - 1u},
    };
    pxa_net_http_request_t request = {0};
    response_size = 0;
    response_length = 0;
    response_flags = 0;
    http_status = 0;
    stream_waiting = 0;
    auto_refresh_ticks = 0;
    state = WEATHER_LOADING;
    request.method = PXA_NET_METHOD_GET;
    request.url = weather_url;
    request.url_length = (uint16_t)string_length(weather_url);
    request.permission_handle = network_permission_handle;
    request.max_response_bytes = sizeof(response_body);
    request.timeout_ms = 10000;
    request.headers = headers;
    request.header_count = 1;
    return pxa_net_http_request(REQUEST_WEATHER, &request, request_payload,
                                sizeof(request_payload), packet,
                                sizeof(packet));
}

static int start_weather_request(void) {
    if (device_permission_handle == 0) return acquire_device_permission();
    if (!has_mac) return fetch_device_mac();
    if (network_permission_handle == 0) return acquire_network_permission();
    return fetch_weather();
}

static int finish_response(void) {
    uint8_t valid = (uint8_t)parse_weather_response();
    if (!close_handle(body_handle)) return 0;
    body_handle = 0;
    stream_waiting = 0;
    if (!schedule_auto_refresh()) return 0;
    if ((response_flags & PXA_NET_RESPONSE_BODY_LENGTH_KNOWN) != 0 &&
        response_length != response_size) valid = 0;
    state = valid ? WEATHER_READY : WEATHER_ERROR;
    return 1;
}

static int consume_response(void) {
    uint8_t overflow_byte;
    for (;;) {
        uint32_t remaining = (uint32_t)sizeof(response_body) - response_size;
        uint8_t *output = remaining == 0 ? &overflow_byte :
                                          response_body + response_size;
        uint32_t capacity = remaining == 0 ? 1u : remaining;
        int32_t count = pxa_io(body_handle, PXA_IO_READ, output, capacity);
        if (count == PXA_STATUS_WOULD_BLOCK) {
            stream_waiting = 1;
            return pxa_clock_set_period(50);
        }
        if (count < 0 || (remaining == 0 && count != 0) ||
            (uint32_t)(count < 0 ? 0 : count) > remaining) {
            if (!close_handle(body_handle)) return 0;
            body_handle = 0;
            stream_waiting = 0;
            (void)schedule_auto_refresh();
            state = WEATHER_ERROR;
            return 1;
        }
        if (count == 0) return finish_response();
        response_size += (uint32_t)count;
    }
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)pxa_i18n_init_from_start_config(
        &i18n, &pxa_app_i18n_bundle, config, config_length);
    state = WEATHER_IDLE;
    device_permission_handle = 0;
    network_permission_handle = 0;
    has_mac = 0;
    auto_refresh_ticks = 0;
    if (!pxa_window_fullscreen() || !render() || !start_weather_request())
        return PXA_STATUS_INTERNAL;
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    {
        int locale_result = pxa_i18n_handle_event(&i18n, &parsed);
        if (locale_result != 0) {
            return locale_result == 1 && !render()
                       ? PXA_STATUS_INTERNAL
                       : PXA_EVENT_HANDLED;
        }
    }
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == NODE_REFRESH &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND &&
        state != WEATHER_LOADING) {
        int started = start_weather_request();
        return started && render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_PERMISSION &&
        parsed.opcode == PXA_PERMISSION_ACQUIRE &&
        (parsed.request_id == REQUEST_DEVICE_PERMISSION ||
         parsed.request_id == REQUEST_NETWORK_PERMISSION)) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(&parsed, &result))
            return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK) {
            state = WEATHER_DENIED;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (parsed.request_id == REQUEST_DEVICE_PERMISSION) {
            device_permission_handle = result.handle;
            return fetch_device_mac() && render() ? PXA_EVENT_HANDLED :
                                               PXA_STATUS_INTERNAL;
        }
        network_permission_handle = result.handle;
        return fetch_weather() && render() ? PXA_EVENT_HANDLED :
                                             PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_DEVICE &&
        parsed.opcode == PXA_DEVICE_GET_MAC &&
        parsed.request_id == REQUEST_DEVICE_MAC) {
        pxa_device_mac_result_t result;
        if (!pxa_device_parse_mac(&parsed, &result) ||
            result.status != PXA_STATUS_OK ||
            result.kind != PXA_DEVICE_MAC_KIND_WIFI_STATION_HARDWARE ||
            !set_weather_mac(result.mac)) {
            state = WEATHER_ERROR;
            (void)schedule_auto_refresh();
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        has_mac = 1;
        return acquire_network_permission() && render() ? PXA_EVENT_HANDLED :
                                                          PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_NET &&
        parsed.opcode == PXA_NET_HTTP_REQUEST &&
        parsed.request_id == REQUEST_WEATHER) {
        pxa_net_http_result_t result;
        if (!pxa_net_parse_http_result(&parsed, &result)) {
            state = WEATHER_ERROR;
            (void)schedule_auto_refresh();
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        if (result.status != PXA_STATUS_OK) {
            state = WEATHER_ERROR;
            (void)schedule_auto_refresh();
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        http_status = result.status_code;
        response_flags = (uint8_t)result.flags;
        response_length = result.body_length;
        body_handle = result.body_handle;
        if (http_status != 200 || body_handle == 0) {
            if (body_handle != 0 && !close_handle(body_handle))
                return PXA_STATUS_INTERNAL;
            body_handle = 0;
            state = WEATHER_ERROR;
            (void)schedule_auto_refresh();
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return consume_response() && render() ? PXA_EVENT_HANDLED :
                                                PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK) {
        if (stream_waiting && body_handle != 0) {
            stream_waiting = 0;
            return consume_response() && render() ? PXA_EVENT_HANDLED :
                                                    PXA_STATUS_INTERNAL;
        }
        if (state != WEATHER_LOADING && ++auto_refresh_ticks >= AUTO_REFRESH_TICKS) {
            int started = start_weather_request();
            return started && render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
    if (body_handle != 0) (void)close_handle(body_handle);
    body_handle = 0;
}
