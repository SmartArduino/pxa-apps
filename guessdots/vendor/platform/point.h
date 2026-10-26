#pragma once

template <typename T>
struct Point2D {
  T x;
  T y;
};

using PointU8 = Point2D<uint8_t>;
using PointS8 = Point2D<int8_t>;