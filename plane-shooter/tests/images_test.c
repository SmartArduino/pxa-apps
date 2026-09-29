#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "images.h"

static uint64_t pending, sequence, next_handle=UINT64_C(0x1234567800000000);
static uint64_t live[PLANE_IMAGE_COUNT];
static unsigned loads, cancels, closes;
static int32_t reject;

int32_t pxa_submit(const uint8_t *data, uint32_t size) {
    pxa_event_t event;
    assert(pxa_parse_event(data,size,&event));
    if (event.service==PXA_ASSETS_SERVICE) {
        assert(event.opcode==PXA_ASSETS_LOAD);
        assert(event.payload[2]==PXA_ASSET_IMAGE && !pending);
        assert(event.token>UINT32_MAX && event.token>sequence);
        sequence=event.token;
        if (reject) return reject;
        pending=event.token; ++loads;
        return 0;
    }
    assert(event.service==PXA_CORE_SERVICE && event.payload_size==8);
    uint64_t value=pxa_load_u64(event.payload);
    if (event.opcode==PXA_CORE_CANCEL_REQUEST) {
        assert(value==pending); ++cancels; return 0;
    }
    assert(event.opcode==PXA_CORE_CLOSE_HANDLE);
    for (unsigned i=0;i<PLANE_IMAGE_COUNT;++i) if (live[i]==value) {
        live[i]=0; ++closes; return 0;
    }
    assert(!"double close or truncated/unknown handle");
    return PXA_STATUS_INTERNAL;
}

static unsigned count_live(void) {
    unsigned count=0;
    for (unsigned i=0;i<PLANE_IMAGE_COUNT;++i) count+=live[i]!=0;
    return count;
}
static pxa_event_t completed(int32_t status, uint8_t kind, uint8_t payload[32]) {
    assert(pending);
    memset(payload,0,32);
    pxa_store_u32(payload,(uint32_t)status);
    if (!status) {
        uint64_t handle=++next_handle;
        unsigned slot=0;
        while (slot<PLANE_IMAGE_COUNT && live[slot]) ++slot;
        assert(slot<PLANE_IMAGE_COUNT); live[slot]=handle;
        pxa_store_u64(payload+4,handle); payload[12]=kind;
        payload[13]=PXA_ASSET_ENCODING_BGRA8888;
        pxa_store_u16(payload+14,1);
        pxa_store_u16(payload+16,2); pxa_store_u16(payload+18,2);
        pxa_store_u32(payload+20,40); pxa_store_u32(payload+24,16);
        pxa_store_u32(payload+28,80);
    }
    pxa_event_t event={PXA_ASSETS_SERVICE,PXA_ASSETS_LOAD,pending,payload,status ? 4 : 32};
    pending=0;
    return event;
}
static void complete(int32_t status) {
    uint8_t payload[32];
    pxa_event_t event=completed(status,PXA_ASSET_IMAGE,payload);
    assert(plane_images_on_event(&event));
    assert(!plane_images_on_event(&event)); /* Duplicate result is not ours. */
}
static void ready(uint32_t mask) {
    plane_images_select(mask);
    while (!plane_images_ready()) {
        assert(!plane_images_error()); plane_images_pump(); assert(pending);
        unsigned count=loads; plane_images_pump(); assert(count==loads);
        complete(0);
    }
}

int main(void) {
    const uint32_t first=PLANE_IMAGE_BIT(0), second=PLANE_IMAGE_BIT(1), third=PLANE_IMAGE_BIT(2);
    ready(first|second);
    assert(count_live()==2 && plane_image_handle(0)>UINT32_MAX);
    assert(!plane_image_handle(PLANE_IMAGE_NONE));
    unsigned count=loads; ready(first|second); assert(loads==count);
    plane_images_select(second|third);
    assert(count_live()==1 && !plane_image_handle(0) && plane_image_handle(1));
    plane_images_pump(); assert(pending);
    plane_images_select(second); assert(cancels==1);
    complete(0); /* A cancellation racing a successful load must close it. */
    assert(count_live()==1 && plane_images_ready());

    plane_images_select(first|second); plane_images_pump();
    uint64_t old=pending;
    pxa_event_t unrelated={PXA_ASSETS_SERVICE,PXA_ASSETS_LOAD,old+1,NULL,0};
    assert(!plane_images_on_event(&unrelated) && pending==old);
    plane_images_pause(1); assert(cancels==2);
    complete(PXA_STATUS_CANCELLED);
    assert(!plane_images_error() && count_live()==1);
    count=loads; plane_images_pump(); assert(loads==count && !pending);
    plane_images_pause(0); plane_images_pump(); assert(pending>old); complete(0);
    assert(plane_images_ready() && count_live()==2);

    plane_images_select(first|second|third); plane_images_pump();
    complete(PXA_STATUS_IO_ERROR);
    assert(plane_images_error()==PXA_STATUS_IO_ERROR && !count_live());
    count=loads; plane_images_pump(); assert(loads==count && !pending);
    plane_images_retry(); ready(first|second|third); assert(count_live()==3);

    plane_images_select(first|second|third|PLANE_IMAGE_BIT(3));
    reject=PXA_STATUS_WOULD_BLOCK; plane_images_pump();
    assert(!pending && !count_live() && plane_images_error()==reject);
    reject=0; plane_images_retry(); ready(first); assert(count_live()==1);

    plane_images_select(first|second); plane_images_pump();
    uint8_t payload[32];
    pxa_event_t event=completed(0,PXA_ASSET_TEXTURE,payload);
    assert(plane_images_on_event(&event));
    assert(plane_images_error()==PXA_STATUS_PROTOCOL_ERROR && !count_live());

    plane_images_retry(); ready(first);
    plane_images_select(first|second); plane_images_pump();
    plane_images_stop(); assert(!count_live() && pending);
    complete(0); assert(!count_live());
    count=loads; plane_images_pump(); assert(loads==count && !pending);
    assert(closes>0);
    puts("Plane images: bounded LOAD, 64-bit identity, scene release, cancellation race, background, errors, explicit retry and exit passed.");
}
