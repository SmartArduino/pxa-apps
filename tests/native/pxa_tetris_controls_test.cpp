#include "pxa_control_capture.h"
#include "pxa_canvas.h"

#include <assert.h>

int32_t pxa_app_start(const uint8_t* config, uint32_t config_length);
int32_t pxa_app_on_event(const uint8_t* event, uint32_t length);
void pxa_app_stop(uint32_t reason);

static uint32_t canvas_presents;

int32_t pxa_test_control(const uint8_t* data, uint32_t length) {
    assert(data != NULL);
    assert(length >= PXA_HEADER_BYTES);
    if (pxa_read_u16(data) == PXA_SERVICE_UI &&
        pxa_read_u16(data + 2) == PXA_UI_CANVAS_PRESENT)
        ++canvas_presents;
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t* data,
               uint32_t length) {
    (void)handle;
    (void)operation;
    (void)data;
    (void)length;
    return PXA_STATUS_UNSUPPORTED;
}

static void deliver_pointer(uint16_t x, uint16_t y, uint8_t phase) {
    uint8_t pointer[36];
    uint8_t event[64];
    pxa_writer_t payload;
    pxa_writer_init(&payload, pointer, sizeof(pointer));
    assert(pxa_put_u32(&payload, PXA_UI_PRIMARY_SURFACE));
    assert(pxa_put_u32(&payload, 2));
    assert(pxa_put_u32(&payload, 1));
    assert(pxa_put_u16(&payload, PXA_UI_EVENT_POINTER_KIND));
    assert(pxa_put_u16(&payload, 0));
    assert(pxa_put_u32(&payload, 0));
    assert(pxa_put_u32(&payload, 0));
    assert(pxa_put_u8(&payload, 0));
    assert(pxa_put_u8(&payload, phase));
    assert(pxa_put_u16(&payload, phase == PXA_POINTER_UP ? 0 : 1));
    assert(pxa_put_u32(&payload, x));
    assert(pxa_put_u32(&payload, y));
    uint32_t size = 0;
    assert(pxa_build_message(event, sizeof(event), PXA_SERVICE_UI, PXA_UI_EVENT,
                             0, pointer, payload.length, &size));
    assert(pxa_app_on_event(event, size) == PXA_EVENT_HANDLED);
}

static void expect_single_button_refresh(uint16_t x, uint16_t y) {
    const uint32_t before = canvas_presents;
    deliver_pointer(x, y, PXA_POINTER_DOWN);
    assert(canvas_presents == before + 1);
    deliver_pointer(x, y, PXA_POINTER_UP);
    assert(canvas_presents == before + 1);
}

int main(void) {
    assert(pxa_app_start(NULL, 0) == PXA_STATUS_OK);

    expect_single_button_refresh(50, 138);  /* left */
    expect_single_button_refresh(245, 138); /* right */

    {
        const uint32_t before = canvas_presents;
        deliver_pointer(148, 138, PXA_POINTER_DOWN);
        deliver_pointer(244, 138, PXA_POINTER_MOVE);
        deliver_pointer(244, 138, PXA_POINTER_UP);
        assert(canvas_presents == before);
    }

    expect_single_button_refresh(50, 188);  /* rotate */
    expect_single_button_refresh(245, 188); /* hard drop */
    pxa_app_stop(0);
    return 0;
}
