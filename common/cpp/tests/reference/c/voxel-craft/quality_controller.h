#ifndef VOXEL_CRAFT_QUALITY_CONTROLLER_H
#define VOXEL_CRAFT_QUALITY_CONTROLLER_H

#include <stdint.h>

#define VOXEL_QUALITY_SAMPLE_CAP_US UINT32_C(250000)
#define VOXEL_QUALITY_DOWNGRADE_US UINT32_C(26000)
#define VOXEL_QUALITY_UPGRADE_US UINT32_C(12000)
#define VOXEL_QUALITY_PANIC_US UINT32_C(80000)
#define VOXEL_QUALITY_BUFFER_WAIT_BLOCK_UPGRADE_US UINT32_C(10000)
#define VOXEL_QUALITY_SAMPLE_INTERVAL_FRAMES UINT8_C(8)
#define VOXEL_QUALITY_WINDOW_SAMPLES UINT8_C(4)
#define VOXEL_QUALITY_WARMUP_SAMPLES UINT8_C(2)
#define VOXEL_QUALITY_DOWNGRADE_WINDOWS UINT8_C(2)
#define VOXEL_QUALITY_UPGRADE_WINDOWS UINT8_C(5)
/* One sample is taken every eight 33 ms frames. Eight samples keep quality
 * switches at least about 2.1 seconds apart at the target frame rate. */
#define VOXEL_QUALITY_COOLDOWN_SAMPLES UINT8_C(8)
#define VOXEL_QUALITY_PANIC_SAMPLES UINT8_C(3)

typedef enum {
    VOXEL_QUALITY_ACTION_NONE = 0,
    VOXEL_QUALITY_ACTION_LOWER_DETAIL,
    VOXEL_QUALITY_ACTION_HIGHER_DETAIL,
} voxel_quality_action_t;

typedef struct {
    uint32_t raycast_ema_us;
    uint32_t raycast_max_us;
    uint8_t window_samples;
    uint8_t warmup_samples;
    uint8_t bad_windows;
    uint8_t good_windows;
    uint8_t panic_samples;
    uint8_t cooldown_samples;
} voxel_quality_controller_t;

static inline void voxel_quality_controller_reset(
    voxel_quality_controller_t *controller) {
    *controller = (voxel_quality_controller_t){0};
    controller->warmup_samples = VOXEL_QUALITY_WARMUP_SAMPLES;
}

static inline void voxel_quality_controller_reset_windows(
    voxel_quality_controller_t *controller) {
    controller->window_samples = 0;
    controller->bad_windows = 0;
    controller->good_windows = 0;
    controller->panic_samples = 0;
}

static inline voxel_quality_action_t voxel_quality_observe(
    voxel_quality_controller_t *controller, uint64_t raycast_us,
    uint32_t buffer_wait_ema_us, uint8_t current_quality,
    uint8_t minimum_quality, uint8_t maximum_quality) {
    uint32_t sample;
    voxel_quality_action_t action = VOXEL_QUALITY_ACTION_NONE;
    if (raycast_us > VOXEL_QUALITY_SAMPLE_CAP_US)
        raycast_us = VOXEL_QUALITY_SAMPLE_CAP_US;
    sample = (uint32_t)raycast_us;
    if (controller->warmup_samples != 0) {
        --controller->warmup_samples;
        return VOXEL_QUALITY_ACTION_NONE;
    }
    if (controller->raycast_ema_us == 0) {
        controller->raycast_ema_us = sample;
    } else {
        controller->raycast_ema_us =
            (controller->raycast_ema_us * 7u + sample) / 8u;
    }
    if (sample > controller->raycast_max_us)
        controller->raycast_max_us = sample;
    if (sample >= VOXEL_QUALITY_PANIC_US) {
        if (controller->panic_samples != UINT8_MAX)
            ++controller->panic_samples;
    } else {
        controller->panic_samples = 0;
    }
    if (controller->cooldown_samples != 0)
        --controller->cooldown_samples;
    if (controller->panic_samples >= VOXEL_QUALITY_PANIC_SAMPLES &&
        controller->cooldown_samples == 0 &&
        current_quality < maximum_quality) {
        action = VOXEL_QUALITY_ACTION_LOWER_DETAIL;
    } else if (++controller->window_samples >=
               VOXEL_QUALITY_WINDOW_SAMPLES) {
        controller->window_samples = 0;
        if (controller->raycast_ema_us > VOXEL_QUALITY_DOWNGRADE_US) {
            if (controller->bad_windows != UINT8_MAX)
                ++controller->bad_windows;
            controller->good_windows = 0;
        } else if (controller->raycast_ema_us <
                       VOXEL_QUALITY_UPGRADE_US &&
                   buffer_wait_ema_us <
                       VOXEL_QUALITY_BUFFER_WAIT_BLOCK_UPGRADE_US) {
            if (controller->good_windows != UINT8_MAX)
                ++controller->good_windows;
            controller->bad_windows = 0;
        } else {
            controller->bad_windows = 0;
            controller->good_windows = 0;
        }
        if (controller->cooldown_samples == 0 &&
            controller->bad_windows >= VOXEL_QUALITY_DOWNGRADE_WINDOWS &&
            current_quality < maximum_quality) {
            action = VOXEL_QUALITY_ACTION_LOWER_DETAIL;
        } else if (controller->cooldown_samples == 0 &&
                   controller->good_windows >=
                       VOXEL_QUALITY_UPGRADE_WINDOWS &&
                   current_quality > minimum_quality) {
            action = VOXEL_QUALITY_ACTION_HIGHER_DETAIL;
        }
    }
    if (action != VOXEL_QUALITY_ACTION_NONE) {
        voxel_quality_controller_reset_windows(controller);
        controller->cooldown_samples = VOXEL_QUALITY_COOLDOWN_SAMPLES;
    }
    return action;
}

#endif
