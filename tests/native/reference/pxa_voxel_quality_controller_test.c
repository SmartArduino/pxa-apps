#include <assert.h>

#include "quality_controller.h"

static void finish_warmup(voxel_quality_controller_t *controller) {
    unsigned index;
    for (index = 0; index < VOXEL_QUALITY_WARMUP_SAMPLES; ++index) {
        assert(voxel_quality_observe(controller, 16000, 0, 2, 1, 4) ==
               VOXEL_QUALITY_ACTION_NONE);
    }
}

static void test_sustained_raycast_cost_lowers_detail(void) {
    voxel_quality_controller_t controller;
    unsigned index;
    voxel_quality_controller_reset(&controller);
    finish_warmup(&controller);
    for (index = 1;
         index < VOXEL_QUALITY_WINDOW_SAMPLES *
                     VOXEL_QUALITY_DOWNGRADE_WINDOWS;
         ++index) {
        assert(voxel_quality_observe(&controller, 40000, 0, 2, 1, 4) ==
               VOXEL_QUALITY_ACTION_NONE);
    }
    assert(voxel_quality_observe(&controller, 40000, 0, 2, 1, 4) ==
           VOXEL_QUALITY_ACTION_LOWER_DETAIL);
}

static void test_display_wait_blocks_detail_upgrade(void) {
    voxel_quality_controller_t controller;
    unsigned index;
    voxel_quality_controller_reset(&controller);
    finish_warmup(&controller);
    for (index = 0; index < 40; ++index) {
        assert(voxel_quality_observe(
                   &controller, 5000,
                   VOXEL_QUALITY_BUFFER_WAIT_BLOCK_UPGRADE_US, 4, 1, 4) ==
               VOXEL_QUALITY_ACTION_NONE);
    }
}

static void test_stable_headroom_raises_detail(void) {
    voxel_quality_controller_t controller;
    unsigned index;
    voxel_quality_controller_reset(&controller);
    finish_warmup(&controller);
    for (index = 1;
         index < VOXEL_QUALITY_WINDOW_SAMPLES * VOXEL_QUALITY_UPGRADE_WINDOWS;
         ++index) {
        assert(voxel_quality_observe(&controller, 5000, 0, 4, 1, 4) ==
               VOXEL_QUALITY_ACTION_NONE);
    }
    assert(voxel_quality_observe(&controller, 5000, 0, 4, 1, 4) ==
           VOXEL_QUALITY_ACTION_HIGHER_DETAIL);
}

static void test_panic_and_cooldown(void) {
    voxel_quality_controller_t controller;
    unsigned index;
    voxel_quality_controller_reset(&controller);
    finish_warmup(&controller);
    for (index = 1; index < VOXEL_QUALITY_PANIC_SAMPLES; ++index) {
        assert(voxel_quality_observe(&controller, 100000, 0, 1, 1, 4) ==
               VOXEL_QUALITY_ACTION_NONE);
    }
    assert(voxel_quality_observe(&controller, 100000, 0, 1, 1, 4) ==
           VOXEL_QUALITY_ACTION_LOWER_DETAIL);
    for (index = 0; index < VOXEL_QUALITY_COOLDOWN_SAMPLES - 1u; ++index) {
        assert(voxel_quality_observe(&controller, 100000, 0, 2, 1, 4) ==
               VOXEL_QUALITY_ACTION_NONE);
    }
}

int main(void) {
    test_sustained_raycast_cost_lowers_detail();
    test_display_wait_blocks_detail_upgrade();
    test_stable_headroom_raises_detail();
    test_panic_and_cooldown();
    return 0;
}
