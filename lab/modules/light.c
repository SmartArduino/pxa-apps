#define PXA_LAB_MODULE_PREFIX pxa_lab_light_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"
#include "pxa_permission.h"
#include "pxa_sensor.h"

#define REQUEST_PERMISSION 1u
#define REQUEST_LIST 2u
#define REQUEST_SUBSCRIBE 3u

static uint8_t packet[1664];
static uint8_t request_payload[160];
static uint32_t permission_handle;
static uint32_t subscription_handle;
static int32_t illuminance_milli_lux;
static uint8_t waiting;
static uint8_t failed;
static uint8_t has_sample;

static int is_light_semantic(const pxa_sensor_descriptor_t *descriptor) {
    static const char semantic[] = "ambient.light";
    if (descriptor->semantic_length != sizeof(semantic) - 1) return 0;
    for (size_t index = 0; index < sizeof(semantic) - 1; ++index) {
        if (descriptor->semantic[index] != (uint8_t)semantic[index]) return 0;
    }
    return 1;
}

static int render(void) {
    char value[12];
    static const char semantic[] = "ambient.light";
    static const char searching[] = "Discovering sensor...";
    static const char denied[] = "Permission denied";
    static const char unsupported[] = "No compatible sensor";
    static const char waiting_sample[] = "Waiting for samples...";
    const char *state = failed ? unsupported : waiting ? searching :
                        subscription_handle != 0 ? waiting_sample : denied;
    pxa_ui_demo_page_t page = {0};
    value[0] = '\0';
    if (subscription_handle != 0 && has_sample) {
        size_t value_length = pxa_ui_demo_format_u32(value,
            (uint32_t)(illuminance_milli_lux < 0 ? -illuminance_milli_lux : illuminance_milli_lux));
        if (illuminance_milli_lux < 0 && value_length + 1 < sizeof(value)) {
            for (size_t index = value_length; index > 0; --index) value[index] = value[index - 1];
            value[0] = '-';
            ++value_length;
        }
    }
    page.title = "Light Sensor Lab";
    page.body = value[0] != '\0' ? value : semantic;
    page.status = state;
    page.action = "REQUEST ACCESS";
    page.features = PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_SLIDER | PXA_UI_DEMO_PAGE_HAS_PROGRESS;
    page.icon = 3;
    page.progress = has_sample ? 100 : waiting ? 50 : 0;
    page.slider_value = has_sample ? (uint8_t)((uint32_t)(illuminance_milli_lux < 0 ? 0 : illuminance_milli_lux) % 101u) : 0;
    page.enabled = (uint8_t)(!waiting && subscription_handle == 0);
    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}

static int acquire_permission(void) {
    static const char name[] = "sensor.read";
    static const uint8_t scope[] = "ambient.light";
    waiting = 1;
    failed = 0;
    return pxa_permission_acquire(REQUEST_PERMISSION, name, sizeof(name) - 1,
                                  scope, sizeof(scope) - 1, request_payload,
                                  sizeof(request_payload), packet, sizeof(packet));
}

static int discover(void) {
    waiting = 1;
    return pxa_sensor_list(REQUEST_LIST, packet, sizeof(packet));
}

static int subscribe(uint16_t sensor_id) {
    return pxa_sensor_subscribe(REQUEST_SUBSCRIBE, sensor_id, 500, permission_handle,
                                request_payload, sizeof(request_payload), packet,
                                sizeof(packet));
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    if (!pxa_window_fullscreen() || !render() || !acquire_permission())
        return PXA_STATUS_INTERNAL;
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == PXA_UI_DEMO_PAGE_NODE_BUTTON &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND && !waiting && subscription_handle == 0) {
        if (!acquire_permission()) return PXA_STATUS_INTERNAL;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_PERMISSION &&
        parsed.opcode == PXA_PERMISSION_ACQUIRE && parsed.request_id == REQUEST_PERMISSION) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status != PXA_STATUS_OK) {
            waiting = 0;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        permission_handle = result.handle;
        return discover() && render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SENSOR && parsed.opcode == PXA_SENSOR_LIST &&
        parsed.request_id == REQUEST_LIST) {
        pxa_sensor_list_result_t result;
        pxa_sensor_descriptor_t descriptor;
        size_t offset = 0;
        uint16_t sensor_id = 0;
        if (!pxa_sensor_parse_list(&parsed, &result)) return PXA_STATUS_INTERNAL;
        while (pxa_sensor_descriptor_next(&result, &offset, &descriptor)) {
            if (is_light_semantic(&descriptor)) {
                sensor_id = descriptor.id;
                break;
            }
        }
        if (sensor_id == 0 || !subscribe(sensor_id)) {
            waiting = 0;
            failed = 1;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SERVICE_SENSOR && parsed.opcode == PXA_SENSOR_SUBSCRIBE &&
        parsed.request_id == REQUEST_SUBSCRIBE) {
        pxa_sensor_subscribe_result_t result;
        if (!pxa_sensor_parse_subscribe(&parsed, &result)) return PXA_STATUS_INTERNAL;
        subscription_handle = result.status == PXA_STATUS_OK ? result.handle : 0;
        waiting = 0;
        failed = subscription_handle == 0;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SENSOR && parsed.opcode == PXA_SENSOR_SAMPLE) {
        pxa_sensor_sample_t sample;
        if (!pxa_sensor_parse_sample(&parsed, &sample) || sample.handle != subscription_handle ||
            sample.values_length != 4) return PXA_EVENT_UNHANDLED;
        illuminance_milli_lux = (int32_t)pxa_read_u32(sample.values);
        has_sample = 1;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
