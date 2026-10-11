#include "pxa_control_capture.h"
#include "pxa_canvas.h"

#include <assert.h>

int32_t pxa_app_start(const uint8_t *config, uint32_t config_length);
int32_t pxa_app_on_event(const uint8_t *event, uint32_t length);
void pxa_app_stop(uint32_t reason);

static uint32_t control_count;

int32_t pxa_test_control(const uint8_t *data, uint32_t length) {
    assert(data != NULL);
    assert(length >= PXA_HEADER_BYTES);
    ++control_count;
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data,
               uint32_t length) {
    (void)handle;
    (void)operation;
    (void)data;
    (void)length;
    return PXA_STATUS_UNSUPPORTED;
}

static void deliver(uint16_t service, uint16_t opcode, const uint8_t *payload,
                    size_t payload_length) {
    uint8_t event[64];
    uint32_t size = 0;

    assert(pxa_build_message(event, sizeof(event), service, opcode, 0,
                                 payload, payload_length, &size));
    assert(pxa_app_on_event(event, size) >= 0);
}

static void deliver_pointer(uint8_t phase, uint16_t x, uint16_t y) {
    uint8_t pointer[36];
    pxa_writer_t writer;

    pxa_writer_init(&writer, pointer, sizeof(pointer));
    assert(pxa_put_u32(&writer, PXA_UI_PRIMARY_SURFACE));
    assert(pxa_put_u32(&writer, 2));
    assert(pxa_put_u32(&writer, 1));
    assert(pxa_put_u16(&writer, PXA_UI_EVENT_POINTER_KIND));
    assert(pxa_put_u16(&writer, 0));
    assert(pxa_put_u32(&writer, 0));
    assert(pxa_put_u32(&writer, 0));
    assert(pxa_put_u8(&writer, 0));
    assert(pxa_put_u8(&writer, phase));
    assert(pxa_put_u16(&writer, phase == PXA_POINTER_UP ? 0 : 1));
    assert(pxa_put_u32(&writer, x));
    assert(pxa_put_u32(&writer, y));
    deliver(PXA_SERVICE_UI, PXA_UI_EVENT, pointer, sizeof(pointer));
}

static void deliver_tick(uint32_t timestamp) {
    uint8_t tick[8];
    pxa_writer_t writer;

    pxa_writer_init(&writer, tick, sizeof(tick));
    assert(pxa_put_u32(&writer, timestamp));
    assert(pxa_put_u32(&writer, 0));
    deliver(PXA_SERVICE_CLOCK, PXA_CLOCK_TICK, tick, sizeof(tick));
}

int main(void) {
    uint32_t frames_after_down;

    assert(pxa_app_start(NULL, 0) == PXA_STATUS_OK);

    deliver_pointer(PXA_POINTER_DOWN, 148, 140);
    frames_after_down = control_count;
    deliver_pointer(PXA_POINTER_MOVE, 210, 160);
    assert(control_count == frames_after_down);

    deliver_tick(33000);
    assert(control_count > frames_after_down);
    pxa_app_stop(0);
    return 0;
}
