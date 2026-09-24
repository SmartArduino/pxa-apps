#include "pxa_app_messages.h"
#include "pxa_i18n.h"
#include "pxa_net.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include "pxa_ui.h"
#include "weather_providers.h"

#define REQUEST_NETWORK_PERMISSION UINT32_C(1)
#define REQUEST_WEATHER UINT32_C(2)
#define REQUEST_LOCATION_GET UINT32_C(3)
#define REQUEST_LOCATION_SET UINT32_C(4)
#define REQUEST_LOCATION_REMOVE UINT32_C(5)
#define REQUEST_WINDOW_SNAPSHOT UINT32_C(6)
#define NODE_REFRESH UINT32_C(18)
#define NODE_LOCATION_PICKER UINT32_C(6)
#define NODE_CITY_INPUT UINT32_C(35)
#define NODE_CITY_SEARCH UINT32_C(36)
#define NODE_LOCATION_AUTO UINT32_C(37)
#define NODE_CITY_RESULT_BASE UINT32_C(41)
#define LOCATION_STORAGE_KEY "location.v1"
#define AUTO_REFRESH_TICKS UINT16_C(600)

#define WEATHER_IDLE UINT8_C(0)
#define WEATHER_LOADING UINT8_C(1)
#define WEATHER_READY UINT8_C(2)
#define WEATHER_DENIED UINT8_C(3)
#define WEATHER_ERROR UINT8_C(4)

static uint8_t packet[3072];
static uint8_t request_payload[1024];
static uint8_t response_body[4096];
static char weather_url[512];
static uint32_t generation;
static uint32_t network_permission_handles[WEATHER_PROVIDER_COUNT];
static uint32_t body_handle;
static uint32_t response_size;
static uint16_t http_status;
static uint64_t response_length;
static uint8_t response_flags;
static uint8_t state;
static uint8_t stream_waiting;
static weather_provider_t provider;
static weather_provider_t active_weather_provider;
static uint8_t permission_denied;
static uint8_t has_location;
static uint8_t has_weather;
static uint8_t manual_location;
static uint8_t show_city_picker;
static uint8_t search_failed;
static uint8_t location_storage_error;
static uint8_t candidate_count;
static uint16_t auto_refresh_ticks;
static weather_location_t location;
static weather_city_result_t candidates[WEATHER_CITY_RESULTS];
static char search_draft[65];
static char search_query[65];
static int32_t temperature;
static int32_t apparent_temperature;
static int32_t humidity = -1;
static int32_t wind_speed = -1;
static int32_t air_aqi = -1;
static int32_t air_pm25_tenths = -1;
static int32_t high_temperature;
static int32_t low_temperature;
static uint8_t has_apparent;
static uint8_t has_range;
static uint8_t is_day = 1;
static int32_t weather_code = -1;
static char city[64];
static char last_update[64];
static weather_forecast_t forecast;
static weather_day_t history_days[WEATHER_DAILY_COUNT];
static size_t history_count;
static pxa_i18n_t i18n;
static uint32_t display_width;
static uint32_t display_height;
static uint32_t display_shape;
static uint32_t safe_insets[4];
static uint32_t bar_insets[4];
static uint32_t corner_radii[4];

typedef struct {
    uint16_t header_height;
    uint16_t header_icon;
    uint16_t hero_height;
    uint16_t hero_icon;
    uint16_t hour_width;
    uint16_t hour_height;
    uint16_t row_height;
    uint16_t row_icon;
    uint16_t row_date_width;
    uint16_t search_height;
    uint16_t margin;
    uint16_t gap;
    uint16_t radius;
} weather_metrics_t;

static const weather_metrics_t compact_metrics = {
    42, 25, 112, 68, 68, 105, 39, 24, 44, 35, 12, 8, 8};
static const weather_metrics_t regular_metrics = {
    54, 30, 145, 86, 82, 125, 47, 28, 52, 42, 16, 10, 12};
static const weather_metrics_t large_metrics = {
    64, 36, 172, 104, 94, 140, 55, 34, 58, 48, 20, 12, 16};
static const weather_metrics_t circle_metrics = {
    40, 20, 96, 56, 58, 98, 39, 20, 52, 35, 8, 6, 12};

static const weather_metrics_t *metrics(void) {
    if (display_shape == PXA_UI_DISPLAY_SHAPE_CIRCLE)
        return &circle_metrics;
    if (display_width >= 480u) return &large_metrics;
    if (display_width >= 340u) return &regular_metrics;
    return &compact_metrics;
}

static uint16_t effective_inset(uint8_t edge) {
    uint32_t value = safe_insets[edge] > bar_insets[edge] ?
                     safe_insets[edge] : bar_insets[edge];
    uint32_t shape = 0;
    if (display_shape == PXA_UI_DISPLAY_SHAPE_CIRCLE) {
        uint32_t diameter = display_width < display_height ?
                            display_width : display_height;
        shape = diameter / 6u;
        shape += (edge == 1u || edge == 3u ?
                  display_width - diameter : display_height - diameter) / 2u;
    } else if (display_shape == PXA_UI_DISPLAY_SHAPE_ROUNDED_RECTANGLE) {
        uint32_t first = corner_radii[edge];
        uint32_t second = corner_radii[(edge + 1u) % 4u];
        if (edge == 1u) { first = corner_radii[1]; second = corner_radii[2]; }
        if (edge == 2u) { first = corner_radii[2]; second = corner_radii[3]; }
        if (edge == 3u) { first = corner_radii[3]; second = corner_radii[0]; }
        shape = (first > second ? first : second) / 3u;
    }
    if (shape > value) value = shape;
    return (uint16_t)(value > 512u ? 512u : value);
}

static uint16_t inset_padding(uint8_t edge, uint16_t margin) {
    uint16_t inset = effective_inset(edge);
    return inset > margin ? (uint16_t)(inset - margin) : 0;
}

static int configure_window(void) {
    uint8_t records[15];
    pxa_writer_t writer;
    const uint8_t edge_to_edge = 0;
    const uint8_t visible = PXA_WINDOW_BAR_VISIBLE;
    pxa_writer_init(&writer, records, sizeof(records));
    return pxa_record(&writer, PXA_WINDOW_EDGE_TO_EDGE, &edge_to_edge, 1) &&
           pxa_record(&writer, PXA_WINDOW_STATUS_BAR_MODE, &visible, 1) &&
           pxa_record(&writer, PXA_WINDOW_NAVIGATION_BAR_MODE, &visible, 1) &&
           pxa_send(PXA_SERVICE_WINDOW, PXA_WINDOW_CONFIGURE, 0,
                    writer.data, writer.length);
}

static void apply_environment(const pxa_ui_environment_t *environment) {
    display_width = environment->width;
    display_height = environment->height;
    display_shape = environment->display_shape;
    for (size_t index = 0; index < 4u; ++index) {
        safe_insets[index] = environment->safe_insets[index];
        corner_radii[index] = environment->corner_radii[index];
    }
}

