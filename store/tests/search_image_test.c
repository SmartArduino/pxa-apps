#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "search_image.h"

static uint64_t pending, last_token, live, next_handle=UINT64_C(0xabc0000000000000);
static unsigned loads,cancels,closes;
static int32_t reject;
int32_t pxa_submit(const uint8_t *data,uint32_t size) {
    pxa_event_t event; assert(pxa_parse_event(data,size,&event));
    if(event.service==PXA_ASSETS_SERVICE) {
        assert(event.opcode==PXA_ASSETS_LOAD && !pending);
        assert(event.token>last_token && event.token>UINT32_MAX);
        assert(event.payload[2]==PXA_ASSET_IMAGE);
        assert(event.payload_size==4+strlen("assets/search.pxr"));
        assert(!memcmp(event.payload+4,"assets/search.pxr",strlen("assets/search.pxr")));
        last_token=event.token;
        if(reject) return reject;
        pending=event.token; ++loads; return 0;
    }
    assert(event.service==PXA_CORE_SERVICE && event.payload_size==8);
    uint64_t value=pxa_load_u64(event.payload);
    if(event.opcode==PXA_CORE_CANCEL_REQUEST) { assert(value==pending);++cancels;return 0; }
    assert(event.opcode==PXA_CORE_CLOSE_HANDLE && live && value==live);
    live=0; ++closes;return 0;
}
static void complete(int32_t status,uint8_t kind) {
    uint8_t payload[32]={0}; assert(pending);
    pxa_store_u32(payload,(uint32_t)status);
    if(!status) {
        assert(!live);live=++next_handle;pxa_store_u64(payload+4,live);
        payload[12]=kind;payload[13]=PXA_ASSET_ENCODING_BGRA8888;
        pxa_store_u16(payload+14,1);pxa_store_u16(payload+16,2);pxa_store_u16(payload+18,2);
        pxa_store_u32(payload+20,48);pxa_store_u32(payload+24,16);pxa_store_u32(payload+28,80);
    }
    pxa_event_t event={PXA_ASSETS_SERVICE,PXA_ASSETS_LOAD,pending,payload,status?4:32};
    pending=0;assert(store_search_image_on_event(&event));assert(!store_search_image_on_event(&event));
}
int main(int argc, char **argv) {
    (void)argv;
    store_search_image_request();assert(pending && loads==1);
    if (argc>1) {
        store_search_image_stop();assert(cancels==1);
        complete(0,PXA_ASSET_IMAGE);
        assert(!live && !pending && closes==1);
        store_search_image_pause(0);store_search_image_retry();assert(loads==1);
        puts("Store image: stop during LOAD closes late success and cannot restart");
        return 0;
    }
    store_search_image_request();assert(loads==1);
    pxa_event_t unrelated={PXA_ASSETS_SERVICE,PXA_ASSETS_LOAD,pending+1,NULL,0};
    assert(!store_search_image_on_event(&unrelated));
    store_search_image_pause(1);store_search_image_pause(1);assert(cancels==1);
    complete(0,PXA_ASSET_IMAGE);assert(!live && closes==1 && !pending);
    store_search_image_pause(0);assert(pending);
    store_search_image_pause(1);store_search_image_pause(0);
    unsigned count=loads;complete(PXA_STATUS_CANCELLED,PXA_ASSET_IMAGE);
    assert(pending && loads==count+1); /* Foreground before cancel completion. */
    complete(PXA_STATUS_IO_ERROR,PXA_ASSET_IMAGE);
    assert(!pending && store_search_image_error()==PXA_STATUS_IO_ERROR);
    store_search_image_request();assert(loads==count+1);
    reject=PXA_STATUS_WOULD_BLOCK;store_search_image_retry();
    assert(!pending && store_search_image_error()==PXA_STATUS_WOULD_BLOCK);
    reject=0;store_search_image_retry();complete(0,PXA_ASSET_TEXTURE);
    assert(!live && store_search_image_error()==PXA_STATUS_PROTOCOL_ERROR);
    store_search_image_retry();store_search_image_pause(1);
    complete(PXA_STATUS_CANCELLED,PXA_ASSET_IMAGE);assert(!pending && !live);
    store_search_image_pause(0);complete(0,PXA_ASSET_IMAGE);
    assert(live && store_search_image_handle()==live);
    count=loads;store_search_image_pause(1);store_search_image_pause(0);
    store_search_image_request();assert(loads==count && live); /* Warm lifetime reuse. */
    store_search_image_stop();assert(!live && !store_search_image_handle());
    store_search_image_pause(0);store_search_image_retry();assert(loads==count);
    printf("Store image: loads=%u closes=%u cancels=%u; bounded requests, retry, cancellation race and lifetime passed\n",loads,closes,cancels);
}
