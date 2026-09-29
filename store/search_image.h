#ifndef STORE_SEARCH_IMAGE_H
#define STORE_SEARCH_IMAGE_H

#include "pxa_ui.h"
#include "pxa_assets.h"

/* One explicit app-owned immutable image, one outstanding LOAD, no private
 * event loop or pixel buffer. Feed completions from the normal dispatcher. */
void store_search_image_request(void);
void store_search_image_retry(void);
void store_search_image_pause(int paused);
void store_search_image_stop(void);
int store_search_image_on_event(const pxa_event_t *event);
uint64_t store_search_image_handle(void);
int32_t store_search_image_error(void);

/* Reserve normal image geometry while loading; publish prepared pixels only
 * after READY. All three search icons share this same retained Host object. */
static inline int store_search_image_set(pxa_ui_transaction_t *transaction, uint32_t node) {
    uint64_t handle=store_search_image_handle();
    return !handle || pxa_ui_set_image(transaction,node,handle);
}
#endif
