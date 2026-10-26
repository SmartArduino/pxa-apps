#pragma once
#include <Arduboy2.h>

constexpr uint8_t TOP_LEFT_POINT     = (1 << 0);
constexpr uint8_t TOP_RIGHT_POINT    = (1 << 1);
constexpr uint8_t BOTTOM_LEFT_POINT  = (1 << 2);
constexpr uint8_t BOTTOM_RIGHT_POINT = (1 << 3);

constexpr uint8_t COLLIDE_RECT_MASKS[4] = {
    TOP_LEFT_POINT,
    TOP_RIGHT_POINT, 
    BOTTOM_LEFT_POINT,
    BOTTOM_RIGHT_POINT,
};

// constexpr uint8_t TOP_LEFT = TOP_LEFT_POINT;
enum class CollisionType : uint8_t {
    NONE = 0,
    TOP_LEFT = TOP_LEFT_POINT,
    TOP_RIGHT = TOP_RIGHT_POINT,
    BOTTOM_LEFT = BOTTOM_LEFT_POINT,
    BOTTOM_RIGHT = BOTTOM_RIGHT_POINT,
    TOP = TOP_LEFT_POINT | TOP_RIGHT_POINT,
    BOTTOM = BOTTOM_LEFT_POINT | BOTTOM_RIGHT_POINT,
    LEFT = TOP_LEFT_POINT | BOTTOM_LEFT_POINT,
    RIGHT = TOP_RIGHT_POINT | BOTTOM_RIGHT_POINT,
    FULL = TOP | BOTTOM,
};

CollisionType collide_rects(const Rect& a, const Rect& b) {
    uint8_t result = 0;
    if (Arduboy2::collide(Point(a.x, a.y), b)) {
        result |= COLLIDE_RECT_MASKS[0];
    }
    if (Arduboy2::collide(Point(a.x + a.width, a.y), b)) {
        result |= COLLIDE_RECT_MASKS[1];
    }
    if (Arduboy2::collide(Point(a.x, a.y + a.height), b)) {
        result |= COLLIDE_RECT_MASKS[2];
    }
    if (Arduboy2::collide(Point(a.x + a.width, a.y + a.height), b)) {
        result |= COLLIDE_RECT_MASKS[3];
    }
    return static_cast<CollisionType>(result);
}