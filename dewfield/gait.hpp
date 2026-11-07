// SPDX-License-Identifier: MIT
// Included inside namespace art after the raster and sprite helpers.
#pragma once
namespace gait {
struct Limb {
  int swing, lift;
};
struct Pose {
  Limb legs[2], arms[2];
  int bob;
};
inline Pose pose(int frame, int dir = 0) {
  constexpr int swing[] = {3, 2, 0, -2, -3, -2, 0, 2};
  constexpr int lift[] = {0, 0, 0, 0, 0, 1, 2, 1};
  // Bind phase to anatomical left/right across turns. East's near limb and
  // south's screen-left limb are the farmer's right side.
  int p = (frame - 1 + ((dir == 1 || dir == 2) ? 4 : 0)) & 7;
  int q = (p + 4) & 7;
  return {{{swing[p], lift[p]}, {swing[q], lift[q]}},
          {{swing[q], 0}, {swing[p], 0}},
          p == 2 || p == 6 ? -1 : 0};
}
inline const sprites::Rig &rig(int dir) {
  const sprites::Rig *rigs[] = {&sprites::north_rig, &sprites::east_rig,
                               &sprites::south_rig, &sprites::west_rig};
  return *rigs[dir & 3];
}
inline int rounded_third(int n) { return n < 0 ? (n - 1) / 3 : (n + 1) / 3; }
struct Foot {
  int dx, ground, sole;
};
inline Foot foot(int dir, const Pose &p, int leg) {
  bool side = dir & 1;
  int offset = side && leg == 1 ? (dir == 1 ? 1 : -1) : 0;
  int dx = side ? (dir == 1 ? 1 : -1) * p.legs[leg].swing + offset
                : (leg ? 1 : -1) * (p.legs[leg].lift / 2);
  int ground = side ? 29 - leg
                    : 29 + rounded_third((dir == 2 ? 1 : -1) * p.legs[leg].swing);
  return {dx, ground, ground - p.legs[leg].lift};
}
inline void row(Raster &r, const sprites::Bone &bone, int src_y, int x, int y,
                bool far = false) {
  x -= r.ox;
  y -= r.oy;
  if (y < 0 || y >= r.h || src_y < 0 || src_y >= bone.sprite->h)
    return;
  const auto &s = *bone.sprite;
  for (int i = bone.rows[src_y]; i < bone.rows[src_y + 1]; ++i) {
    const auto &span = s.spans[i];
    int left = std::max(0, x + span.x);
    int right = std::min(r.w, x + span.x + span.count);
    for (int xx = left; xx < right; ++xx) {
      uint16_t color = s.pixels[span.offset + xx - x - span.x];
      if(far)color=blend(color,ShadowTint,1);
      r.p[y*r.w+xx]=r.tint?r.tint[color]:color;
    }
  }
}
inline void leg(Raster &r, const sprites::Rig &s, const Pose &p, int dir,
                int index, int x, int y) {
  Foot f = foot(dir, p, index);
  int top = s.hip + p.bob - (s.side ? index : 0);
  int boot = f.sole - 2;
  for (int yy = top; yy <= f.sole; ++yy) {
    // Boots retain their original three pixel height; only trouser rows bend.
    int sy = yy >= boot ? 27 + yy - boot
                        : s.hip + (yy - top) * (27-s.hip) / std::max(1, boot - top);
    int dx = f.dx * (yy - top) / std::max(1, boot - top);
    if (yy >= boot)
      dx = f.dx;
    row(r, *s.legs[index], sy, x + dx, y + yy, s.side && index == 1);
  }
}
inline void arm(Raster &r, const sprites::Rig &s, const Pose &p, int dir,
                int index, int x, int y) {
  bool far = s.side && index == 1;
  int heading = dir == 1 ? 1 : -1;
  int offset = far ? heading * 2 : 0;
  int top = 16 + p.bob - int(far);
  int end = s.arm_end[index];
  int dy = s.side ? -int(std::abs(p.arms[index].swing) == 3)
                  : rounded_third((dir == 2 ? 2 : -2) * p.arms[index].swing);
  int hand = end + p.bob - int(far) + dy;
  int dx = s.side ? heading * rounded_third(p.arms[index].swing * 2) : 0;
  for (int yy = top; yy <= hand; ++yy) {
    int sy = yy >= hand - 2 ? end - (hand - yy)
                           : 16 + (yy - top) * (end - 18) /
                                      std::max(1, hand - 2 - top);
    int shift = dx * std::min(yy - top, hand - 2 - top) /
                std::max(1, hand - 2 - top);
    row(r, *s.arms[index], sy, x + offset + shift, y + yy, far);
  }
}
inline void draw(Raster &r, int x, int y, int dir, int frame, bool carrying = false, const sprites::Rig *custom = nullptr) {
  const auto &s = custom ? *custom : rig(dir);
  Pose p = frame ? pose(frame, dir) : Pose{};
  x -= 12;
  y -= 30;
  if (carrying) {
    for (int side : {-1,1}) {
      r.line(x+12+side*5,y+16,x+12+side*9,y+9,Ink);
      r.line(x+12+side*9,y+9,x+12+side*6,y,Ink);
      r.line(x+12+side*5,y+17,x+12+side*8,y+9,rgb(152,153,103));
      r.line(x+12+side*8,y+9,x+12+side*6,y+1,rgb(226,180,131));
    }
  } else if (s.side)
    arm(r, s, p, dir, 1, x, y);
  else if (dir == 0) {
    arm(r, s, p, dir, 0, x, y);
    arm(r, s, p, dir, 1, x, y);
  }
  leg(r, s, p, dir, 1, x, y);
  leg(r, s, p, dir, 0, x, y);
  if (s.side) {
    // Reveal the shirt/overalls under the moving near arm, using source colors.
    r.rect(x + (dir == 1 ? 10 : 9), y + 16 + p.bob, 5, 7, s.cloth);
    r.rect(x + (dir == 1 ? 10 : 9), y + 16 + p.bob, 5, 2, s.shirt);
  }
  sprite(r, *s.body, x, y + p.bob);
  if (carrying) {
    r.line(x+6,y+1,x+10,y,rgb(226,180,131));
    r.line(x+18,y+1,x+14,y,rgb(226,180,131));
    r.rect(x+9,y,3,2,rgb(235,191,142));
    r.rect(x+13,y,3,2,rgb(235,191,142));
  } else if (s.side)
    arm(r, s, p, dir, 0, x, y);
  else if (dir == 2) {
    arm(r, s, p, dir, 0, x, y);
    arm(r, s, p, dir, 1, x, y);
  }
}
inline void shadow(Raster &r, int x, int y, int dir, int frame, const sprites::Rig *custom = nullptr) {
  const auto &s = custom ? *custom : rig(dir);
  Pose p = frame ? pose(frame, dir) : Pose{};
  // Union both contact profiles before blending to avoid dark overlaps.
  uint8_t coverage[3][32]{};
  for (int i = 0; i < 2; ++i) {
    if (p.legs[i].lift)
      continue;
    Foot f = foot(dir, p, i);
    const auto &b = *s.legs[i];
    for (int n = b.rows[29]; n < b.rows[30]; ++n) {
      const auto &span = b.sprite->spans[n];
      for (int xx = span.x - 1; xx <= span.x + span.count; ++xx) {
        int at = xx + f.dx + 4;
        int alpha = xx == span.x - 1 || xx == span.x + span.count ? 1 : 3;
        if (at >= 0 && at < 32)
          coverage[f.ground - 28][at] =
              std::max(coverage[f.ground - 28][at], uint8_t(alpha));
      }
    }
  }
  for (int yy = 0; yy < 3; ++yy)
    for (int xx = 0; xx < 32; ++xx)
      if (coverage[yy][xx])
        shadow_strip(r, x - 16 + xx, y - 1 + yy, 1, coverage[yy][xx]);
}
} // namespace gait
