#ifndef PLANE_IMAGES_H
#define PLANE_IMAGES_H

#include "pxa_canvas.h"
#include "pxa_assets.h"

typedef enum {
    PLANE_IMAGE_PLANE_SHOOTER_BOSS_CARRIER,
    PLANE_IMAGE_PLANE_SHOOTER_BOSS_DREADNOUGHT,
    PLANE_IMAGE_PLANE_SHOOTER_BOSS_LEVIATHAN,
    PLANE_IMAGE_PLANE_SHOOTER_ENEMY_CRUISER,
    PLANE_IMAGE_PLANE_SHOOTER_ENEMY_DART,
    PLANE_IMAGE_PLANE_SHOOTER_ENEMY_FIGHTER,
    PLANE_IMAGE_PLANE_SHOOTER_ENEMY_GUNSHIP,
    PLANE_IMAGE_PLANE_SHOOTER_ENEMY_PLANE,
    PLANE_IMAGE_PLANE_SHOOTER_PICKUP_ENERGY,
    PLANE_IMAGE_PLANE_SHOOTER_PICKUP_OVERDRIVE,
    PLANE_IMAGE_PLANE_SHOOTER_PICKUP_SHIELD,
    PLANE_IMAGE_PLANE_SHOOTER_PLAYER_PLANE_LEFT,
    PLANE_IMAGE_PLANE_SHOOTER_PLAYER_PLANE_MK2,
    PLANE_IMAGE_UI_DIALOG_PANEL,
    PLANE_IMAGE_UI_HOME_HERO,
    PLANE_IMAGE_UI_HUD_PANEL,
    PLANE_IMAGE_UI_ICON_COMMS,
    PLANE_IMAGE_UI_ICON_HANGAR,
    PLANE_IMAGE_UI_ICON_MISSION,
    PLANE_IMAGE_UI_ICON_SHOP,
    PLANE_IMAGE_UI_SHIP_MK2,
    PLANE_IMAGE_COUNT,
    PLANE_IMAGE_NONE = 255
} plane_image_id_t;

#define PLANE_IMAGE_BIT(id) (UINT32_C(1) << (id))
/* One bounded in-flight LOAD, using the app's existing event dispatcher.
 * select releases handles outside the new scene; pump is called only after
 * presenting an image-free loading frame, so old frame pins cannot deadlock
 * admission. No guest pixel buffers or implicit all-app residency. */
void plane_images_select(uint32_t wanted);
void plane_images_pump(void);
int plane_images_on_event(const pxa_event_t *event);
int plane_images_ready(void);
int32_t plane_images_error(void);
void plane_images_retry(void);
void plane_images_pause(int paused);
void plane_images_stop(void);
uint64_t plane_image_handle(plane_image_id_t image);

static inline int plane_image(pxa_canvas_frame_t *frame, int32_t x, int32_t y,
    uint32_t width, uint32_t height, uint8_t opacity, uint8_t fit, plane_image_id_t image) {
    if (pxa_canvas_image_handle(frame,x,y,width,height,opacity,fit,plane_image_handle(image))) return 1;
    if (frame) frame->failed=1;
    return 0;
}
#endif
