/* SPDX-License-Identifier: MIT
 * Small event-driven PXA adapter for upstream software-rendered games.
 * Pixels and the upload scratch are owned by the port, never by the Host.
 * Uses only the public pxa.core.v2 Guest transport and SDK builders.
 */
#ifndef PXA_PORT_FRAMEBUFFER_H
#define PXA_PORT_FRAMEBUFFER_H
#include <stdint.h>
#include <string.h>
#include "viewport.h"
#include "pxa.h"
#include "pxa_ui.h"
#include "pxa_clock.h"
#include "pxa_window.h"
#include "pxa_game_render.h"
#include "pxa_raster.h"

#define PXFB_INPUT_NODE 2u
#define PXFB_CREATE_TOKEN UINT64_C(0x50465801)
#define PXFB_READY 1u
#define PXFB_TICK 2u
#define PXFB_POINTER 4u
#define PXFB_CONTROLLER 8u
#define PXFB_BACK 16u
#define PXFB_ERROR 32u
#define PXFB_LIFECYCLE 64u
#define PXFB_TILE_SIZE 256u
#define PXFB_UPLOAD_BYTES (PXA_RASTER_UPLOAD_HEADER_BYTES + PXFB_TILE_SIZE * PXFB_TILE_SIZE)
/* Sixteen slots also work on Hosts without the optional 48-slot capability. */
#define PXFB_MAX_TILES 16u
#define PXFB_DRAW_BYTES (PXA_RASTER_DRAW_HEADER_BYTES + PXA_RASTER_CLEAR_BYTES + \
                         PXFB_MAX_TILES * PXA_RASTER_SPRITE_BYTES)

typedef struct {
    uint8_t *pixels, *upload;
    uint32_t upload_bytes;
    uint16_t width, height, screen_width, screen_height, render_width, render_height;
    uint16_t viewport_top, viewport_bottom;
    uint32_t density_q16;
    retro_viewport_t viewport;
    int32_t cursor_x,cursor_y;
    uint64_t cursor_due_us;
    uint8_t pointer_controller,cursor_visible;
    uint16_t palette[256];
    uint8_t packet[192], draw[PXFB_DRAW_BYTES];
    uint64_t context, frame, timestamp_us, first_tick_us;
    uint32_t time_ms, buttons;
    int32_t pointer_x, pointer_y;
    uint8_t pointer_phase, ready, background, creating, pending;
    int32_t error;
} pxfb_t;

static inline int32_t pxfb_present(pxfb_t *fb);
static inline uint32_t pxfb_cursor(pxfb_t *fb,uint32_t previous,int fresh) {
    if(!fb->pointer_controller)return 0;
    uint32_t pressed=fb->buttons&~previous,changed=fb->buttons^previous;
    int dx=!!(fb->buttons&PXA_CONTROLLER_RIGHT)-!!(fb->buttons&PXA_CONTROLLER_LEFT);
    int dy=!!(fb->buttons&PXA_CONTROLLER_DOWN)-!!(fb->buttons&PXA_CONTROLLER_UP);
    if(!fresh&&(!(dx||dy)||fb->timestamp_us<fb->cursor_due_us))return 0;
    if(fresh&&!(changed&(PXA_CONTROLLER_A|15))&&!(pressed&(PXA_CONTROLLER_B|PXA_CONTROLLER_START)))return 0;
    if(dx||dy){int step=(fb->buttons&PXA_CONTROLLER_R)?1:4;
        fb->cursor_x+=dx*step;fb->cursor_y+=dy*step;fb->cursor_due_us=fb->timestamp_us+(fresh?180000:50000);
        if(fb->cursor_x<0)fb->cursor_x=0;if(fb->cursor_x>=fb->width)fb->cursor_x=fb->width-1;
        if(fb->cursor_y<0)fb->cursor_y=0;if(fb->cursor_y>=fb->height)fb->cursor_y=fb->height-1;
    }
    fb->cursor_visible=1;fb->pointer_x=fb->cursor_x;fb->pointer_y=fb->cursor_y;
    fb->pointer_phase=pressed&PXA_CONTROLLER_A?PXA_POINTER_DOWN:
        (changed&PXA_CONTROLLER_A)&&!(fb->buttons&PXA_CONTROLLER_A)?PXA_POINTER_UP:PXA_POINTER_MOVE;
    fb->pending=1;
    return PXFB_POINTER|((pressed&(PXA_CONTROLLER_B|PXA_CONTROLLER_START))?PXFB_BACK:0);
}
static inline int32_t pxfb_create(pxfb_t *fb) {
    pxa_game_render_options_t options={0};uint32_t written=0;
    fb->render_width=fb->screen_width;fb->render_height=fb->screen_height;
    options.width=fb->render_width;options.height=fb->render_height;
    options.buffer_count=2;options.flags=PXA_GAME_RENDER_FLAG_PREFER_DIRECT_SCANOUT;
    options.scratch_mode=PXA_GAME_RENDER_SCRATCH_NONE;options.max_draw_bytes=sizeof(fb->draw);
    if(!pxa_game_render_build_create(fb->packet,sizeof(fb->packet),PXFB_CREATE_TOKEN,&options,&written))return PXA_STATUS_INTERNAL;
    int32_t status=pxa_control_packet(fb->packet,written);fb->creating=status==0;return status;
}
static inline int pxfb_environment(pxfb_t *fb,const pxa_ui_environment_t *env) {
    if(!env->width||!env->height||env->width>INT16_MAX||env->height>INT16_MAX||!env->density_q16)return 0;
    if(((env->width+255)/256)*((env->height+255)/256)>PXFB_MAX_TILES)return 0;
    uint32_t insets[4];for(unsigned i=0;i<4;++i){uint64_t n=(uint64_t)env->safe_insets[i]*env->density_q16/65536;insets[i]=n>UINT32_MAX?UINT32_MAX:(uint32_t)n;}
    fb->screen_width=env->width;fb->screen_height=env->height;fb->density_q16=env->density_q16;
    fb->viewport=retro_viewport(env->width,env->height,fb->width,fb->height,insets);return 1;
}

