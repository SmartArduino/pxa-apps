#include "voxel_assets.h"
#include "pxa_raster.h"

#define TOKEN_FIRST UINT64_C(0x564f580000000001)
static uint64_t next_token = TOKEN_FIRST, request_token, surface;
static uint32_t caps;
static uint8_t position, block_count, font_count, paused;

static int request_next(void) {
    char block_path[] = "assets/b00.pxr";
    char font_path[] = "assets/font0.pxr";
    const char *path;
    uint8_t kind = PXA_ASSET_TEXTURE;
    if (!surface || paused) return 0;
    if (next_token == UINT64_MAX) return -1;
    if (!position) {
        kind = PXA_ASSET_PALETTE;
        path = caps & PXA_RASTER_CAP_LIT_PALETTE_DEPTH
            ? "assets/palette-lit.pxr" : "assets/palette.pxr";
    } else if (position <= block_count) {
        unsigned index = block_count == 45 ? position - 1 : (position - 1) * 3 + 1;
        block_path[8] = (char)('0' + index / 10);
        block_path[9] = (char)('0' + index % 10);
        path = block_path;
    } else if (font_count == 3) {
        font_path[11] = (char)('0' + position - block_count - 1);
        path = font_path;
    } else path = "assets/font-legacy.pxr";
    request_token = next_token++;
    if (pxa_assets_load(request_token, path, kind) != 0) {
        request_token = 0;
        return -1;
    }
    return 0;
}

void voxel_assets_cancel(void) {
    if (request_token) (void)pxa_cancel(request_token);
    request_token = 0;
    surface = 0;
    paused = 0;
}

int voxel_assets_begin(uint64_t context, uint32_t capabilities) {
    voxel_assets_cancel();
    if (!context) return -1;
    surface = context; caps = capabilities; position = 0;
    block_count = caps & PXA_RASTER_CAP_TEXTURE_SLOTS_48 ? 45 : 15;
    font_count = block_count == 45 && (caps & PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA) ? 3 : 1;
    return request_next();
}

int voxel_assets_suspend(int suspended) {
    if (!surface) return 0;
    if (suspended) {
        paused = 1;
        if (request_token) (void)pxa_cancel(request_token);
        request_token = 0;
        return 0;
    }
    if (!paused) return 0;
    paused = 0;
    return request_next();
}

int voxel_assets_on_event(const pxa_event_t *event) {
    pxa_asset_result_t result;
    if (event->service != PXA_ASSETS_SERVICE ||
        event->token < TOKEN_FIRST || event->token >= next_token) return 0;
    if (!pxa_assets_parse_result(event, event->token, PXA_ASSETS_LOAD, &result)) return -1;
    /* Cancellation can lose to an already queued success. Such handles still
     * belong to us, but must never bind into a replacement context. */
    if (event->token != request_token || !surface) {
        if (!result.status) (void)pxa_close_handle(result.handle);
        return 1;
    }
    request_token = 0;
    if (result.status) return -1;
    uint8_t slot = position ? (uint8_t)(position - 1) : 0;
    pxa_game_render_binding_t binding = {result.handle,
        position ? PXA_ASSET_TEXTURE : PXA_ASSET_PALETTE, slot};
    int32_t bound = pxa_game_render_bind_assets(surface, &binding, 1);
    int32_t closed = pxa_close_handle(result.handle);
    if (bound < 0 || closed != 0) return -1;
    if (++position == 1 + block_count + font_count) {
        surface = 0;
        return 2;
    }
    return request_next() ? -1 : 1;
}
