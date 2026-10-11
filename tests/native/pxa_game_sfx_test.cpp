#include "pxa_control_capture.h"
#include <assert.h>
#include <string.h>

#include "pxa_game_sfx.h"

static pxa_event_t submitted;
static uint8_t last_packet[128];
static uint32_t pcm_writes;

int32_t pxa_test_control(const uint8_t *data, uint32_t size) {
    assert(size <= sizeof(last_packet));
    memcpy(last_packet, data, size);
    assert(pxa_parse_event(last_packet, size, &submitted));
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data,
                  uint32_t size) {
    assert(handle == UINT64_C(0x300000004));
    assert(operation == PXA_AUDIO_IO_WRITE);
    assert(data != NULL && size == PXA_GAME_SFX_FRAME_SAMPLES * 2u);
    ++pcm_writes;
    return (int32_t)size;
}

static void append_record(uint8_t *out, size_t *offset, uint16_t tag,
                          const uint8_t *value, size_t length) {
    size_t written = 0;
    assert(pxa_wire_record_encode(out + *offset, 64 - *offset,
                                  tag, value, length, &written));
    *offset += written;
}

int main(void) {
    pxa_game_sfx_t sfx = {0};
    pxa_event_t event = {0};
    uint8_t packet[128];
    uint8_t payload[64] = {0};
    uint8_t value[8] = {0};
    size_t offset = 4;

    pxa_game_sfx_start(&sfx, packet, sizeof(packet));
    assert(sfx.state == PXA_GAME_SFX_WAIT_PERMISSION);
    assert(submitted.service == PXA_PERMISSION_SERVICE &&
           submitted.opcode == PXA_PERMISSION_ACQUIRE &&
           submitted.token == PXA_GAME_SFX_PERMISSION_REQUEST);

    pxa_store_u64(payload + 4, UINT64_C(0x200000003));
    event.service = PXA_PERMISSION_SERVICE;
    event.opcode = PXA_PERMISSION_ACQUIRE;
    event.token = PXA_GAME_SFX_PERMISSION_REQUEST;
    event.payload = payload;
    event.payload_size = 12;
    assert(pxa_game_sfx_handle_event(&sfx, &event, packet, sizeof(packet)));
    assert(sfx.permission_handle == UINT64_C(0x200000003));
    assert(sfx.state == PXA_GAME_SFX_WAIT_OPEN);
    assert(submitted.service == PXA_AUDIO_SERVICE &&
           submitted.opcode == PXA_AUDIO_OPEN_SESSION &&
           submitted.token == PXA_GAME_SFX_OPEN_REQUEST);

    pxa_store_u64(value, UINT64_C(0x300000004));
    append_record(payload, &offset, 3, value, 8);
    pxa_store_u32(value, PXA_GAME_SFX_SAMPLE_RATE);
    append_record(payload, &offset, 4, value, 4);
    value[0] = 1;
    append_record(payload, &offset, 5, value, 1);
    pxa_store_u16(value, 20);
    append_record(payload, &offset, 6, value, 2);
    event.opcode = PXA_AUDIO_OPEN_SESSION;
    event.service = PXA_AUDIO_SERVICE;
    event.token = PXA_GAME_SFX_OPEN_REQUEST;
    event.payload_size = (uint32_t)offset;
    assert(pxa_game_sfx_handle_event(&sfx, &event, packet, sizeof(packet)));
    assert(sfx.session_handle == UINT64_C(0x300000004));
    assert(sfx.state == PXA_GAME_SFX_WAIT_GRAPH);
    assert(submitted.opcode == PXA_AUDIO_COMMIT_GRAPH &&
           submitted.token == PXA_GAME_SFX_GRAPH_REQUEST);

    event.opcode = PXA_AUDIO_COMMIT_GRAPH;
    event.token = PXA_GAME_SFX_GRAPH_REQUEST;
    event.payload_size = 4;
    assert(pxa_game_sfx_handle_event(&sfx, &event, packet, sizeof(packet)));
    assert(sfx.state == PXA_GAME_SFX_READY &&
           pcm_writes == PXA_GAME_SFX_PREFILL_FRAMES);

    pxa_store_u64(payload, 1000000);
    event.service = PXA_CLOCK_SERVICE;
    event.opcode = PXA_CLOCK_TICK;
    event.token = 0;
    event.payload_size = 8;
    pxa_game_sfx_tick(&sfx, &event);
    assert(pcm_writes == PXA_GAME_SFX_PREFILL_FRAMES + 2u);
    return 0;
}