static int apply_window_snapshot(const pxa_event_t *event, int with_status) {
    pxa_window_insets_view_t view;
    if (!pxa_window_parse_snapshot(event->payload, event->payload_length,
                                   with_status, &view)) return 0;
    for (size_t index = 0; index < 4u; ++index) {
        if (view.has_safe_insets) safe_insets[index] = view.safe_insets[index];
        if (view.has_bar_insets) bar_insets[index] = view.bar_insets[index];
    }
    return 1;
}

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

static void format_i32(char *output, size_t capacity, int32_t value) {
    size_t offset = 0;
    uint32_t magnitude = value < 0 ? (uint32_t)(-(value + 1)) + 1u :
                                     (uint32_t)value;
    if (value < 0) offset = append_text(output, capacity, offset, "-");
    (void)append_u32(output, capacity, offset, magnitude);
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

static const char *icon_for_code(int32_t code, uint8_t day) {
    if (code >= 95) return "assets/storm.png";
    if (code >= 71 && code <= 77) return "assets/snow.png";
    if (code >= 85 && code <= 86) return "assets/snow.png";
    if (code >= 51 && code <= 67) return "assets/rain.png";
    if (code >= 80 && code <= 82) return "assets/rain.png";
    if (code >= 45 && code <= 48) return "assets/fog.png";
    if (code == 0) return day ? "assets/sun.png" : "assets/moon.png";
    if (code <= 2) return day ? "assets/partly-cloudy.png" :
                               "assets/moon.png";
    return "assets/cloud.png";
}

static const char *weather_icon(void) {
    return has_weather ? icon_for_code(weather_code, is_day) :
                         "assets/cloud.png";
}

static const char *weather_condition(void) {
    if (!has_weather) return message(PXA_MSG_WEATHER_WAITING);
    if (weather_code >= 95) return message(PXA_MSG_WEATHER_THUNDERSTORM);
    if ((weather_code >= 71 && weather_code <= 77) ||
        (weather_code >= 85 && weather_code <= 86))
        return message(PXA_MSG_WEATHER_SNOW);
    if ((weather_code >= 51 && weather_code <= 67) ||
        (weather_code >= 80 && weather_code <= 82))
        return message(PXA_MSG_WEATHER_RAIN);
    if (weather_code >= 45 && weather_code <= 48)
        return message(PXA_MSG_WEATHER_FOG);
    if (weather_code == 0) return message(PXA_MSG_WEATHER_CLEAR);
    if (weather_code <= 2) return message(PXA_MSG_WEATHER_PARTLY_CLOUDY);
    return message(PXA_MSG_WEATHER_CLOUDY);
}

static const char *weather_summary(void) {
    if (!has_weather) return message(PXA_MSG_SUMMARY_LOCATING);
    if (weather_code >= 95)
        return message(PXA_MSG_SUMMARY_THUNDERSTORM);
    if ((weather_code >= 51 && weather_code <= 67) ||
        (weather_code >= 80 && weather_code <= 82))
        return message(PXA_MSG_SUMMARY_RAIN);
    if ((weather_code >= 71 && weather_code <= 77) ||
        (weather_code >= 85 && weather_code <= 86))
        return message(PXA_MSG_SUMMARY_SNOW);
    if (weather_code >= 45 && weather_code <= 48)
        return message(PXA_MSG_SUMMARY_LOW_VISIBILITY);
    if (wind_speed >= 35)
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
    int32_t feels = has_apparent ? apparent_temperature : temperature;
    if (feels >= 35) return message(PXA_MSG_FEEL_HOT);
    if (feels >= 28) return message(PXA_MSG_FEEL_WARM);
    if (feels >= 22) return message(PXA_MSG_FEEL_COMFORTABLE);
    if (feels >= 15) return message(PXA_MSG_FEEL_COOL);
    if (feels >= 5) return message(PXA_MSG_FEEL_COLD);
    return message(PXA_MSG_FEEL_VERY_COLD);
}

static const char *day_phase(void) {
    if (!has_weather) return message(PXA_MSG_SCREEN_PHASE_CONDITIONS);
    return !is_day
               ? message(PXA_MSG_SCREEN_PHASE_NIGHT)
               : message(PXA_MSG_SCREEN_PHASE_DAY);
}

static uint8_t weather_accent(void) {
    if (!has_weather) return PXA_UI_THEME_MUTED;
    if (weather_code >= 45) return PXA_UI_THEME_PRIMARY;
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
        copy_text(output, capacity, message(has_weather &&
                  active_weather_provider == WEATHER_WTTR ?
                  PXA_MSG_UPDATE_WTTR : PXA_MSG_UPDATE_PROVIDER));
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

static int create_icon(pxa_ui_transaction_t *transaction, uint32_t node,
                       uint32_t parent, const char *path, int32_t size) {
    return pxa_ui_create(transaction, node, parent, 0, PXA_UI_NODE_IMAGE) &&
           pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_PX, size) &&
           pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_PX, size) &&
           pxa_ui_set_u8(transaction, node, PXA_UI_PROPERTY_IMAGE_FIT,
                         PXA_UI_IMAGE_FIT_CONTAIN) &&
           pxa_ui_set_property(transaction, node, PXA_UI_PROPERTY_ASSET,
                               path, string_length(path));
}

static int render_city_candidates(pxa_ui_transaction_t *transaction) {
    const weather_metrics_t *layout = metrics();
    for (uint8_t index = 0; index < candidate_count; ++index) {
        uint32_t node = NODE_CITY_RESULT_BASE + index;
        uint32_t label = 50u + index;
        if (!pxa_ui_create_typed(transaction, node, 5, 0,
                                 PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) ||
            !pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) ||
            !pxa_ui_set_length(transaction, node, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, layout->search_height) ||
            !pxa_ui_set_padding(transaction, node, 8, 5, 8, 5) ||
            !pxa_ui_set_dp(transaction, node, PXA_UI_PROPERTY_RADIUS,
                           layout->radius) ||
            !pxa_ui_set_theme_color(transaction, node,
                                   PXA_UI_PROPERTY_BACKGROUND,
                                   PXA_UI_THEME_SURFACE) ||
            !pxa_ui_set_event_mask(transaction, node, PXA_UI_EVENT_MASK_CLICK) ||
            !pxa_ui_create(transaction, label, node, 0, PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_text(transaction, label, candidates[index].label,
                             string_length(candidates[index].label)) ||
            !pxa_ui_set_font_role(transaction, label, PXA_UI_FONT_ROLE_CAPTION))
            return 0;
    }
    return 1;
}

