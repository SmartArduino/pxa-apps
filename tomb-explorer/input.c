#include "input.h"

#define TOMB_STICK_RADIUS_MIN 24
#define TOMB_STICK_RADIUS 70 /* panel pixels for full deflection at 480 wide */
#define TOMB_PANEL_REFERENCE_WIDTH 480
#define TOMB_STICK_DEADZONE 8
#define TOMB_ORBIT_PER_PIXEL 0.008f /* radians */
#define TOMB_TILT_PER_PIXEL 0.005f
#define TOMB_TAP_TRAVEL 12
#define TOMB_TAP_MAX_US UINT64_C(240000)

typedef struct {
    int down;
    uint32_t id;
    int x, y;
    int origin_x, origin_y;
    int travel;
    uint64_t down_at_us;
} tomb_finger_t;

static int g_half_width = 240;
static int g_stick_radius = TOMB_STICK_RADIUS;
static tomb_finger_t g_stick;
static tomb_finger_t g_look;
static float g_orbit_accum;
static float g_tilt_accum;
static int g_jump_pending;
static uint32_t g_buttons_previous;

static int abs_int(int value) { return value < 0 ? -value : value; }

static float deflection(int delta) {
    if (abs_int(delta) < TOMB_STICK_DEADZONE) return 0.0f;
    const float value = (float)delta / (float)g_stick_radius;
    return value > 1.0f ? 1.0f : (value < -1.0f ? -1.0f : value);
}

void tomb_input_init(int panel_width) {
    g_half_width = panel_width / 2;
    g_stick_radius = panel_width * TOMB_STICK_RADIUS / TOMB_PANEL_REFERENCE_WIDTH;
    if (g_stick_radius < TOMB_STICK_RADIUS_MIN) g_stick_radius = TOMB_STICK_RADIUS_MIN;
    g_stick = (tomb_finger_t){0};
    g_look = (tomb_finger_t){0};
    g_orbit_accum = 0.0f;
    g_tilt_accum = 0.0f;
    g_jump_pending = 0;
    g_buttons_previous = 0u;
}

void tomb_input_pointer(const pxa_ui_pointer_data_t *pointer) {
    const uint64_t now_us = pointer->timestamp_us;
    const uint32_t id = pointer->pointer_id;
    const int x = pointer->x;
    const int y = pointer->y;
    if (pointer->phase == PXA_POINTER_DOWN) {
        tomb_finger_t *finger = x < g_half_width ? &g_stick : &g_look;
        if (finger->down) return; /* one finger per half */
        finger->down = 1;
        finger->id = id;
        finger->x = x;
        finger->y = y;
        finger->origin_x = x;
        finger->origin_y = y;
        finger->travel = 0;
        finger->down_at_us = now_us;
        return;
    }
    if (pointer->phase == PXA_POINTER_MOVE) {
        if (g_stick.down && g_stick.id == id) {
            g_stick.x = x;
            g_stick.y = y;
        } else if (g_look.down && g_look.id == id) {
            const int dx = x - g_look.x;
            const int dy = y - g_look.y;
            g_look.travel += abs_int(dx) + abs_int(dy);
            g_orbit_accum += (float)dx * TOMB_ORBIT_PER_PIXEL;
            g_tilt_accum -= (float)dy * TOMB_TILT_PER_PIXEL;
            g_look.x = x;
            g_look.y = y;
        }
        return;
    }
    /* PXA_POINTER_UP */
    if (g_stick.down && g_stick.id == id) {
        g_stick.down = 0;
    } else if (g_look.down && g_look.id == id) {
        if (g_look.travel < TOMB_TAP_TRAVEL &&
            now_us - g_look.down_at_us < TOMB_TAP_MAX_US) {
            g_jump_pending = 1;
        }
        g_look.down = 0;
    }
}

void tomb_input_button(uint32_t buttons) {
    const uint32_t pressed = buttons & ~g_buttons_previous;
    if ((pressed & PXA_CONTROLLER_A) != 0u) g_jump_pending = 1;
    g_buttons_previous = buttons;
}

tomb_controls_t tomb_input_consume(void) {
    tomb_controls_t controls;
    controls.forward = 0.0f;
    controls.strafe = 0.0f;
    controls.orbit = g_orbit_accum;
    controls.tilt = g_tilt_accum;
    controls.jump = g_jump_pending;
    if (g_stick.down) {
        controls.forward = -deflection(g_stick.y - g_stick.origin_y);
        controls.strafe = deflection(g_stick.x - g_stick.origin_x);
    }
    g_orbit_accum = 0.0f;
    g_tilt_accum = 0.0f;
    g_jump_pending = 0;
    return controls;
}

tomb_stick_overlay_t tomb_input_overlay(void) {
    tomb_stick_overlay_t overlay;
    overlay.stick_active = g_stick.down;
    overlay.origin_x = g_stick.origin_x;
    overlay.origin_y = g_stick.origin_y;
    overlay.x = g_stick.x;
    overlay.y = g_stick.y;
    return overlay;
}
