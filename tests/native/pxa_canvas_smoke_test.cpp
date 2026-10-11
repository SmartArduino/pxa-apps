#include "pxa_control_capture.h"
#include "pxa_canvas.h"

#include <assert.h>
#include <string.h>

int32_t pxa_app_start(const uint8_t* config, uint32_t config_length);
int32_t pxa_app_on_event(const uint8_t* event, uint32_t length);
void pxa_app_stop(uint32_t reason);

static uint32_t control_count;
static uint16_t max_primitives;
static uint8_t display_list[16u * 1024u];
static size_t display_list_size;

static void validate_display_list(const uint8_t* data, size_t size) {
    size_t offset = 0;
    uint16_t primitives = 0;
    while (offset < size) {
        uint8_t type;
        size_t payload_size;
        assert(size - offset >= 4);
        type = data[offset];
        payload_size = pxa_read_u16(data + offset + 2);
        offset += 4;
        assert(type >= PXA_CANVAS_DRAW_RECT && type <= PXA_CANVAS_DRAW_TEXT_BOX);
        assert(payload_size <= size - offset);
        offset += payload_size;
        assert(++primitives <= 1024);
    }
    if (primitives > max_primitives) max_primitives = primitives;
}

static int32_t smoke_control(const uint8_t* data, uint32_t length,
                             uint32_t header_bytes) {
    assert(data != NULL);
    assert(length >= header_bytes && length <= 4096);
    assert(pxa_read_u32(data + (header_bytes == 20 ? 12 : 8)) ==
           length - header_bytes);
    ++control_count;
    if (pxa_read_u16(data) != PXA_SERVICE_UI) return PXA_STATUS_OK;
    {
        const uint16_t opcode = pxa_read_u16(data + 2);
        const uint8_t* payload = data + header_bytes;
        const size_t payload_size = length - header_bytes;
        if (opcode == PXA_UI_CANVAS_BEGIN) {
            assert(payload_size == 16);
            display_list_size = 0;
        } else if (opcode == PXA_UI_CANVAS_WRITE) {
            assert(payload_size >= 12);
            assert(payload_size - 12 <= sizeof(display_list) - display_list_size);
            memcpy(display_list + display_list_size, payload + 12,
                   payload_size - 12);
            display_list_size += payload_size - 12;
        } else if (opcode == PXA_UI_CANVAS_PRESENT) {
            assert(payload_size >= 13);
            validate_display_list(display_list, display_list_size);
        }
    }
    return PXA_STATUS_OK;
}

int32_t pxa_test_control(const uint8_t* data, uint32_t length) {
    return smoke_control(data, length, PXA_HEADER_BYTES);
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t* data,
               uint32_t length) {
    (void)handle;
    (void)operation;
    (void)data;
    (void)length;
    return PXA_STATUS_UNSUPPORTED;
}

static void deliver(uint16_t service, uint16_t opcode,
                    const uint8_t* payload, size_t payload_length) {
    uint8_t event[64];
    uint32_t size = 0;
    assert(pxa_build_message(event, sizeof(event), service, opcode, 0,
                                 payload, payload_length, &size));
    assert(pxa_app_on_event(event, size) >= 0);
}

static void deliver_pointer(uint32_t step, uint8_t phase) {
    uint8_t pointer[36];
    pxa_writer_t writer;
    const uint16_t x = (uint16_t)((step * 37u) % 296u);
    const uint16_t y = (step / 23u) % 3u == 0
                           ? 220u
                           : (uint16_t)(30u + (step * 19u) % 190u);
    pxa_writer_init(&writer, pointer, sizeof(pointer));
    assert(pxa_put_u32(&writer, PXA_UI_PRIMARY_SURFACE));
    assert(pxa_put_u32(&writer, 2));
    assert(pxa_put_u32(&writer, 1));
    assert(pxa_put_u16(&writer, PXA_UI_EVENT_POINTER_KIND));
    assert(pxa_put_u16(&writer, 0));
    assert(pxa_put_u32(&writer, step * 33000u));
    assert(pxa_put_u32(&writer, step >> 16));
    assert(pxa_put_u8(&writer, 0));
    assert(pxa_put_u8(&writer, phase));
    assert(pxa_put_u16(&writer, phase == PXA_POINTER_UP ? 0 : 1));
    assert(pxa_put_u32(&writer, x));
    assert(pxa_put_u32(&writer, y));
    deliver(PXA_SERVICE_UI, PXA_UI_EVENT, pointer, sizeof(pointer));
}

int main(void) {
    assert(pxa_app_start(NULL, 0) == PXA_STATUS_OK);
    for (uint32_t step = 0; step < 2000; ++step) {
        uint8_t tick[8];
        pxa_writer_t writer;
        pxa_writer_init(&writer, tick, sizeof(tick));
        assert(pxa_put_u32(&writer, step * 33000u));
        assert(pxa_put_u32(&writer, step >> 16));
        deliver(PXA_SERVICE_CLOCK, PXA_CLOCK_TICK, tick, sizeof(tick));
        if (step % 23u == 0) deliver_pointer(step, PXA_POINTER_DOWN);
        else if (step % 23u == 12u) {
            deliver_pointer(step - 12u, PXA_POINTER_MOVE);
            deliver_pointer(step - 12u, PXA_POINTER_UP);
        }
    }
    pxa_app_stop(0);
    assert(control_count > 10);
    assert(max_primitives > 0 && max_primitives <= 1024);
    return 0;
}