static void format_metrics(char *output, size_t capacity) {
    char feels[16];
    char moisture[16];
    char wind[16];
    pxa_i18n_argument_t arguments[3] = {0};
    if (has_weather) {
        format_i32(feels, sizeof(feels),
                   has_apparent ? apparent_temperature : temperature);
    } else copy_text(feels, sizeof(feels), "–");
    if (humidity >= 0 && has_weather)
        format_i32(moisture, sizeof(moisture), humidity);
    else copy_text(moisture, sizeof(moisture), "–");
    if (wind_speed >= 0 && has_weather)
        format_i32(wind, sizeof(wind), wind_speed);
    else copy_text(wind, sizeof(wind), "–");
    arguments[0].name = "feels";
    arguments[0].name_size = 5;
    arguments[0].value.string.data = feels;
    arguments[0].value.string.size = string_length(feels);
    arguments[1].name = "humidity";
    arguments[1].name_size = 8;
    arguments[1].value.string.data = moisture;
    arguments[1].value.string.size = string_length(moisture);
    arguments[2].name = "wind";
    arguments[2].name_size = 4;
    arguments[2].value.string.data = wind;
    arguments[2].value.string.size = string_length(wind);
    for (size_t index = 0; index < 3; ++index)
        arguments[index].type = PXA_I18N_ARGUMENT_STRING;
    (void)pxa_i18n_format(&i18n, PXA_MSG_DETAIL_METRICS, arguments, 3,
                          output, capacity);
}

static void format_range(char *output, size_t capacity) {
    char low[16];
    char high[16];
    pxa_i18n_argument_t arguments[2] = {0};
    if (!has_weather || !has_range) { output[0] = '\0'; return; }
    format_i32(low, sizeof(low), low_temperature);
    format_i32(high, sizeof(high), high_temperature);
    arguments[0].name = "low";
    arguments[0].name_size = 3;
    arguments[0].value.string.data = low;
    arguments[0].value.string.size = string_length(low);
    arguments[1].name = "high";
    arguments[1].name_size = 4;
    arguments[1].value.string.data = high;
    arguments[1].value.string.size = string_length(high);
    arguments[0].type = arguments[1].type = PXA_I18N_ARGUMENT_STRING;
    (void)pxa_i18n_format(&i18n, PXA_MSG_DETAIL_RANGE, arguments, 2,
                          output, capacity);
}

static void format_air(char *output, size_t capacity) {
    char aqi[16];
    char pm25[20];
    pxa_i18n_argument_t arguments[2] = {0};
    size_t offset;
    if (air_aqi < 0 || air_pm25_tenths < 0) {
        output[0] = '\0';
        return;
    }
    format_i32(aqi, sizeof(aqi), air_aqi);
    format_i32(pm25, sizeof(pm25), air_pm25_tenths / 10);
    offset = string_length(pm25);
    offset = append_text(pm25, sizeof(pm25), offset, ".");
    (void)append_u32(pm25, sizeof(pm25), offset,
                     (uint32_t)(air_pm25_tenths % 10));
    arguments[0].name = "aqi";
    arguments[0].name_size = 3;
    arguments[0].value.string.data = aqi;
    arguments[0].value.string.size = string_length(aqi);
    arguments[1].name = "pm25";
    arguments[1].name_size = 4;
    arguments[1].value.string.data = pm25;
    arguments[1].value.string.size = string_length(pm25);
    arguments[0].type = arguments[1].type = PXA_I18N_ARGUMENT_STRING;
    (void)pxa_i18n_format(&i18n, PXA_MSG_DETAIL_AIR, arguments, 2,
                          output, capacity);
}

static int section_heading(pxa_ui_transaction_t *transaction, uint32_t node,
                           pxa_i18n_message_id_t title) {
    return pxa_ui_create(transaction, node, 5, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, node, message(title),
                           pxa_i18n_size(&i18n, title)) &&
           pxa_ui_set_font_role(transaction, node, PXA_UI_FONT_ROLE_TITLE) &&
           pxa_ui_set_theme_color(transaction, node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_TEXT);
}

