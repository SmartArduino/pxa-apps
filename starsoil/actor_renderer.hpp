// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include "items.hpp"
#include <algorithm>
#include <cstdint>

// The game and native anatomy checks use this exact compositor.
namespace starsoil_actor {
enum Pose : uint8_t { Walk, Carry, BowAim, ToolUse };
enum class Tag : uint8_t {
  Empty,
  Body,
  Neck,
  Clothing,
  Armour,
  Greaves,
  Boots,
  Cape,
  Headgear,
  Arm,
  Sleeve,
  Weapon,
  Grip,
  Accessory,
  RearArm,
  RearSleeve,
  RearLeg,
  CameraLeg,
  RearGreaves,
  RearBoots
};
enum Joint {
  Hand,
  Offhand,
  Neck,
  Shoulder,
  FarShoulder,
  Elbow,
  FarElbow,
  FarFoot,
  NearFoot,
  Pendant,
  FarHip,
  NearHip,
  FarKnee,
  NearKnee,
  FarAnkle,
  NearAnkle
};
// One anatomical identity in every pose: camera-side hand operates the tool or
// draws the bowstring; the rear hand supports the bow. Mirror the actor once.
constexpr Joint CameraHand = Hand, RearHand = Offhand;
constexpr Joint CameraShoulder = Shoulder, RearShoulder = FarShoulder;
constexpr Joint CameraElbow = Elbow, RearElbow = FarElbow;
struct Point {
  int x, y;
};
struct Appearance {
  int x = 0, feet = 32, frame = art::StandFrame, scale = 1, light = 255,
      skin = 0;
  bool face = true, using_item = false;
  const uint8_t *equipment = nullptr;
  uint8_t held = 0;
  int action_frame = -1;
};
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
constexpr uint16_t color(uint32_t c) {
  return rgb(c >> 16, (c >> 8) & 255, c & 255);
}
inline uint16_t dim(uint16_t c, int v) {
  v = std::clamp(v, 0, 255);
  return uint16_t(((((c >> 11) & 31) * v / 255) << 11) |
                  ((((c >> 5) & 63) * v / 255) << 5) | ((c & 31) * v / 255));
}
inline uint16_t mix(uint16_t a, uint16_t b, int k) {
  return uint16_t(
      (((((a >> 11) & 31) * (255 - k) + ((b >> 11) & 31) * k) / 255) << 11) |
      (((((a >> 5) & 63) * (255 - k) + ((b >> 5) & 63) * k) / 255) << 5) |
      (((a & 31) * (255 - k) + (b & 31) * k) / 255));
}
inline Pose pose(uint8_t item, bool active) {
  auto kind = starsoil::items[item].kind;
  return kind == starsoil::Bow ? BowAim
         : active && (kind == starsoil::Pick || kind == starsoil::Axe ||
                      kind == starsoil::Sword || kind == starsoil::Wand)
             ? ToolUse
         : item ? Carry
                : Walk;
}
struct ActionClock {
  uint16_t elapsed = 0;
  uint8_t item = 0;
  bool active = false;
  static uint16_t duration(uint8_t id) {
    auto kind = starsoil::items[id].kind;
    return kind == starsoil::Sword ? 240 : kind == starsoil::Wand ? 320 : 480;
  }
  void step(bool pressed, uint8_t held, uint32_t ms) {
    if (held != item) {
      item = held;
      elapsed = 0;
      active = false;
    }
    bool tool = pose(item, true) == ToolUse;
    if (!active) {
      if (pressed && tool) {
        active = true;
        elapsed = 0;
      }
      return;
    }
    elapsed = uint16_t(elapsed + ms);
    if (elapsed >= duration(item)) {
      if (pressed && tool)
        elapsed %= duration(item);
      else {
        active = false;
        elapsed = 0;
      }
    }
  }
  int frame() const {
    return active ? std::min(art::WalkFrames - 1,
                             int(elapsed) * art::WalkFrames / duration(item))
                  : 0;
  }
};
inline Point joint(Pose p, int frame, Joint j) {
  const auto *r = art::anchors[int(p) * art::HumanFrames +
                               std::clamp(frame, 0, art::HumanFrames - 1)];
  return {r[int(j) * 2], r[int(j) * 2 + 1]};
}
inline int body_frame(const Appearance &a) {
  return std::clamp(a.frame, 0, art::HumanFrames - 1);
}
inline int action_frame(const Appearance &a) {
  return pose(a.held, a.using_item) == ToolUse
             ? (a.action_frame < 0 ? body_frame(a) : a.action_frame) %
                   art::WalkFrames
             : body_frame(a);
}
inline int arm_bob(const Appearance &a) {
  return pose(a.held, a.using_item) == ToolUse ? art::body_bob[0][body_frame(a)]
                                               : 0;
}
inline Point joint(const Appearance &a, Joint j) {
  bool limb = int(j) <= int(FarElbow) && j != Neck;
  Point v = limb ? joint(pose(a.held, a.using_item), action_frame(a), j)
                 : joint(Walk, body_frame(a), j);
  if (limb)
    v.y += arm_bob(a);
  return v;
}
inline int locomotion_frame(bool grounded, bool moving, uint32_t phase) {
  return !grounded ? art::JumpFrame
         : moving  ? int(phase % art::WalkFrames)
                   : art::StandFrame;
}
inline Point world(const Appearance &a, Point p) {
  return {
      a.x + ((a.face ? p.x : art::HumanWidth - 1 - p.x) - art::HumanWidth / 2) *
                a.scale,
      a.feet - art::HumanHeight * a.scale + p.y * a.scale};
}
template <class Plot> void draw(const Appearance &a, Plot plot) {
  using namespace starsoil;
  if (!a.equipment || a.scale < 1 || a.scale > 4)
    return;
  constexpr uint16_t ink = color(0xeee3c9), gold = color(0xe5bd73),
                     teal = color(0x71b8a6);
  const int frame = body_frame(a), arm_frame = action_frame(a),
            arm_dy = arm_bob(a);
  const Pose p = pose(a.held, a.using_item);
  const auto *eq = a.equipment;
  const uint32_t shirt = eq[3] ? items[eq[3]].color : 0x527d91;
  const uint16_t base[] = {0,
                           color(0x2a252a),
                           color(a.skin == 1   ? 0xd69b73
                                 : a.skin == 2 ? 0xe7b8a0
                                               : 0xf7ac7d),
                           color(a.skin == 1   ? 0xaa674a
                                 : a.skin == 2 ? 0xbb8979
                                               : 0xce7f5c),
                           color(a.skin == 2   ? 0x777078
                                 : a.skin == 1 ? 0x31303a
                                               : 0x4f302b),
                           color(a.skin == 2   ? 0xb7a898
                                 : a.skin == 1 ? 0x61606b
                                               : 0x985b39),
                           dim(color(shirt), 130),
                           color(shirt),
                           mix(color(shirt), ink, 95),
                           color(0x343b4c),
                           color(0x596a7e),
                           color(0x593e32),
                           color(0xffd09b),
                           color(0x9b6039),
                           color(0xf5e6c2),
                           color(a.skin == 2   ? 0xe3cfb0
                                 : a.skin == 1 ? 0x96929d
                                               : 0xc78445)};
  constexpr int RearArmTone = 232;
  uint16_t rear_base[16];
  for (int i = 0; i < 16; ++i)
    rear_base[i] = i == 1 ? base[i] : dim(base[i], RearArmTone);
  auto dot = [&](Point at, uint16_t c, Tag tag, uint8_t index = 0) {
    Point v = world(a, at);
    for (int sy = 0; sy < a.scale; ++sy)
      for (int sx = 0; sx < a.scale; ++sx)
        plot(v.x + sx, v.y + sy, dim(c, a.light), tag, index);
  };
  auto indexed = [&](const uint8_t *data, const uint16_t *palette, Tag tag,
                     int dy = 0) {
    for (int y = 0; y < art::HumanHeight; ++y)
      for (int x = 0; x < art::HumanWidth; ++x) {
        uint8_t v = data[y * art::HumanWidth + x];
        if (tag == Tag::Body && eq[0] && y < joint(a, Neck).y) {
          int bob = joint(a, Neck).y - 13;
          if (eq[0] == StrawHat) {
            if (y < 6 + bob)
              continue;
          } else {
            bool skin = v == 2 || v == 3 || v == 12 || v == 14;
            bool face = x >= 10 && x <= 15 && y >= 8 + bob;
            if (!skin && !face)
              continue;
          }
        }
        if (v)
          dot({x, y + dy}, palette[v], tag, v);
      }
  };
  auto data = [&](int part) {
    return art::humans[art::human_rows[part][frame]];
  };
  auto arm_data = [&](int part) {
    return art::humans[art::human_rows[int(p) * art::HumanParts + part]
                                      [arm_frame]];
  };
  auto garment = [&](int part, uint32_t tint, Tag tag, int helmet = 0,
                     const uint8_t *source = nullptr, int tone = 255,
                     int dy = 0) {
    uint16_t c = color(tint);
    uint16_t palette[] = {0,
                          color(0x2a252a),
                          dim(c, 135),
                          c,
                          mix(c, ink, 105),
                          gold,
                          mix(c, ink, 45),
                          mix(c, teal, 95),
                          ink,
                          color(0x654331),
                          color(0xb47c4b)};
    for (auto &v : palette)
      v = dim(v, tone);
    indexed(source   ? source
            : helmet ? art::helmets[(helmet - 1) * art::HumanFrames + frame]
                     : data(part),
            palette, tag, dy);
  };
  if (eq[4])
    garment(5, items[eq[4]].color, Tag::Cape);
  indexed(arm_data(13), rear_base, Tag::RearArm, arm_dy);
  garment(eq[1] ? 15 : 14, eq[1] ? items[eq[1]].color : shirt, Tag::RearSleeve,
          0, arm_data(eq[1] ? 15 : 14), RearArmTone, arm_dy);
  // Assemble each leg with its own worn items before the camera leg covers it.
  indexed(data(17), rear_base, Tag::RearLeg);
  if (eq[2])
    garment(18, items[eq[2]].color, Tag::RearGreaves, 0, nullptr, RearArmTone);
  if (eq[5])
    garment(19, items[eq[5]].color, Tag::RearBoots, 0, nullptr, RearArmTone);
  indexed(data(16), base, Tag::CameraLeg);
  if (eq[2])
    garment(4, items[eq[2]].color, Tag::Greaves);
  if (eq[5])
    garment(6, items[eq[5]].color, Tag::Boots);
  indexed(data(0), base, Tag::Body);
  if (eq[3])
    garment(1, shirt, Tag::Clothing, 0,
            art::outfits[((eq[3] == SageRobe   ? 2
                           : eq[3] == RedTunic ? 1
                                               : 0)) *
                             art::HumanFrames +
                         frame]);
  if (eq[1])
    garment(3, items[eq[1]].color, Tag::Armour, 0,
            art::mail[(items[eq[1]].tier - 1) * art::HumanFrames + frame]);
  // The exposed neck joins jaw and collar after clothing is composited.
  indexed(data(9), base, Tag::Neck);
  if (eq[0])
    garment(eq[0] == StrawHat ? 7 : 2, items[eq[0]].color, Tag::Headgear,
            eq[0] == StrawHat ? 0 : items[eq[0]].tier);
  if (eq[7]) {
    Point at = joint(a, Pendant);
    dot({10, at.y - 3}, gold, Tag::Accessory);
    dot({10, at.y - 2}, gold, Tag::Accessory);
    dot({11, at.y - 1}, gold, Tag::Accessory);
    uint16_t c = color(items[eq[7]].color);
    dot({at.x - 1, at.y}, dim(c, 170), Tag::Accessory);
    dot(at, c, Tag::Accessory);
    dot({at.x + 1, at.y}, c, Tag::Accessory);
    dot({at.x, at.y + 1}, mix(c, ink, 100), Tag::Accessory);
  }
  // The camera-facing item arm is always in front of the trunk.
  indexed(arm_data(10), base, Tag::Arm, arm_dy);
  garment(eq[1] ? 12 : 11, eq[1] ? items[eq[1]].color : shirt, Tag::Sleeve, 0,
          arm_data(eq[1] ? 12 : 11), 255, arm_dy);
  if (a.held) {
    const auto h = joint(a, p == BowAim ? Offhand : Hand);
    const auto *grip = art::item_grips[a.held];
    constexpr int cos256[8] = {256, 181, 128, 240, 222, 0, 181, 256};
    constexpr int sin256[8] = {0, -181, -222, -88, 128, 256, 181, 0};
    auto rotate = [&](int v) {
      return v >= 0 ? (v + 128) / 256 : -((-v + 128) / 256);
    };
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x) {
        uint16_t c = art::icons[a.held][y * 16 + x];
        if (c) {
          int dx = x - grip[0], dy = y - grip[1];
          if (p == ToolUse) {
            int rx = rotate(dx * cos256[arm_frame] - dy * sin256[arm_frame]);
            dy = rotate(dx * sin256[arm_frame] + dy * cos256[arm_frame]);
            dx = rx;
          }
          dot({h.x + dx, h.y + dy}, c, Tag::Weapon);
        }
      }
    if (p == BowAim) {
      // A nocked arrow extends beyond the bow, with the string hand behind it.
      Point off = joint(a, Hand);
      for (int x = off.x; x < h.x + 7; ++x)
        dot({x, h.y}, color(0xd6c9a5), Tag::Weapon);
      dot({h.x + 7, h.y}, color(0xb6c6ca), Tag::Weapon);
      dot({h.x + 6, h.y - 1}, color(0xb6c6ca), Tag::Weapon);
      dot({h.x + 6, h.y + 1}, color(0xb6c6ca), Tag::Weapon);
    }
    auto palm = [&](Point v, bool rear) {
      const uint16_t *skin = rear ? rear_base : base;
      dot({v.x - 1, v.y - 1}, skin[12], Tag::Grip);
      dot({v.x, v.y - 1}, skin[2], Tag::Grip);
      dot({v.x - 1, v.y}, skin[2], Tag::Grip);
      dot(v, skin[2], Tag::Grip);
    };
    if (p == BowAim)
      palm(joint(a, Offhand), true);
    palm(joint(a, Hand), false);
  }
}
} // namespace starsoil_actor
