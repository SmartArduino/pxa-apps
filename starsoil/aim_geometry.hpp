// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace starsoil_aim {
struct Vector { float x, y; };
inline Vector unit(float x, float y, bool face = true) {
  float length = std::sqrt(x*x+y*y);
  if (!std::isfinite(length) || length < .001f) return {face ? 1.f : -1.f, 0};
  return {x/length, y/length};
}
// Centre of the authored upper-body rig; mirrored about the half-pixel axis.
inline int bob(bool grounded,float vx,uint32_t phase) {
  return grounded && std::fabs(vx)>1 ? int((phase/512)%2) : 0;
}
inline Vector origin(float x, float feet,int bob=0) { return {x-.5f,feet-16+bob}; }
inline Vector direction(float x, float feet, float target_x, float target_y, bool face,int bob=0) {
  auto at=origin(x,feet,bob);return unit(target_x-at.x,target_y-at.y,face);
}
inline Vector elbow(Vector shoulder, Vector hand, float upper, float lower) {
  float dx=hand.x-shoulder.x,dy=hand.y-shoulder.y;
  float d=std::max(.001f,std::sqrt(dx*dx+dy*dy));
  float along=(upper*upper-lower*lower+d*d)/(2*d);
  float across=std::sqrt(std::max(0.f,upper*upper-along*along));
  return {shoulder.x+(dx*along-dy*across)/d,
          shoulder.y+(dy*along+dx*across)/d};
}
struct Bow {
  Vector direction, support, string, rear_elbow, camera_elbow;
};
inline Bow bow(float dx,float dy,int bob,int draw) {
  auto d=unit(dx,dy);
  Vector support{std::round(9.5f+6*d.x),std::round(16.f+bob+6*d.y)};
  float pull=4.f+3.f*std::clamp(draw,0,256)/256;
  Vector string{std::round(support.x-pull*d.x),std::round(support.y-pull*d.y)};
  return {d,support,string,elbow({12,15.f+bob},support,3.5f,3.8f),
          elbow({8,14.f+bob},string,4.25f,4.25f)};
}
} // namespace starsoil_aim