static int section_empty(pxa_ui_transaction_t *transaction, uint32_t node,
                         pxa_i18n_message_id_t title) {
    return pxa_ui_create(transaction, node, 5, 0, PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(transaction, node, message(title),
                           pxa_i18n_size(&i18n, title)) &&
           pxa_ui_set_font_role(transaction, node, PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(transaction, node,
                                  PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED);
}

static int render_hourly(pxa_ui_transaction_t *transaction) {
    const weather_metrics_t *layout = metrics();
    if (!section_heading(transaction, 100, PXA_MSG_FORECAST_HOURLY)) return 0;
    if (forecast.hour_count == 0)
        return section_empty(transaction, 102, PXA_MSG_FORECAST_UNAVAILABLE);
    if (!pxa_ui_create(transaction, 101, 5, 0, PXA_UI_NODE_SCROLL) ||
        !pxa_ui_set_length(transaction, 101, PXA_UI_PROPERTY_WIDTH,
                            PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(transaction, 101, PXA_UI_PROPERTY_HEIGHT,
                            PXA_UI_LENGTH_PX, layout->hour_height) ||
        !pxa_ui_set_u8(transaction, 101, PXA_UI_PROPERTY_LAYOUT,
                        PXA_UI_LAYOUT_ROW) ||
        !pxa_ui_set_u8(transaction, 101, PXA_UI_PROPERTY_SCROLL_AXIS, 1) ||
        !pxa_ui_set_event_mask(transaction, 101, PXA_UI_EVENT_MASK_SCROLL) ||
        !pxa_ui_set_dp(transaction, 101, PXA_UI_PROPERTY_GAP,
                       layout->gap)) return 0;
    for (size_t index = 0; index < forecast.hour_count; ++index) {
        uint32_t tile = 110u + (uint32_t)index * 4u;
        int hour = (forecast.hours[index].time[11] - '0') * 10 +
                   forecast.hours[index].time[12] - '0';
        char temperature_text[20];
        char rain_text[20];
        size_t offset;
        format_i32(temperature_text, sizeof(temperature_text),
                   forecast.hours[index].temperature);
        offset = string_length(temperature_text);
        (void)append_text(temperature_text, sizeof(temperature_text),
                          offset, "°");
        rain_text[0] = '\0';
        offset = append_u32(rain_text, sizeof(rain_text), 0,
                            (uint32_t)forecast.hours[index].rain_chance);
        (void)append_text(rain_text, sizeof(rain_text), offset, "%");
        if (!pxa_ui_create(transaction, tile, 101, 0, PXA_UI_NODE_BOX) ||
            !pxa_ui_set_length(transaction, tile, PXA_UI_PROPERTY_WIDTH,
                                PXA_UI_LENGTH_PX, layout->hour_width) ||
            !pxa_ui_set_length(transaction, tile, PXA_UI_PROPERTY_HEIGHT,
                                PXA_UI_LENGTH_PX,
                                layout->hour_height - 7) ||
            !pxa_ui_set_u8(transaction, tile, PXA_UI_PROPERTY_LAYOUT,
                            PXA_UI_LAYOUT_COLUMN) ||
            !pxa_ui_set_u8(transaction, tile, PXA_UI_PROPERTY_ALIGN,
                            PXA_UI_ALIGN_CENTER) ||
            !pxa_ui_set_padding(transaction, tile, 3, 5, 3, 5) ||
            !pxa_ui_set_dp(transaction, tile, PXA_UI_PROPERTY_RADIUS,
                            layout->radius) ||
            !pxa_ui_set_theme_color(transaction, tile,
                                    PXA_UI_PROPERTY_BACKGROUND,
                                    PXA_UI_THEME_SURFACE) ||
            !pxa_ui_create(transaction, tile + 1u, tile, 0,
                            PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_text(transaction, tile + 1u,
                             forecast.hours[index].time + 11u, 5) ||
            !pxa_ui_set_font_role(transaction, tile + 1u,
                                  PXA_UI_FONT_ROLE_CAPTION) ||
            !create_icon(transaction, tile + 2u, tile,
                         icon_for_code(forecast.hours[index].code,
                                       (uint8_t)(index == 0 ? is_day :
                                                  hour >= 6 && hour < 18)),
                         layout->row_icon + 4) ||
            !pxa_ui_create(transaction, tile + 3u, tile, 0,
                            PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_text(transaction, tile + 3u, temperature_text,
                             string_length(temperature_text)) ||
            !pxa_ui_create(transaction, 150u + (uint32_t)index, tile, 0,
                            PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_text(transaction, 150u + (uint32_t)index, rain_text,
                             string_length(rain_text)) ||
            !pxa_ui_set_font_role(transaction, 150u + (uint32_t)index,
                                  PXA_UI_FONT_ROLE_CAPTION) ||
            !pxa_ui_set_theme_color(transaction, 150u + (uint32_t)index,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_PRIMARY)) return 0;
    }
    return 1;
}

static int render_daily_rows(pxa_ui_transaction_t *transaction,
                             const weather_day_t *days, size_t count,
                             uint32_t base, uint8_t historical) {
    const weather_metrics_t *layout = metrics();
    for (size_t index = 0; index < count; ++index) {
        uint32_t row = base + (uint32_t)index * 5u;
        char range[32];
        char rain[24];
        size_t offset;
        format_i32(range, sizeof(range), days[index].low);
        offset = string_length(range);
        offset = append_text(range, sizeof(range), offset, "° / ");
        format_i32(range + offset, sizeof(range) - offset, days[index].high);
        offset = string_length(range);
        (void)append_text(range, sizeof(range), offset, "°");
        if (historical) {
            format_i32(rain, sizeof(rain), days[index].precipitation / 10);
            offset = string_length(rain);
            offset = append_text(rain, sizeof(rain), offset, ".");
            offset = append_u32(rain, sizeof(rain), offset,
                                (uint32_t)(days[index].precipitation % 10));
            (void)append_text(rain, sizeof(rain), offset, "mm");
        } else {
            rain[0] = '\0';
            offset = append_u32(rain, sizeof(rain), 0,
                                (uint32_t)days[index].precipitation);
            (void)append_text(rain, sizeof(rain), offset, "%");
        }
        if (!pxa_ui_create(transaction, row, 5, 0, PXA_UI_NODE_BOX) ||
            !pxa_ui_set_length(transaction, row, PXA_UI_PROPERTY_WIDTH,
                                PXA_UI_LENGTH_FILL, 0) ||
            !pxa_ui_set_length(transaction, row, PXA_UI_PROPERTY_HEIGHT,
                                PXA_UI_LENGTH_PX, layout->row_height) ||
            !pxa_ui_set_u8(transaction, row, PXA_UI_PROPERTY_LAYOUT,
                            PXA_UI_LAYOUT_ROW) ||
            !pxa_ui_set_u8(transaction, row, PXA_UI_PROPERTY_ALIGN,
                            PXA_UI_ALIGN_CENTER) ||
            !pxa_ui_set_padding(transaction, row, 7, 5, 7, 5) ||
            !pxa_ui_set_dp(transaction, row, PXA_UI_PROPERTY_GAP, 5) ||
            !pxa_ui_set_dp(transaction, row, PXA_UI_PROPERTY_RADIUS,
                            layout->radius) ||
            !pxa_ui_set_theme_color(transaction, row,
                                    PXA_UI_PROPERTY_BACKGROUND,
                                    PXA_UI_THEME_SURFACE) ||
            !pxa_ui_create(transaction, row + 1u, row, 0,
                            PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_length(transaction, row + 1u, PXA_UI_PROPERTY_WIDTH,
                                PXA_UI_LENGTH_PX, layout->row_date_width) ||
            !pxa_ui_set_text(transaction, row + 1u, days[index].date + 5u,
                             5) ||
            !create_icon(transaction, row + 2u, row,
                         icon_for_code(days[index].code, 1),
                         layout->row_icon) ||
            !pxa_ui_create(transaction, row + 3u, row, 0,
                            PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_u16(transaction, row + 3u, PXA_UI_PROPERTY_GROW, 1) ||
            !pxa_ui_set_text(transaction, row + 3u, range,
                             string_length(range)) ||
            !pxa_ui_create(transaction, row + 4u, row, 0,
                            PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_text(transaction, row + 4u, rain,
                             string_length(rain)) ||
            !pxa_ui_set_font_role(transaction, row + 4u,
                                  PXA_UI_FONT_ROLE_CAPTION) ||
            !pxa_ui_set_theme_color(transaction, row + 4u,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_PRIMARY)) return 0;
    }
    return 1;
}

static int render_outlook(pxa_ui_transaction_t *transaction) {
    char details[100];
    size_t offset;
    if (!has_weather) return 1;
    if (!render_hourly(transaction) ||
        !section_heading(transaction, 300, PXA_MSG_FORECAST_DAILY)) return 0;
    if (forecast.day_count == 0) {
        if (!section_empty(transaction, 301, PXA_MSG_FORECAST_UNAVAILABLE))
            return 0;
    } else if (!render_daily_rows(transaction, forecast.days,
                                  forecast.day_count, 310, 0)) return 0;
    if (forecast.day_count != 0) {
        details[0] = '\0';
        offset = append_text(details, sizeof(details), 0, "UV ");
        if (forecast.uv_max >= 0)
            offset = append_u32(details, sizeof(details), offset,
                                (uint32_t)forecast.uv_max);
        else offset = append_text(details, sizeof(details), offset, "–");
        if (forecast.sunrise[0] != '\0' && forecast.sunset[0] != '\0') {
            offset = append_text(details, sizeof(details), offset, " · ");
            offset = append_text(details, sizeof(details), offset,
                                 message(PXA_MSG_FORECAST_SUNRISE));
            offset = append_text(details, sizeof(details), offset, " ");
            offset = append_text(details, sizeof(details), offset,
                                 forecast.sunrise + 11u);
            offset = append_text(details, sizeof(details), offset, " · ");
            offset = append_text(details, sizeof(details), offset,
                                 message(PXA_MSG_FORECAST_SUNSET));
            offset = append_text(details, sizeof(details), offset, " ");
            (void)append_text(details, sizeof(details), offset,
                              forecast.sunset + 11u);
        }
        if (!pxa_ui_create(transaction, 302, 5, 0, PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_text(transaction, 302, details,
                             string_length(details)) ||
            !pxa_ui_set_font_role(transaction, 302,
                                  PXA_UI_FONT_ROLE_CAPTION)) return 0;
    }
    if (!section_heading(transaction, 500, PXA_MSG_HISTORY_TITLE)) return 0;
    if (history_count == 0)
        return section_empty(transaction, 501, PXA_MSG_HISTORY_UNAVAILABLE);
    return render_daily_rows(transaction, history_days, history_count, 510, 1);
}

static int render_city_controls(pxa_ui_transaction_t *transaction) {
    const weather_metrics_t *layout = metrics();
    if (!pxa_ui_create(transaction, 40, 5, 0, PXA_UI_NODE_TEXT) ||
        !pxa_ui_set_text(transaction, 40,
                         message(PXA_MSG_LOCATION_SEARCH_LABEL),
                         pxa_i18n_size(&i18n,
                                       PXA_MSG_LOCATION_SEARCH_LABEL)) ||
        !pxa_ui_set_font_role(transaction, 40, PXA_UI_FONT_ROLE_TITLE) ||
        !pxa_ui_create(transaction, 39, 5, 0, PXA_UI_NODE_BOX) ||
        !pxa_ui_set_length(transaction, 39, PXA_UI_PROPERTY_WIDTH,
                            PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_u8(transaction, 39, PXA_UI_PROPERTY_LAYOUT,
                        PXA_UI_LAYOUT_ROW) ||
        !pxa_ui_set_dp(transaction, 39, PXA_UI_PROPERTY_GAP, 6) ||
        !pxa_ui_create_typed(transaction, NODE_CITY_INPUT, 39, 0,
                             PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_TEXT_INPUT) ||
        !pxa_ui_set_u16(transaction, NODE_CITY_INPUT, PXA_UI_PROPERTY_GROW,
                         1) ||
        !pxa_ui_set_length(transaction, NODE_CITY_INPUT,
                            PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                            layout->search_height) ||
        !pxa_ui_set_padding(transaction, NODE_CITY_INPUT, 8, 5, 8, 5) ||
        !pxa_ui_set_dp(transaction, NODE_CITY_INPUT,
                        PXA_UI_PROPERTY_RADIUS, layout->radius) ||
        !pxa_ui_set_theme_color(transaction, NODE_CITY_INPUT,
                                 PXA_UI_PROPERTY_BACKGROUND,
                                 PXA_UI_THEME_SURFACE) ||
        !pxa_ui_set_event_mask(transaction, NODE_CITY_INPUT,
                                PXA_UI_EVENT_MASK_TEXT |
                                PXA_UI_EVENT_MASK_CLICK) ||
        !pxa_ui_set_text(transaction, NODE_CITY_INPUT, search_draft,
                          string_length(search_draft)) ||
        !pxa_ui_create_typed(transaction, NODE_CITY_SEARCH, 39, 0,
                             PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) ||
        !pxa_ui_set_length(transaction, NODE_CITY_SEARCH,
                            PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 54) ||
        !pxa_ui_set_length(transaction, NODE_CITY_SEARCH,
                            PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                            layout->search_height) ||
        !pxa_ui_set_dp(transaction, NODE_CITY_SEARCH,
                        PXA_UI_PROPERTY_RADIUS, layout->radius) ||
        !pxa_ui_set_theme_color(transaction, NODE_CITY_SEARCH,
                                 PXA_UI_PROPERTY_BACKGROUND,
                                 PXA_UI_THEME_PRIMARY) ||
        !pxa_ui_set_u8(transaction, NODE_CITY_SEARCH,
                        PXA_UI_PROPERTY_ENABLED,
                        (uint8_t)(state != WEATHER_LOADING)) ||
        !pxa_ui_set_event_mask(transaction, NODE_CITY_SEARCH,
                                PXA_UI_EVENT_MASK_CLICK) ||
        !pxa_ui_create(transaction, 46, NODE_CITY_SEARCH, 0,
                        PXA_UI_NODE_TEXT) ||
        !pxa_ui_set_text(transaction, 46, message(PXA_MSG_LOCATION_SEARCH),
                          pxa_i18n_size(&i18n, PXA_MSG_LOCATION_SEARCH)) ||
        !pxa_ui_set_theme_color(transaction, 46, PXA_UI_PROPERTY_FOREGROUND,
                                 PXA_UI_THEME_ON_PRIMARY)) return 0;
    if (manual_location &&
        (!pxa_ui_create_typed(transaction, NODE_LOCATION_AUTO, 5, 0,
                              PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) ||
         !pxa_ui_set_event_mask(transaction, NODE_LOCATION_AUTO,
                                 PXA_UI_EVENT_MASK_CLICK) ||
         !pxa_ui_create(transaction, 47, NODE_LOCATION_AUTO, 0,
                         PXA_UI_NODE_TEXT) ||
         !pxa_ui_set_text(transaction, 47, message(PXA_MSG_LOCATION_AUTO),
                           pxa_i18n_size(&i18n, PXA_MSG_LOCATION_AUTO))))
        return 0;
    if (search_failed) {
        pxa_i18n_message_id_t error = search_failed == 2 ?
            PXA_MSG_LOCATION_SEARCH_TOO_SHORT : PXA_MSG_LOCATION_NO_RESULTS;
        if (!pxa_ui_create(transaction, 49, 5, 0, PXA_UI_NODE_TEXT) ||
            !pxa_ui_set_text(transaction, 49, message(error),
                              pxa_i18n_size(&i18n, error))) return 0;
    }
    if (location_storage_error &&
        (!pxa_ui_create(transaction, 48, 5, 0, PXA_UI_NODE_TEXT) ||
         !pxa_ui_set_text(transaction, 48,
                           message(PXA_MSG_LOCATION_SAVE_FAILED),
                           pxa_i18n_size(&i18n,
                                         PXA_MSG_LOCATION_SAVE_FAILED))))
        return 0;
    return render_city_candidates(transaction);
}

static int schedule_auto_refresh(void) {
    auto_refresh_ticks = 0;
    return pxa_clock_set_period(1000);
}

static int render(void) {
    const weather_metrics_t *layout = metrics();
    char temperature_text[24];
    char update_text[48];
    char metrics_text[160];
    char range_text[80];
    char air_text[100];
    const char *display_city = has_location ? city :
        message(PXA_MSG_LOCATION_LOCATING);
    const char *display_weather = weather_condition();
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
    format_metrics(metrics_text, sizeof(metrics_text));
    format_range(range_text, sizeof(range_text));
    format_air(air_text, sizeof(air_text));
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
                           PXA_UI_LENGTH_PX,
                           layout->header_height + inset_padding(0, 4)) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 2,
                            layout->margin + inset_padding(3, layout->margin),
                            4 + inset_padding(0, 4),
                            layout->margin + inset_padding(1, layout->margin),
                            4) &&
         pxa_ui_set_dp(&transaction, 2, PXA_UI_PROPERTY_GAP, 6) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         create_icon(&transaction, 3, 2, "assets/partly-cloudy.png",
                     layout->header_icon) &&
         pxa_ui_create(&transaction, 4, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 4, message(PXA_MSG_SCREEN_TITLE),
                         pxa_i18n_size(&i18n, PXA_MSG_SCREEN_TITLE)) &&
         pxa_ui_set_font_role(&transaction, 4, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 4, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 21, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u16(&transaction, 21, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_text(&transaction, 21, day_phase(), string_length(day_phase())) &&
         pxa_ui_set_font_role(&transaction, 21, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_u8(&transaction, 21, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_END) &&
         pxa_ui_set_theme_color(&transaction, 21, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
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
         pxa_ui_set_padding(&transaction, 5,
                            layout->margin + inset_padding(3, layout->margin),
                            layout->margin,
                            layout->margin + inset_padding(1, layout->margin),
                            layout->margin + inset_padding(2, layout->margin)) &&
         pxa_ui_set_dp(&transaction, 5, PXA_UI_PROPERTY_GAP,
                        layout->gap) &&
         pxa_ui_create(&transaction, NODE_LOCATION_PICKER, 5, 0,
                       PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, NODE_LOCATION_PICKER,
                           PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u8(&transaction, NODE_LOCATION_PICKER, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, NODE_LOCATION_PICKER, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, NODE_LOCATION_PICKER,
                       PXA_UI_PROPERTY_GAP, 6) &&
         pxa_ui_set_event_mask(&transaction, NODE_LOCATION_PICKER,
                               PXA_UI_EVENT_MASK_CLICK) &&
         create_icon(&transaction, 7, NODE_LOCATION_PICKER,
                     "assets/location.png", 19) &&
         pxa_ui_create(&transaction, 8, NODE_LOCATION_PICKER, 0,
                       PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u16(&transaction, 8, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_text(&transaction, 8, display_city,
                         string_length(display_city)) &&
         pxa_ui_set_font_role(&transaction, 8, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 8, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 38, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 38,
                         message(manual_location ? PXA_MSG_LOCATION_SELECTED :
                                                  PXA_MSG_LOCATION_IP_ESTIMATE),
                         pxa_i18n_size(&i18n, manual_location ?
                                        PXA_MSG_LOCATION_SELECTED :
                                        PXA_MSG_LOCATION_IP_ESTIMATE)) &&
         pxa_ui_set_font_role(&transaction, 38, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 38, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         (!show_city_picker || render_city_controls(&transaction)) &&
         pxa_ui_create(&transaction, 9, 5, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 9, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 9, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, layout->hero_height) &&
         pxa_ui_set_u8(&transaction, 9, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 9, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 9, 12, 10, 12, 10) &&
         pxa_ui_set_dp(&transaction, 9, PXA_UI_PROPERTY_GAP, 12) &&
         pxa_ui_set_dp(&transaction, 9, PXA_UI_PROPERTY_RADIUS,
                       layout->radius) &&
         pxa_ui_set_dp(&transaction, 9, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
         pxa_ui_set_theme_color(&transaction, 9, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE) &&
         pxa_ui_set_theme_color(&transaction, 9, PXA_UI_PROPERTY_BORDER_COLOR,
                                PXA_UI_THEME_BORDER) &&
         create_icon(&transaction, 10, 9, weather_icon(),
                     layout->hero_icon) &&
         pxa_ui_create(&transaction, 11, 9, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_u16(&transaction, 11, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 11, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_dp(&transaction, 11, PXA_UI_PROPERTY_GAP, 3) &&
         pxa_ui_create(&transaction, 12, 11, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 12, temperature_text,
                         string_length(temperature_text)) &&
         pxa_ui_set_font_role(&transaction, 12, PXA_UI_FONT_ROLE_DISPLAY) &&
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
         pxa_ui_set_dp(&transaction, 22, PXA_UI_PROPERTY_RADIUS,
                       layout->radius) &&
         pxa_ui_set_theme_color(&transaction, 22, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE) &&
         create_icon(&transaction, 23, 22, "assets/info.png", 18) &&
         pxa_ui_create(&transaction, 24, 22, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u16(&transaction, 24, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_text(&transaction, 24, summary, string_length(summary)) &&
         pxa_ui_set_font_role(&transaction, 24, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 24, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 32, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 32, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 32, metrics_text,
                         string_length(metrics_text)) &&
         pxa_ui_set_font_role(&transaction, 32, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_theme_color(&transaction, 32, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         (!has_range || (pxa_ui_create(&transaction, 33, 5, 0,
                                       PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(&transaction, 33, range_text,
                           string_length(range_text)) &&
           pxa_ui_set_font_role(&transaction, 33, PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(&transaction, 33, PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED))) &&
         (air_aqi < 0 || (pxa_ui_create(&transaction, 34, 5, 0,
                                       PXA_UI_NODE_TEXT) &&
           pxa_ui_set_text(&transaction, 34, air_text,
                           string_length(air_text)) &&
           pxa_ui_set_font_role(&transaction, 34, PXA_UI_FONT_ROLE_CAPTION) &&
           pxa_ui_set_theme_color(&transaction, 34, PXA_UI_PROPERTY_FOREGROUND,
                                  PXA_UI_THEME_MUTED))) &&
         render_outlook(&transaction) &&
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
         pxa_ui_set_dp(&transaction, 26, PXA_UI_PROPERTY_RADIUS,
                       layout->radius) &&
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
         pxa_ui_set_dp(&transaction, 29, PXA_UI_PROPERTY_RADIUS,
                       layout->radius) &&
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
         create_icon(&transaction, 15, 14, "assets/clock.png", 17) &&
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
                           PXA_UI_LENGTH_PX, layout->search_height) &&
         pxa_ui_set_u8(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_u8(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_dp(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_GAP, 7) &&
         pxa_ui_set_dp(&transaction, NODE_REFRESH, PXA_UI_PROPERTY_RADIUS,
                       layout->radius) &&
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
         create_icon(&transaction, 19, NODE_REFRESH, "assets/refresh.png", 17) &&
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

static int acquire_network_permission(void) {
    static const char permission_name[] = "net.client";
    const char *origin = weather_provider_origin(provider);
    state = WEATHER_LOADING;
    return pxa_permission_acquire(
        REQUEST_NETWORK_PERMISSION, permission_name, sizeof(permission_name) - 1u,
        (const uint8_t *)origin, string_length(origin), request_payload,
        sizeof(request_payload), packet, sizeof(packet));
}

static int fetch_weather(void) {
    static const char accept_name[] = "accept";
    static const uint8_t accept_value[] = "application/json";
    static const pxa_net_header_t json_headers[] = {
        {accept_name, sizeof(accept_name) - 1u, accept_value,
         sizeof(accept_value) - 1u},
    };
    pxa_net_http_request_t request = {0};
    if (!(provider == WEATHER_HISTORY ?
          weather_provider_history_url(&location, last_update, weather_url,
                                       sizeof(weather_url)) :
          provider == WEATHER_GEOCODING ?
          weather_provider_search_url(search_query, weather_url,
                                      sizeof(weather_url)) :
          weather_provider_url(provider, &location, weather_url,
                               sizeof(weather_url)))) return 0;
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
    request.permission_handle = network_permission_handles[provider];
    request.max_response_bytes = sizeof(response_body);
    request.timeout_ms = 12000;
    if (provider != WEATHER_WTTR) {
        request.headers = json_headers;
        request.header_count = 1;
    }
    return pxa_net_http_request(REQUEST_WEATHER, &request, request_payload,
                                sizeof(request_payload), packet,
                                sizeof(packet));
}

static int start_weather_request(void) {
    provider = manual_location ? WEATHER_OPEN_METEO : WEATHER_IPWHO;
    permission_denied = 0;
    return network_permission_handles[provider] == 0 ?
           acquire_network_permission() : fetch_weather();
}

static int start_city_search(void) {
    candidate_count = 0;
    search_failed = 0;
    location_storage_error = 0;
    if (string_length(search_draft) < 2u) {
        search_failed = 2;
        return 1;
    }
    copy_text(search_query, sizeof(search_query), search_draft);
    provider = WEATHER_GEOCODING;
    permission_denied = 0;
    return network_permission_handles[provider] == 0 ?
           acquire_network_permission() : fetch_weather();
}

static int start_history_request(void) {
    if (active_weather_provider != WEATHER_OPEN_METEO ||
        !weather_provider_history_url(&location, last_update, weather_url,
                                      sizeof(weather_url))) {
        state = WEATHER_READY;
        return schedule_auto_refresh();
    }
    provider = WEATHER_HISTORY;
    return network_permission_handles[provider] == 0 ?
           acquire_network_permission() : fetch_weather();
}

static int advance_provider(void) {
    weather_provider_t backup = weather_provider_backup(provider);
    if (provider == WEATHER_GEOCODING) {
        search_failed = 1;
        state = has_weather ? WEATHER_READY : WEATHER_ERROR;
        return schedule_auto_refresh();
    }
    if (provider == WEATHER_AIR_QUALITY) return start_history_request();
    if (provider == WEATHER_HISTORY) {
        history_count = 0;
        state = WEATHER_READY;
        return schedule_auto_refresh();
    }
    if (backup == WEATHER_PROVIDER_COUNT) {
        state = permission_denied ? WEATHER_DENIED : WEATHER_ERROR;
        return schedule_auto_refresh();
    }
    provider = backup;
    return network_permission_handles[provider] == 0 ?
           acquire_network_permission() : fetch_weather();
}

static int finish_response(void) {
    weather_conditions_t conditions;
    weather_location_t parsed_location;
    weather_air_t air;
    int valid = (response_flags & PXA_NET_RESPONSE_BODY_LENGTH_KNOWN) == 0 ||
                response_length == response_size;
    if (valid) valid = provider == WEATHER_GEOCODING ?
        (candidate_count = (uint8_t)weather_provider_search_results(
            response_body, response_size, candidates, WEATHER_CITY_RESULTS)) != 0 :
        provider == WEATHER_HISTORY ?
        (history_count = weather_provider_history(response_body, response_size,
            history_days, WEATHER_DAILY_COUNT)) != 0 :
        provider == WEATHER_AIR_QUALITY ?
        weather_provider_air(response_body, response_size, &air) :
        provider < WEATHER_OPEN_METEO ?
        weather_provider_location(provider, response_body, response_size,
                                  &parsed_location) :
        weather_provider_conditions(provider, response_body, response_size,
                                    &conditions);
    if (!close_handle(body_handle)) return 0;
    body_handle = 0;
    stream_waiting = 0;
    if (!valid) return advance_provider();
    if (provider == WEATHER_GEOCODING) {
        search_failed = 0;
        state = has_weather ? WEATHER_READY : WEATHER_IDLE;
        return schedule_auto_refresh();
    }
    if (provider == WEATHER_AIR_QUALITY) {
        air_aqi = air.european_aqi;
        air_pm25_tenths = air.pm25_tenths;
        return start_history_request();
    }
    if (provider == WEATHER_HISTORY) {
        state = WEATHER_READY;
        return schedule_auto_refresh();
    }
    if (provider < WEATHER_OPEN_METEO) {
        location = parsed_location;
        has_location = 1;
        forecast.hour_count = forecast.day_count = 0;
        history_count = 0;
        copy_text(city, sizeof(city), location.city);
        provider = WEATHER_OPEN_METEO;
        return network_permission_handles[provider] == 0 ?
               acquire_network_permission() : fetch_weather();
    }
    temperature = conditions.temperature;
    apparent_temperature = conditions.apparent_temperature;
    has_apparent = conditions.has_apparent;
    humidity = conditions.humidity;
    wind_speed = conditions.wind_speed;
    high_temperature = conditions.high_temperature;
    low_temperature = conditions.low_temperature;
    has_range = conditions.has_range;
    weather_code = conditions.code;
    is_day = conditions.is_day;
    copy_text(last_update, sizeof(last_update), conditions.updated);
    has_weather = 1;
    active_weather_provider = provider;
    forecast.hour_count = forecast.day_count = 0;
    history_count = 0;
    if (provider == WEATHER_OPEN_METEO)
        (void)weather_provider_forecast(response_body, response_size, &forecast);
    air_aqi = -1;
    air_pm25_tenths = -1;
    provider = WEATHER_AIR_QUALITY;
    return network_permission_handles[provider] == 0 ?
           acquire_network_permission() : fetch_weather();
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
            return advance_provider();
        }
        if (count == 0) return finish_response();
        response_size += (uint32_t)count;
    }
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)pxa_i18n_init_from_start_config(
        &i18n, &pxa_app_i18n_bundle, config, config_length);
    {
        pxa_ui_environment_t environment;
        if (pxa_ui_parse_start_environment(config, config_length, &environment))
            apply_environment(&environment);
    }
    state = WEATHER_LOADING;
    for (size_t index = 0; index < WEATHER_PROVIDER_COUNT; ++index)
        network_permission_handles[index] = 0;
    has_location = 0;
    has_weather = 0;
    manual_location = 0;
    show_city_picker = 0;
    forecast.hour_count = forecast.day_count = 0;
    history_count = 0;
    candidate_count = 0;
    search_failed = 0;
    location_storage_error = 0;
    search_draft[0] = '\0';
    air_aqi = -1;
    air_pm25_tenths = -1;
    auto_refresh_ticks = 0;
    if (!configure_window())
        return PXA_STATUS_INTERNAL;
    (void)pxa_send(PXA_SERVICE_WINDOW, PXA_WINDOW_GET_SNAPSHOT,
                   REQUEST_WINDOW_SNAPSHOT, NULL, 0);
    if (!render())
        return PXA_STATUS_INTERNAL;
    if (!pxa_storage_get(REQUEST_LOCATION_GET, LOCATION_STORAGE_KEY,
                         sizeof(LOCATION_STORAGE_KEY) - 1u, request_payload,
                         sizeof(request_payload), packet, sizeof(packet)) &&
        !start_weather_request()) return PXA_STATUS_INTERNAL;
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
    if (parsed.service == PXA_SERVICE_WINDOW &&
        parsed.opcode == PXA_WINDOW_GET_SNAPSHOT &&
        parsed.request_id == REQUEST_WINDOW_SNAPSHOT) {
        if (!apply_window_snapshot(&parsed, 1)) return PXA_EVENT_UNHANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_WINDOW &&
        parsed.opcode == PXA_WINDOW_METRICS_CHANGED) {
        if (!apply_window_snapshot(&parsed, 0)) return PXA_EVENT_UNHANDLED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_UI &&
        parsed.opcode == PXA_UI_ENVIRONMENT_CHANGED) {
        pxa_ui_environment_t environment;
        if (!pxa_ui_parse_environment_event(&parsed, &environment))
            return PXA_EVENT_UNHANDLED;
        apply_environment(&environment);
        (void)pxa_send(PXA_SERVICE_WINDOW, PXA_WINDOW_GET_SNAPSHOT,
                       REQUEST_WINDOW_SNAPSHOT, NULL, 0);
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_WINDOW &&
        parsed.opcode == PXA_WINDOW_BACK_REQUESTED && show_city_picker) {
        show_city_picker = 0;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_STORAGE &&
        parsed.opcode == PXA_STORAGE_GET &&
        parsed.request_id == REQUEST_LOCATION_GET) {
        pxa_storage_get_result_t saved;
        if (pxa_storage_parse_get(&parsed, &saved) &&
            saved.status == PXA_STATUS_OK &&
            weather_location_decode(saved.value, saved.value_length,
                                    &location)) {
            has_location = 1;
            manual_location = 1;
            copy_text(city, sizeof(city), location.city);
        }
        return start_weather_request() && render() ? PXA_EVENT_HANDLED :
                                                   PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_STORAGE &&
        (parsed.request_id == REQUEST_LOCATION_SET ||
         parsed.request_id == REQUEST_LOCATION_REMOVE)) {
        int32_t status;
        if (!pxa_storage_parse_status(&parsed,
                                      parsed.request_id == REQUEST_LOCATION_SET ?
                                          PXA_STORAGE_SET : PXA_STORAGE_REMOVE,
                                      &status)) return PXA_STATUS_INTERNAL;
        location_storage_error = (uint8_t)(status != PXA_STATUS_OK);
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == NODE_CITY_INPUT &&
        ui_event.kind == PXA_UI_EVENT_TEXT_KIND) {
        (void)pxa_ui_event_text(&ui_event, search_draft,
                                sizeof(search_draft));
        return PXA_EVENT_HANDLED;
    }
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == NODE_LOCATION_PICKER &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND) {
        show_city_picker = (uint8_t)!show_city_picker;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND &&
        state != WEATHER_LOADING) {
        if (ui_event.node == NODE_CITY_SEARCH) {
            return start_city_search() && render() ? PXA_EVENT_HANDLED :
                                                     PXA_STATUS_INTERNAL;
        }
        if (ui_event.node == NODE_LOCATION_AUTO) {
            location_storage_error = (uint8_t)!pxa_storage_remove(
                REQUEST_LOCATION_REMOVE, LOCATION_STORAGE_KEY,
                sizeof(LOCATION_STORAGE_KEY) - 1u, request_payload,
                sizeof(request_payload), packet, sizeof(packet));
            manual_location = 0;
            show_city_picker = 0;
            has_location = 0;
            has_weather = 0;
            forecast.hour_count = forecast.day_count = 0;
            history_count = 0;
            candidate_count = 0;
            search_failed = 0;
            return start_weather_request() && render() ? PXA_EVENT_HANDLED :
                                                       PXA_STATUS_INTERNAL;
        }
        if (ui_event.node >= NODE_CITY_RESULT_BASE &&
            ui_event.node < NODE_CITY_RESULT_BASE + candidate_count) {
            uint8_t saved[73];
            size_t size;
            location = candidates[ui_event.node - NODE_CITY_RESULT_BASE].location;
            copy_text(city, sizeof(city), location.city);
            has_location = 1;
            manual_location = 1;
            show_city_picker = 0;
            has_weather = 0;
            forecast.hour_count = forecast.day_count = 0;
            history_count = 0;
            air_aqi = -1;
            candidate_count = 0;
            search_failed = 0;
            size = weather_location_encode(&location, saved, sizeof(saved));
            location_storage_error = (uint8_t)(size == 0 || !pxa_storage_set(
                REQUEST_LOCATION_SET, LOCATION_STORAGE_KEY,
                sizeof(LOCATION_STORAGE_KEY) - 1u, saved, size,
                request_payload, sizeof(request_payload), packet,
                sizeof(packet)));
            return start_weather_request() && render() ? PXA_EVENT_HANDLED :
                                                       PXA_STATUS_INTERNAL;
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
        parsed.request_id == REQUEST_NETWORK_PERMISSION) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(&parsed, &result))
            return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK) {
            permission_denied = 1;
            return advance_provider() && render() ? PXA_EVENT_HANDLED :
                                                    PXA_STATUS_INTERNAL;
        }
        network_permission_handles[provider] = result.handle;
        permission_denied = 0;
        return fetch_weather() && render() ? PXA_EVENT_HANDLED :
                                             PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_NET &&
        parsed.opcode == PXA_NET_HTTP_REQUEST &&
        parsed.request_id == REQUEST_WEATHER) {
        pxa_net_http_result_t result;
        if (!pxa_net_parse_http_result(&parsed, &result)) {
            return advance_provider() && render() ? PXA_EVENT_HANDLED :
                                                    PXA_STATUS_INTERNAL;
        }
        if (result.status != PXA_STATUS_OK) {
            return advance_provider() && render() ? PXA_EVENT_HANDLED :
                                                    PXA_STATUS_INTERNAL;
        }
        http_status = result.status_code;
        response_flags = (uint8_t)result.flags;
        response_length = result.body_length;
        body_handle = result.body_handle;
        if (http_status != 200 || body_handle == 0) {
            if (body_handle != 0 && !close_handle(body_handle))
                return PXA_STATUS_INTERNAL;
            body_handle = 0;
            return advance_provider() && render() ? PXA_EVENT_HANDLED :
                                                    PXA_STATUS_INTERNAL;
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
