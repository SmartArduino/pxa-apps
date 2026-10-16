#pragma once
#include <algorithm>

namespace voxel {
// Digital keys start gently for aiming, then reach full turning speed in 180 ms.
// Integrate the ramp so camera motion does not depend on the render frame rate.
class ControllerLookAxis {
    float held_ = 0;
    int direction_ = 0;
public:
    void reset() noexcept { held_ = 0; direction_ = 0; }
    float step(int direction, float dt, float rate) noexcept {
        direction = std::clamp(direction, -1, 1);
        if (!direction) { reset(); return 0; }
        if (direction != direction_) { held_ = 0; direction_ = direction; }
        constexpr float ramp = .18f, initial = .35f;
        const auto integral = [ramp,initial](float t) {
            const float r = std::min(t, ramp);
            return initial*r+(1-initial)*r*r/(2*ramp)+std::max(0.f,t-ramp);
        };
        const float angle = direction*rate*(integral(held_+dt)-integral(held_));
        held_ = std::min(ramp, held_+dt);
        return angle;
    }
};
}
