#undef NDEBUG
#include "../voxel_assets.h"
#include "pxa_raster.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t token, context = 101, last_handle, last_cancel;
static unsigned loads, binds, closes;
static int reject_submit, reject_bind;
static char path[64];
static uint8_t kind, slot;

int32_t pxa_submit(const uint8_t *data, uint32_t length) {
    pxa_event_t message;
    assert(pxa_parse_event(data, length, &message));
    if (message.service == PXA_ASSETS_SERVICE) {
        if (reject_submit) return -9;
        assert(message.opcode == PXA_ASSETS_LOAD && message.payload_size >= 5);
        unsigned size = pxa_load_u16(message.payload);
        assert(size < sizeof(path) && size + 4 == message.payload_size);
        memcpy(path, message.payload + 4, size); path[size] = 0;
        token = message.token; kind = message.payload[2]; ++loads;
    } else {
        assert(message.service == PXA_CORE_SERVICE && message.payload_size == 8);
        if (message.opcode == PXA_CORE_CANCEL_REQUEST)
            last_cancel = pxa_load_u64(message.payload);
        else {
            assert(message.opcode == PXA_CORE_CLOSE_HANDLE);
            last_handle = pxa_load_u64(message.payload); ++closes;
        }
    }
    return 0;
}
int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *data, uint32_t length) {
    assert(handle == context && operation == PXA_GAME_RENDER_IO_BIND_ASSETS);
    assert(length == 16 && pxa_load_u16(data) == 1);
    assert(pxa_load_u64(data + 8) != 0);
    slot = data[5]; ++binds;
    return reject_bind ? -9 : (int32_t)length;
}
static int result(uint64_t request, int status, uint64_t handle, uint8_t asset_kind) {
    uint8_t payload[32] = {0};
    pxa_event_t event = {PXA_ASSETS_SERVICE, PXA_ASSETS_LOAD,
                            request, payload, status ? 4 : sizeof(payload)};
    pxa_store_u32(payload, (uint32_t)status);
    pxa_store_u64(payload + 4, handle);
    payload[12] = asset_kind;
    return voxel_assets_on_event(&event);
}
static void complete(unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned before_closes = closes;
        uint64_t request = token, resource = UINT64_C(0x100000001) + i;
        uint8_t asset_kind = kind;
        if (i == 0) assert(strstr(path, "palette") != NULL);
        assert(result(request, 0, resource, asset_kind) == (i + 1 == count ? 2 : 1));
        assert(slot == (i ? i - 1 : 0));
        assert(closes == before_closes + 1 && last_handle == resource);
    }
}
int main(void) {
    uint32_t caps = PXA_RASTER_CAP_TEXTURE_SLOTS_48 |
        PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA | PXA_RASTER_CAP_LIT_PALETTE_DEPTH;
    assert(!voxel_assets_begin(context, caps));
    assert(!strcmp(path, "assets/palette-lit.pxr"));
    complete(49);
    assert(loads == 49 && binds == 49 && closes == 49);
    assert(!voxel_assets_begin(context, 0));
    assert(!strcmp(path, "assets/palette.pxr"));
    assert(result(token, 0, 1, kind) == 1);
    assert(!strcmp(path, "assets/b01.pxr"));
    for (unsigned i = 0; i < 15; ++i) assert(result(token, 0, 2 + i, kind) == 1);
    assert(!strcmp(path, "assets/font-legacy.pxr"));
    assert(result(token, 0, 99, kind) == 2 && slot == 15);

    assert(!voxel_assets_begin(context, caps));
    uint64_t old = token;
    assert(!voxel_assets_suspend(1) && last_cancel == old);
    unsigned bound = binds;
    assert(result(old, 0, 1001, PXA_ASSET_PALETTE) == 1);
    assert(binds == bound && last_handle == 1001); /* Late ready after pause. */
    assert(!voxel_assets_suspend(0) && token != old);
    assert(result(token, 0, 1002, kind) == 1);
    old = token;
    voxel_assets_cancel();
    ++context;
    assert(!voxel_assets_begin(context, caps));
    assert(result(old, 0, 1003, PXA_ASSET_TEXTURE) == 1);
    assert(binds == bound + 1 && last_handle == 1003); /* Old context cannot bind. */
    assert(result(token, -7, 0, kind) == -1);
    voxel_assets_cancel();
    reject_submit = 1;
    assert(voxel_assets_begin(context, caps) == -1);
    reject_submit = 0;
    assert(!voxel_assets_begin(context, caps));
    reject_bind = 1;
    assert(result(token, 0, 1004, kind) == -1 && last_handle == 1004);
    voxel_assets_cancel();
    puts("voxel file resources: 49/17 binding plans, pause/resume, cancellation, late success, submit/load/bind failures passed");
    return 0;
}
