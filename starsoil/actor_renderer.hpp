// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
#include "color_math.hpp"
#include "indexed_sprite.hpp"
#include "items.hpp"
#include "aim_geometry.hpp"
#include <algorithm>
#include <cstdlib>
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
  bool aimed = false;
  float aim_x = 1, aim_y = 0;
  int bow_draw = 256;
  bool bow_loaded = true;
};
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
constexpr uint16_t color(uint32_t c) {
  return rgb(c >> 16, (c >> 8) & 255, c & 255);
}
inline uint16_t dim(uint16_t c, int v) {
  v = std::clamp(v, 0, 255);
  return starsoil_color::dim(c, unsigned(v));
}
inline uint16_t mix(uint16_t a, uint16_t b, int k) {
  return starsoil_color::mix(a, b, unsigned(k));
}
inline Pose pose(uint8_t item, bool active) {
  auto kind = starsoil::items[item].kind;
  return kind == starsoil::Bow ? BowAim
         : active && (starsoil::tool(kind) ||
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
  if(a.aimed && (j==Hand || j==Offhand || j==Elbow || j==FarElbow) &&
     pose(a.held,a.using_item)==BowAim) {
    auto b=starsoil_aim::bow(a.face?a.aim_x:-a.aim_x,a.aim_y,
                            art::body_bob[0][body_frame(a)],a.bow_draw);
    auto point=[](starsoil_aim::Vector v) { return Point{int(std::lround(v.x)),int(std::lround(v.y))}; };
    if(j==Hand)return point(b.string);
    if(j==Offhand)return point(b.support);
    if(j==Elbow)return point(b.camera_elbow);
    if(j==FarElbow)return point(b.rear_elbow);
  }
  bool limb = int(j) <= int(FarElbow) && j != Neck;
  Point v = limb ? joint(pose(a.held, a.using_item), action_frame(a), j)
                 : joint(Walk, body_frame(a), j);
  if (limb)
    v.y += arm_bob(a);
  return v;
}
inline int locomotion_frame(bool grounded, bool moving, uint32_t phase, float vy=-100) {
  return !grounded ? (vy < -65 ? art::JumpFrame : vy > 65 ? art::FallFrame : art::ApexFrame)
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
  auto indexed = [&](IndexedImage sprite, const uint16_t *palette, Tag tag,
                     int dy = 0) {
    const int neck_y = joint(a, Neck).y, bob = neck_y - 13;
    for (int i = 0; i < sprite.count; ++i) {
        uint16_t command = sprite.pixels[i];
        int x = command & 31, y = (command >> 5) & 31;
        uint8_t v = uint8_t(command >> 10);
        if (tag == Tag::Body && eq[0] && y < neck_y) {
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
        dot({x, y + dy}, palette[v], tag, v);
      }
  };
  auto stroke=[&](Point from,Point to,uint16_t c,Tag tag,int width=1,uint8_t index=0) {
    int dx=std::abs(to.x-from.x),sx=from.x<to.x?1:-1;
    int dy=-std::abs(to.y-from.y),sy=from.y<to.y?1:-1,error=dx+dy;
    for(;;) {
      for(int y=-(width/2);y<width-width/2;++y)
        for(int x=-(width/2);x<width-width/2;++x)dot({from.x+x,from.y+y},c,tag,index);
      if(from.x==to.x && from.y==to.y)break;
      int e=2*error;if(e>=dy){error+=dy;from.x+=sx;}if(e<=dx){error+=dx;from.y+=sy;}
    }
  };
  const bool aiming=p==BowAim && a.aimed;
  auto aimed_arm=[&](bool rear,bool fore_only=false) {
    Point sh=joint(a,rear?FarShoulder:Shoulder),el=joint(a,rear?FarElbow:Elbow),
          hand=joint(a,rear?Offhand:Hand);
    const auto *skin=rear?rear_base:base;
    const Tag arm=rear?Tag::RearArm:Tag::Arm,sleeve=rear?Tag::RearSleeve:Tag::Sleeve;
    const int width=rear?3:4;
    if(!fore_only)stroke(sh,el,skin[1],arm,width,1);
    stroke(el,hand,skin[1],arm,width,1);
    stroke(el,hand,skin[2],arm,2,2);
    stroke({el.x-1,el.y},{hand.x-1,hand.y},skin[12],arm,1,12);
    stroke({el.x+1,el.y},{hand.x+1,hand.y},skin[3],arm,1,3);
    if(!fore_only) {
      auto cloth=color(eq[1]?items[eq[1]].color:shirt);
      int tone=rear?RearArmTone:255;
      stroke(sh,el,dim(color(0x2a252a),tone),sleeve,width);
      stroke(sh,el,dim(cloth,tone),sleeve,2);
      stroke({sh.x-1,sh.y},{el.x-1,el.y},dim(mix(cloth,ink,105),tone),sleeve);
      stroke({sh.x+1,sh.y},{el.x+1,el.y},dim(cloth,135*tone/255),sleeve);
      stroke({el.x-1,el.y},{el.x+1,el.y},dim(mix(cloth,ink,45),tone),sleeve);
    }
    // Fingers cover the cuff and keep both hands attached at every angle.
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)dot({hand.x+x,hand.y+y},skin[2],arm,2);
  };
  auto data = [&](int part) {
    return image<art::humans>(art::human_rows[part][frame]);
  };
  auto arm_data = [&](int part) {
    return image<art::humans>(art::human_rows[int(p) * art::HumanParts + part]
                                           [arm_frame]);
  };
  auto garment = [&](int part, uint32_t tint, Tag tag, int helmet = 0,
                     IndexedImage source = {}, int tone = 255,
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
    indexed(source.pixels ? source
            : helmet ? image<art::helmets>((helmet - 1) * art::HumanFrames + frame)
                     : data(part),
            palette, tag, dy);
  };
  if (eq[4])
    garment(5, items[eq[4]].color, Tag::Cape);
  if(aiming)aimed_arm(true);
  else {
    indexed(arm_data(13), rear_base, Tag::RearArm, arm_dy);
    garment(eq[1] ? 15 : 14, eq[1] ? items[eq[1]].color : shirt, Tag::RearSleeve,
          0, arm_data(eq[1] ? 15 : 14), RearArmTone, arm_dy);
  }
  // Assemble each leg with its own worn items before the camera leg covers it.
  indexed(data(17), rear_base, Tag::RearLeg);
  if (eq[2])
    garment(18, items[eq[2]].color, Tag::RearGreaves, 0, {}, RearArmTone);
  if (eq[5])
    garment(19, items[eq[5]].color, Tag::RearBoots, 0, {}, RearArmTone);
  indexed(data(16), base, Tag::CameraLeg);
  if (eq[2])
    garment(4, items[eq[2]].color, Tag::Greaves);
  if (eq[5])
    garment(6, items[eq[5]].color, Tag::Boots);
  indexed(data(0), base, Tag::Body);
  if (eq[3])
    garment(1, shirt, Tag::Clothing, 0,
            image<art::outfits>(((eq[3] == SageRobe   ? 2
                           : eq[3] == RedTunic ? 1
                                               : 0)) *
                             art::HumanFrames +
                         frame));
  if (eq[1])
    garment(3, items[eq[1]].color, Tag::Armour, 0,
            image<art::mail>((items[eq[1]].tier - 1) * art::HumanFrames + frame));
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
  if(aiming) {
    // The support upper arm stays behind the torso; its forearm crosses in
    // front to reach the bow, particularly while aiming straight down.
    aimed_arm(true,true);
    aimed_arm(false);
  }
  else {
    indexed(arm_data(10), base, Tag::Arm, arm_dy);
    garment(eq[1] ? 12 : 11, eq[1] ? items[eq[1]].color : shirt, Tag::Sleeve, 0,
          arm_data(eq[1] ? 12 : 11), 255, arm_dy);
  }
  if (a.held) {
    const auto h = joint(a, p == BowAim ? Offhand : Hand);
    const auto *grip = art::item_grips[a.held];
    constexpr int cos256[8] = {256, 181, 128, 240, 222, 0, 181, 256};
    constexpr int sin256[8] = {0, -181, -222, -88, 128, 256, 181, 0};
    auto rotate = [&](int v) {
      return v >= 0 ? (v + 128) / 256 : -((-v + 128) / 256);
    };
    const auto kind = items[a.held].kind;
    const bool compact = !tool(kind) && (kind < Pick || kind > Wand);
    if(aiming) {
      auto d=starsoil_aim::unit(a.face?a.aim_x:-a.aim_x,a.aim_y);
      auto at=[&](float x,float y) {return Point{h.x+int(std::lround(x*d.x-y*d.y)),
                                               h.y+int(std::lround(x*d.y+y*d.x))};};
      const Point limb[]={at(-6,-7),at(-3,-6),at(0,-2),at(0,1),at(-3,5),at(-6,6)};
      for(int pass=0;pass<2;++pass)for(int i=1;i<6;++i)
        stroke(limb[i-1],limb[i],pass?color(items[a.held].color):base[1],Tag::Weapon,pass?2:3);
      Point string=joint(a,Hand);
      stroke(limb[0],string,color(0xded7ad),Tag::Weapon);
      stroke(string,limb[5],color(0xded7ad),Tag::Weapon);
      if(a.bow_loaded) {
        stroke(string,at(7,0),color(0xd6c9a5),Tag::Weapon);
        stroke(at(7,0),at(4,-2),color(0xb6c6ca),Tag::Weapon);
        stroke(at(7,0),at(4,2),color(0xb6c6ca),Tag::Weapon);
      }
    } else if (compact) {
      // Materials and building previews are palm-sized. A full terrain icon
      // anchored like a sword can cover the face or an entire forearm.
      // Reduce opaque 2x2 cells without adding interpolated colors.
      uint16_t small[64]{};
      for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
          uint16_t c = art::icons[a.held][y * 16 + x];
          auto &out = small[y / 2 * 8 + x / 2];
          if (c && !out) out = c;
        }
      int gx = 4, gy = 4, closest = 100;
      for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
          if (small[y * 8 + x] && std::abs(x - 4) + std::abs(y - 4) < closest) {
            gx = x; gy = y; closest = std::abs(x - 4) + std::abs(y - 4);
          }
      for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
          if (small[y * 8 + x]) dot({h.x + x - gx, h.y + y - gy}, small[y * 8 + x], Tag::Weapon);
    } else for (int y = 0; y < 16; ++y)
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
    if (p == BowAim && !aiming) {
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
