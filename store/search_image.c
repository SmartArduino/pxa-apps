#include "search_image.h"
#include "pxa_image_set.h"

static const char *const paths[]={"assets/search.pxr"};
static uint64_t handles[1];
static pxa_image_set_t images=PXA_IMAGE_SET_INIT(paths,handles,UINT64_C(0x5352434800000000));

uint64_t store_search_image_handle(void) { return pxa_image_set_handle(&images,0); }
int32_t store_search_image_error(void) { return images.error; }
void store_search_image_request(void) {
    (void)pxa_image_set_select(&images,1);
    pxa_image_set_pump(&images);
}
void store_search_image_retry(void) {
    pxa_image_set_retry(&images);store_search_image_request();
}
void store_search_image_pause(int paused) {
    pxa_image_set_pause(&images,paused);
    if (!paused) store_search_image_retry();
}
void store_search_image_stop(void) { pxa_image_set_stop(&images); }
int store_search_image_on_event(const pxa_event_t *event) {
    if (!pxa_image_set_on_event(&images,event)) return 0;
    pxa_image_set_pump(&images);
    return 1;
}