static inline int32_t pxfb_start(pxfb_t *fb, const uint8_t *config,
                                 uint32_t config_bytes, uint16_t period_ms) {
    pxa_ui_transaction_t transaction = {0};
    pxa_ui_environment_t environment;
    if (!fb || !fb->pixels || !fb->upload || !fb->width || !fb->height ||
        fb->upload_bytes < PXFB_UPLOAD_BYTES)
        return PXA_STATUS_INVALID_ARGUMENT;
    /* Reject a missing/malformed environment instead of guessing a screen
       size and silently delivering touches to the wrong game coordinates. */
    if (!pxa_ui_parse_start_environment(config, config_bytes, &environment) ||
        !environment.width || !environment.height ||
        environment.width > INT16_MAX || environment.height > INT16_MAX)
        return PXA_STATUS_PROTOCOL_ERROR;
    if(!pxfb_environment(fb,&environment))return PXA_STATUS_RESOURCE_LIMIT;
    fb->render_width = fb->screen_width;
    fb->render_height = fb->screen_height;
    if (((fb->render_width + PXFB_TILE_SIZE - 1) / PXFB_TILE_SIZE) *
        ((fb->render_height + PXFB_TILE_SIZE - 1) / PXFB_TILE_SIZE) > PXFB_MAX_TILES)
        return PXA_STATUS_RESOURCE_LIMIT;
    if (!pxa_ui_transaction_begin(&transaction, 1,
            PXA_UI_TRANSACTION_REPLACE_SURFACE, fb->packet, sizeof(fb->packet)) ||
        !pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) ||
        /* The GameRender image is a sibling behind the Guest UI root. The
           input-only root must be transparent so it cannot cover that image. */
        !pxa_ui_set_u8(&transaction, 1, PXA_UI_PROPERTY_OPACITY, 0) ||
        !pxa_ui_set_event_mask(&transaction, 1, PXA_UI_EVENT_MASK_CONTROLLER_STATE) ||
        !pxa_ui_create(&transaction, PXFB_INPUT_NODE, 1, 0, PXA_UI_NODE_CANVAS) ||
        !pxa_ui_set_length(&transaction, PXFB_INPUT_NODE, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(&transaction, PXFB_INPUT_NODE, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_event_mask(&transaction, PXFB_INPUT_NODE,
            PXA_UI_EVENT_MASK_POINTER) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return PXA_STATUS_INTERNAL;
    }
    if (pxa_window_fullscreen() != PXA_STATUS_OK) return PXA_STATUS_INTERNAL;
    int32_t status=pxfb_create(fb);if(status)return status;
    fb->cursor_x=fb->width/2;fb->cursor_y=fb->height/2;
    return pxa_clock_set_period(period_ms);
}

/* Parse and copy all transient event payloads before returning to the Host. */
static inline uint32_t pxfb_event(pxfb_t *fb, const uint8_t *bytes, uint32_t size) {
    pxa_event_t event;
    pxa_game_render_create_result_t created;
    pxa_ui_pointer_data_t pointer;
    pxa_ui_controller_data_t controller;
    pxa_ui_environment_t environment;
    uint8_t lifecycle;
    if (!pxa_parse_event(bytes, size, &event)) return 0;
    if (pxa_ui_parse_environment_event(&event, &environment) &&
        environment.width && environment.height &&
        environment.width <= UINT16_MAX && environment.height <= UINT16_MAX) {
        if(!pxfb_environment(fb,&environment)){fb->error=PXA_STATUS_RESOURCE_LIMIT;return PXFB_ERROR;}
        if(!fb->creating&&(fb->render_width!=fb->screen_width||fb->render_height!=fb->screen_height)){
            if(fb->context)pxa_game_render_close(fb->context);
            fb->context=0;fb->ready=0;fb->error=pxfb_create(fb);return fb->error?PXFB_ERROR:0;
        }
        return fb->ready?PXFB_READY:0;
    }
    if (pxa_game_render_parse_create(&event, PXFB_CREATE_TOKEN, &created)) {
        fb->creating=0;
        if (created.status != PXA_STATUS_OK ||
            !(created.capabilities & PXA_RASTER_CAP_TEXTURED_QUAD)) {
            fb->error = created.status ? created.status : PXA_STATUS_UNSUPPORTED;
            return PXFB_ERROR;
        }
        fb->context = created.handle;
        if(fb->render_width!=fb->screen_width||fb->render_height!=fb->screen_height){
            pxa_game_render_close(fb->context);fb->context=0;fb->error=pxfb_create(fb);return fb->error?PXFB_ERROR:0;
        }
        if (pxa_raster_upload_palette_rgb565(fb->context, fb->palette,
                fb->upload, fb->upload_bytes) != 532) {
            fb->error = PXA_STATUS_INTERNAL; return PXFB_ERROR;
        }
        fb->ready = 1;
        return PXFB_READY;
    }
    if (pxa_lifecycle_parse(&event, &lifecycle)) {
        fb->background = lifecycle == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
        fb->buttons = 0; fb->pointer_phase = PXA_POINTER_CANCEL;
        fb->first_tick_us = 0;
        return PXFB_LIFECYCLE;
    }
    if (pxa_clock_parse_tick(&event, &fb->timestamp_us)) {
        uint64_t delta_us = fb->first_tick_us && fb->timestamp_us > fb->first_tick_us
                            ? fb->timestamp_us - fb->first_tick_us : 0;
        fb->first_tick_us = fb->timestamp_us;
        /* Bound upstream elapsed-time catch-up to at most 100 ms per callback.
           Paused/background time never enters the game's virtual clock. */
        if (delta_us > 100000) delta_us = 100000;
        if (!fb->background) fb->time_ms += (uint32_t)(delta_us / 1000);
        if(fb->pending&&fb->ready&&!fb->background){fb->error=pxfb_present(fb);if(fb->error<0)return PXFB_ERROR;}
        return fb->ready && !fb->background ? PXFB_TICK|pxfb_cursor(fb,fb->buttons,0) : 0;
    }
    if (pxa_ui_parse_pointer(&event, &pointer) && pointer.node == PXFB_INPUT_NODE) {
        fb->cursor_visible=0;
        int64_t x=(int64_t)pointer.x*fb->density_q16/65536,y=(int64_t)pointer.y*fb->density_q16/65536;
        int outside=x<INT32_MIN||x>INT32_MAX||y<INT32_MIN||y>INT32_MAX||
            !retro_viewport_point(fb->viewport,(int32_t)x,(int32_t)y,fb->width,fb->height,&fb->pointer_x,&fb->pointer_y);
        if(outside)fb->pointer_x=fb->pointer_y=-1;
        fb->pointer_phase = outside ? PXA_POINTER_CANCEL : pointer.phase;
        return PXFB_POINTER;
    }
    if (pxa_ui_parse_controller(&event, &controller) &&
        controller.node == 1) {
        uint32_t previous=fb->buttons;fb->buttons=controller.connected?controller.buttons:0;
        return PXFB_CONTROLLER|pxfb_cursor(fb,previous,1);
    }
    if (pxa_window_is_back_requested(&event)) return PXFB_BACK;
    return 0;
}

static inline int32_t pxfb_present(pxfb_t *fb) {
    pxa_raster_draw_list_t list;
    uint8_t slot = 0;
    if (!fb->ready||fb->background){fb->pending=1;return 0;}
    pxa_raster_draw_list_begin(&list, fb->draw, sizeof(fb->draw), ++fb->frame);
    if (!pxa_raster_clear(&list, 0)) return PXA_STATUS_INTERNAL;
    uint8_t light=0,dark=0;
    if(fb->cursor_visible)for(unsigned i=1;i<256;++i){
        if(fb->palette[i]>fb->palette[light])light=(uint8_t)i;
        if(fb->palette[i]<fb->palette[dark])dark=(uint8_t)i;
    }
    /* Nearest-neighbour sampling uses the same scale as inverse touch input.
       Upload physical-sized tiles, so the Host never retains a second desktop
       framebuffer. Sampling global coordinates also prevents tile-edge seams. */
    for (uint32_t y = 0; y < fb->viewport.height; y += PXFB_TILE_SIZE) {
        uint16_t height = (uint16_t)(fb->viewport.height - y);
        if (height > PXFB_TILE_SIZE) height = PXFB_TILE_SIZE;
        for (uint32_t x = 0; x < fb->viewport.width; x += PXFB_TILE_SIZE, ++slot) {
            uint16_t width = (uint16_t)(fb->viewport.width - x);
            uint8_t *tile = fb->upload + PXA_RASTER_UPLOAD_HEADER_BYTES;
            int32_t uploaded;
            if (width > PXFB_TILE_SIZE) width = PXFB_TILE_SIZE;
            /* Native-sized frontends can preserve each row without per-pixel
               scaling. Opt-in keeps the other ports' sampling path unchanged. */
            if (fb->width == fb->viewport.width && fb->height == fb->viewport.height) {
                for (uint32_t row = 0; row < height; ++row)
                    memcpy(tile + row * width,
                           fb->pixels + (y + row) * fb->width + x, width);
            } else
            {
                uint16_t source_x[PXFB_TILE_SIZE];
                for (uint32_t col = 0; col < width; ++col)
                    source_x[col] = (uint16_t)((x + col) * fb->width / fb->viewport.width);
                for (uint32_t row = 0; row < height; ++row) {
                    uint32_t source_y = (y + row) * fb->height / fb->viewport.height;
                    const uint8_t *source = fb->pixels + source_y * fb->width;
                    for (uint32_t col = 0; col < width; ++col)
                        tile[row * width + col] = source[source_x[col]];
                }
            }
            if(fb->cursor_visible){
                int cx=(int)((uint64_t)fb->cursor_x*fb->viewport.width/fb->width)-(int)x;
                int cy=(int)((uint64_t)fb->cursor_y*fb->viewport.height/fb->height)-(int)y;
                for(int i=-5;i<=5;++i){int xx[2]={cx+i,cx},yy[2]={cy,cy+i};
                    for(int j=0;j<2;++j)if((i||!j)&&xx[j]>=0&&xx[j]<width&&yy[j]>=0&&yy[j]<height){
                        uint8_t *pixel=tile+yy[j]*width+xx[j];
                        *pixel=fb->palette[*pixel]>0x7fff?dark:light;
                    }
                }
            }
            uploaded = pxa_raster_upload_texture_index8(fb->context, slot,
                width, height, tile, fb->upload, fb->upload_bytes);
            if(uploaded==PXA_STATUS_WOULD_BLOCK){fb->pending=1;return 0;}
            if (uploaded != (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                                       (uint32_t)width * height)) return uploaded;
            if (!pxa_raster_sprite(&list, slot, 0, PXA_RASTER_CAP_TEXTURED_QUAD,
                    (int16_t)(x+fb->viewport.x), (int16_t)(y+fb->viewport.y), width, height,
                    0, 0, width, height, 0)) return PXA_STATUS_INTERNAL;
        }
    }
    int32_t status=pxa_raster_submit(fb->context,&list);
    if(status==PXA_STATUS_WOULD_BLOCK){fb->pending=1;return 0;}
    if(status>=0)fb->pending=0;
    return status;
}

static inline void pxfb_stop(pxfb_t *fb) {
    (void)pxa_clock_set_period(0);
    if (fb->context) (void)pxa_game_render_close(fb->context);
    fb->context = 0; fb->ready = 0; fb->buttons = 0;
}
#endif
