#include "../input.h"

#include <cassert>
#include <cmath>

int main() {
    touch_controls_t touch{};
    touch_init(&touch, 296);

    // The reliable release must complete a drag whose last MOVE was omitted.
    touch_on_down(&touch, 0, 210, 120, 1000);
    touch_on_move(&touch, 0, 220, 120, 2000);
    touch_on_release(&touch, 0, 240, 120, 3000);
    auto controls = touch_consume(&touch, 4000);
    assert(std::abs(controls.turn - 30 * 0.0075F) < 0.00001F);
    assert(!controls.fire && !touch.look.down);
    touch_on_release(&touch, 0, 240, 120, 5000);
    controls = touch_consume(&touch, 6000);
    assert(controls.turn == 0 && !controls.fire);

    // An ordinary tap fires exactly once; cancellation never fires.
    touch_on_down(&touch, 0, 210, 120, 10000);
    touch_on_release(&touch, 0, 210, 120, 100000);
    assert(touch_consume(&touch, 100001).fire);
    assert(!touch_consume(&touch, 100002).fire);
    touch_on_down(&touch, 0, 210, 120, 200000);
    touch_on_cancel(&touch, 0);
    controls = touch_consume(&touch, 202000);
    assert(!controls.fire && controls.turn == 0 && !touch.look.down);

    // Cancelling one finger leaves the independent walking finger active.
    touch_on_down(&touch, 0, 70, 150, 300000);
    touch_on_move(&touch, 0, 70, 100, 301000);
    touch_on_down(&touch, 1, 220, 120, 302000);
    touch_on_cancel(&touch, 1);
    controls = touch_consume(&touch, 304000);
    assert(controls.forward > 0 && !controls.fire && touch.stick.down);
    touch_on_cancel(&touch, 0);
    controls = touch_consume(&touch, 306000);
    assert(controls.forward == 0 && controls.strafe == 0);

    // Sparse IDs are identities, not array indices. Fingers may cross the
    // screen centre without switching controls or stealing an occupied half.
    touch_on_down(&touch, 17, 70, 150, 400000);
    touch_on_down(&touch, 255, 220, 120, 401000);
    touch_on_down(&touch, 99, 40, 140, 402000);
    touch_on_move(&touch, 17, 170, 100, 403000);
    touch_on_move(&touch, 255, 130, 120, 404000);
    controls = touch_consume(&touch, 405000);
    assert(controls.forward > 0 && controls.strafe == 1);
    assert(std::abs(controls.turn + 90 * 0.0075F) < 0.00001F);
    assert(touch.stick.id == 17 && touch.look.id == 255 && !controls.fire);
    touch_on_release(&touch, 17, 170, 100, 406000);
    touch_on_move(&touch, 255, 120, 120, 407000);
    controls = touch_consume(&touch, 408000);
    assert(controls.forward == 0 && controls.turn < 0 && touch.look.down);
    touch_on_cancel(&touch, 255);

    // Duplicate/replayed DOWN for one ID cannot acquire both halves.
    touch_on_down(&touch, 17, 70, 150, 500000);
    touch_on_down(&touch, 17, 220, 120, 501000);
    assert(touch.stick.down && !touch.look.down);
    touch_on_release(&touch, 17, 70, 150, 502000);
    assert(!touch.stick.down && !touch.look.down);
    touch_on_down(&touch, 17, 220, 120, 503000);
    touch_on_release(&touch, 17, 220, 120, 504000);
    assert(touch_consume(&touch, 505000).fire);
}
