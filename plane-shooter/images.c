#include "images.h"
#include "pxa_image_set.h"

static const char *const paths[PLANE_IMAGE_COUNT] = {
    "assets/plane-shooter/boss-carrier.pxr",
    "assets/plane-shooter/boss-dreadnought.pxr",
    "assets/plane-shooter/boss-leviathan.pxr",
    "assets/plane-shooter/enemy-cruiser.pxr",
    "assets/plane-shooter/enemy-dart.pxr",
    "assets/plane-shooter/enemy-fighter.pxr",
    "assets/plane-shooter/enemy-gunship.pxr",
    "assets/plane-shooter/enemy-plane.pxr",
    "assets/plane-shooter/pickup-energy.pxr",
    "assets/plane-shooter/pickup-overdrive.pxr",
    "assets/plane-shooter/pickup-shield.pxr",
    "assets/plane-shooter/player-plane-left.pxr",
    "assets/plane-shooter/player-plane-mk2.pxr",
    "assets/ui/dialog-panel.pxr",
    "assets/ui/home-hero.pxr",
    "assets/ui/hud-panel.pxr",
    "assets/ui/icon-comms.pxr",
    "assets/ui/icon-hangar.pxr",
    "assets/ui/icon-mission.pxr",
    "assets/ui/icon-shop.pxr",
    "assets/ui/ship-mk2.pxr",
};
static uint64_t handles[PLANE_IMAGE_COUNT];
static pxa_image_set_t images=PXA_IMAGE_SET_INIT(paths,handles,UINT64_C(0x50494d4700000000));

void plane_images_select(uint32_t mask) { (void)pxa_image_set_select(&images,mask); }
int plane_images_ready(void) { return pxa_image_set_ready(&images); }
int32_t plane_images_error(void) { return images.error; }
uint64_t plane_image_handle(plane_image_id_t image) { return pxa_image_set_handle(&images,image); }
void plane_images_pump(void) {
    pxa_image_set_pump(&images);
    if (images.error) pxa_image_set_close_except(&images,0);
}
int plane_images_on_event(const pxa_event_t *event) {
    if (!pxa_image_set_on_event(&images,event)) return 0;
    /* This game publishes a loading/error frame for the whole scene. */
    if (images.error) pxa_image_set_close_except(&images,0);
    return 1;
}
void plane_images_retry(void) { pxa_image_set_retry(&images); }
void plane_images_pause(int value) { pxa_image_set_pause(&images,value); }
void plane_images_stop(void) { pxa_image_set_stop(&images); }
