#include "input.h"

#define STICK_RADIUS 70 /* panel pixels for full deflection */
#define STICK_DEADZONE 10
#define TURN_PER_PIXEL 0.0075F /* radians */
#define TAP_TRAVEL 14
#define TAP_MAX_US UINT64_C(260000)

static int abs_int(int value) { return value < 0 ? -value : value; }

static float deflection(int delta) {
    float value;
    if (abs_int(delta) < STICK_DEADZONE) {
        return 0.0F;
    }
    value = (float)delta / (float)STICK_RADIUS;
    return value > 1.0F ? 1.0F : (value < -1.0F ? -1.0F : value);
}

static void release(touch_controls_t *controls, touch_finger_t *finger,
                    uint64_t now_us) {
    if (finger == &controls->look && finger->down &&
        finger->travel < TAP_TRAVEL &&
        now_us - finger->down_at_us < TAP_MAX_US) {
        controls->tap_fired = 1;
    }
    finger->down = 0;
}

void touch_init(touch_controls_t *controls, int view_width) {
    controls->half_width = view_width / 2;
    controls->turn_accum = 0.0F;
    controls->tap_fired = 0;
    controls->stick.down = 0;
    controls->look.down = 0;
}

void touch_on_down(touch_controls_t *controls, uint32_t id, int x, int y,
                   uint64_t now_us) {
    touch_finger_t *finger =
        x < controls->half_width ? &controls->stick : &controls->look;
    if (finger->down) {
        return; /* one finger per half */
    }
    finger->down = 1;
    finger->id = id;
    finger->x = x;
    finger->y = y;
    finger->origin_x = x;
    finger->origin_y = y;
    finger->down_at_us = now_us;
    finger->travel = 0;
}

void touch_on_move(touch_controls_t *controls, uint32_t id, int x, int y,
                   uint64_t now_us) {
    (void)now_us;
    if (controls->stick.down && controls->stick.id == id) {
        controls->stick.x = x;
        controls->stick.y = y;
    } else if (controls->look.down && controls->look.id == id) {
        const int dx = x - controls->look.x;
        controls->look.travel += abs_int(dx) + abs_int(y - controls->look.y);
        controls->turn_accum += (float)dx * TURN_PER_PIXEL;
        controls->look.x = x;
        controls->look.y = y;
    }
}

void touch_on_up(touch_controls_t *controls, uint32_t id, uint64_t now_us) {
    if (controls->stick.down && controls->stick.id == id) {
        release(controls, &controls->stick, now_us);
    } else if (controls->look.down && controls->look.id == id) {
        release(controls, &controls->look, now_us);
    }
}

controls_t touch_consume(touch_controls_t *controls, uint64_t now_us) {
    controls_t out;
    (void)now_us;
    out.forward = 0.0F;
    out.strafe = 0.0F;
    out.turn = controls->turn_accum;
    controls->turn_accum = 0.0F;

    if (controls->stick.down) {
        out.forward =
            -deflection(controls->stick.y - controls->stick.origin_y);
        out.strafe = deflection(controls->stick.x - controls->stick.origin_x);
    }

    // Firing on a stationary hold makes a two-finger look drag ambiguous:
    // users commonly touch first and begin dragging a moment later. Fire only
    // after an unambiguous short tap has been released.
    out.fire = controls->tap_fired;
    controls->tap_fired = 0;
    return out;
}
